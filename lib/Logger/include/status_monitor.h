#ifndef STATUS_MONITOR_H
#define STATUS_MONITOR_H

#include "sensor_hal.h"
#include "rt_logger_thread.h"
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief Status Monitor Thread - Core 0
 * Periodically reports on system status, write count, and latest sensor values
 */
class StatusMonitor {
public:
    /**
     * @brief Constructor
     * @param rt_logger RTLoggerThread instance for accessing last sensor values
     * @param report_interval_ms How often to print status (default 10 seconds)
     */
    StatusMonitor(RTLoggerThread* rt_logger, uint32_t report_interval_ms = 10000);
    
    ~StatusMonitor();
    
    /**
     * @brief Start the status monitor thread on core 0
     * @return true if successfully started
     */
    bool start();
    
    /**
     * @brief Stop the status monitor thread
     */
    void stop();
    
    /**
     * @brief Get the write count since startup
     */
    uint32_t get_write_count() const;
    
    /**
     * @brief Increment write counter (called by storage write callback)
     */
    void increment_write_count();
    
    /**
     * @brief Print current status immediately
     */
    void print_status_now();

private:
    RTLoggerThread* m_rt_logger;
    uint32_t m_report_interval_ms;
    TaskHandle_t m_task_handle;
    bool m_running;
    uint32_t m_write_count;
    uint32_t m_last_report_time;
    
    // Static task function wrapper
    static void task_wrapper(void* arg);
    
    // Main task loop
    void task_loop();
    
    // Helper to print sensor status
    void print_sensor_status();
};

#endif // STATUS_MONITOR_H
