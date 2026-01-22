#include "flash_storage.h"
#include "rtc_manager.h"
#include <esp_crc.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <esp_log.h>

static const char* TAG = "FlashStorage";

FlashStorage::FlashStorage()
    : m_partition(nullptr), m_nvs_handle(0),
      m_partition_size(0), m_write_offset(0), m_session_start_offset(0),
      m_bytes_written(0), m_sample_buffer_pos(0), m_block_timestamp_us(0),
      m_writer_task(nullptr), m_sample_queue(nullptr),
      m_queue_buffer(nullptr), m_queue_storage(nullptr),
      m_running(false), m_paused(false),
      m_queue_overruns(0), m_samples_queued(0) {
    memset(&m_session_header, 0, sizeof(m_session_header));
    memset(m_startup_id, 0, sizeof(m_startup_id));
    memset(m_sample_buffer, 0, sizeof(m_sample_buffer));
}

FlashStorage::~FlashStorage() {
    end();
}

bool FlashStorage::begin(RTCManager* rtc_manager) {
    ESP_LOGI(TAG, "Initializing...");
    
    // Restore system time from RTC or NVS if available
    if (rtc_manager != nullptr) {
        rtc_manager->restore_system_time_on_boot();
    }
    
    // Find storage partition
    m_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "storage"
    );
    
    if (!m_partition) {
        ESP_LOGE(TAG, "Storage partition not found!");
        return false;
    }
    
    m_partition_size = m_partition->size;
    ESP_LOGI(TAG, "Found partition: size=%d bytes (%.2f MB)",
                  m_partition_size, m_partition_size / (1024.0f * 1024.0f));
    
    // Open NVS for offset tracking
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &m_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %d", err);
        return false;
    }
    
    // Load write offset from NVS (or start fresh)
    size_t saved_offset = 0;
    err = nvs_get_u32(m_nvs_handle, "write_offset", (uint32_t*)&saved_offset);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded offset from NVS: %d", saved_offset);
        m_write_offset = saved_offset;
    } else {
        ESP_LOGI(TAG, "Starting fresh (no saved offset)");
        m_write_offset = 0;
    }
    
    m_session_start_offset = m_write_offset;
    
    // Generate session UUID
    esp_fill_random(m_startup_id, 16);
    // Make it a valid UUIDv4
    m_startup_id[6] = (m_startup_id[6] & 0x0F) | 0x40;  // Version 4
    m_startup_id[8] = (m_startup_id[8] & 0x3F) | 0x80;  // Variant
    
    // Create session header
    memset(&m_session_header, 0, sizeof(m_session_header));
    m_session_header.magic = SESSION_START_MAGIC;
    m_session_header.version = 0x01;
    m_session_header.compression_type = COMPRESSION_NONE;  // No compression for now
    m_session_header.rtc_available = (rtc_manager != nullptr) ? rtc_manager->is_available() : 0;
    memcpy(m_session_header.startup_id, m_startup_id, 16);
    m_session_header.esp_time_at_start = esp_timer_get_time();
    m_session_header.gps_utc_at_lock = 0;  // Will be updated when GPS locks
    
    // Get MAC address
    esp_read_mac(m_session_header.mac_addr, ESP_MAC_WIFI_STA);
    
    // Get firmware SHA (would come from version_info.h in real impl)
    memset(m_session_header.fw_sha, 0xAA, 8);  // Placeholder
    
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
    ESP_LOGI(TAG, "RTC available: %d", m_session_header.rtc_available);
    
    // Write session header to flash
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
    
    // Start writer task on Core 0 with low priority
    // Core 1 writes to PSRAM queue (fast, non-blocking), Core 0 drains to flash (slow, blocking OK)
    // This decouples real-time sensor reads (Core 1) from blocking flash operations (Core 0)
    m_running = true;
    BaseType_t result = xTaskCreatePinnedToCore(
        writer_task_wrapper,
        "FlashWriter",
        8192,              // Stack size
        this,              // Parameter
        1,                 // Priority (low)
        &m_writer_task,
        0                  // Core 0 - handles blocking flash writes while Core 1 stays responsive
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create writer task!");
        m_running = false;
        return false;
    }
    
    ESP_LOGI(TAG, "Started successfully on Core 0 (blocking flash ops isolated from Core 1 sensors)");
    return true;
}

void FlashStorage::end() {
    if (m_running) {
        ESP_LOGI(TAG, "Stopping...");
        
        // Flush any pending data
        flush_block();
        
        m_running = false;
        
        if (m_writer_task) {
            vTaskDelete(m_writer_task);
            m_writer_task = nullptr;
        }
        
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

        // Save final offset
        save_offset_to_nvs();
        
        if (m_nvs_handle) {
            nvs_close(m_nvs_handle);
            m_nvs_handle = 0;
        }

        ESP_LOGI(TAG, "Stopped");
    }
}

void FlashStorage::write_sample(const gps_data_t& gps, const accel_data_t& accel,
                                const gyro_data_t& gyro, const compass_data_t& compass,
                                const battery_data_t& battery, const obd_data_t& obd) {
    if (!m_running || m_paused) {
        return;
    }
    
    int64_t now = esp_timer_get_time();

    // Helper lambda for non-blocking queue send with monitoring
    // Returns true if queued, false if queue full (overrun)
    auto queue_sample = [this](SampleData& sample) -> bool {
        if (xQueueSend(m_sample_queue, &sample, 0) == pdTRUE) {
            m_samples_queued++;
            return true;
        } else {
            m_queue_overruns++;
            // Warn on first overrun, then every 100 overruns
            if (m_queue_overruns == 1 || m_queue_overruns % 100 == 0) {
                ESP_LOGW(TAG, "Queue overrun! Dropped %u samples (Core 0 can't drain fast enough)",
                              m_queue_overruns);
            }
            return false;
        }
    };

    // Queue accelerometer (Core 1 → PSRAM queue → Core 0, non-blocking)
    SampleData sample;
    sample.type = 0x01;  // SAMPLE_ACCEL
    sample.timestamp_us = now;
    sample.data.xyz.x = accel.x;
    sample.data.xyz.y = accel.y;
    sample.data.xyz.z = accel.z;
    queue_sample(sample);
    
    // Queue gyroscope
    sample.type = 0x02;  // SAMPLE_GYRO
    sample.data.xyz.x = gyro.x;
    sample.data.xyz.y = gyro.y;
    sample.data.xyz.z = gyro.z;
    queue_sample(sample);

    // Queue compass
    sample.type = 0x03;  // SAMPLE_COMPASS
    sample.data.xyz.x = compass.x;
    sample.data.xyz.y = compass.y;
    sample.data.xyz.z = compass.z;
    queue_sample(sample);
    
    // Queue GPS if valid
    if (gps.valid) {
        // Update session header with GPS time on first lock
        if (m_session_header.gps_utc_at_lock == 0) {
            // Convert GPS time to Unix timestamp
            // This is simplified - real impl would parse GPS date/time properly
            // For now, just use current system time when GPS lock occurs
            m_session_header.gps_utc_at_lock = time(nullptr);
            ESP_LOGI(TAG, "GPS lock acquired");
        }
        
        sample.type = 0x04;  // SAMPLE_GPS
        sample.data.gps.latitude = gps.latitude;
        sample.data.gps.longitude = gps.longitude;
        sample.data.gps.altitude = gps.altitude;
        sample.data.gps.speed = gps.speed;
        // Note: heading and hdop not in gps_data_t, would need derived from other data
        queue_sample(sample);
    }
    
    // Queue OBD-II data if valid
    if (obd.engine_rpm > 0 || obd.vehicle_speed > 0) {  // Simple validity check
        sample.type = 0x06;  // SAMPLE_OBD
        sample.data.obd.rpm = obd.engine_rpm;
        sample.data.obd.speed = obd.vehicle_speed;
        sample.data.obd.throttle = obd.throttle_position;
        sample.data.obd.coolant_temp = obd.coolant_temp;
        sample.data.obd.maf = obd.maf_flow;
        sample.data.obd.intake_temp = obd.intake_temp;
        queue_sample(sample);
    }
    
    // Queue battery
    sample.type = 0x07;  // SAMPLE_BATTERY
    sample.data.battery.voltage = battery.voltage;
    sample.data.battery.current = battery.current;
    sample.data.battery.soc = battery.state_of_charge;
    queue_sample(sample);
}

void FlashStorage::queue_obd_sample(const obd_data_t& obd) {
    if (!m_running || m_paused || !obd.valid) {
        return;
    }

    // Helper lambda for non-blocking queue send with monitoring
    auto queue_sample = [this](SampleData& sample) -> bool {
        if (xQueueSend(m_sample_queue, &sample, 0) == pdTRUE) {
            m_samples_queued++;
            return true;
        } else {
            m_queue_overruns++;
            // Warn on first overrun, then every 100 overruns
            if (m_queue_overruns == 1 || m_queue_overruns % 100 == 0) {
                ESP_LOGW(TAG, "Queue overrun! Dropped %u samples (Core 0 can't drain fast enough)",
                              m_queue_overruns);
            }
            return false;
        }
    };

    // Queue OBD sample with its embedded timestamp
    SampleData sample;
    sample.type = 0x06;  // SAMPLE_OBD
    sample.timestamp_us = obd.timestamp_us;  // Use the timestamp captured when data arrived
    sample.data.obd.rpm = obd.engine_rpm;
    sample.data.obd.speed = obd.vehicle_speed;
    sample.data.obd.throttle = obd.throttle_position;
    sample.data.obd.coolant_temp = obd.coolant_temp;
    sample.data.obd.maf = obd.maf_flow;
    sample.data.obd.intake_temp = obd.intake_temp;
    queue_sample(sample);
}

void FlashStorage::pause() {
    ESP_LOGI(TAG, "Pausing writes...");
    m_paused = true;

    // Flush pending block
    flush_block();
}

void FlashStorage::resume() {
    ESP_LOGI(TAG, "Resuming writes...");
    m_paused = false;
}

void FlashStorage::writer_task_wrapper(void* arg) {
    FlashStorage* storage = static_cast<FlashStorage*>(arg);
    if (storage) {
        storage->writer_task_loop();
    }
}

void FlashStorage::writer_task_loop() {
    ESP_LOGI(TAG, "Writer task started on Core 0 - draining PSRAM queue to flash");
    ESP_LOGI(TAG, "Queue buffer: %u samples (~%.1f seconds at 10Hz)",
                  QUEUE_SIZE, QUEUE_SIZE / 10.0f);

    SampleData sample;
    TickType_t last_flush = xTaskGetTickCount();
    const TickType_t flush_interval = pdMS_TO_TICKS(5000);  // Flush every 5 seconds

    // Queue health monitoring
    uint32_t last_health_check = millis();
    const uint32_t health_check_interval = 30000;  // Report queue health every 30 seconds

    while (m_running) {
        // Receive samples from queue with timeout
        if (xQueueReceive(m_sample_queue, &sample, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (m_paused) {
                continue;  // Drop samples while paused
            }
            
            // Set block timestamp on first sample
            if (m_sample_buffer_pos == 0) {
                m_block_timestamp_us = sample.timestamp_us;
            }
            
            // Calculate timestamp delta from block start
            int64_t delta = sample.timestamp_us - m_block_timestamp_us;
            if (delta < 0) delta = 0;
            uint32_t delta_us = (uint32_t)delta;
            
            // Check if buffer has space for this sample
            size_t sample_size = 1 + 4;  // type + timestamp_delta
            if (sample.type == 0x01 || sample.type == 0x02 || sample.type == 0x03) {
                sample_size += 12;  // xyz floats
            } else if (sample.type == 0x04) {
                sample_size += 32;  // GPS data
            } else if (sample.type == 0x07) {
                sample_size += 12;  // Battery data
            }
            
            if (m_sample_buffer_pos + sample_size > SAMPLE_BUFFER_SIZE) {
                // Buffer full - flush it
                flush_block();
                m_block_timestamp_us = sample.timestamp_us;
                delta_us = 0;
            }
            
            // Write sample to buffer
            m_sample_buffer[m_sample_buffer_pos++] = sample.type;
            memcpy(&m_sample_buffer[m_sample_buffer_pos], &delta_us, 4);
            m_sample_buffer_pos += 4;
            
            if (sample.type == 0x01 || sample.type == 0x02 || sample.type == 0x03) {
                memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.data.xyz, 12);
                m_sample_buffer_pos += 12;
            } else if (sample.type == 0x04) {
                memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.data.gps, 32);
                m_sample_buffer_pos += 32;
            } else if (sample.type == 0x07) {
                memcpy(&m_sample_buffer[m_sample_buffer_pos], &sample.data.battery, 12);
                m_sample_buffer_pos += 12;
            }
        }
        
        // Periodic flush
        if (xTaskGetTickCount() - last_flush >= flush_interval) {
            if (m_sample_buffer_pos > 0 && !m_paused) {
                flush_block();
            }
            last_flush = xTaskGetTickCount();
        }

        // Periodic queue health check
        uint32_t now_ms = millis();
        if (now_ms - last_health_check >= health_check_interval) {
            UBaseType_t queue_waiting = uxQueueMessagesWaiting(m_sample_queue);
            UBaseType_t queue_available = uxQueueSpacesAvailable(m_sample_queue);
            float queue_usage_pct = (queue_waiting * 100.0f) / QUEUE_SIZE;

            ESP_LOGI(TAG, "Queue health: %u/%u used (%.1f%%), %u overruns, %u queued",
                          queue_waiting, QUEUE_SIZE, queue_usage_pct, m_queue_overruns, m_samples_queued);

            if (queue_usage_pct > 80.0f) {
                ESP_LOGW(TAG, "Queue >80%% full! Core 0 struggling to keep up with Core 1");
            }

            last_health_check = now_ms;
        }
    }

    ESP_LOGI(TAG, "Writer task exiting");
}

void FlashStorage::flush_block() {
    if (m_sample_buffer_pos == 0) {
        return;  // Nothing to flush
    }
    
    // No compression for now - store data as-is
    // For future: can add heatshrink or other compression here
    uint8_t* data_to_write = m_sample_buffer;
    size_t data_size = m_sample_buffer_pos;
    
    // Calculate CRC32 of data
    uint32_t crc = esp_crc32_le(0, data_to_write, data_size);
    
    // Create block header
    log_block_header_t block_header;
    memset(&block_header, 0, sizeof(block_header));
    block_header.magic = LOG_BLOCK_MAGIC;
    block_header.version = 0x01;
    memcpy(block_header.startup_id, m_startup_id, 16);
    block_header.timestamp_us = m_block_timestamp_us;
    block_header.uncompressed_size = m_sample_buffer_pos;
    block_header.compressed_size = data_size;  // Same size, no compression
    block_header.crc32 = crc;
    
    // Calculate total block size
    size_t total_size = sizeof(log_block_header_t) + data_size;
    
    // Check if we need to wrap around
    if (m_write_offset + total_size > m_partition_size) {
        ESP_LOGI(TAG, "Wrapping circular buffer...");
        m_write_offset = sizeof(session_start_header_t);  // Start after session header
    }
    
    // Erase sectors if needed (flash must be erased before writing)
    size_t erase_start = m_write_offset & ~(SPI_FLASH_SEC_SIZE - 1);
    size_t erase_end = (m_write_offset + total_size + SPI_FLASH_SEC_SIZE - 1) & ~(SPI_FLASH_SEC_SIZE - 1);
    
    for (size_t addr = erase_start; addr < erase_end; addr += SPI_FLASH_SEC_SIZE) {
        if (addr < m_partition_size) {
            esp_partition_erase_range(m_partition, addr, SPI_FLASH_SEC_SIZE);
        }
    }
    
    // Write block header
    esp_err_t err = esp_partition_write(m_partition, m_write_offset,
                                       &block_header, sizeof(log_block_header_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write block header: %d", err);
        m_sample_buffer_pos = 0;
        return;
    }
    
    m_write_offset += sizeof(log_block_header_t);
    
    // Write data (uncompressed)
    err = esp_partition_write(m_partition, m_write_offset,
                             data_to_write, data_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write payload: %d", err);
        m_sample_buffer_pos = 0;
        return;
    }
    
    m_write_offset += data_size;
    m_bytes_written += total_size;
    
    // Save offset periodically (every 10 blocks)
    static int block_count = 0;
    if (++block_count >= 10) {
        save_offset_to_nvs();
        block_count = 0;
    }
    
    // Debug output (throttled)
    static uint32_t last_debug = 0;
    if (millis() - last_debug > 10000) {
        ESP_LOGI(TAG, "Wrote block: %d bytes (uncompressed), offset=%d",
                     total_size, m_write_offset);
        last_debug = millis();
    }
    
    // Reset buffer
    m_sample_buffer_pos = 0;
}

void FlashStorage::write_session_header() {
    // Calculate CRC32 (exclude crc32 field itself)
    uint32_t crc = esp_crc32_le(0, (uint8_t*)&m_session_header,
                                offsetof(session_start_header_t, crc32));
    m_session_header.crc32 = crc;
    
    // Erase first sector
    esp_partition_erase_range(m_partition, 0, SPI_FLASH_SEC_SIZE);
    
    // Write header at start of partition
    esp_err_t err = esp_partition_write(m_partition, 0,
                                       &m_session_header, sizeof(session_start_header_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write session header: %d", err);
        return;
    }

    m_write_offset = sizeof(session_start_header_t);
    ESP_LOGI(TAG, "Wrote session header at offset 0, next write at %d", m_write_offset);
}

void FlashStorage::save_offset_to_nvs() {
    if (m_nvs_handle) {
        nvs_set_u32(m_nvs_handle, "write_offset", (uint32_t)m_write_offset);
        nvs_commit(m_nvs_handle);
    }
}

size_t FlashStorage::read_flash(size_t offset, uint8_t* buffer, size_t size) {
    if (!m_partition || offset >= m_partition_size) {
        return 0;
    }
    
    size_t to_read = size;
    if (offset + to_read > m_partition_size) {
        to_read = m_partition_size - offset;
    }
    
    esp_err_t err = esp_partition_read(m_partition, offset, buffer, to_read);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read failed at offset %d: %d", offset, err);
        return 0;
    }

    return to_read;
}

bool FlashStorage::read_session_header(session_start_header_t* header) {
    if (!m_partition || !header) {
        return false;
    }
    
    esp_err_t err = esp_partition_read(m_partition, 0, header, sizeof(session_start_header_t));
    if (err != ESP_OK) {
        return false;
    }
    
    // Verify magic
    if (header->magic != SESSION_START_MAGIC) {
        return false;
    }
    
    // Verify CRC
    uint32_t crc = esp_crc32_le(0, (uint8_t*)header, offsetof(session_start_header_t, crc32));
    if (crc != header->crc32) {
        return false;
    }
    
    return true;
}
