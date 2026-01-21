#include "status_monitor.h"
#include "units_helper.h"
#include "wifi_manager.h"
#include "config_manager.h"
#include "icar_ble_driver.h"
#include "debug_flags.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>
#include <esp_sleep.h>
#include <ArduinoJson.h>

// Button GPIO pins
#define BUTTON_D0 0   // Pause/Resume
#define BUTTON_D1 1   // Cycle display mode
#define BUTTON_D2 2   // Mark Event
#define VBUS_DETECT_PIN 19  // USB power detection

// Power management settings
#define USB_TIMEOUT_MS 60000  // 1 minute

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
      m_last_report_time(0),
      m_usb_powered(true),
      m_usb_loss_time(0),
      m_shutdown_pending(false),
      m_shutdown_initiated(false) {
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
        6144,
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
        
        float sample_hz = sample_count > 0 && uptime_sec > 0 ? (float)sample_count / uptime_sec : 0.0f;
        if (!isfinite(sample_hz) || sample_hz < 0.0f) {
            sample_hz = 0.0f;
        }
        // Sample count
        snprintf(buffer, sizeof(buffer), "║ Samples logged: %u (%.1f samples/sec)",
             sample_count, sample_hz);
        Serial.println(buffer);
        
        // OBD BLE status
        bool obd_connected = IcarBleDriver::is_connected();
        const char* obd_device = IcarBleDriver::get_device_name();
        snprintf(buffer, sizeof(buffer), "║ OBD status check: connected=%d", obd_connected);
        Serial.println(buffer);
        if (obd_connected && obd_device[0] != '\0') {
            snprintf(buffer, sizeof(buffer), "║ OBD device: %s", obd_device);
            Serial.println(buffer);
        }
        
        // Show recently seen BLE devices
        const auto& recent_devices = IcarBleDriver::get_recent_devices();
        if (!recent_devices.empty()) {
            snprintf(buffer, sizeof(buffer), "║ Recent BLE devices (%zu seen):", recent_devices.size());
            Serial.println(buffer);
            for (const auto& dev : recent_devices) {
                snprintf(buffer, sizeof(buffer), "║   '%s' (%s) RSSI=%d", 
                         dev.name, dev.address, dev.rssi);
                Serial.println(buffer);
            }
        } else {
            const bool pending = IcarBleDriver::has_pending_connection();
            const uint32_t pending_age = IcarBleDriver::pending_connection_age_ms();
            snprintf(buffer, sizeof(buffer), "║ Recent BLE devices: none in last 30s (pending_conn=%d, age=%ums)",
                     pending ? 1 : 0, pending_age);
            Serial.println(buffer);
        }
        
        // NeoPixel state updated in main task loop (not here)
        // Display update moved to main task loop
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
    
    uint32_t loop_count = 0;
    uint32_t broadcast_count = 0;
    uint32_t yield_count = 0;
    
    Serial.println("[StatusMonitor] Task loop started on Core 0");
    
    while (m_running) {
        loop_count++;
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
            // Serial.flush() removed - blocks Core 0 for 10-50ms
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
            // Serial.flush() removed - blocks Core 0 for 10-50ms
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
        
        // ===== USB Power Monitoring =====
        // Dual detection method: GPIO19 + battery voltage
        // GPIO19 should read HIGH when USB connected, but may be unreliable
        // Battery voltage >4.05V usually indicates charging (USB present)
        bool gpio_usb = (digitalRead(VBUS_DETECT_PIN) == HIGH);
        
        // Get battery voltage for secondary detection
        battery_data_t battery_data = m_rt_logger ? m_rt_logger->get_last_battery() : battery_data_t{};
        float battery_voltage = battery_data.voltage;
        bool voltage_usb = (battery_voltage > 4.05f);  // Charging voltage threshold
        
        // Use OR logic: if either method detects USB, consider it present
        // This makes detection more robust
        bool usb_present = gpio_usb || voltage_usb;
        
        // Debug: Print USB detection status every 10 seconds
        static uint32_t last_debug_time = 0;
        if (DebugFlags::ENABLE_POWER_DEBUG && now - last_debug_time >= 10000) {
            Serial.printf("[Power DEBUG] GPIO19=%d, BattV=%.2fV, gpio_usb=%d, voltage_usb=%d, final_usb=%d, shutdown=%d\n",
                         digitalRead(VBUS_DETECT_PIN), battery_voltage, gpio_usb, voltage_usb, usb_present, m_shutdown_pending);
            last_debug_time = now;
        }
        
        if (!m_shutdown_pending) {
            if (usb_present && !m_usb_powered) {
                // USB power restored
                Serial.println("[Power] USB power restored!");
                m_usb_powered = true;
                m_usb_loss_time = 0;
            } else if (!usb_present && m_usb_powered) {
                // USB power lost - start countdown
                Serial.println("[Power] USB power lost! Starting 60-second countdown...");
                m_usb_powered = false;
                m_usb_loss_time = now;
                m_shutdown_pending = true;
                
                // Set NeoPixel to shutdown state (purple pulsing)
                NeoPixelStatus::setState(NeoPixelStatus::State::SHUTDOWN);
            }
        }
        
        // Handle shutdown countdown
        if (m_shutdown_pending && !m_shutdown_initiated) {
            if (usb_present) {
                // Power restored - cancel shutdown
                Serial.println("[Power] USB restored - shutdown canceled!");
                m_shutdown_pending = false;
                m_usb_powered = true;
                m_usb_loss_time = 0;
                
                // Restore previous NeoPixel state (will be updated in next status update)
            } else {
                // Check if timeout expired
                uint32_t time_since_loss = now - m_usb_loss_time;
                if (time_since_loss >= USB_TIMEOUT_MS) {
                    // Timeout expired - initiate shutdown
                    Serial.println("[Power] Countdown complete - initiating shutdown...");
                    m_shutdown_initiated = true;
                    
                    // Stop the task loop after this iteration
                    m_running = false;
                } else {
                    // Update shutdown screen every second
                    static uint32_t last_shutdown_update = 0;
                    if (now - last_shutdown_update >= 1000) {
                        uint32_t seconds_remaining = (USB_TIMEOUT_MS - time_since_loss) / 1000;
                        ST7789Display::show_shutdown_screen(seconds_remaining);
                        last_shutdown_update = now;
                    }
                }
            }
        }

        // Drive BLE scanning/connection from Core 0 to avoid NimBLE crashes on other cores
        // Reduced from 5Hz to 2Hz to minimize Core 0 blocking during BLE operations
        static uint32_t last_obd_update = 0;
        if (now - last_obd_update >= 500) { // 2 Hz on Core 0 (reduced from 5Hz)
            IcarBleDriver::update();
            last_obd_update = now;
            // Yield after BLE update as it can be blocking
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        // Yield to watchdog to prevent TWDT reset on Core 0
        vTaskDelay(pdMS_TO_TICKS(1));
        yield_count++;

        // NOTE: WebSocket broadcasts removed from StatusMonitor to reduce Core 0 load.
        // RTLoggerThread on Core 1 handles all WebSocket broadcasts at 5Hz (200ms interval).
        // This eliminates duplicate broadcasts and moves JSON serialization off Core 0.
        
        // Update display every 4 seconds (reduced from 2s to minimize Core 0 load)
        static uint32_t last_display_update = 0;
        if (m_rt_logger != nullptr && now - last_display_update >= 4000) {
            DisplayMode current_mode = ST7789Display::get_display_mode();
            bool is_paused = m_rt_logger->is_storage_paused();
            
            if (current_mode == DisplayMode::MAIN_SCREEN) {
                // Get latest sensor data for display
                gps_data_t gps = m_rt_logger->get_last_gps();
                accel_data_t accel = m_rt_logger->get_last_accel();
                gyro_data_t gyro = m_rt_logger->get_last_gyro();
                battery_data_t battery = m_rt_logger->get_last_battery();
                uint32_t sample_count = m_rt_logger->get_sample_count();
                uint32_t uptime_sec = now / 1000;
                float sample_hz = sample_count > 0 && uptime_sec > 0 ? (float)sample_count / uptime_sec : 0.0f;
                if (!isfinite(sample_hz) || sample_hz < 0.0f) sample_hz = 0.0f;
                
                uint32_t display_start = millis();
                ST7789Display::update(
                    now,
                    accel.temperature,
                    accel.x, accel.y, accel.z,
                    gyro.x, gyro.y, gyro.z,
                    battery.state_of_charge, battery.voltage,
                    gps.valid, sample_count, sample_hz,
                    is_paused,
                    gps.latitude, gps.longitude, gps.altitude,
                    gps.hour, gps.minute, gps.second,
                    gps.speed
                );
                
                if (DebugFlags::ENABLE_DISPLAY_TIMING) {
                    uint32_t display_elapsed = millis() - display_start;
                    if (display_elapsed > 10) {
                        Serial.printf("[Display] Update took %ums\n", display_elapsed);
                    }
                }
            } else if (current_mode == DisplayMode::INFO_SCREEN) {
                ST7789Display::show_info_screen("192.168.4.1", "OpenPonyLogger");
            }
            // DisplayMode::DARK - do nothing

            last_display_update = now;

            // Yield after display update to prevent watchdog starvation
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        // Print status at regular intervals (rate-limited to reduce Core 0 serial overhead)
        if (now - m_last_report_time >= m_report_interval_ms) {
            // Show sensor sample counts - rate limited to every 10 seconds instead of 1Hz
            static uint32_t last_sample_count_print = 0;
            if (DebugFlags::ENABLE_SAMPLE_COUNTS && m_rt_logger != nullptr &&
                now - last_sample_count_print >= 10000) {
                uint32_t gps_samples = m_rt_logger->get_gps_sample_count();
                uint32_t accel_samples = m_rt_logger->get_accel_sample_count();
                uint32_t gyro_samples = m_rt_logger->get_gyro_sample_count();
                uint32_t obd_samples = m_rt_logger->get_obd_sample_count();
                bool is_paused = m_rt_logger->is_storage_paused();
                Serial.printf("[10s] GPS:%u IMU:%u Gyro:%u OBD:%u | Paused:%d | Heap:%u\n",
                    gps_samples, accel_samples, gyro_samples, obd_samples, is_paused, ESP.getFreeHeap());
                last_sample_count_print = now;
            }
            
            // Full status report if enabled
            if (DebugFlags::ENABLE_STATUS_REPORT) {
                Serial.printf("[StatusMonitor] STATUS REPORT #%u (loops=%u, broadcasts=%u, yields=%u)\n",
                    m_write_count, loop_count, broadcast_count, yield_count);
                print_status_now();
                // Yield after large serial output to prevent blocking
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            m_last_report_time = now;
        }
        
        // Update NeoPixel state based on pause and GPS status
        if (m_rt_logger != nullptr) {
            bool is_paused = m_rt_logger->is_storage_paused();
            gps_data_t gps = m_rt_logger->get_last_gps();
            
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
        
        // Update NeoPixel animation (for flashing states)
        NeoPixelStatus::update(now);

        // Reduced delay from 100ms to 50ms now that WebSocket broadcasts are removed
        // This provides better button responsiveness while still yielding to other tasks
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Task loop has exited - handle shutdown if initiated
    if (m_shutdown_initiated) {
        Serial.println("[Power] Executing graceful shutdown sequence...");
        
        // Close current logging session
        if (m_rt_logger != nullptr) {
            Serial.println("[Power] Closing logging session...");
            // The RTLogger thread will write the final header when stopped
        }
        
        // Configure wake sources
        Serial.println("[Power] Configuring wake sources...");
        
        // Wake on GPIO19 (VBUS) rising edge - USB power restored
        esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_DETECT_PIN, 1);
        
        // Wake on GPIO0 (D0 button) falling edge - button press
        esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_D0, ESP_EXT1_WAKEUP_ANY_LOW);
        
        // Turn off display
        ST7789Display::off();
        
        // Final message
        Serial.println("[Power] Entering deep sleep...");
        Serial.println("[Power] Wake sources: USB power (GPIO19) or D0 button (GPIO0)");
        Serial.flush();
        
        delay(100);
        
        // Enter deep sleep
        esp_deep_sleep_start();
    }
}
