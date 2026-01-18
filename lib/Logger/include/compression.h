#pragma once

#include <stdint.h>
#include <string.h>

/**
 * @brief Simple RLE + Delta compression for sensor data
 * 
 * Optimized for ESP32: low memory, fast encoding/decoding
 * Works well on sensor data with repeated values and small deltas
 */

/**
 * @brief Compress data using RLE+Delta encoding
 * 
 * @param src Source data
 * @param src_len Source size
 * @param dst Destination buffer
 * @param dst_len Destination buffer size
 * @return Compressed size, or 0 if compression failed
 */
size_t compress_rle_delta(const uint8_t* src, size_t src_len, 
                         uint8_t* dst, size_t dst_len);

/**
 * @brief Decompress RLE+Delta encoded data
 * 
 * @param src Compressed data
 * @param src_len Compressed size
 * @param dst Destination buffer
 * @param dst_len Destination buffer size
 * @return Decompressed size, or 0 if decompression failed
 */
size_t decompress_rle_delta(const uint8_t* src, size_t src_len,
                           uint8_t* dst, size_t dst_len);
