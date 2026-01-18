#include "log_file_manager.h"
#include <esp_crc.h>

// Note: miniz is not available in Arduino ESP32 by default
// We'll need to add decompression support or use raw data
// For now, we'll skip decompression validation in the read function

// Static member initialization
String LogFileManager::m_mount_point = "/sd";
std::vector<log_file_info_t> LogFileManager::m_log_files;
bool LogFileManager::m_initialized = false;
bool LogFileManager::m_download_active = false;
uint32_t LogFileManager::m_last_scan_time = 0;

bool LogFileManager::init(const char* sd_mount_point) {
    if (m_initialized) {
        return true;
    }
    
    m_mount_point = String(sd_mount_point);
    
    // Verify SD card is mounted
    if (!SD.begin()) {
        Serial.println("[LogFileMgr] ERROR: SD card not mounted");
        return false;
    }
    
    Serial.printf("[LogFileMgr] Initialized with mount point: %s\n", sd_mount_point);
    m_initialized = true;
    
    // Initial scan
    scan_log_files(true);
    
    return true;
}

uint32_t LogFileManager::scan_log_files(bool force_rescan) {
    if (!m_initialized) {
        Serial.println("[LogFileMgr] ERROR: Not initialized");
        return 0;
    }
    
    // Cache results for 5 seconds unless forced
    uint32_t now = millis();
    if (!force_rescan && (now - m_last_scan_time < 5000)) {
        return m_log_files.size();
    }
    
    m_log_files.clear();
    
    File root = SD.open(m_mount_point.c_str());
    if (!root || !root.isDirectory()) {
        Serial.printf("[LogFileMgr] ERROR: Cannot open directory: %s\n", m_mount_point.c_str());
        return 0;
    }
    
    Serial.println("[LogFileMgr] Scanning for .opl log files...");
    
    File file = root.openNextFile();
    while (file) {
        String filename = String(file.name());
        
        // Only process .opl files
        if (!file.isDirectory() && filename.endsWith(".opl")) {
            log_file_info_t info;
            info.filename = filename;
            info.file_size = file.size();
            info.valid = false;
            
            // Parse session header
            if (parse_session_header(m_mount_point + "/" + filename, info)) {
                m_log_files.push_back(info);
                Serial.printf("[LogFileMgr]   ✓ %s (%u bytes, %u blocks)\n", 
                             filename.c_str(), info.file_size, info.block_count);
            } else {
                Serial.printf("[LogFileMgr]   ✗ %s (invalid header)\n", filename.c_str());
            }
        }
        
        file = root.openNextFile();
    }
    
    root.close();
    
    // Sort by GPS UTC timestamp (newest first), fallback to ESP timestamp
    std::sort(m_log_files.begin(), m_log_files.end(), 
              [](const log_file_info_t& a, const log_file_info_t& b) {
        if (a.gps_utc_timestamp != 0 && b.gps_utc_timestamp != 0) {
            return a.gps_utc_timestamp > b.gps_utc_timestamp;
        } else if (a.gps_utc_timestamp != 0) {
            return true;  // Valid GPS time comes first
        } else if (b.gps_utc_timestamp != 0) {
            return false;
        } else {
            return a.esp_timestamp_us > b.esp_timestamp_us;
        }
    });
    
    m_last_scan_time = now;
    Serial.printf("[LogFileMgr] Found %d valid log files\n", m_log_files.size());
    
    return m_log_files.size();
}

const std::vector<log_file_info_t>& LogFileManager::get_log_files() {
    return m_log_files;
}

bool LogFileManager::get_file_info(const String& filename, log_file_info_t& info) {
    for (const auto& file_info : m_log_files) {
        if (file_info.filename == filename) {
            info = file_info;
            return true;
        }
    }
    return false;
}

bool LogFileManager::parse_session_header(const String& filename, log_file_info_t& info) {
    File file = SD.open(filename.c_str(), FILE_READ);
    if (!file) {
        Serial.printf("[LogFileMgr] ERROR: Cannot open file: %s\n", filename.c_str());
        return false;
    }
    
    // Read session header
    session_start_header_t header;
    size_t bytes_read = file.read((uint8_t*)&header, sizeof(header));
    
    if (bytes_read != sizeof(header)) {
        Serial.printf("[LogFileMgr] ERROR: Short read on header (%d bytes)\n", bytes_read);
        file.close();
        return false;
    }
    
    // Validate magic number
    if (header.magic != SESSION_START_MAGIC) {
        Serial.printf("[LogFileMgr] ERROR: Invalid magic 0x%08X (expected 0x%08X)\n", 
                     header.magic, SESSION_START_MAGIC);
        file.close();
        return false;
    }
    
    // Validate CRC32 (exclude last 4 bytes which contain the CRC)
    uint32_t calculated_crc = calculate_crc32((uint8_t*)&header, sizeof(header) - 4);
    if (calculated_crc != header.crc32) {
        Serial.printf("[LogFileMgr] ERROR: CRC mismatch (calculated=0x%08X, stored=0x%08X)\n",
                     calculated_crc, header.crc32);
        file.close();
        return false;
    }
    
    // Extract metadata
    info.gps_utc_timestamp = header.gps_utc_at_lock;
    info.esp_timestamp_us = header.esp_time_at_start;
    memcpy(info.startup_id, header.startup_id, 16);
    info.valid = true;
    
    // Count data blocks
    info.block_count = 0;
    while (file.available()) {
        log_block_header_t block_header;
        bytes_read = file.read((uint8_t*)&block_header, sizeof(block_header));
        
        if (bytes_read != sizeof(block_header)) {
            break;  // End of file or incomplete block
        }
        
        if (block_header.magic != LOG_BLOCK_MAGIC) {
            break;  // Invalid block or end marker
        }
        
        info.block_count++;
        
        // Skip compressed payload
        file.seek(file.position() + block_header.compressed_size);
    }
    
    file.close();
    return true;
}

bool LogFileManager::validate_file(const String& filename, 
                                   void (*progress_callback)(uint32_t, uint32_t)) {
    String full_path = m_mount_point + "/" + filename;
    File file = SD.open(full_path.c_str(), FILE_READ);
    
    if (!file) {
        Serial.printf("[LogFileMgr] ERROR: Cannot open file: %s\n", full_path.c_str());
        return false;
    }
    
    // Skip session header
    file.seek(sizeof(session_start_header_t));
    
    uint32_t block_num = 0;
    uint32_t total_blocks = 0;
    
    // First pass: count blocks
    long saved_pos = file.position();
    while (file.available()) {
        log_block_header_t header;
        if (file.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) break;
        if (header.magic != LOG_BLOCK_MAGIC) break;
        total_blocks++;
        file.seek(file.position() + header.compressed_size);
    }
    file.seek(saved_pos);
    
    // Second pass: validate blocks
    bool all_valid = true;
    static EXT_RAM_ATTR uint8_t compressed_buffer[16384];  // Max block size
    
    while (file.available()) {
        log_block_header_t header;
        size_t bytes_read = file.read((uint8_t*)&header, sizeof(header));
        
        if (bytes_read != sizeof(header)) {
            break;
        }
        
        if (header.magic != LOG_BLOCK_MAGIC) {
            Serial.printf("[LogFileMgr] Block %d: Invalid magic\n", block_num);
            all_valid = false;
            break;
        }
        
        // Read compressed payload
        if (header.compressed_size > sizeof(compressed_buffer)) {
            Serial.printf("[LogFileMgr] Block %d: Payload too large (%u bytes)\n", 
                         block_num, header.compressed_size);
            all_valid = false;
            break;
        }
        
        bytes_read = file.read(compressed_buffer, header.compressed_size);
        if (bytes_read != header.compressed_size) {
            Serial.printf("[LogFileMgr] Block %d: Short read\n", block_num);
            all_valid = false;
            break;
        }
        
        // Validate CRC32
        uint32_t calculated_crc = calculate_crc32(compressed_buffer, header.compressed_size);
        if (calculated_crc != header.crc32) {
            Serial.printf("[LogFileMgr] Block %d: CRC mismatch\n", block_num);
            all_valid = false;
            break;
        }
        
        block_num++;
        
        if (progress_callback) {
            progress_callback(block_num, total_blocks);
        }
    }
    
    file.close();
    
    if (all_valid) {
        Serial.printf("[LogFileMgr] Validation passed: %d blocks OK\n", block_num);
    }
    
    return all_valid;
}

size_t LogFileManager::read_and_decompress_block(File& file, 
                                                 log_block_header_t& block_header,
                                                 uint8_t* decompressed_data, 
                                                 size_t max_size) {
    // Read block header
    size_t bytes_read = file.read((uint8_t*)&block_header, sizeof(block_header));
    if (bytes_read != sizeof(block_header)) {
        return 0;
    }
    
    if (block_header.magic != LOG_BLOCK_MAGIC) {
        return 0;
    }
    
    // Read compressed payload
    static EXT_RAM_ATTR uint8_t compressed_buffer[16384];
    if (block_header.compressed_size > sizeof(compressed_buffer)) {
        Serial.println("[LogFileMgr] ERROR: Compressed block too large");
        return 0;
    }
    
    bytes_read = file.read(compressed_buffer, block_header.compressed_size);
    if (bytes_read != block_header.compressed_size) {
        Serial.println("[LogFileMgr] ERROR: Short read on compressed data");
        return 0;
    }
    
    // Verify CRC32
    uint32_t calculated_crc = calculate_crc32(compressed_buffer, block_header.compressed_size);
    if (calculated_crc != block_header.crc32) {
        Serial.println("[LogFileMgr] ERROR: CRC32 mismatch");
        return 0;
    }
    
    // TODO: Add decompression support when miniz or zlib is available
    // For now, just copy the compressed data as-is for download
    // The client-side tool will need to decompress
    Serial.println("[LogFileMgr] WARNING: Decompression not implemented, returning compressed data");
    
    size_t copy_size = min((size_t)block_header.compressed_size, max_size);
    memcpy(decompressed_data, compressed_buffer, copy_size);
    
    return copy_size;
}

bool LogFileManager::delete_file(const String& filename) {
    String full_path = m_mount_point + "/" + filename;
    
    if (SD.remove(full_path.c_str())) {
        Serial.printf("[LogFileMgr] Deleted: %s\n", filename.c_str());
        
        // Remove from cache
        m_log_files.erase(
            std::remove_if(m_log_files.begin(), m_log_files.end(),
                          [&filename](const log_file_info_t& info) {
                              return info.filename == filename;
                          }),
            m_log_files.end()
        );
        
        return true;
    }
    
    Serial.printf("[LogFileMgr] ERROR: Failed to delete %s\n", filename.c_str());
    return false;
}

uint32_t LogFileManager::delete_all_files(void (*progress_callback)(uint32_t, uint32_t)) {
    uint32_t deleted_count = 0;
    uint32_t total_files = m_log_files.size();
    
    // Make a copy since we'll be modifying the vector
    std::vector<log_file_info_t> files_to_delete = m_log_files;
    
    for (size_t i = 0; i < files_to_delete.size(); i++) {
        if (delete_file(files_to_delete[i].filename)) {
            deleted_count++;
        }
        
        if (progress_callback) {
            progress_callback(i + 1, total_files);
        }
    }
    
    Serial.printf("[LogFileMgr] Deleted %d of %d files\n", deleted_count, total_files);
    return deleted_count;
}

size_t LogFileManager::get_total_log_size() {
    size_t total = 0;
    for (const auto& info : m_log_files) {
        total += info.file_size;
    }
    return total;
}

size_t LogFileManager::get_free_space() {
    return SD.totalBytes() - SD.usedBytes();
}

bool LogFileManager::is_download_active() {
    return m_download_active;
}

void LogFileManager::set_download_active(bool active) {
    if (active != m_download_active) {
        m_download_active = active;
        Serial.printf("[LogFileMgr] Download %s (logging %s)\n", 
                     active ? "started" : "stopped",
                     active ? "PAUSED" : "resumed");
    }
}

uint32_t LogFileManager::calculate_crc32(const uint8_t* data, size_t length) {
    return esp_crc32_le(0, data, length);
}
