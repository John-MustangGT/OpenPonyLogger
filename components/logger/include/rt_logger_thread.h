#ifndef RT_LOGGER_THREAD_H
#define RT_LOGGER_THREAD_H

#include "sensor_hal.h"
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief Real-Time Logger Thread
 * Manages continuous sensor data collection and storage
 */
class RTLoggerThread {
public:
    /**
     * @brief Constructor
     * @param sensor_manager Initialized SensorManager instance
     * @param update_rate_ms Update frequency in milliseconds
     * @param storage_write_callback Callback function on storage write
     */
    RTLoggerThread(SensorManager* sensor_manager, uint32_t update_rate_ms = 100);
    
    ~RTLoggerThread();
    
    /**
     * @brief Start the logger thread
     * @return true if successfully started
     */
    bool start();
    
    /**
     * @brief Stop the logger thread
     */
    void stop();
    
    /**
     * @brief Register a callback for storage write events
     * @param callback Function pointer that receives latest sensor data
     */
    void set_storage_write_callback(void (*callback)(const gps_data_t&, const accel_data_t&, 
                                                      const gyro_data_t&, const compass_data_t&));
    
    /**
     * @brief Get the latest GPS data
     */
    gps_data_t get_last_gps() const;
    
    /**
     * @brief Get the latest accelerometer data
     */
    accel_data_t get_last_accel() const;
    
    /**
     * @brief Get the latest gyroscope data
     */
    gyro_data_t get_last_gyro() const;
    
    /**
     * @brief Get the latest compass data
     */
    compass_data_t get_last_compass() const;
    
    /**
     * @brief Get the number of samples logged
     */
    uint32_t get_sample_count() const;
    
    /**
     * @brief Force a storage write and report via callback
     */
    void trigger_storage_write();

private:
    SensorManager* m_sensor_manager;
    uint32_t m_update_rate_ms;
    TaskHandle_t m_task_handle;
    bool m_running;
    
    // Latest sensor data
    gps_data_t m_last_gps;
    accel_data_t m_last_accel;
    gyro_data_t m_last_gyro;
    compass_data_t m_last_compass;
    uint32_t m_sample_count;
    
    // Storage write callback
    void (*m_storage_write_callback)(const gps_data_t&, const accel_data_t&, 
                                     const gyro_data_t&, const compass_data_t&);
    
    // Static task function wrapper
    static void task_wrapper(void* arg);
    
    // Main task loop
    void task_loop();
};

#endif // RT_LOGGER_THREAD_H
