#ifndef LOG_BLOCK_H
#define LOG_BLOCK_H

#include <stdint.h>

#define LOG_BLOCK_MAGIC 0x4C4F4742 // 'LOGB'
#define LOG_BLOCK_VERSION 0x01

typedef struct __attribute__((packed)) {
    uint32_t magic;           // LOG_BLOCK_MAGIC
    uint8_t  version;         // LOG block header version
    uint8_t  reserved[3];     // padding / future flags
    uint8_t  startup_id[16];  // UUID v4 or generated session ID
    int64_t  timestamp_us;    // esp_timer_get_time() reference (µs)
    uint32_t uncompressed_size; // bytes before compression
    uint32_t compressed_size;   // bytes of compressed payload
    uint32_t crc32;             // CRC32 of compressed payload
} log_block_header_t;

#endif // LOG_BLOCK_H
