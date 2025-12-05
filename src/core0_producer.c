/**
 * @file core0_producer.c
 * @brief Core 0 - Data Acquisition (Producer)
 * 
 * Handles all sensor polling and data collection:
 * - GPS UART reading (10Hz target)
 * - Accelerometer I2C polling (10Hz target)
 * - OBD-II Bluetooth LE queries (1-5Hz)
 * 
 * All data is timestamped and pushed to the inter-core ring buffer.
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/timer.h"
#include "telemetry_types.h"
#include "ring_buffer.h"
#include <stdio.h>

// Sensor polling intervals (microseconds)
#define GPS_POLL_INTERVAL_US      100000  // 100ms = 10Hz
#define ACCEL_POLL_INTERVAL_US    100000  // 100ms = 10Hz
#define OBD_POLL_INTERVAL_US      500000  // 500ms = 2Hz (conservative for BLE)

// PID polling configuration
#define MAX_OBD_PIDS 16

typedef struct {
    uint8_t mode;
    uint8_t pid;
    uint32_t interval_us;      // Poll interval for this specific PID
    uint64_t last_poll_time;   // Last time this PID was polled
    bool enabled;
} obd_pid_config_t;

// Global ring buffer (shared with Core 1)
extern ring_buffer_t g_telemetry_buffer;

// OBD-II PID configuration table
static obd_pid_config_t g_obd_pids[MAX_OBD_PIDS] = {
    // High-priority PIDs (faster polling)
    {0x01, 0x0C, 100000, 0, true},  // RPM - 10Hz
    {0x01, 0x0D, 100000, 0, true},  // Speed - 10Hz
    {0x01, 0x11, 100000, 0, true},  // Throttle - 10Hz
    
    // Medium-priority PIDs
    {0x01, 0x04, 500000, 0, true},  // Engine load - 2Hz
    {0x01, 0x05, 1000000, 0, true}, // Coolant temp - 1Hz
    {0x01, 0x0F, 1000000, 0, true}, // Intake air temp - 1Hz
    
    // Low-priority PIDs
    {0x01, 0x2F, 5000000, 0, true}, // Fuel level - 0.2Hz
    {0x01, 0x46, 5000000, 0, true}, // Ambient temp - 0.2Hz
    
    // Remaining slots for user-configured PIDs
    {0, 0, 0, 0, false},
    {0, 0, 0, 0, false},
    {0, 0, 0, 0, false},
    {0, 0, 0, 0, false},
    {0, 0, 0, 0, false},
    {0, 0, 0, 0, false},
    {0, 0, 0, 0, false},
    {0, 0, 0, 0, false},
};

// Timing state
static uint64_t last_gps_poll = 0;
static uint64_t last_accel_poll = 0;

// Statistics
static uint32_t gps_messages_sent = 0;
static uint32_t accel_messages_sent = 0;
static uint32_t obd_messages_sent = 0;

/**
 * Get current timestamp with microsecond precision
 */
static inline uint64_t get_timestamp_us(void) {
    return time_us_64();
}

/**
 * Write a message to the ring buffer with error handling
 */
static bool write_telemetry_message(sensor_type_t sensor,
                                    data_type_t data_type,
                                    time_source_t time_source,
                                    const void *payload,
                                    uint16_t payload_len) {
    telemetry_msg_t msg;
    msg.timestamp_us = get_timestamp_us();
    msg.time_source = time_source;
    msg.sensor = sensor;
    msg.data_type = data_type;
    msg.length = payload_len;
    
    bool success = ring_buffer_write_message(&g_telemetry_buffer, &msg, payload, payload_len);
    
    if (!success) {
        // Could increment an error counter or flash LED here
        printf("[Core 0] Warning: Ring buffer full, message dropped\n");
    }
    
    return success;
}

/**
 * Poll GPS module (placeholder - actual UART reading would go here)
 */
static void poll_gps(void) {
    // TODO: Actual GPS UART reading implementation
    // For now, just a placeholder that shows the structure
    
    // Example NMEA sentence (in real implementation, read from UART)
    const char *example_nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    uint16_t nmea_len = strlen(example_nmea) + 1; // Include null terminator
    
    if (write_telemetry_message(SENSOR_GPS, DATA_GPS_NMEA, TIME_SOURCE_UPTIME,
                                 example_nmea, nmea_len)) {
        gps_messages_sent++;
    }
    
    // Also write parsed fix data
    gps_fix_t fix = {
        .latitude = 42.2793,
        .longitude = -71.4162,
        .altitude = 525.0,
        .speed = 18.5,
        .heading = 135.0,
        .fix_quality = 1,
        .satellites = 8,
        .hdop = 0.9
    };
    
    write_telemetry_message(SENSOR_GPS, DATA_GPS_FIX, TIME_SOURCE_GPS,
                           &fix, sizeof(gps_fix_t));
}

/**
 * Poll accelerometer (placeholder - actual I2C reading would go here)
 */
static void poll_accelerometer(void) {
    // TODO: Actual LIS3DH I2C reading implementation
    // For now, just a placeholder with example data
    
    accel_data_t accel = {
        .accel_x = 0.12,  // Lateral G
        .accel_y = -0.25, // Longitudinal G
        .accel_z = 1.02,  // Vertical G
        .gyro_x = 0.0,
        .gyro_y = 0.0,
        .gyro_z = 0.0
    };
    
    if (write_telemetry_message(SENSOR_ACCELEROMETER, DATA_ACCEL_COMBINED,
                                 TIME_SOURCE_UPTIME, &accel, sizeof(accel_data_t))) {
        accel_messages_sent++;
    }
}

/**
 * Poll OBD-II PIDs that are due
 */
static void poll_obd_ii(void) {
    uint64_t current_time = get_timestamp_us();
    
    for (int i = 0; i < MAX_OBD_PIDS; i++) {
        obd_pid_config_t *pid_config = &g_obd_pids[i];
        
        // Skip disabled PIDs
        if (!pid_config->enabled) {
            continue;
        }
        
        // Check if it's time to poll this PID
        if ((current_time - pid_config->last_poll_time) >= pid_config->interval_us) {
            // TODO: Actual BLE OBD-II query implementation
            // For now, just create example response
            
            obd_pid_t response = {
                .mode = pid_config->mode,
                .pid = pid_config->pid,
                .data_len = 2,
                .data = {0x1F, 0x40, 0, 0, 0, 0, 0, 0} // Example: RPM = 2000
            };
            
            if (write_telemetry_message(SENSOR_OBD_II, DATA_OBD_PID,
                                        TIME_SOURCE_UPTIME, &response, sizeof(obd_pid_t))) {
                obd_messages_sent++;
            }
            
            // Update last poll time
            pid_config->last_poll_time = current_time;
        }
    }
}

/**
 * Add a custom PID to the polling list
 */
bool core0_add_custom_pid(uint8_t mode, uint8_t pid, uint32_t interval_us) {
    // Find first available slot
    for (int i = 0; i < MAX_OBD_PIDS; i++) {
        if (!g_obd_pids[i].enabled) {
            g_obd_pids[i].mode = mode;
            g_obd_pids[i].pid = pid;
            g_obd_pids[i].interval_us = interval_us;
            g_obd_pids[i].last_poll_time = 0;
            g_obd_pids[i].enabled = true;
            return true;
        }
    }
    return false; // No slots available
}

/**
 * Remove a custom PID from the polling list
 */
bool core0_remove_custom_pid(uint8_t mode, uint8_t pid) {
    for (int i = 0; i < MAX_OBD_PIDS; i++) {
        if (g_obd_pids[i].enabled && 
            g_obd_pids[i].mode == mode && 
            g_obd_pids[i].pid == pid) {
            g_obd_pids[i].enabled = false;
            return true;
        }
    }
    return false;
}

/**
 * Get Core 0 statistics
 */
void core0_get_stats(uint32_t *gps_count, uint32_t *accel_count, uint32_t *obd_count) {
    if (gps_count) *gps_count = gps_messages_sent;
    if (accel_count) *accel_count = accel_messages_sent;
    if (obd_count) *obd_count = obd_messages_sent;
}

/**
 * Main scheduling loop for Core 0
 * 
 * This runs continuously, polling sensors at their configured rates
 * and writing data to the ring buffer.
 */
void core0_main_loop(void) {
    printf("[Core 0] Data acquisition loop started\n");
    
    // Send session start message
    session_start_t session_start = {
        .session_id = "20250102_143027",
        .firmware_version = {1, 0, 0},
        .gps_module_type = 0, // ATGM336H
        .accel_module_type = 0, // LIS3DH
        .config_flags = 0x0007 // GPS + Accel + OBD enabled
    };
    
    write_telemetry_message(SENSOR_SYSTEM, DATA_SESSION_START,
                           TIME_SOURCE_UPTIME, &session_start, sizeof(session_start_t));
    
    while (true) {
        uint64_t current_time = get_timestamp_us();
        
        // Poll GPS at configured rate
        if ((current_time - last_gps_poll) >= GPS_POLL_INTERVAL_US) {
            poll_gps();
            last_gps_poll = current_time;
        }
        
        // Poll accelerometer at configured rate
        if ((current_time - last_accel_poll) >= ACCEL_POLL_INTERVAL_US) {
            poll_accelerometer();
            last_accel_poll = current_time;
        }
        
        // Poll OBD-II PIDs (each PID has its own schedule)
        poll_obd_ii();
        
        // Small sleep to prevent busy-waiting
        // This allows other interrupts/DMA to run
        sleep_us(100); // 100µs sleep = minimal impact on timing
    }
}

/**
 * Entry point for Core 0
 */
void core0_entry(void) {
    printf("[Core 0] Starting data acquisition core\n");
    
    // Initialize hardware interfaces
    // TODO: Initialize GPS UART
    // TODO: Initialize Accelerometer I2C
    // TODO: Initialize BLE stack for OBD-II
    
    // Start the main scheduling loop (never returns)
    core0_main_loop();
}
