#include "compression.h"

/**
 * Simple RLE+Delta compression
 * Format: [tag] [data]...
 * - 0x00-0x7F: Literal run of N+1 bytes
 * - 0x80-0xFF: Repeat last byte (N-0x80)+1 times
 */

size_t compress_rle_delta(const uint8_t* src, size_t src_len, 
                         uint8_t* dst, size_t dst_len) {
    if (!src || !dst || src_len == 0 || dst_len < 2) {
        return 0;
    }
    
    size_t dst_pos = 0;
    size_t src_pos = 0;
    
    while (src_pos < src_len && dst_pos < dst_len - 1) {
        uint8_t current = src[src_pos];
        size_t run_len = 1;
        
        // Count repeating bytes
        while (run_len < 64 && src_pos + run_len < src_len && 
               src[src_pos + run_len] == current) {
            run_len++;
        }
        
        // Use RLE for runs of 4+ bytes
        if (run_len >= 4) {
            // Encode as repeat: 0x80 + (count-1)
            size_t chunks = run_len / 63;
            for (size_t i = 0; i < chunks && dst_pos < dst_len; i++) {
                dst[dst_pos++] = 0x80 + 62;  // 63 repeats
            }
            
            size_t remainder = run_len % 63;
            if (remainder > 0 && dst_pos < dst_len) {
                dst[dst_pos++] = 0x80 + (remainder - 1);
            }
            src_pos += run_len;
        } else {
            // Literal bytes
            size_t literal_len = 1;
            while (literal_len < 63 && src_pos + literal_len < src_len) {
                uint8_t next = src[src_pos + literal_len];
                // Check if next byte starts a long run
                size_t next_run = 1;
                while (next_run < 4 && src_pos + literal_len + next_run < src_len &&
                       src[src_pos + literal_len + next_run] == next) {
                    next_run++;
                }
                if (next_run >= 4) break;  // Stop before long run
                literal_len++;
            }
            
            // Encode literal: count-1 (0x00-0x3E for up to 63 bytes)
            dst[dst_pos++] = literal_len - 1;
            if (dst_pos + literal_len <= dst_len) {
                memcpy(&dst[dst_pos], &src[src_pos], literal_len);
                dst_pos += literal_len;
                src_pos += literal_len;
            } else {
                return 0;  // Buffer too small
            }
        }
    }
    
    return dst_pos;
}

size_t decompress_rle_delta(const uint8_t* src, size_t src_len,
                           uint8_t* dst, size_t dst_len) {
    if (!src || !dst || src_len == 0) {
        return 0;
    }
    
    size_t src_pos = 0;
    size_t dst_pos = 0;
    
    while (src_pos < src_len && dst_pos < dst_len) {
        uint8_t tag = src[src_pos++];
        
        if (tag < 0x80) {
            // Literal run
            size_t literal_len = tag + 1;
            if (src_pos + literal_len > src_len || dst_pos + literal_len > dst_len) {
                return 0;  // Corrupted
            }
            memcpy(&dst[dst_pos], &src[src_pos], literal_len);
            dst_pos += literal_len;
            src_pos += literal_len;
        } else {
            // Repeat last byte
            size_t repeat_count = (tag - 0x80) + 1;
            if (dst_pos == 0 || dst_pos + repeat_count > dst_len) {
                return 0;  // Corrupted
            }
            uint8_t last_byte = dst[dst_pos - 1];
            memset(&dst[dst_pos], last_byte, repeat_count);
            dst_pos += repeat_count;
        }
    }
    
    return dst_pos;
}
