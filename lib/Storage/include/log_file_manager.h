#ifndef LOG_FILE_MANAGER_H
#define LOG_FILE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <FS.h>
#include <SD.h>
#include "session_header.h"
#include "log_block.h"

/**
 * @brief Metadata for a single log file session
 */
struct log_file_info_t {
    String filename;              // e.g., "session_20260118_143022.opl"
    size_t file_size;             // Total file size in bytes
    uint64_t gps_utc_timestamp;   // GPS UTC time from session header (0 if unknown)
    uint64_t esp_timestamp_us;    // ESP timer at session start
    uint8_t startup_id[16];       // Session UUID
    bool valid;                   // True if header parsed successfully
    uint32_t block_count;         // Number of data blocks in file
};

/**
 * @brief Manages log file operations on SD card
 * Handles listing, reading, validation, and deletion of .opl binary log files
 */
class LogFileManager {
public:
    /**
     * @brief Initialize the log file manager
     * @param sd_mount_point Path to SD card mount point (default "/sd")
     * @return true if successful
     */
    static bool init(const char* sd_mount_point = "/sd");
    
    /**
     * @brief Scan SD card and build list of available log files
     * @param force_rescan If true, always rescan. If false, use cached list
     * @return Number of valid log files found
     */
    static uint32_t scan_log_files(bool force_rescan = false);
    
    /**
     * @brief Get list of all valid log files
     * @return Vector of log file metadata, sorted newest first
     */
    static const std::vector<log_file_info_t>& get_log_files();
    
    /**
     * @brief Get metadata for a specific log file
     * @param filename Name of the log file
     * @param info Output parameter for file info
     * @return true if file found and valid
     */
    static bool get_file_info(const String& filename, log_file_info_t& info);
    
    /**
     * @brief Validate a log file by checking all block checksums
     * @param filename Name of the log file
     * @param progress_callback Optional callback(current_block, total_blocks)
     * @return true if all blocks are valid
     */
    static bool validate_file(const String& filename, 
                             void (*progress_callback)(uint32_t, uint32_t) = nullptr);
    
    /**
     * @brief Read and decompress a data block from log file
     * @param file Open file handle
     * @param block_header Output parameter for block header
     * @param decompressed_data Output buffer for decompressed data
     * @param max_size Maximum size of output buffer
     * @return Number of decompressed bytes, or 0 on error
     */
    static size_t read_and_decompress_block(File& file, 
                                           log_block_header_t& block_header,
                                           uint8_t* decompressed_data, 
                                           size_t max_size);
    
    /**
     * @brief Delete a log file
     * @param filename Name of the log file to delete
     * @return true if deleted successfully
     */
    static bool delete_file(const String& filename);
    
    /**
     * @brief Delete all log files
     * @param progress_callback Optional callback(current_file, total_files)
     * @return Number of files deleted
     */
    static uint32_t delete_all_files(void (*progress_callback)(uint32_t, uint32_t) = nullptr);
    
    /**
     * @brief Get total size of all log files
     * @return Total bytes of all .opl files
     */
    static size_t get_total_log_size();
    
    /**
     * @brief Get SD card free space
     * @return Free bytes on SD card
     */
    static size_t get_free_space();
    
    /**
     * @brief Check if logging should be paused for download
     * @return true if download is in progress
     */
    static bool is_download_active();
    
    /**
     * @brief Set download active state (suspends logging)
     * @param active true to suspend logging, false to resume
     */
    static void set_download_active(bool active);

private:
    static String m_mount_point;
    static std::vector<log_file_info_t> m_log_files;
    static bool m_initialized;
    static bool m_download_active;
    static uint32_t m_last_scan_time;
    
    /**
     * @brief Parse session header from file
     * @param filename Path to log file
     * @param info Output parameter for file info
     * @return true if header parsed successfully
     */
    static bool parse_session_header(const String& filename, log_file_info_t& info);
    
    /**
     * @brief Calculate CRC32 of data
     * @param data Data buffer
     * @param length Data length
     * @return CRC32 checksum
     */
    static uint32_t calculate_crc32(const uint8_t* data, size_t length);
};

#endif // LOG_FILE_MANAGER_H
