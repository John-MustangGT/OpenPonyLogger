#include "rt_logger_thread.h"
#include <Arduino.h>
#include <cstring>

RTLoggerThread::RTLoggerThread(SensorManager* sensor_manager, uint32_t update_rate_ms)
    : m_sensor_manager(sensor_manager), m_update_rate_ms(update_rate_ms),
      m_task_handle(nullptr), m_running(false), m_storage_paused(false),
      m_mark_event(false), m_sample_count(0),
      m_storage_write_callback(nullptr) {
    memset(&m_last_gps, 0, sizeof(m_last_gps));
    memset(&m_last_accel, 0, sizeof(m_last_accel));
    memset(&m_last_gyro, 0, sizeof(m_last_gyro));
    memset(&m_last_compass, 0, sizeof(m_last_compass));
    memset(&m_last_battery, 0, sizeof(m_last_battery));
}

RTLoggerThread::~RTLoggerThread() {
    stop();
}

bool RTLoggerThread::start() {
    if (m_running || !m_sensor_manager) {
        return false;
    }
    
    m_running = true;
    m_sample_count = 0;
    
    // Create FreeRTOS task
    BaseType_t result = xTaskCreate(
        task_wrapper,           // Task function
        "RTLogger",             // Task name
        4096,                   // Stack size
        this,                   // Parameter (pointer to this)
        2,                      // Priority (0 = lowest, higher = more important)
        &m_task_handle          // Task handle
    );
    
    return result == pdPASS;
}

void RTLoggerThread::stop() {
    if (m_running && m_task_handle) {
        m_running = false;
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
}

void RTLoggerThread::set_storage_write_callback(
    void (*callback)(const gps_data_t&, const accel_data_t&, 
                     const gyro_data_t&, const compass_data_t&,
                     const battery_data_t&)) {
    m_storage_write_callback = callback;
}

gps_data_t RTLoggerThread::get_last_gps() const {
    return m_last_gps;
}

accel_data_t RTLoggerThread::get_last_accel() const {
    return m_last_accel;
}

gyro_data_t RTLoggerThread::get_last_gyro() const {
    return m_last_gyro;
}

compass_data_t RTLoggerThread::get_last_compass() const {
    return m_last_compass;
}

battery_data_t RTLoggerThread::get_last_battery() const {
    return m_last_battery;
}

uint32_t RTLoggerThread::get_sample_count() const {
    return m_sample_count;
}

void RTLoggerThread::trigger_storage_write() {
    if (m_storage_write_callback) {
        m_storage_write_callback(m_last_gps, m_last_accel, m_last_gyro, m_last_compass, m_last_battery);
    }
}

void RTLoggerThread::task_wrapper(void* arg) {
    RTLoggerThread* logger = static_cast<RTLoggerThread*>(arg);
    if (logger) {
        logger->task_loop();
    }
}

void RTLoggerThread::task_loop() {
    const TickType_t delay_ticks = pdMS_TO_TICKS(m_update_rate_ms);
    
    // Debug: Print once at start
    static bool first_run = true;
    if (first_run) {
        Serial.println("RT Logger thread loop started");
        Serial.flush();
        first_run = false;
    }
    
    TickType_t next_wake_time = xTaskGetTickCount();
    
    while (m_running) {
        // Use absolute timing for consistent rate
        next_wake_time += delay_ticks;
        // Update all sensors through HAL
        if (m_sensor_manager->update_all()) {
            // Capture latest sensor data
            m_last_gps = m_sensor_manager->get_gps();
            m_last_accel = m_sensor_manager->get_accel();
            m_last_gyro = m_sensor_manager->get_gyro();
            m_last_compass = m_sensor_manager->get_comp();
            m_last_battery = m_sensor_manager->get_battery();
            
            m_sample_count++;
        } else {
            // Debug: Report update failure periodically
            static uint32_t last_error_report = 0;
            uint32_t now = millis();
            if (now - last_error_report > 5000) {
                Serial.println("WARNING: update_all() returned false");
                Serial.flush();
                last_error_report = now;
            }
        }
        
        // Delay until next update using absolute timing with safety checks
        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed_since_target = now - next_wake_time;
        
        if (elapsed_since_target > (delay_ticks * 2)) {
            // Severely behind - reset timing and warn
            static uint32_t last_warn = 0;
            if (millis() - last_warn > 5000) {
                Serial.printf("[RTLogger] WARNING: Running slow - execution taking >%dms\n", m_update_rate_ms * 2);
                last_warn = millis();
            }
            next_wake_time = now + delay_ticks;  // Start fresh from now
            vTaskDelay(2);  // Yield more aggressively
        } else if (elapsed_since_target > delay_ticks) {
            // Moderately behind - reset and minimal yield
            next_wake_time = now + delay_ticks;
            vTaskDelay(1);
        } else {
            // On time - use precise absolute timing
            vTaskDelayUntil(&next_wake_time, delay_ticks);
        }
    }
}

void RTLoggerThread::pause_storage() {
    m_storage_paused = true;
    Serial.println("[RTLogger] Storage paused");
    Serial.flush();
}

void RTLoggerThread::resume_storage() {
    m_storage_paused = false;
    Serial.println("[RTLogger] Storage resumed");
    Serial.flush();
}

bool RTLoggerThread::is_storage_paused() const {
    return m_storage_paused;
}

void RTLoggerThread::mark_event() {
    m_mark_event = true;
    Serial.println("[RTLogger] Event marked");
    Serial.flush();
}
