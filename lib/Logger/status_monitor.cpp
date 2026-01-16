#include "status_monitor.h"
#include <Arduino.h>
#include <cstdio>

StatusMonitor::StatusMonitor(RTLoggerThread* rt_logger, uint32_t report_interval_ms)
    : m_rt_logger(rt_logger),
      m_report_interval_ms(report_interval_ms),
      m_task_handle(nullptr),
      m_running(false),
      m_write_count(0),
      m_last_report_time(0) {
}

StatusMonitor::~StatusMonitor() {
    stop();
}

bool StatusMonitor::start() {
    if (m_running) {
        return false;
    }
    
    m_running = true;
    m_last_report_time = millis();
    
    // Create task on core 0
    // xTaskCreatePinnedToCore(function, name, stack, param, priority, handle, core)
    BaseType_t result = xTaskCreatePinnedToCore(
        StatusMonitor::task_wrapper,
        "StatusMonitor",
        4096,
        this,
        1,  // Priority
        &m_task_handle,
        0   // Core 0
    );
    
    return result == pdPASS;
}

void StatusMonitor::stop() {
    m_running = false;
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
}

uint32_t StatusMonitor::get_write_count() const {
    return m_write_count;
}

void StatusMonitor::increment_write_count() {
    m_write_count++;
}

void StatusMonitor::print_status_now() {
    uint32_t uptime_ms = millis();
    uint32_t uptime_sec = uptime_ms / 1000;
    
    Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
    Serial.printf("║ STATUS REPORT - Uptime: %u:%02u:%02u (writes: %u)           ║\n",
                  uptime_sec / 3600, (uptime_sec / 60) % 60, uptime_sec % 60, m_write_count);
    Serial.println("╠═══════════════════════════════════════════════════════════╣");
    
    if (m_rt_logger != nullptr) {
        // Get latest sensor data
        gps_data_t gps = m_rt_logger->get_last_gps();
        accel_data_t accel = m_rt_logger->get_last_accel();
        gyro_data_t gyro = m_rt_logger->get_last_gyro();
        compass_data_t compass = m_rt_logger->get_last_compass();
        battery_data_t battery = m_rt_logger->get_last_battery();
        uint32_t sample_count = m_rt_logger->get_sample_count();
        
        // GPS Status
        Serial.print("║ GPS: ");
        if (gps.valid) {
            Serial.printf("VALID - Lat:%.6f Lon:%.6f Alt:%.1fm Sats:%d\n",
                         gps.latitude, gps.longitude, gps.altitude, gps.satellites);
        } else {
            Serial.println("NO FIX                                           ");
        }
        Serial.println("║");
        
        // IMU Status
        Serial.printf("║ Accel: X=%.2fg Y=%.2fg Z=%.2fg                             \n",
                     accel.x, accel.y, accel.z);
        Serial.printf("║ Gyro:  X=%.1fdps Y=%.1fdps Z=%.1fdps                       \n",
                     gyro.x, gyro.y, gyro.z);
        Serial.printf("║ Compass: X=%.1fuT Y=%.1fuT Z=%.1fuT                       \n",
                     compass.x, compass.y, compass.z);
        Serial.println("║");
        
        // Battery Status
        Serial.printf("║ Battery: %.1f%% SOC | %.2fV | %d mA | %.1f°C           \n",
                     battery.state_of_charge, battery.voltage, (int)battery.current, 
                     battery.temperature / 100.0f);
        Serial.println("║");
        
        // Sample count
        Serial.printf("║ Samples logged: %u (%.1f samples/sec)                    \n",
                     sample_count, sample_count > 0 ? (float)sample_count / (uptime_sec > 0 ? uptime_sec : 1) : 0.0f);
    }
    
    Serial.println("╚═══════════════════════════════════════════════════════════╝\n");
}

void StatusMonitor::task_wrapper(void* arg) {
    StatusMonitor* monitor = static_cast<StatusMonitor*>(arg);
    if (monitor != nullptr) {
        monitor->task_loop();
    }
}

void StatusMonitor::task_loop() {
    while (m_running) {
        uint32_t now = millis();
        
        // Print status at regular intervals
        if (now - m_last_report_time >= m_report_interval_ms) {
            print_status_now();
            m_last_report_time = now;
        }
        
        // Small delay to prevent task from consuming all CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
