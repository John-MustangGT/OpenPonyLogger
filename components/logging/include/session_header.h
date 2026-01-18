#ifndef SESSION_HEADER_H
#define SESSION_HEADER_H

#include <stdint.h>

#define SESSION_START_MAGIC 0x53545230 // 'STR0'
#define SESSION_FORMAT_VERSION 0x01

/**
 * @brief Compression types supported by OpenPony logger
 */
typedef enum {
    COMPRESSION_NONE       = 0x00,  // No compression, raw data
    COMPRESSION_HEATSHRINK = 0x01,  // Heatshrink (LZSS) compression
    COMPRESSION_RLE_DELTA  = 0x02,  // RLE+Delta encoding
} compression_type_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;             // SESSION_START_MAGIC
    uint8_t  version;           // format version
    uint8_t  compression_type;  // Compression type used (compression_type_t)
    uint8_t  reserved[2];
    uint8_t  startup_id[16];    // UUIDv4 for this session
    int64_t  esp_time_at_start; // esp_timer_get_time() at startup (µs)
    int64_t  gps_utc_at_lock;   // seconds since epoch (0 if unknown)
    uint8_t  mac_addr[6];       // device MAC (efuse)
    uint8_t  fw_sha[8];         // short git SHA (8 bytes)
    uint32_t startup_counter;   // monotonic counter persisted in NVS (optional)
    uint32_t reserved2[2];      // for future use
    uint32_t crc32;             // CRC32 of the header (exclude this field)
} session_start_header_t;

#endif // SESSION_HEADER_H
