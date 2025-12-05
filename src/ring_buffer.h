/**
 * @file ring_buffer.h
 * @brief Lock-free ring buffer for inter-core communication
 * 
 * Single producer (Core 0), single consumer (Core 1) ring buffer.
 * Uses atomic operations for thread-safe access without locks.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "telemetry_types.h"
#include <string.h>

// Ring buffer size - 64KB provides ~30+ seconds of buffering
#define RING_BUFFER_SIZE (64 * 1024)

/**
 * Ring buffer structure
 */
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    ring_buffer_meta_t meta;
} ring_buffer_t;

/**
 * Initialize the ring buffer
 * 
 * @param rb Pointer to ring buffer structure
 */
static inline void ring_buffer_init(ring_buffer_t *rb) {
    memset(rb, 0, sizeof(ring_buffer_t));
    rb->meta.capacity = RING_BUFFER_SIZE;
    rb->meta.write_index = 0;
    rb->meta.read_index = 0;
    rb->meta.dropped_count = 0;
    rb->meta.overflow = false;
}

/**
 * Get available space for writing
 * 
 * @param rb Pointer to ring buffer
 * @return Number of bytes available for writing
 */
static inline uint32_t ring_buffer_write_available(ring_buffer_t *rb) {
    uint32_t write_idx = rb->meta.write_index;
    uint32_t read_idx = rb->meta.read_index;
    
    if (write_idx >= read_idx) {
        return rb->meta.capacity - (write_idx - read_idx) - 1;
    } else {
        return read_idx - write_idx - 1;
    }
}

/**
 * Get number of bytes available for reading
 * 
 * @param rb Pointer to ring buffer
 * @return Number of bytes available for reading
 */
static inline uint32_t ring_buffer_read_available(ring_buffer_t *rb) {
    uint32_t write_idx = rb->meta.write_index;
    uint32_t read_idx = rb->meta.read_index;
    
    if (write_idx >= read_idx) {
        return write_idx - read_idx;
    } else {
        return rb->meta.capacity - read_idx + write_idx;
    }
}

/**
 * Write a telemetry message to the ring buffer (Core 0 - Producer)
 * 
 * @param rb Pointer to ring buffer
 * @param msg Pointer to message to write
 * @param payload_data Pointer to payload data
 * @param payload_len Length of payload in bytes
 * @return true if message written successfully, false if buffer full
 */
static inline bool ring_buffer_write_message(ring_buffer_t *rb, 
                                              const telemetry_msg_t *msg,
                                              const void *payload_data,
                                              uint16_t payload_len) {
    uint32_t total_size = TELEMETRY_MSG_TOTAL_SIZE(payload_len);
    
    // Check if we have enough space
    if (ring_buffer_write_available(rb) < total_size) {
        rb->meta.dropped_count++;
        rb->meta.overflow = true;
        return false;
    }
    
    uint32_t write_idx = rb->meta.write_index;
    
    // Write message header
    const uint8_t *header = (const uint8_t *)msg;
    for (uint32_t i = 0; i < TELEMETRY_MSG_HEADER_SIZE; i++) {
        rb->buffer[write_idx] = header[i];
        write_idx = (write_idx + 1) % rb->meta.capacity;
    }
    
    // Write payload
    const uint8_t *payload = (const uint8_t *)payload_data;
    for (uint16_t i = 0; i < payload_len; i++) {
        rb->buffer[write_idx] = payload[i];
        write_idx = (write_idx + 1) % rb->meta.capacity;
    }
    
    // Update write index (atomic operation on Pico)
    __atomic_store_n(&rb->meta.write_index, write_idx, __ATOMIC_RELEASE);
    
    return true;
}

/**
 * Read a telemetry message from the ring buffer (Core 1 - Consumer)
 * 
 * @param rb Pointer to ring buffer
 * @param msg_buffer Buffer to store the message (must be large enough)
 * @param max_size Maximum size of the message buffer
 * @return Number of bytes read, or 0 if no message available
 */
static inline uint32_t ring_buffer_read_message(ring_buffer_t *rb,
                                                 uint8_t *msg_buffer,
                                                 uint32_t max_size) {
    // Check if data is available
    if (ring_buffer_read_available(rb) < TELEMETRY_MSG_HEADER_SIZE) {
        return 0;
    }
    
    uint32_t read_idx = rb->meta.read_index;
    
    // Peek at the header to get message size
    uint8_t header_temp[TELEMETRY_MSG_HEADER_SIZE];
    uint32_t temp_idx = read_idx;
    for (uint32_t i = 0; i < TELEMETRY_MSG_HEADER_SIZE; i++) {
        header_temp[i] = rb->buffer[temp_idx];
        temp_idx = (temp_idx + 1) % rb->meta.capacity;
    }
    
    // Extract payload length from header
    telemetry_msg_t *temp_msg = (telemetry_msg_t *)header_temp;
    uint16_t payload_len = temp_msg->length;
    uint32_t total_size = TELEMETRY_MSG_TOTAL_SIZE(payload_len);
    
    // Check if complete message is available and fits in buffer
    if (ring_buffer_read_available(rb) < total_size || total_size > max_size) {
        return 0;
    }
    
    // Read the complete message
    for (uint32_t i = 0; i < total_size; i++) {
        msg_buffer[i] = rb->buffer[read_idx];
        read_idx = (read_idx + 1) % rb->meta.capacity;
    }
    
    // Update read index (atomic operation)
    __atomic_store_n(&rb->meta.read_index, read_idx, __ATOMIC_RELEASE);
    
    return total_size;
}

/**
 * Get ring buffer statistics
 * 
 * @param rb Pointer to ring buffer
 * @param bytes_used Pointer to store bytes currently used
 * @param bytes_free Pointer to store bytes available
 * @param overflow_flag Pointer to store overflow flag
 * @param dropped_count Pointer to store dropped message count
 */
static inline void ring_buffer_get_stats(ring_buffer_t *rb,
                                          uint32_t *bytes_used,
                                          uint32_t *bytes_free,
                                          bool *overflow_flag,
                                          uint32_t *dropped_count) {
    if (bytes_used) {
        *bytes_used = ring_buffer_read_available(rb);
    }
    if (bytes_free) {
        *bytes_free = ring_buffer_write_available(rb);
    }
    if (overflow_flag) {
        *overflow_flag = rb->meta.overflow;
        rb->meta.overflow = false; // Clear flag after reading
    }
    if (dropped_count) {
        *dropped_count = rb->meta.dropped_count;
    }
}

#endif // RING_BUFFER_H
