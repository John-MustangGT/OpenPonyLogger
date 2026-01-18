#pragma once

#include <Arduino.h>
#include <esp_partition.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "sensor_hal.h"
#include "session_header.h"
#include "log_block.h"

/**
 * @brief Flash storage writer for circular buffer logging
 * 
 * Writes binary log data to a dedicated flash partition in a circular buffer.
 * Tracks write position in NVS for persistence across reboots.
 * Runs on Core 0 with low priority to avoid interfering with RT operations.
 */
class FlashStorage {
public:
    FlashStorage();
    ~FlashStorage();
    
    /**
     * @brief Initialize flash storage subsystem
     * 
     * - Finds storage partition
     * - Loads write offset from NVS
     * - Creates session header
     * - Starts writer task on Core 0
     * 
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Stop storage and cleanup
     */
    void end();
    
    /**
     * @brief Queue sensor data for writing
     * 
     * Called from RT logger at ~10Hz. Data is buffered and written
     * in compressed blocks by the Core 0 writer task.
     * 
     * @param gps GPS data
     * @param accel Accelerometer data
     * @param gyro Gyroscope data
     * @param compass Compass data
     * @param battery Battery data
     */
    void write_sample(const gps_data_t& gps, const accel_data_t& accel,
                     const gyro_data_t& gyro, const compass_data_t& compass,
                     const battery_data_t& battery);
    
    /**
     * @brief Pause writing (for downloads)
     */
    void pause();
    
    /**
     * @brief Resume writing after pause
     */
    void resume();
    
    /**
     * @brief Check if storage is paused
     */
    bool is_paused() const { return m_paused; }
    
    /**
     * @brief Get current write position in flash
     */
    size_t get_write_offset() const { return m_write_offset; }
    
    /**
     * @brief Get total bytes written this session
     */
    size_t get_bytes_written() const { return m_bytes_written; }
    
    /**
     * @brief Get partition size
     */
    size_t get_partition_size() const { return m_partition_size; }
    
    /**
     * @brief Read raw data from flash partition
     * 
     * Used by download system to retrieve logged data.
     * 
     * @param offset Offset in partition
     * @param buffer Output buffer
     * @param size Number of bytes to read
     * @return Number of bytes actually read
     */
    size_t read_flash(size_t offset, uint8_t* buffer, size_t size);
    
    /**
     * @brief Get session header from flash
     */
    bool read_session_header(session_header_t* header);
    
private:
    // Core 0 writer task
    static void writer_task_wrapper(void* arg);
    void writer_task_loop();
    
    // Block compression and writing
    void flush_block();
    void write_session_header();
    void save_offset_to_nvs();
    
    // Partition and NVS
    const esp_partition_t* m_partition;
    nvs_handle_t m_nvs_handle;
    
    // Circular buffer management
    size_t m_partition_size;
    size_t m_write_offset;
    size_t m_session_start_offset;
    size_t m_bytes_written;
    
    // Session tracking
    session_header_t m_session_header;
    uint8_t m_startup_id[16];  // UUID
    
    // Sample buffering
    static constexpr size_t SAMPLE_BUFFER_SIZE = 4096;
    uint8_t m_sample_buffer[SAMPLE_BUFFER_SIZE];
    size_t m_sample_buffer_pos;
    int64_t m_block_timestamp_us;
    
    // Task control
    TaskHandle_t m_writer_task;
    QueueHandle_t m_sample_queue;
    bool m_running;
    bool m_paused;
    
    // Sample queue item
    struct SampleData {
        uint8_t type;
        int64_t timestamp_us;
        union {
            struct {
                float x, y, z;
            } xyz;
            struct {
                double latitude, longitude;
                float altitude, speed, heading, hdop;
            } gps;
            struct {
                float voltage, current, soc;
            } battery;
        } data;
    };
    
    static constexpr size_t QUEUE_SIZE = 50;  // ~5 seconds at 10Hz
};
