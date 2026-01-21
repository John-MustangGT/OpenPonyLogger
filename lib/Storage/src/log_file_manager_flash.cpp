#include "log_file_manager_flash.h"
#include "flash_storage.h"
#include <esp_crc.h>
#include <esp_log.h>

static const char* TAG = "LogFileMgr";

const esp_partition_t* LogFileManager::s_partition = nullptr;
FlashStorage* LogFileManager::s_flash_storage = nullptr;
std::vector<log_file_info_t> LogFileManager::s_log_files;
bool LogFileManager::s_initialized = false;
bool LogFileManager::s_download_active = false;
uint32_t LogFileManager::s_download_ref_count = 0;

bool LogFileManager::init() {
    if (s_initialized) {
        return true;
    }
    
    // Find storage partition
    s_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "storage"
    );
    
    if (!s_partition) {
        ESP_LOGE(TAG, "Storage partition not found");
        return false;
    }
    
    ESP_LOGI(TAG, "Found partition: %d bytes", s_partition->size);
    s_initialized = true;
    return true;
}

void LogFileManager::set_flash_storage(FlashStorage* storage) {
    s_flash_storage = storage;
}

uint32_t LogFileManager::scan_log_files() {
    s_log_files.clear();
    
    if (!s_partition) {
        return 0;
    }
    
    // Read session header
    session_start_header_t header;
    esp_err_t err = esp_partition_read(s_partition, 0, &header, sizeof(session_start_header_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read session header");
        return 0;
    }
    
    // Validate magic
    if (header.magic != SESSION_START_MAGIC) {
        ESP_LOGW(TAG, "No valid session found");
        return 0;
    }
    
    // Verify CRC
    uint32_t crc = esp_crc32_le(0, (uint8_t*)&header, offsetof(session_start_header_t, crc32));
    if (crc != header.crc32) {
        ESP_LOGW(TAG, "Session header CRC mismatch");
        return 0;
    }
    
    // Create file info
    log_file_info_t info;
    info.filename = "current_session.opl";
    info.file_size = s_flash_storage ? s_flash_storage->get_bytes_written() : 0;
    info.gps_utc_timestamp = header.gps_utc_at_lock;
    info.esp_timestamp_us = header.esp_time_at_start;
    memcpy(info.startup_id, header.startup_id, 16);
    info.valid = true;
    info.block_count = 0;  // Will be counted during stream
    
    s_log_files.push_back(info);

    ESP_LOGI(TAG, "Found session: %zu bytes", info.file_size);
    return 1;
}

const std::vector<log_file_info_t>& LogFileManager::get_log_files() {
    return s_log_files;
}

void LogFileManager::set_download_active(bool active) {
    s_download_active = active;
    
    if (s_flash_storage) {
        if (active) {
            s_flash_storage->pause();
            ESP_LOGI(TAG, "Logging paused for download");
        } else {
            s_flash_storage->resume();
            ESP_LOGI(TAG, "Logging resumed after download");
        }
    }
}

size_t LogFileManager::stream_to_client(Stream& output) {
    if (!s_partition || !s_flash_storage) {
        return 0;
    }
    
    // Pause logging
    set_download_active(true);
    
    size_t total_written = 0;
    const size_t chunk_size = 1024;
    uint8_t buffer[chunk_size];
    
    // Get current write position (total data size)
    size_t data_size = s_flash_storage->get_write_offset();

    ESP_LOGI(TAG, "Streaming %zu bytes to client...", data_size);
    
    // Stream data in chunks
    for (size_t offset = 0; offset < data_size; offset += chunk_size) {
        size_t to_read = chunk_size;
        if (offset + to_read > data_size) {
            to_read = data_size - offset;
        }
        
        // Read from flash
        esp_err_t err = esp_partition_read(s_partition, offset, buffer, to_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Read error at offset %zu", offset);
            break;
        }
        
        // Write to client
        size_t written = output.write(buffer, to_read);
        if (written != to_read) {
            ESP_LOGE(TAG, "Client write error: %zu != %zu", written, to_read);
            break;
        }
        
        total_written += written;

        // Progress indicator
        if (offset % (32 * 1024) == 0) {
            ESP_LOGD(TAG, "Streamed %zu / %zu bytes (%.1f%%)",
                         offset, data_size, (offset * 100.0f) / data_size);
        }
        
        // Yield to prevent watchdog
        if (offset % (4 * 1024) == 0) {
            delay(1);
        }
    }
    
    ESP_LOGI(TAG, "Stream complete: %zu bytes", total_written);

    // Resume logging
    set_download_active(false);
    
    return total_written;
}

size_t LogFileManager::read_flash(size_t offset, uint8_t* buffer, size_t size) {
    if (!s_partition || !s_flash_storage) {
        return 0;
    }

    // On first chunk (offset==0), increment ref count and pause logging
    if (offset == 0) {
        s_download_ref_count++;
        if (!s_download_active) {
            set_download_active(true);
        }
    }

    size_t data_size = s_flash_storage->get_write_offset();
    if (offset >= data_size) {
        // End of data - decrement ref count and resume if no more downloads
        s_download_ref_count--;
        if (s_download_ref_count == 0 && s_download_active) {
            set_download_active(false);
        }
        return 0;
    }

    size_t to_read = size;
    if (offset + to_read > data_size) {
        to_read = data_size - offset;
    }

    esp_err_t err = esp_partition_read(s_partition, offset, buffer, to_read);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read failed at %zu: %d", offset, err);
        // Decrement ref count on error
        if (s_download_ref_count > 0) {
            s_download_ref_count--;
            if (s_download_ref_count == 0 && s_download_active) {
                set_download_active(false);
            }
        }
        return 0;
    }

    return to_read;
}

bool LogFileManager::erase_all_data() {
    if (!s_partition) {
        return false;
    }

    ESP_LOGI(TAG, "Erasing partition...");
    
    // Pause logging
    set_download_active(true);
    
    // Erase entire partition
    esp_err_t err = esp_partition_erase_range(s_partition, 0, s_partition->size);
    
    // Resume logging (will start fresh)
    set_download_active(false);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erase failed: %d", err);
        return false;
    }
    
    Serial.println("[LogFileManager] Partition erased");
    return true;
}

size_t LogFileManager::get_total_log_size() {
    if (s_flash_storage) {
        return s_flash_storage->get_bytes_written();
    }
    return 0;
}

size_t LogFileManager::get_free_space() {
    if (s_flash_storage) {
        return s_flash_storage->get_partition_size() - s_flash_storage->get_write_offset();
    }
    return 0;
}
