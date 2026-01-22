#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "sensor_hal.h"
#include "session_header.h"
#include "log_block.h"

// Forward declaration
class RTCManager;

/**
 * @brief SD card storage writer for logging
 *
 * Writes binary log data to SD card in .opl files.
 * Tracks current session in NVS for persistence across reboots.
 * Runs on Core 0 with low priority to avoid interfering with RT operations.
 *
 * API-compatible with FlashStorage for easy build-time substitution.
 */
class SDStorage {
public:
    SDStorage();
    ~SDStorage();

    /**
     * @brief Initialize SD card storage subsystem
     *
     * - Initializes SD card with SPI
     * - Creates log directory
     * - Restores system time from RTC or NVS
     * - Creates session header and file
     * - Starts writer task on Core 0
     *
     * @param rtc_manager Optional RTCManager for time recovery
     * @return true if initialization successful
     */
    bool begin(RTCManager* rtc_manager = nullptr);

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
     * @param obd OBD-II data
     */
    void write_sample(const gps_data_t& gps, const accel_data_t& accel,
                     const gyro_data_t& gyro, const compass_data_t& compass,
                     const battery_data_t& battery, const obd_data_t& obd);

    /**
     * @brief Queue OBD sample directly from Core 0 (StatusMonitor)
     *
     * Called when OBD data is updated via BLE. Uses the timestamp embedded
     * in the obd_data_t structure for accurate timing.
     *
     * @param obd OBD-II data with timestamp_us already set
     */
    void queue_obd_sample(const obd_data_t& obd);

    /**
     * @brief Pause writing (for downloads/file access)
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
     * @brief Get current write position in file
     */
    size_t get_write_offset() const { return m_bytes_written; }

    /**
     * @brief Get total bytes written this session
     */
    size_t get_bytes_written() const { return m_bytes_written; }

    /**
     * @brief Get SD card total size
     */
    size_t get_partition_size() const { return m_sd_total_bytes; }

    /**
     * @brief Get current log file path
     */
    String get_current_file_path() const { return m_current_file_path; }

    /**
     * @brief Get session header
     */
    bool read_session_header(session_start_header_t* header);

    /**
     * @brief Check if SD card is available
     */
    bool is_card_present() const { return m_card_present; }

private:
    // Core 0 writer task
    static void writer_task_wrapper(void* arg);
    void writer_task_loop();

    // Block compression and writing
    void flush_block();
    void write_session_header();
    bool create_new_session_file();

    // SD card management
    bool m_card_present;
    uint64_t m_sd_total_bytes;
    String m_current_file_path;
    File m_current_file;

    // NVS for session tracking
    nvs_handle_t m_nvs_handle;

    // Session tracking
    session_start_header_t m_session_header;
    uint8_t m_startup_id[16];  // UUID
    size_t m_bytes_written;

    // Sample buffering
    static constexpr size_t SAMPLE_BUFFER_SIZE = 4096;
    uint8_t m_sample_buffer[SAMPLE_BUFFER_SIZE];
    size_t m_sample_buffer_pos;
    int64_t m_block_timestamp_us;

    // Task control
    TaskHandle_t m_writer_task;
    QueueHandle_t m_sample_queue;
    StaticQueue_t* m_queue_buffer;   // PSRAM-allocated queue control structure
    uint8_t* m_queue_storage;        // PSRAM-allocated queue storage area
    bool m_running;
    bool m_paused;

    // Queue monitoring
    uint32_t m_queue_overruns;
    uint32_t m_samples_queued;

    // Sample queue item (same structure as FlashStorage)
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
                float rpm, speed, throttle;
                float coolant_temp, maf, intake_temp;
            } obd;
            struct {
                float voltage, current, soc;
            } battery;
        } data;
    };

    static constexpr size_t QUEUE_SIZE = 500;  // ~50 seconds at 10Hz
};
