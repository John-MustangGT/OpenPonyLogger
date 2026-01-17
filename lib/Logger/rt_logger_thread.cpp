#include "rt_logger_thread.h"
#include "../../../lib/WiFi/include/wifi_manager.h"
#include <Arduino.h>
#include <cstring>
#include <ArduinoJson.h>

RTLoggerThread::RTLoggerThread(SensorManager* sensor_manager, uint32_t update_rate_ms,
                               uint32_t gps_rate_ms, uint32_t imu_rate_ms, uint32_t obd_rate_ms)
    : m_sensor_manager(sensor_manager), m_update_rate_ms(update_rate_ms),
      m_gps_rate_ms(gps_rate_ms == 0 ? update_rate_ms : gps_rate_ms),
      m_imu_rate_ms(imu_rate_ms == 0 ? update_rate_ms : imu_rate_ms),
      m_obd_rate_ms(obd_rate_ms == 0 ? update_rate_ms : obd_rate_ms),
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
    
    // Timing trackers for individual sensors
    uint32_t last_gps_update = 0;
    uint32_t last_imu_update = 0;
    uint32_t last_obd_update = 0;
    
    // Debug: Print once at start
    static bool first_run = true;
    if (first_run) {
        Serial.printf("RT Logger thread started - Main: %dms, GPS: %dms, IMU: %dms, OBD: %dms\n",
                     m_update_rate_ms, m_gps_rate_ms, m_imu_rate_ms, m_obd_rate_ms);
        Serial.flush();
        first_run = false;
    }
    
    while (m_running) {
        uint32_t now_ms = millis();
        bool any_updated = false;
        
        // Update GPS if interval elapsed
        if (now_ms - last_gps_update >= m_gps_rate_ms) {
            m_sensor_manager->update_gps();
            m_last_gps = m_sensor_manager->get_gps();
            last_gps_update = now_ms;
            any_updated = true;
        }
        
        // Update IMU if interval elapsed
        if (now_ms - last_imu_update >= m_imu_rate_ms) {
            m_sensor_manager->update_imu();
            m_last_accel = m_sensor_manager->get_accel();
            m_last_gyro = m_sensor_manager->get_gyro();
            m_last_compass = m_sensor_manager->get_comp();
            last_imu_update = now_ms;
            any_updated = true;
        }
        
        // Update OBD if interval elapsed (handled separately in OBD driver)
        if (now_ms - last_obd_update >= m_obd_rate_ms) {
            // OBD updates would go here when implemented
            last_obd_update = now_ms;
        }
        
        // Always update battery (low frequency sensor)
        m_sensor_manager->update_battery();
        m_last_battery = m_sensor_manager->get_battery();
        
        if (any_updated) {
            m_sample_count++;
            
            // Broadcast to WebSocket clients at 5Hz (every 200ms)
            static uint32_t last_broadcast_ms = 0;
            
            if (WiFiManager::is_initialized() && (now_ms - last_broadcast_ms) >= 200) {
                last_broadcast_ms = now_ms;
                
                // Create JSON document with sensor data
                // Use JsonDocument to avoid dynamic allocation
                JsonDocument doc;
                doc["type"] = "sensor";
                doc["uptime_ms"] = now_ms;
                doc["sample_count"] = m_sample_count;
                doc["is_paused"] = m_storage_paused;
                
                // GPS data
                doc["gps_valid"] = m_last_gps.valid;
                doc["latitude"] = m_last_gps.latitude;
                doc["longitude"] = m_last_gps.longitude;
                doc["altitude"] = m_last_gps.altitude;
                doc["speed"] = m_last_gps.speed;
                doc["satellites"] = m_last_gps.satellites;
                
                // Accelerometer
                doc["accel_x"] = m_last_accel.x;
                doc["accel_y"] = m_last_accel.y;
                doc["accel_z"] = m_last_accel.z;
                doc["temperature"] = m_last_accel.temperature;
                
                // Gyroscope
                doc["gyro_x"] = m_last_gyro.x;
                doc["gyro_y"] = m_last_gyro.y;
                doc["gyro_z"] = m_last_gyro.z;
                
                // Battery data
                doc["battery_soc"] = m_last_battery.state_of_charge;
                doc["battery_voltage"] = m_last_battery.voltage;
                doc["battery_current"] = m_last_battery.current;
                doc["battery_temp"] = m_last_battery.temperature / 100.0f;
                
                // Serialize and broadcast
                char json_buffer[512];
                size_t n = serializeJson(doc, json_buffer, sizeof(json_buffer));
                if (n > 0) {
                    WiFiManager::broadcast_json(json_buffer);
                }
            }
        }
        
        // Delay until next update
        vTaskDelay(delay_ticks);
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
