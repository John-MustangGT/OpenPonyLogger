#include "status_monitor.h"
#include "units_helper.h"
#include "wifi_manager.h"
#include "config_manager.h"
#include "icar_ble_driver.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>
#include <ArduinoJson.h>

// Button GPIO pins
#define BUTTON_D0 0   // Pause/Resume
#define BUTTON_D1 1   // Cycle display mode
#define BUTTON_D2 2   // Mark Event

// Button debounce settings
#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_LONG_PRESS_MS 1000

static const char* TAG = "STATUS";

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
    
    char buffer[128];
    
    Serial.println("╔═══════════════════════════════════════════════════════════╗");
    snprintf(buffer, sizeof(buffer), "║ STATUS REPORT - Uptime: %u:%02u:%02u (writes: %u)", 
             uptime_sec / 3600, (uptime_sec / 60) % 60, uptime_sec % 60, m_write_count);
    Serial.println(buffer);
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
        if (gps.valid) {
            snprintf(buffer, sizeof(buffer), "║ GPS: VALID - Lat:%.6f Lon:%.6f Alt:%.1fm Sats:%d Time:%02u:%02u:%02u",
                     gps.latitude, gps.longitude, gps.altitude, gps.satellites, gps.hour, gps.minute, gps.second);
            Serial.println(buffer);
        } else {
            Serial.println("║ GPS: NO FIX");
        }
        Serial.println("║");
        
        // IMU Status
        snprintf(buffer, sizeof(buffer), "║ Accel: X=%.2fg Y=%.2fg Z=%.2fg | Temp: %.1f%s",
                 accel.x, accel.y, accel.z, convert_temperature(accel.temperature), get_temp_unit());
        Serial.println(buffer);
        snprintf(buffer, sizeof(buffer), "║ Gyro:  X=%.1fdps Y=%.1fdps Z=%.1fdps",
                 gyro.x, gyro.y, gyro.z);
        Serial.println(buffer);
        snprintf(buffer, sizeof(buffer), "║ Compass: X=%.1fuT Y=%.1fuT Z=%.1fuT",
                 compass.x, compass.y, compass.z);
        Serial.println(buffer);
        Serial.println("║");
        
        // Battery Status
        snprintf(buffer, sizeof(buffer), "║ Battery: %.1f%% SOC | %.2fV | %d mA | %.1f°C",
                 battery.state_of_charge, battery.voltage, (int)battery.current, 
                 battery.temperature / 100.0f);
        Serial.println(buffer);
        Serial.println("║");
        
        // Sample count
        snprintf(buffer, sizeof(buffer), "║ Samples logged: %u (%.1f samples/sec)",
                 sample_count, sample_count > 0 ? (float)sample_count / (uptime_sec > 0 ? uptime_sec : 1) : 0.0f);
        Serial.println(buffer);
        
        // Update display based on current mode
        DisplayMode current_mode = ST7789Display::get_display_mode();
        bool is_paused = m_rt_logger->is_storage_paused();
        
        if (current_mode == DisplayMode::MAIN_SCREEN) {
            // Show sensor data
            ST7789Display::update(
                uptime_ms,
                accel.temperature,
                accel.x, accel.y, accel.z,
                gyro.x, gyro.y, gyro.z,
                battery.state_of_charge, battery.voltage,
                gps.valid, sample_count,
                is_paused,
                gps.latitude, gps.longitude, gps.altitude,
                gps.hour, gps.minute, gps.second,
                gps.speed
            );
        } else if (current_mode == DisplayMode::INFO_SCREEN) {
            // Show IP/BLE information
            ST7789Display::show_info_screen("192.168.1.1", "OpenPonyLogger");
        }
        // DisplayMode::DARK - do nothing, display is off
        
        // Update NeoPixel state based on pause and GPS status
        if (is_paused) {
            // When paused, show slow flash regardless of GPS state
            NeoPixelStatus::setState(NeoPixelStatus::State::PAUSED);
        } else if (gps.valid) {
            // Normal operation with GPS lock
            NeoPixelStatus::setState(NeoPixelStatus::State::GPS_3D_FIX);
        } else {
            // Normal operation, searching for GPS
            NeoPixelStatus::setState(NeoPixelStatus::State::NO_GPS_FIX);
        }
    }
    
    Serial.println("╚═══════════════════════════════════════════════════════════╝");
    Serial.flush();
}

void StatusMonitor::task_wrapper(void* arg) {
    StatusMonitor* monitor = static_cast<StatusMonitor*>(arg);
    if (monitor != nullptr) {
        monitor->task_loop();
    }
}

void StatusMonitor::task_loop() {
    // Button state tracking
    int d0_last_state = HIGH;
    int d1_last_state = HIGH;
    int d2_last_state = HIGH;
    uint32_t d0_press_time = 0;
    uint32_t d1_press_time = 0;
    uint32_t d2_press_time = 0;
    bool d0_pressed = false;
    bool d1_pressed = false;
    bool d2_pressed = false;
    
    while (m_running) {
        uint32_t now = millis();
        
        // ===== Handle D0 Button (Pause/Resume) =====
        int d0_state = digitalRead(BUTTON_D0);
        if (d0_state != d0_last_state) {
            d0_press_time = now;
            d0_pressed = false;
        }
        
        // Check for debounced button press
        if (d0_state == LOW && !d0_pressed && (now - d0_press_time) >= BUTTON_DEBOUNCE_MS) {
            d0_pressed = true;
            // Toggle pause state
            if (m_rt_logger != nullptr) {
                if (m_rt_logger->is_storage_paused()) {
                    m_rt_logger->resume_storage();
                    Serial.println("[Button] D0: Storage RESUMED");
                } else {
                    m_rt_logger->pause_storage();
                    Serial.println("[Button] D0: Storage PAUSED");
                }
            }
        }
        d0_last_state = d0_state;
        
        // ===== Handle D1 Button (Cycle Display Mode) =====
        // D1 is pulled LOW by default, goes HIGH when pressed
        int d1_state = digitalRead(BUTTON_D1);
        if (d1_state != d1_last_state) {
            d1_press_time = now;
            d1_pressed = false;
            Serial.printf("[D1] State changed: %d\n", d1_state);
            Serial.flush();
        }
        
        // Check for debounced button press (HIGH state for D1)
        if (d1_state == HIGH && !d1_pressed && (now - d1_press_time) >= BUTTON_DEBOUNCE_MS) {
            d1_pressed = true;
            Serial.println("[Button] D1: Display mode cycled!");
            // Cycle display mode
            ST7789Display::cycle_display_mode();
            
            // Handle NeoPixel enable/disable based on mode
            DisplayMode current_mode = ST7789Display::get_display_mode();
            if (current_mode == DisplayMode::DARK) {
                NeoPixelStatus::set_enabled(false);
            } else {
                NeoPixelStatus::set_enabled(true);
            }
        }
        d1_last_state = d1_state;
        
        // ===== Handle D2 Button (Mark Event) =====
        // D2 is pulled LOW by default, goes HIGH when pressed
        int d2_state = digitalRead(BUTTON_D2);
        if (d2_state != d2_last_state) {
            d2_press_time = now;
            d2_pressed = false;
            Serial.printf("[D2] State changed: %d\n", d2_state);
            Serial.flush();
        }
        
        // Check for debounced button press (HIGH state for D2)
        if (d2_state == HIGH && !d2_pressed && (now - d2_press_time) >= BUTTON_DEBOUNCE_MS) {
            d2_pressed = true;
            // Mark event - log regardless of pause state
            Serial.println("Event");
            if (m_rt_logger != nullptr && !m_rt_logger->is_storage_paused()) {
                m_rt_logger->mark_event();
                Serial.println("[Button] D2: Event marked in storage!");
            }
        }
        d2_last_state = d2_state;
        
        // Broadcast sensor data via WebSocket at 2Hz (every 500ms)
        // Only when clients are connected to minimize overhead
        static uint32_t last_ws_broadcast_ms = 0;
        static uint32_t last_config_check_ms = 0;
        static bool obd_ble_enabled = true;
        
        // Update OBD BLE enabled state every 5 seconds
        if (now - last_config_check_ms >= 5000) {
            obd_ble_enabled = ConfigManager::get_current().obd_ble_enabled;
            last_config_check_ms = now;
        }
        
        if (WiFiManager::is_initialized() && WiFiManager::has_clients() && 
            (now - last_ws_broadcast_ms) >= 500) {
            last_ws_broadcast_ms = now;
            
            if (m_rt_logger != nullptr) {
                // Get latest sensor data
                gps_data_t gps = m_rt_logger->get_last_gps();
                accel_data_t accel = m_rt_logger->get_last_accel();
                gyro_data_t gyro = m_rt_logger->get_last_gyro();
                battery_data_t battery = m_rt_logger->get_last_battery();
                uint32_t sample_count = m_rt_logger->get_sample_count();
                bool is_paused = m_rt_logger->is_storage_paused();
                
                // Create JSON document with sensor data
                JsonDocument doc;
                doc["type"] = "sensor";
                doc["uptime_ms"] = now;
                doc["sample_count"] = sample_count;
                doc["is_paused"] = is_paused;
                
                // GPS data
                doc["gps_valid"] = gps.valid;
                doc["latitude"] = gps.latitude;
                doc["longitude"] = gps.longitude;
                doc["altitude"] = gps.altitude;
                doc["speed"] = gps.speed;
                doc["satellites"] = gps.satellites;
                
                // Accelerometer
                doc["accel_x"] = accel.x;
                doc["accel_y"] = accel.y;
                doc["accel_z"] = accel.z;
                doc["temperature"] = accel.temperature;
                
                // Gyroscope
                doc["gyro_x"] = gyro.x;
                doc["gyro_y"] = gyro.y;
                doc["gyro_z"] = gyro.z;
                
                // Battery data
                doc["battery_soc"] = battery.state_of_charge;
                doc["battery_voltage"] = battery.voltage;
                doc["battery_current"] = battery.current;
                doc["battery_temp"] = battery.temperature / 100.0f;
                
                // OBD data (if connected and enabled)
                bool obd_available = false;
                if (obd_ble_enabled) {
                    try {
                        obd_available = IcarBleDriver::is_connected();
                    } catch (...) {
                        obd_available = false;
                    }
                }
                
                if (obd_available) {
                    try {
                        obd_data_t obd = IcarBleDriver::get_data();
                        JsonObject obd_obj = doc["obd"].to<JsonObject>();
                        obd_obj["connected"] = true;
                        obd_obj["rpm"] = obd.engine_rpm;
                        obd_obj["speed"] = obd.vehicle_speed;
                        obd_obj["throttle"] = obd.throttle_position;
                        obd_obj["load"] = obd.engine_load;
                        obd_obj["coolant_temp"] = obd.coolant_temp;
                        obd_obj["intake_temp"] = obd.intake_temp;
                        obd_obj["maf"] = obd.maf_flow;
                        obd_obj["timing_advance"] = obd.timing_advance;
                    } catch (...) {
                        doc["obd"]["connected"] = false;
                    }
                } else {
                    doc["obd"]["connected"] = false;
                }
                
                // Serialize and broadcast
                static EXT_RAM_ATTR char json_buffer[768];
                size_t n = serializeJson(doc, json_buffer, sizeof(json_buffer));
                if (n > 0 && n < sizeof(json_buffer)) {
                    WiFiManager::broadcast_json(json_buffer);
                }
            }
        }
        
        // Print status at regular intervals
        if (now - m_last_report_time >= m_report_interval_ms) {
            print_status_now();
            m_last_report_time = now;
        }
        
        // Update NeoPixel animation (for flashing states)
        NeoPixelStatus::update(now);
        
        // Small delay to prevent task from consuming all CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
