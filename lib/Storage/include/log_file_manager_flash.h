#pragma once

#include <Arduino.h>
#include <vector>
#include <esp_partition.h>
#include "session_header.h"
#include "log_block.h"

/**
 * @brief Metadata for current logging session in flash
 */
struct log_file_info_t {
    String filename;              // Always "current_session.opl"
    size_t file_size;             // Total logged data size
    uint64_t gps_utc_timestamp;   // GPS UTC time (0 if no lock)
    uint64_t esp_timestamp_us;    // ESP timer at start
    uint8_t startup_id[16];       // Session UUID
    bool valid;                   // True if header is valid
    uint32_t block_count;         // Estimated block count
};

/**
 * @brief Manages flash partition logging for downloads
 * 
 * Works with FlashStorage to provide download interface.
 * Since we use circular buffer, there's only ever one "current_session.opl"
 */
class LogFileManager {
public:
    /**
     * @brief Initialize with flash partition
     */
    static bool init();
    
    /**
     * @brief Get current session info (returns 0 or 1)
     */
    static uint32_t scan_log_files();
    
    /**
     * @brief Get list (always 1 item or empty)
     */
    static const std::vector<log_file_info_t>& get_log_files();
    
    /**
     * @brief Stream entire flash partition content as .opl file
     * 
     * Streams session header + all data blocks for download.
     * Pauses logging during download.
     */
    static size_t stream_to_client(Stream& output);
    
    /**
     * @brief Read raw data from flash partition
     * 
     * Used by download handler for streaming.
     */
    static size_t read_flash(size_t offset, uint8_t* buffer, size_t size);
    
    /**
     * @brief Erase all data (full partition erase)
     */
    static bool erase_all_data();
    
    /**
     * @brief Get total logged bytes
     */
    static size_t get_total_log_size();
    
    /**
     * @brief Get free space in partition
     */
    static size_t get_free_space();
    
    /**
     * @brief Check if download is active
     */
    static bool is_download_active() { return s_download_active; }
    
    /**
     * @brief Suspend logging for download
     */
    static void set_download_active(bool active);
    
    /**
     * @brief Set pointer to FlashStorage for pause/resume
     */
    static void set_flash_storage(class FlashStorage* storage);

private:
    static const esp_partition_t* s_partition;
    static FlashStorage* s_flash_storage;
    static std::vector<log_file_info_t> s_log_files;
    static bool s_initialized;
    static bool s_download_active;
};
