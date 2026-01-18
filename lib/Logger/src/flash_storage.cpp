#include "flash_storage.h"
#include <esp_crc.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <string.h>

FlashStorage::FlashStorage()
    : m_partition(nullptr), m_nvs_handle(0),
      m_partition_size(0), m_write_offset(0), m_session_start_offset(0),
      m_bytes_written(0), m_sample_buffer_pos(0), m_block_timestamp_us(0),
      m_writer_task(nullptr), m_sample_queue(nullptr),
      m_running(false), m_paused(false) {
    memset(&m_session_header, 0, sizeof(m_session_header));
    memset(m_startup_id, 0, sizeof(m_startup_id));
    memset(m_sample_buffer, 0, sizeof(m_sample_buffer));
}

FlashStorage::~FlashStorage() {
    end();
}

bool FlashStorage::begin() {
    Serial.println("[FlashStorage] Initializing...");
    
    // Find storage partition
    m_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "storage"
    );
    
    if (!m_partition) {
        Serial.println("[FlashStorage] ERROR: Storage partition not found!");
        return false;
    }
    
    m_partition_size = m_partition->size;
    Serial.printf("[FlashStorage] Found partition: size=%d bytes (%.2f MB)\n",
                  m_partition_size, m_partition_size / (1024.0f * 1024.0f));
    
    // Open NVS for offset tracking
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &m_nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("[FlashStorage] ERROR: Failed to open NVS: %d\n", err);
        return false;
    }
    
    // Load write offset from NVS (or start fresh)
    size_t saved_offset = 0;
    err = nvs_get_u32(m_nvs_handle, "write_offset", (uint32_t*)&saved_offset);
    if (err == ESP_OK) {
        Serial.printf("[FlashStorage] Loaded offset from NVS: %d\n", saved_offset);
        m_write_offset = saved_offset;
    } else {
        Serial.println("[FlashStorage] Starting fresh (no saved offset)");
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
    
    Serial.printf("[FlashStorage] Session UUID: ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02x", m_startup_id[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) Serial.print("-");
    }
    Serial.println();
    
    // Write session header to flash
    write_session_header();
    
    // Create sample queue
    m_sample_queue = xQueueCreate(QUEUE_SIZE, sizeof(SampleData));
    if (!m_sample_queue) {
        Serial.println("[FlashStorage] ERROR: Failed to create queue!");
        return false;
    }
    
    // Start writer task on Core 0 with low priority
    m_running = true;
    BaseType_t result = xTaskCreatePinnedToCore(
        writer_task_wrapper,
        "FlashWriter",
        8192,              // Stack size
        this,              // Parameter
        1,                 // Priority (low)
        &m_writer_task,
        0                  // Core 0
    );
    
    if (result != pdPASS) {
        Serial.println("[FlashStorage] ERROR: Failed to create writer task!");
        m_running = false;
        return false;
    }
    
    Serial.println("[FlashStorage] Started successfully on Core 0");
    return true;
}

void FlashStorage::end() {
    if (m_running) {
        Serial.println("[FlashStorage] Stopping...");
        
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
        
        // Save final offset
        save_offset_to_nvs();
        
        if (m_nvs_handle) {
            nvs_close(m_nvs_handle);
            m_nvs_handle = 0;
        }
        
        Serial.println("[FlashStorage] Stopped");
    }
}

void FlashStorage::write_sample(const gps_data_t& gps, const accel_data_t& accel,
                                const gyro_data_t& gyro, const compass_data_t& compass,
                                const battery_data_t& battery, const obd_data_t& obd) {
    if (!m_running || m_paused) {
        return;
    }
    
    int64_t now = esp_timer_get_time();
    
    // Queue accelerometer
    SampleData sample;
    sample.type = 0x01;  // SAMPLE_ACCEL
    sample.timestamp_us = now;
    sample.data.xyz.x = accel.x;
    sample.data.xyz.y = accel.y;
    sample.data.xyz.z = accel.z;
    xQueueSend(m_sample_queue, &sample, 0);  // Don't block
    
    // Queue gyroscope
    sample.type = 0x02;  // SAMPLE_GYRO
    sample.data.xyz.x = gyro.x;
    sample.data.xyz.y = gyro.y;
    sample.data.xyz.z = gyro.z;
    xQueueSend(m_sample_queue, &sample, 0);
    
    // Queue compass
    sample.type = 0x03;  // SAMPLE_COMPASS
    sample.data.xyz.x = compass.x;
    sample.data.xyz.y = compass.y;
    sample.data.xyz.z = compass.z;
    xQueueSend(m_sample_queue, &sample, 0);
    
    // Queue GPS if valid
    if (gps.valid) {
        // Update session header with GPS time on first lock
        if (m_session_header.gps_utc_at_lock == 0) {
            // Convert GPS time to Unix timestamp
            // This is simplified - real impl would parse GPS date/time properly
            // For now, just use current system time when GPS lock occurs
            m_session_header.gps_utc_at_lock = time(nullptr);
            Serial.printf("[FlashStorage] GPS lock acquired\n");
        }
        
        sample.type = 0x04;  // SAMPLE_GPS
        sample.data.gps.latitude = gps.latitude;
        sample.data.gps.longitude = gps.longitude;
        sample.data.gps.altitude = gps.altitude;
        sample.data.gps.speed = gps.speed;
        // Note: heading and hdop not in gps_data_t, would need derived from other data
        xQueueSend(m_sample_queue, &sample, 0);
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
        xQueueSend(m_sample_queue, &sample, 0);
    }
    
    // Queue battery
    sample.type = 0x07;  // SAMPLE_BATTERY
    sample.data.battery.voltage = battery.voltage;
    sample.data.battery.current = battery.current;
    sample.data.battery.soc = battery.state_of_charge;
    xQueueSend(m_sample_queue, &sample, 0);
}

void FlashStorage::pause() {
    Serial.println("[FlashStorage] Pausing writes...");
    m_paused = true;
    
    // Flush pending block
    flush_block();
}

void FlashStorage::resume() {
    Serial.println("[FlashStorage] Resuming writes...");
    m_paused = false;
}

void FlashStorage::writer_task_wrapper(void* arg) {
    FlashStorage* storage = static_cast<FlashStorage*>(arg);
    if (storage) {
        storage->writer_task_loop();
    }
}

void FlashStorage::writer_task_loop() {
    Serial.println("[FlashStorage] Writer task started on Core 0");
    
    SampleData sample;
    TickType_t last_flush = xTaskGetTickCount();
    const TickType_t flush_interval = pdMS_TO_TICKS(5000);  // Flush every 5 seconds
    
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
    }
    
    Serial.println("[FlashStorage] Writer task exiting");
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
        Serial.println("[FlashStorage] Wrapping circular buffer...");
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
        Serial.printf("[FlashStorage] ERROR: Failed to write block header: %d\n", err);
        m_sample_buffer_pos = 0;
        return;
    }
    
    m_write_offset += sizeof(log_block_header_t);
    
    // Write data (uncompressed)
    err = esp_partition_write(m_partition, m_write_offset,
                             data_to_write, data_size);
    if (err != ESP_OK) {
        Serial.printf("[FlashStorage] ERROR: Failed to write payload: %d\n", err);
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
        Serial.printf("[FlashStorage] Wrote block: %d bytes (uncompressed), offset=%d\n",
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
        Serial.printf("[FlashStorage] ERROR: Failed to write session header: %d\n", err);
        return;
    }
    
    m_write_offset = sizeof(session_start_header_t);
    Serial.printf("[FlashStorage] Wrote session header at offset 0, next write at %d\n", m_write_offset);
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
        Serial.printf("[FlashStorage] ERROR: Read failed at offset %d: %d\n", offset, err);
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
