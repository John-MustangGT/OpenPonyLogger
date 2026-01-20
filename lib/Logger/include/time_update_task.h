#ifndef TIME_UPDATE_TASK_H
#define TIME_UPDATE_TASK_H

#include "rtc_manager.h"
#include <cstdint>
#include <time.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

/**
 * @brief Low-priority task to sync GPS time to RTC and NVS
 * 
 * Runs at ~1Hz priority 1 (lowest priority).
 * When GPS provides valid time, updates:
 * - ESP32-S3 internal RTC
 * - PCF8523 (if available)
 * - NVS "gps_time" key for boot-time recovery
 * 
 * Usage:
 *   auto time_updater = new TimeUpdateTask(rtc_manager);
 *   time_updater->start();
 *   
 *   // Later, when GPS locks:
 *   time_updater->notify_gps_time_available(unix_timestamp);
 */
class TimeUpdateTask {
public:
    /**
     * @brief Constructor
     * @param rtc_manager Reference to RTCManager instance
     */
    explicit TimeUpdateTask(RTCManager& rtc_manager);

    /**
     * @brief Destructor
     */
    ~TimeUpdateTask();

    /**
     * @brief Start the low-priority update task
     * @return true if task started successfully
     */
    bool start();

    /**
     * @brief Notify task that GPS time is available (call from GPS thread)
     * @param unix_time Unix timestamp from valid GPS GPRMC sentence
     */
    void notify_gps_time_available(time_t unix_time);

    /**
     * @brief Stop the task (optional, auto-cleanup on destruct)
     */
    void stop();

private:
    RTCManager& m_rtc_manager;
    TaskHandle_t m_task_handle;
    
    // Queue for GPS time notifications
    static const uint32_t QUEUE_LENGTH = 4;
    QueueHandle_t m_time_queue;

    // Rate limiting intervals (seconds)
    uint32_t m_rtc_update_interval_sec;       // ESP32 RTC update interval
    uint32_t m_pcf8523_update_interval_sec;   // PCF8523 write interval
    uint32_t m_nvs_update_interval_sec;       // NVS commit interval

    // Last update timestamps
    time_t m_last_rtc_update;
    time_t m_last_pcf8523_update;
    time_t m_last_nvs_update;
    
    // Static wrapper for FreeRTOS task
    static void task_wrapper(void* param);
    
    // Actual task implementation
    void run();
};

#endif // TIME_UPDATE_TASK_H
