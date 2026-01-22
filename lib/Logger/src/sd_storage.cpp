#include "sd_storage.h"
#include "rtc_manager.h"
#include <esp_crc.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <esp_log.h>

static const char* TAG = "SDStorage";

// Simple log block structure for SD storage (simplified version without compression)
struct __attribute__((packed)) log_block_t {
    uint32_t magic;          // LOG_BLOCK_MAGIC
    int64_t  timestamp_us;   // Timestamp in microseconds
    uint32_t data_size;      // Size of data following this header
    uint32_t block_crc;      // CRC32 of the data
};

// SD card pin configuration (from build flags)
#ifndef SD_CS_PIN
#define SD_CS_PIN 10
#endif
#ifndef SD_SCK_PIN
#define SD_SCK_PIN 36
#endif
#ifndef SD_MISO_PIN
#define SD_MISO_PIN 37
#endif
#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN 35
#endif

SDStorage::SDStorage()
    : m_card_present(false), m_sd_total_bytes(0),
      m_nvs_handle(0), m_bytes_written(0),
      m_sample_buffer_pos(0), m_block_timestamp_us(0),
      m_writer_task(nullptr), m_sample_queue(nullptr),
      m_queue_buffer(nullptr), m_queue_storage(nullptr),
      m_running(false), m_paused(false),
      m_queue_overruns(0), m_samples_queued(0) {
    memset(&m_session_header, 0, sizeof(m_session_header));
    memset(m_startup_id, 0, sizeof(m_startup_id));
    memset(m_sample_buffer, 0, sizeof(m_sample_buffer));
}

SDStorage::~SDStorage() {
    end();
}

bool SDStorage::begin(RTCManager* rtc_manager) {
    ESP_LOGI(TAG, "Initializing SD card storage...");

    // Restore system time from RTC or NVS if available
    if (rtc_manager != nullptr) {
        rtc_manager->restore_system_time_on_boot();
    }

    // Initialize SD card
    // NOTE: SPI.begin() already called by ST7789Display::init() in main.cpp
    // Both TFT and SD card share the same SPI bus (HSPI: GPIO35/36/37)
    // Calling SPI.begin() again would be redundant and could cause conflicts
    // The Adafruit display library auto-initializes SPI with correct pins

    if (!SD.begin(SD_CS_PIN)) {
        ESP_LOGE(TAG, "SD card initialization failed!");
        ESP_LOGE(TAG, "Check: 1) Card inserted 2) Wiring 3) Card format (FAT32)");
        m_card_present = false;
        return false;
    }

    m_card_present = true;

    // Get SD card info
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        ESP_LOGE(TAG, "No SD card attached");
        m_card_present = false;
        return false;
    }

    const char* cardTypeStr = "UNKNOWN";
    switch(cardType) {
        case CARD_MMC:  cardTypeStr = "MMC"; break;
        case CARD_SD:   cardTypeStr = "SDSC"; break;
        case CARD_SDHC: cardTypeStr = "SDHC"; break;
        default: break;
    }

    m_sd_total_bytes = SD.totalBytes();
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t usedBytes = SD.usedBytes();

    ESP_LOGI(TAG, "SD Card Type: %s", cardTypeStr);
    ESP_LOGI(TAG, "SD Card Size: %llu MB", cardSize);
    ESP_LOGI(TAG, "Total space: %llu MB", m_sd_total_bytes / (1024 * 1024));
    ESP_LOGI(TAG, "Used space: %llu MB", usedBytes / (1024 * 1024));

    // Create logs directory if it doesn't exist
    if (!SD.exists("/logs")) {
        if (!SD.mkdir("/logs")) {
            ESP_LOGE(TAG, "Failed to create /logs directory");
            return false;
        }
        ESP_LOGI(TAG, "Created /logs directory");
    }

    // Open NVS for session tracking
    esp_err_t err = nvs_open("sd_storage", NVS_READWRITE, &m_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %d", err);
        return false;
    }

    // Generate session UUID
    esp_fill_random(m_startup_id, 16);
    // Make it a valid UUIDv4
    m_startup_id[6] = (m_startup_id[6] & 0x0F) | 0x40;  // Version 4
    m_startup_id[8] = (m_startup_id[8] & 0x3F) | 0x80;  // Variant

    // Create session header
    memset(&m_session_header, 0, sizeof(m_session_header));
    m_session_header.magic = SESSION_START_MAGIC;
    m_session_header.version = 0x01;
    m_session_header.compression_type = COMPRESSION_NONE;
    m_session_header.rtc_available = (rtc_manager != nullptr) ? rtc_manager->is_available() : 0;
    memcpy(m_session_header.startup_id, m_startup_id, 16);
    m_session_header.esp_time_at_start = esp_timer_get_time();
    m_session_header.gps_utc_at_lock = 0;  // Updated when GPS locks

    // Get MAC address
    esp_read_mac(m_session_header.mac_addr, ESP_MAC_WIFI_STA);

    // Get firmware SHA (placeholder)
    memset(m_session_header.fw_sha, 0xAA, 8);

    // Get startup counter from NVS
    uint32_t counter = 0;
    nvs_get_u32(m_nvs_handle, "boot_count", &counter);
    counter++;
    nvs_set_u32(m_nvs_handle, "boot_count", counter);
    nvs_commit(m_nvs_handle);
    m_session_header.startup_counter = counter;

    ESP_LOGI(TAG, "Session UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             m_startup_id[0], m_startup_id[1], m_startup_id[2], m_startup_id[3],
             m_startup_id[4], m_startup_id[5], m_startup_id[6], m_startup_id[7],
             m_startup_id[8], m_startup_id[9], m_startup_id[10], m_startup_id[11],
             m_startup_id[12], m_startup_id[13], m_startup_id[14], m_startup_id[15]);

    // Create new session file
    if (!create_new_session_file()) {
        ESP_LOGE(TAG, "Failed to create session file");
        return false;
    }

    // Write session header
    write_session_header();

    // Create sample queue in PSRAM (not internal SRAM)
    // Allocate queue control structure in PSRAM
    m_queue_buffer = (StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_SPIRAM);
    if (!m_queue_buffer) {
        ESP_LOGE(TAG, "Failed to allocate queue buffer in PSRAM!");
        return false;
    }

    // Allocate queue storage area in PSRAM
    size_t queue_storage_size = QUEUE_SIZE * sizeof(SampleData);
    m_queue_storage = (uint8_t*)heap_caps_malloc(queue_storage_size, MALLOC_CAP_SPIRAM);
    if (!m_queue_storage) {
        ESP_LOGE(TAG, "Failed to allocate queue storage in PSRAM!");
        heap_caps_free(m_queue_buffer);
        m_queue_buffer = nullptr;
        return false;
    }

    // Create static queue using PSRAM-allocated buffers
    m_sample_queue = xQueueCreateStatic(QUEUE_SIZE, sizeof(SampleData),
                                        m_queue_storage, m_queue_buffer);
    if (!m_sample_queue) {
        ESP_LOGE(TAG, "Failed to create static queue!");
        heap_caps_free(m_queue_buffer);
        heap_caps_free(m_queue_storage);
        m_queue_buffer = nullptr;
        m_queue_storage = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "✓ Queue created in PSRAM (%zu bytes)", queue_storage_size);

    // Start writer task on Core 0
    m_running = true;
    BaseType_t result = xTaskCreatePinnedToCore(
        writer_task_wrapper,
        "sd_writer",
        8192,  // Stack size
        this,
        1,     // Low priority
        &m_writer_task,
        0      // Core 0
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create writer task");
        m_running = false;
        vQueueDelete(m_sample_queue);
        m_sample_queue = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "✓ SD storage initialized successfully");
    ESP_LOGI(TAG, "  Log file: %s", m_current_file_path.c_str());
    ESP_LOGI(TAG, "  Queue size: %d samples", QUEUE_SIZE);

    return true;
}

void SDStorage::end() {
    if (m_running) {
        m_running = false;

        // Wait for writer task to finish
        if (m_writer_task) {
            vTaskDelay(pdMS_TO_TICKS(100));
            vTaskDelete(m_writer_task);
            m_writer_task = nullptr;
        }

        // Flush and close file
        if (m_current_file) {
            flush_block();
            m_current_file.close();
        }

        // Clean up queue
        if (m_sample_queue) {
            vQueueDelete(m_sample_queue);
            m_sample_queue = nullptr;
        }

        // Free PSRAM allocations
        if (m_queue_storage) {
            heap_caps_free(m_queue_storage);
            m_queue_storage = nullptr;
        }
        if (m_queue_buffer) {
            heap_caps_free(m_queue_buffer);
            m_queue_buffer = nullptr;
        }
    }

    // Close NVS
    if (m_nvs_handle) {
        nvs_close(m_nvs_handle);
        m_nvs_handle = 0;
    }

    ESP_LOGI(TAG, "Shutdown complete. Final stats:");
    ESP_LOGI(TAG, "  Samples queued: %u", m_samples_queued);
    ESP_LOGI(TAG, "  Queue overruns: %u", m_queue_overruns);
    ESP_LOGI(TAG, "  Bytes written: %zu", m_bytes_written);
}

bool SDStorage::create_new_session_file() {
    // Generate filename: /logs/YYYYMMDD_HHMMSS_XXXXX.opl
    struct tm timeinfo;
    time_t now = time(nullptr);
    localtime_r(&now, &timeinfo);

    // Add random suffix to avoid collisions
    uint16_t random_suffix = esp_random() & 0xFFFF;

    char filename[64];
    snprintf(filename, sizeof(filename), "/logs/%04d%02d%02d_%02d%02d%02d_%04X.opl",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             random_suffix);

    m_current_file_path = String(filename);

    // Open file for writing
    m_current_file = SD.open(m_current_file_path.c_str(), FILE_WRITE);
    if (!m_current_file) {
        ESP_LOGE(TAG, "Failed to create file: %s", m_current_file_path.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Created log file: %s", m_current_file_path.c_str());
    m_bytes_written = 0;

    return true;
}

void SDStorage::write_session_header() {
    if (!m_current_file) return;

    // Update CRC
    m_session_header.crc32 = esp_crc32_le(0, (const uint8_t*)&m_session_header,
                                          sizeof(session_start_header_t) - sizeof(uint32_t));

    // Write header
    size_t written = m_current_file.write((const uint8_t*)&m_session_header,
                                           sizeof(session_start_header_t));
    if (written != sizeof(session_start_header_t)) {
        ESP_LOGE(TAG, "Failed to write session header");
    } else {
        m_bytes_written += written;
        m_current_file.flush();
        ESP_LOGI(TAG, "✓ Session header written (%zu bytes)", written);
    }
}

void SDStorage::write_sample(const gps_data_t& gps, const accel_data_t& accel,
                             const gyro_data_t& gyro, const compass_data_t& compass,
                             const battery_data_t& battery, const obd_data_t& obd) {
    if (!m_running || m_paused || !m_sample_queue) return;

    int64_t timestamp = esp_timer_get_time();

    // Queue all sensor samples
    SampleData samples[6];
    int count = 0;

    // GPS
    if (gps.valid) {
        samples[count].type = 0x01;
        samples[count].timestamp_us = timestamp;
        samples[count].data.gps.latitude = gps.latitude;
        samples[count].data.gps.longitude = gps.longitude;
        samples[count].data.gps.altitude = gps.altitude;
        samples[count].data.gps.speed = gps.speed;
        samples[count].data.gps.heading = 0;  // Note: GPS doesn't provide heading
        samples[count].data.gps.hdop = 0;     // Note: GPS doesn't provide HDOP
        count++;
    }

    // Accelerometer (always log if available)
    samples[count].type = 0x02;
    samples[count].timestamp_us = timestamp;
    samples[count].data.xyz.x = accel.x;
    samples[count].data.xyz.y = accel.y;
    samples[count].data.xyz.z = accel.z;
    count++;

    // Gyroscope (always log if available)
    samples[count].type = 0x03;
    samples[count].timestamp_us = timestamp;
    samples[count].data.xyz.x = gyro.x;
    samples[count].data.xyz.y = gyro.y;
    samples[count].data.xyz.z = gyro.z;
    count++;

    // Compass (always log if available)
    samples[count].type = 0x04;
    samples[count].timestamp_us = timestamp;
    samples[count].data.xyz.x = compass.x;
    samples[count].data.xyz.y = compass.y;
    samples[count].data.xyz.z = compass.z;
    count++;

    // Battery
    if (battery.valid) {
        samples[count].type = 0x05;
        samples[count].timestamp_us = timestamp;
        samples[count].data.battery.voltage = battery.voltage;
        samples[count].data.battery.current = battery.current;
        samples[count].data.battery.soc = battery.state_of_charge;
        count++;
    }

    // OBD (if valid and not already queued via queue_obd_sample)
    if (obd.valid && obd.timestamp_us == 0) {
        samples[count].type = 0x06;
        samples[count].timestamp_us = timestamp;
        samples[count].data.obd.rpm = obd.engine_rpm;
        samples[count].data.obd.speed = obd.vehicle_speed;
        samples[count].data.obd.throttle = obd.throttle_position;
        samples[count].data.obd.coolant_temp = obd.coolant_temp;
        samples[count].data.obd.maf = obd.maf_flow;
        samples[count].data.obd.intake_temp = obd.intake_temp;
        count++;
    }

    // Queue all samples
    for (int i = 0; i < count; i++) {
        if (xQueueSend(m_sample_queue, &samples[i], 0) != pdTRUE) {
            m_queue_overruns++;
        } else {
            m_samples_queued++;
        }
    }
}

void SDStorage::queue_obd_sample(const obd_data_t& obd) {
    if (!m_running || m_paused || !m_sample_queue || !obd.valid) return;

    SampleData sample;
    sample.type = 0x06;
    sample.timestamp_us = obd.timestamp_us;  // Use OBD's embedded timestamp
    sample.data.obd.rpm = obd.engine_rpm;
    sample.data.obd.speed = obd.vehicle_speed;
    sample.data.obd.throttle = obd.throttle_position;
    sample.data.obd.coolant_temp = obd.coolant_temp;
    sample.data.obd.maf = obd.maf_flow;
    sample.data.obd.intake_temp = obd.intake_temp;

    if (xQueueSend(m_sample_queue, &sample, 0) != pdTRUE) {
        m_queue_overruns++;
    } else {
        m_samples_queued++;
    }
}

void SDStorage::pause() {
    if (m_paused) return;
    ESP_LOGI(TAG, "Pausing storage...");
    m_paused = true;

    // Wait a bit to let queue drain
    vTaskDelay(pdMS_TO_TICKS(100));

    // Flush any pending data
    flush_block();

    if (m_current_file) {
        m_current_file.flush();
    }
}

void SDStorage::resume() {
    if (!m_paused) return;
    ESP_LOGI(TAG, "Resuming storage...");
    m_paused = false;
}

bool SDStorage::read_session_header(session_start_header_t* header) {
    if (!header || !m_current_file) return false;

    memcpy(header, &m_session_header, sizeof(session_start_header_t));
    return true;
}

void SDStorage::writer_task_wrapper(void* arg) {
    SDStorage* storage = static_cast<SDStorage*>(arg);
    storage->writer_task_loop();
}

void SDStorage::writer_task_loop() {
    ESP_LOGI(TAG, "Writer task started on Core %d", xPortGetCoreID());

    SampleData sample;
    TickType_t last_flush = xTaskGetTickCount();

    while (m_running) {
        // Try to receive sample (100ms timeout)
        if (xQueueReceive(m_sample_queue, &sample, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (m_paused) {
                continue;  // Skip writing when paused
            }

            // Set block timestamp from first sample
            if (m_sample_buffer_pos == 0) {
                m_block_timestamp_us = sample.timestamp_us;
            }

            // Serialize sample to buffer
            // Format: [type:1][timestamp:8][data:variable]
            if (m_sample_buffer_pos + 64 > SAMPLE_BUFFER_SIZE) {
                flush_block();
            }

            m_sample_buffer[m_sample_buffer_pos++] = sample.type;
            memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.timestamp_us, 8);
            m_sample_buffer_pos += 8;

            // Copy data based on type
            switch (sample.type) {
                case 0x01:  // GPS
                    memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.data.gps, sizeof(sample.data.gps));
                    m_sample_buffer_pos += sizeof(sample.data.gps);
                    break;
                case 0x02:  // Accel
                case 0x03:  // Gyro
                case 0x04:  // Compass
                    memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.data.xyz, sizeof(sample.data.xyz));
                    m_sample_buffer_pos += sizeof(sample.data.xyz);
                    break;
                case 0x05:  // Battery
                    memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.data.battery, sizeof(sample.data.battery));
                    m_sample_buffer_pos += sizeof(sample.data.battery);
                    break;
                case 0x06:  // OBD
                    memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.data.obd, sizeof(sample.data.obd));
                    m_sample_buffer_pos += sizeof(sample.data.obd);
                    break;
            }
        }

        // Flush periodically (every 5 seconds) even if buffer not full
        TickType_t now = xTaskGetTickCount();
        if ((now - last_flush) > pdMS_TO_TICKS(5000)) {
            if (m_sample_buffer_pos > 0) {
                flush_block();
            }
            last_flush = now;
        }
    }

    // Final flush on shutdown
    if (m_sample_buffer_pos > 0) {
        flush_block();
    }

    ESP_LOGI(TAG, "Writer task exiting");
}

void SDStorage::flush_block() {
    if (m_sample_buffer_pos == 0 || !m_current_file) return;

    // Create log block header
    log_block_t block;
    block.magic = LOG_BLOCK_MAGIC;
    block.timestamp_us = m_block_timestamp_us;
    block.data_size = m_sample_buffer_pos;
    block.block_crc = esp_crc32_le(0, m_sample_buffer, m_sample_buffer_pos);

    // Write block header
    size_t written = m_current_file.write((const uint8_t*)&block, sizeof(log_block_t));
    if (written != sizeof(log_block_t)) {
        ESP_LOGE(TAG, "Failed to write block header");
        return;
    }
    m_bytes_written += written;

    // Write block data
    written = m_current_file.write(m_sample_buffer, m_sample_buffer_pos);
    if (written != m_sample_buffer_pos) {
        ESP_LOGE(TAG, "Failed to write block data");
        return;
    }
    m_bytes_written += written;

    // Flush to SD card
    m_current_file.flush();

    // Reset buffer
    m_sample_buffer_pos = 0;
}
