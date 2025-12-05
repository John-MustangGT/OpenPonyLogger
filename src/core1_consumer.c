/**
 * @file core1_consumer.c
 * @brief Core 1 - Data Processing and Storage (Consumer)
 * 
 * Handles:
 * - Reading from inter-core ring buffer
 * - Maintaining live telemetry state for web interface
 * - Buffering data for SD card writes
 * - Managing log file rotation (15-minute files)
 * - Serving WiFi AP and HTTP endpoints
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "telemetry_types.h"
#include "ring_buffer.h"
#include "ff.h"  // FatFS for SD card
#include <stdio.h>
#include <string.h>

// Logging buffer configuration
#define LOG_BUFFER_SIZE (4 * 1024)     // 4KB write buffer
#define LOG_FLUSH_INTERVAL_US 60000000  // 1 minute = 60 seconds
#define LOG_FILE_DURATION_SEC (15 * 60) // 15 minutes

// Current values for web interface (latest sensor readings)
typedef struct {
    // GPS data
    gps_fix_t gps_fix;
    bool gps_valid;
    
    // Accelerometer data
    accel_data_t accel;
    bool accel_valid;
    
    // OBD-II data (stored by PID)
    struct {
        uint8_t pid;
        uint8_t data[8];
        uint8_t data_len;
        uint64_t timestamp_us;
        bool valid;
    } obd_values[32]; // Support up to 32 different PIDs
    
    // Session info
    uint64_t session_start_time;
    uint32_t messages_logged;
    bool recording;
} live_telemetry_t;

// Global state
extern ring_buffer_t g_telemetry_buffer;
static live_telemetry_t g_live_telemetry = {0};

// Logging state
static uint8_t g_log_buffer[LOG_BUFFER_SIZE];
static uint32_t g_log_buffer_used = 0;
static uint64_t g_last_flush_time = 0;
static uint64_t g_current_file_start_time = 0;

// FatFS objects
static FATFS g_fs;
static FIL g_log_file;
static bool g_sd_mounted = false;
static bool g_file_open = false;

// Statistics
static uint32_t g_messages_processed = 0;
static uint32_t g_bytes_written = 0;
static uint32_t g_file_count = 0;

/**
 * Generate log filename based on current time
 */
static void generate_log_filename(char *filename, size_t max_len) {
    // TODO: Use RTC time if available, otherwise use uptime-based name
    // For now, use a simple counter-based naming
    snprintf(filename, max_len, "log_%04u.opl", g_file_count);
}

/**
 * Write configuration header to log file
 */
static bool write_config_header(void) {
    // Configuration structure (can be expanded)
    typedef struct {
        uint8_t version[3];           // Firmware version
        uint32_t config_size;         // Size of this config block
        uint16_t gps_rate_hz;         // GPS sample rate
        uint16_t accel_rate_hz;       // Accelerometer sample rate
        uint16_t obd_rate_hz;         // OBD-II average sample rate
        char device_name[32];         // Device identifier
        uint8_t flags;                // Feature flags
    } __attribute__((packed)) config_header_t;
    
    config_header_t config = {
        .version = {1, 0, 0},
        .config_size = sizeof(config_header_t),
        .gps_rate_hz = 10,
        .accel_rate_hz = 10,
        .obd_rate_hz = 2,
        .device_name = "OpenPonyLogger-01",
        .flags = 0x07  // GPS + Accel + OBD enabled
    };
    
    UINT bytes_written;
    FRESULT res = f_write(&g_log_file, &config, sizeof(config), &bytes_written);
    
    if (res != FR_OK || bytes_written != sizeof(config)) {
        printf("[Core 1] Failed to write config header: %d\n", res);
        return false;
    }
    
    g_bytes_written += bytes_written;
    return true;
}

/**
 * Open a new log file
 */
static bool open_new_log_file(void) {
    if (g_file_open) {
        // Flush and close current file
        f_sync(&g_log_file);
        f_close(&g_log_file);
        g_file_open = false;
    }
    
    char filename[32];
    generate_log_filename(filename, sizeof(filename));
    
    FRESULT res = f_open(&g_log_file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        printf("[Core 1] Failed to open log file %s: %d\n", filename, res);
        return false;
    }
    
    printf("[Core 1] Opened new log file: %s\n", filename);
    g_file_open = true;
    g_file_count++;
    g_current_file_start_time = time_us_64();
    
    // Write configuration header
    return write_config_header();
}

/**
 * Check if we need to rotate to a new log file
 */
static bool should_rotate_file(void) {
    if (!g_file_open) {
        return true;
    }
    
    uint64_t current_time = time_us_64();
    uint64_t file_duration_us = current_time - g_current_file_start_time;
    uint64_t duration_threshold_us = LOG_FILE_DURATION_SEC * 1000000ULL;
    
    return (file_duration_us >= duration_threshold_us);
}

/**
 * Flush log buffer to SD card
 */
static bool flush_log_buffer(void) {
    if (!g_sd_mounted || !g_file_open || g_log_buffer_used == 0) {
        return false;
    }
    
    UINT bytes_written;
    FRESULT res = f_write(&g_log_file, g_log_buffer, g_log_buffer_used, &bytes_written);
    
    if (res != FR_OK) {
        printf("[Core 1] Failed to write to SD card: %d\n", res);
        return false;
    }
    
    if (bytes_written != g_log_buffer_used) {
        printf("[Core 1] Partial write: %u of %u bytes\n", bytes_written, g_log_buffer_used);
    }
    
    g_bytes_written += bytes_written;
    g_log_buffer_used = 0;
    g_last_flush_time = time_us_64();
    
    // Sync to ensure data is committed (important for power-loss scenarios)
    f_sync(&g_log_file);
    
    return true;
}

/**
 * Add data to log buffer, flush if necessary
 */
static bool append_to_log_buffer(const void *data, uint32_t size) {
    // Check if we need to flush due to size
    if (g_log_buffer_used + size > LOG_BUFFER_SIZE) {
        if (!flush_log_buffer()) {
            return false;
        }
    }
    
    // Check if we need to flush due to time
    uint64_t current_time = time_us_64();
    if ((current_time - g_last_flush_time) >= LOG_FLUSH_INTERVAL_US) {
        flush_log_buffer();
    }
    
    // Check if we need to rotate to a new file
    if (should_rotate_file()) {
        flush_log_buffer();
        if (!open_new_log_file()) {
            return false;
        }
    }
    
    // Append to buffer
    memcpy(g_log_buffer + g_log_buffer_used, data, size);
    g_log_buffer_used += size;
    
    return true;
}

/**
 * Update live telemetry state based on message type
 */
static void update_live_telemetry(const telemetry_msg_t *msg, const uint8_t *payload) {
    switch (msg->sensor) {
        case SENSOR_GPS:
            if (msg->data_type == DATA_GPS_FIX) {
                memcpy(&g_live_telemetry.gps_fix, payload, sizeof(gps_fix_t));
                g_live_telemetry.gps_valid = true;
            }
            break;
            
        case SENSOR_ACCELEROMETER:
            if (msg->data_type == DATA_ACCEL_COMBINED) {
                memcpy(&g_live_telemetry.accel, payload, sizeof(accel_data_t));
                g_live_telemetry.accel_valid = true;
            }
            break;
            
        case SENSOR_OBD_II:
            if (msg->data_type == DATA_OBD_PID) {
                obd_pid_t *pid_data = (obd_pid_t *)payload;
                
                // Find or create slot for this PID
                int slot = -1;
                for (int i = 0; i < 32; i++) {
                    if (!g_live_telemetry.obd_values[i].valid || 
                        g_live_telemetry.obd_values[i].pid == pid_data->pid) {
                        slot = i;
                        break;
                    }
                }
                
                if (slot >= 0) {
                    g_live_telemetry.obd_values[slot].pid = pid_data->pid;
                    g_live_telemetry.obd_values[slot].data_len = pid_data->data_len;
                    memcpy(g_live_telemetry.obd_values[slot].data, pid_data->data, pid_data->data_len);
                    g_live_telemetry.obd_values[slot].timestamp_us = msg->timestamp_us;
                    g_live_telemetry.obd_values[slot].valid = true;
                }
            }
            break;
            
        case SENSOR_SYSTEM:
            if (msg->data_type == DATA_SESSION_START) {
                g_live_telemetry.session_start_time = msg->timestamp_us;
                g_live_telemetry.messages_logged = 0;
                g_live_telemetry.recording = true;
            }
            break;
    }
}

/**
 * Process messages from ring buffer
 */
static void process_ring_buffer(void) {
    uint8_t msg_buffer[512]; // Temporary buffer for reading messages
    
    while (true) {
        uint32_t bytes_read = ring_buffer_read_message(&g_telemetry_buffer, 
                                                        msg_buffer, 
                                                        sizeof(msg_buffer));
        
        if (bytes_read == 0) {
            // No more messages available
            break;
        }
        
        telemetry_msg_t *msg = (telemetry_msg_t *)msg_buffer;
        uint8_t *payload = msg_buffer + TELEMETRY_MSG_HEADER_SIZE;
        
        // Update live telemetry state for web interface
        update_live_telemetry(msg, payload);
        
        // Write complete message to log buffer
        if (append_to_log_buffer(msg_buffer, bytes_read)) {
            g_messages_processed++;
            g_live_telemetry.messages_logged++;
        } else {
            printf("[Core 1] Failed to append message to log buffer\n");
        }
    }
}

/**
 * Initialize SD card and FatFS
 */
static bool init_sd_card(void) {
    // TODO: Initialize SPI for SD card
    printf("[Core 1] Initializing SD card...\n");
    
    FRESULT res = f_mount(&g_fs, "", 1);
    if (res != FR_OK) {
        printf("[Core 1] Failed to mount SD card: %d\n", res);
        return false;
    }
    
    g_sd_mounted = true;
    printf("[Core 1] SD card mounted successfully\n");
    
    return open_new_log_file();
}

/**
 * Get live telemetry data for web interface (thread-safe copy)
 */
void core1_get_live_telemetry(live_telemetry_t *dest) {
    memcpy(dest, &g_live_telemetry, sizeof(live_telemetry_t));
}

/**
 * Get Core 1 statistics
 */
void core1_get_stats(uint32_t *messages_processed, uint32_t *bytes_written, 
                     uint32_t *buffer_used, bool *sd_ok) {
    if (messages_processed) *messages_processed = g_messages_processed;
    if (bytes_written) *bytes_written = g_bytes_written;
    if (buffer_used) *buffer_used = g_log_buffer_used;
    if (sd_ok) *sd_ok = g_sd_mounted && g_file_open;
}

/**
 * Stop recording and close current log file
 */
void core1_stop_recording(void) {
    printf("[Core 1] Stopping recording...\n");
    
    // Flush any remaining data
    flush_log_buffer();
    
    // Write session end message
    session_end_t session_end = {
        .total_messages = g_messages_processed,
        .dropped_messages = g_telemetry_buffer.meta.dropped_count,
        .duration_sec = (uint32_t)((time_us_64() - g_live_telemetry.session_start_time) / 1000000),
        .file_size_bytes = g_bytes_written
    };
    
    append_to_log_buffer(&session_end, sizeof(session_end_t));
    flush_log_buffer();
    
    // Close file
    if (g_file_open) {
        f_close(&g_log_file);
        g_file_open = false;
    }
    
    g_live_telemetry.recording = false;
}

/**
 * Main processing loop for Core 1
 */
void core1_main_loop(void) {
    printf("[Core 1] Processing and storage loop started\n");
    
    while (true) {
        // Process any pending messages from ring buffer
        process_ring_buffer();
        
        // Check if we need to flush due to time
        uint64_t current_time = time_us_64();
        if ((current_time - g_last_flush_time) >= LOG_FLUSH_INTERVAL_US) {
            flush_log_buffer();
        }
        
        // TODO: Handle web interface requests (AJAX polling)
        // TODO: Service WiFi stack
        
        // Small sleep to prevent busy-waiting
        sleep_ms(10); // 10ms sleep = 100Hz loop rate
    }
}

/**
 * Entry point for Core 1
 */
void core1_entry(void) {
    printf("[Core 1] Starting processing and storage core\n");
    
    // Initialize SD card
    if (!init_sd_card()) {
        printf("[Core 1] WARNING: SD card initialization failed, logging disabled\n");
    }
    
    // TODO: Initialize WiFi AP
    // TODO: Initialize HTTP server
    // TODO: Initialize AJAX endpoints
    
    // Start the main processing loop (never returns)
    core1_main_loop();
}
