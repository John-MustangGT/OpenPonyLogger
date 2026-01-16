/*
 * session_helper.c - Session management helper for OpenPonyLogger
 *
 * This module provides APIs for creating and managing logging sessions, including:
 * - Session header creation with UUIDv4 generation
 * - Writing session headers to flash partitions
 * - NVS rotating slot management for session metadata
 * - CRC32 computation for data integrity
 *
 * CMake configuration snippet to inject GIT_SHORT_SHA:
 * 
 * In your component's CMakeLists.txt, add:
 *
 * execute_process(
 *     COMMAND git rev-parse --short=8 HEAD
 *     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 *     OUTPUT_VARIABLE GIT_SHORT_SHA
 *     OUTPUT_STRIP_TRAILING_WHITESPACE
 * )
 * 
 * target_compile_definitions(${COMPONENT_LIB} PRIVATE
 *     GIT_SHORT_SHA="${GIT_SHORT_SHA}"
 * )
 */

#include "session_helper.h"
#include "session_header.h"
#include <string.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_efuse.h>
#include <esp_partition.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <rom/crc.h>

// Default GIT_SHORT_SHA if not provided at build time
#ifndef GIT_SHORT_SHA
#define GIT_SHORT_SHA "00000000"
#endif

// NVS namespace for session management
#define NVS_NAMESPACE "logging"
#define NVS_SESSION_IDX_KEY "session_idx"
#define NVS_SESSION_META_PREFIX "session_meta_"

// Maximum number of session slots (rotating buffer)
#define MAX_SESSION_SLOTS 8

// Flash sector size for erase operations (typically 4KB for ESP32)
#define FLASH_SECTOR_SIZE 4096

/**
 * @brief Generate a RFC4122 UUIDv4 (random UUID)
 * 
 * @param uuid_out 16-byte buffer to store the UUID
 */
static void generate_uuidv4(uint8_t uuid_out[16]) {
    // Fill with random bytes
    esp_fill_random(uuid_out, 16);
    
    // Set version (4) in bits 12-15 of time_hi_and_version field (byte 6)
    uuid_out[6] = (uuid_out[6] & 0x0F) | 0x40;
    
    // Set variant (RFC4122) in bits 6-7 of clock_seq_hi_and_reserved field (byte 8)
    uuid_out[8] = (uuid_out[8] & 0x3F) | 0x80;
}

/**
 * @brief Convert hex string to binary (8 characters -> 4 bytes)
 * 
 * @param hex_str Input hex string (e.g., "1a2b3c4d")
 * @param bin_out Output binary buffer (4 bytes minimum)
 * @param max_bytes Maximum bytes to convert
 * @return Number of bytes converted
 */
static int hex_to_binary(const char* hex_str, uint8_t* bin_out, size_t max_bytes) {
    if (!hex_str) {
        return 0;
    }
    size_t len = strlen(hex_str);
    size_t bin_len = (len + 1) / 2;
    if (bin_len > max_bytes) {
        bin_len = max_bytes;
    }
    
    for (size_t i = 0; i < bin_len; i++) {
        uint8_t high = 0, low = 0;
        char c;
        
        // High nibble
        if (i * 2 < len) {
            c = hex_str[i * 2];
            if (c >= '0' && c <= '9') high = c - '0';
            else if (c >= 'a' && c <= 'f') high = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') high = c - 'A' + 10;
        }
        
        // Low nibble
        if (i * 2 + 1 < len) {
            c = hex_str[i * 2 + 1];
            if (c >= '0' && c <= '9') low = c - '0';
            else if (c >= 'a' && c <= 'f') low = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') low = c - 'A' + 10;
        }
        
        bin_out[i] = (high << 4) | low;
    }
    
    return bin_len;
}

/**
 * @brief Create a new session start header
 * 
 * @param out Output session header structure
 * @param fw_sha 8-byte firmware SHA (git short hash)
 * @param startup_counter Monotonic startup counter from NVS
 * @return true on success, false on failure
 */
bool session_helper_create_session(session_start_header_t* out, const uint8_t fw_sha[8], uint32_t startup_counter) {
    if (!out) {
        return false;
    }
    
    memset(out, 0, sizeof(session_start_header_t));
    
    // Set magic and version
    out->magic = SESSION_START_MAGIC;
    out->version = SESSION_FORMAT_VERSION;
    
    // Generate UUIDv4 for startup_id
    generate_uuidv4(out->startup_id);
    
    // Capture current ESP timer value (microseconds since boot)
    out->esp_time_at_start = esp_timer_get_time();
    
    // GPS UTC time is 0 (unknown at startup, will be filled when GPS locks)
    out->gps_utc_at_lock = 0;
    
    // Get device MAC address from efuse
    esp_efuse_mac_get_default(out->mac_addr);
    
    // Copy firmware SHA
    if (fw_sha) {
        memcpy(out->fw_sha, fw_sha, 8);
    } else {
        // Convert GIT_SHORT_SHA string to binary
        hex_to_binary(GIT_SHORT_SHA, out->fw_sha, 8);
    }
    
    // Set startup counter
    out->startup_counter = startup_counter;
    
    // Compute CRC32 over the header excluding the crc32 field itself
    uint32_t crc = crc32_le(0, (const uint8_t*)out, 
                             sizeof(session_start_header_t) - sizeof(uint32_t));
    out->crc32 = crc;
    
    return true;
}

/**
 * @brief Write session start header to flash partition
 * 
 * @param hdr Session header to write
 * @param partition_label Partition name (e.g., "storage")
 * @param offset Byte offset within partition (0 for start)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t session_helper_write_session_start_to_partition(
    const session_start_header_t* hdr,
    const char* partition_label,
    size_t offset)
{
    if (!hdr || !partition_label) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Find the partition
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        partition_label
    );
    
    if (!partition) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Erase the sector if needed (typically 4KB aligned)
    // Note: In production, you may want to check if erase is needed
    esp_err_t err = esp_partition_erase_range(partition, offset, FLASH_SECTOR_SIZE);
    if (err != ESP_OK) {
        return err;
    }
    
    // Write the header
    err = esp_partition_write(partition, offset, hdr, sizeof(session_start_header_t));
    return err;
}

/**
 * @brief Commit session metadata to NVS rotating slot
 * 
 * @param session Session header to store
 * @param slot_idx Slot index (0..7)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t session_helper_commit_session_nvs(const session_start_header_t* session, int slot_idx) {
    if (!session || slot_idx < 0 || slot_idx >= MAX_SESSION_SLOTS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Open NVS namespace
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    // Create key name for this slot
    char key[32];
    snprintf(key, sizeof(key), "%s%d", NVS_SESSION_META_PREFIX, slot_idx);
    
    // Write session metadata as blob
    // For simplicity, we store the entire session_start_header_t
    // In production, you might want a smaller metadata structure
    err = nvs_set_blob(nvs_handle, key, session, sizeof(session_start_header_t));
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }
    
    // Commit to NVS
    err = nvs_commit(nvs_handle);
    
    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Get the next rotating slot index from NVS
 * 
 * @return Slot index (0..7), or 0 if not found
 */
int session_helper_get_next_slot_index(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uint8_t current_idx = 0;
    
    // Open NVS namespace
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return 0;
    }
    
    // Read current index
    err = nvs_get_u8(nvs_handle, NVS_SESSION_IDX_KEY, &current_idx);
    if (err != ESP_OK) {
        // Not found, initialize to 0
        current_idx = 0;
    }
    
    // Calculate next index (rotate 0..7)
    uint8_t next_idx = (current_idx + 1) % MAX_SESSION_SLOTS;
    
    // Write back the next index
    nvs_set_u8(nvs_handle, NVS_SESSION_IDX_KEY, next_idx);
    nvs_commit(nvs_handle);
    
    nvs_close(nvs_handle);
    
    return current_idx;  // Return current before increment
}
