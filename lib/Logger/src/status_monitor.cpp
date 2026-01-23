#include "status_monitor.h"

// Include the appropriate storage backend
#if defined(USE_SD_CARD)
    #include "sd_storage.h"
#elif defined(USE_FLASH_STORAGE)
    #include "flash_storage.h"
#endif

#include "units_helper.h"
#include "wifi_manager.h"
#include "config_manager.h"
#include "icar_ble_driver.h"
#include "debug_flags.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <freertos/task.h>

// Button GPIO pins
#define BUTTON_D0 0   // Pause/Resume
#define BUTTON_D1 1   // Cycle display mode
#define BUTTON_D2 2   // Mark Event
#define VBUS_DETECT_PIN 19  // USB power detection

// Power management settings
#define USB_TIMEOUT_MS 60000  // 1 minute countdown before sleep
#define LIGHT_SLEEP_DURATION_US (5 * 60 * 1000000ULL)  // 5 minutes in microseconds

// Button debounce settings
#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_LONG_PRESS_MS 1000

static const char* TAG = "STATUS";

StatusMonitor::StatusMonitor(RTLoggerThread* rt_logger, StorageType* storage,
                             uint32_t report_interval_ms)
    : m_rt_logger(rt_logger),
      m_flash_storage(storage),
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
    
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════╗");
    snprintf(buffer, sizeof(buffer), "║ STATUS REPORT - Uptime: %u:%02u:%02u (writes: %u)",
             uptime_sec / 3600, (uptime_sec / 60) % 60, uptime_sec % 60, m_write_count);
    ESP_LOGI(TAG, "%s", buffer);
    ESP_LOGI(TAG, "╠═══════════════════════════════════════════════════════════╣");
    
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
            ESP_LOGI(TAG, "%s", buffer);
        } else {
            ESP_LOGI(TAG, "║ GPS: NO FIX");
        }
        ESP_LOGI(TAG, "║");
        
        // IMU Status
        snprintf(buffer, sizeof(buffer), "║ Accel: X=%.2fg Y=%.2fg Z=%.2fg | Temp: %.1f%s",
                 accel.x, accel.y, accel.z, convert_temperature(accel.temperature), get_temp_unit());
        ESP_LOGI(TAG, "%s", buffer);
        snprintf(buffer, sizeof(buffer), "║ Gyro:  X=%.1fdps Y=%.1fdps Z=%.1fdps",
                 gyro.x, gyro.y, gyro.z);
        ESP_LOGI(TAG, "%s", buffer);
        snprintf(buffer, sizeof(buffer), "║ Compass: X=%.1fuT Y=%.1fuT Z=%.1fuT",
                 compass.x, compass.y, compass.z);
        ESP_LOGI(TAG, "%s", buffer);
        ESP_LOGI(TAG, "║");
        
        // Battery Status
        snprintf(buffer, sizeof(buffer), "║ Battery: %.1f%% SOC | %.2fV | %d mA | %.1f°C",
                 battery.state_of_charge, battery.voltage, (int)battery.current,
                 battery.temperature / 100.0f);
        ESP_LOGI(TAG, "%s", buffer);
        ESP_LOGI(TAG, "║");
        
        float sample_hz = sample_count > 0 && uptime_sec > 0 ? (float)sample_count / uptime_sec : 0.0f;
        if (!isfinite(sample_hz) || sample_hz < 0.0f) {
            sample_hz = 0.0f;
        }
        // Sample count
        snprintf(buffer, sizeof(buffer), "║ Samples logged: %u (%.1f samples/sec)",
             sample_count, sample_hz);
        ESP_LOGI(TAG, "%s", buffer);
        
        // OBD BLE status
        bool obd_connected = IcarBleDriver::is_connected();
        const char* obd_device = IcarBleDriver::get_device_name();
        snprintf(buffer, sizeof(buffer), "║ OBD status check: connected=%d", obd_connected);
        ESP_LOGI(TAG, "%s", buffer);
        if (obd_connected && obd_device[0] != '\0') {
            snprintf(buffer, sizeof(buffer), "║ OBD device: %s", obd_device);
            ESP_LOGI(TAG, "%s", buffer);
        }
        
        // Show recently seen BLE devices
        const auto& recent_devices = IcarBleDriver::get_recent_devices();
        if (!recent_devices.empty()) {
            snprintf(buffer, sizeof(buffer), "║ Recent BLE devices (%zu seen):", recent_devices.size());
            ESP_LOGI(TAG, "%s", buffer);
            for (const auto& dev : recent_devices) {
                snprintf(buffer, sizeof(buffer), "║   '%s' (%s) RSSI=%d",
                         dev.name, dev.address, dev.rssi);
                ESP_LOGI(TAG, "%s", buffer);
            }
        } else {
            const bool pending = IcarBleDriver::has_pending_connection();
            const uint32_t pending_age = IcarBleDriver::pending_connection_age_ms();
            snprintf(buffer, sizeof(buffer), "║ Recent BLE devices: none in last 30s (pending_conn=%d, age=%ums)",
                     pending ? 1 : 0, pending_age);
            ESP_LOGI(TAG, "%s", buffer);
        }
        
        // NeoPixel state updated in main task loop (not here)
        // Display update moved to main task loop
    }
    
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════╝");
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

    Serial.println(">>> StatusMonitor::task_loop() ENTRY <<<");
    delay(100);  // Ensure it gets flushed
    ESP_LOGI(TAG, "[StatusMonitor] Task loop started on Core 0");
    Serial.println(">>> After ESP_LOGI <<<");

    while (m_running) {
        loop_count++;
        uint32_t now = millis();

        // Debug heartbeat every 2 seconds
        if (loop_count % 2000 == 0) {
            ESP_LOGI(TAG, "[DEBUG] Heartbeat: loop_count=%u, uptime=%ums", loop_count, now);
        }
        
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
                    ESP_LOGI(TAG, "[Button] D0: Storage RESUMED");
                } else {
                    m_rt_logger->pause_storage();
                    ESP_LOGI(TAG, "[Button] D0: Storage PAUSED");
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
            ESP_LOGI(TAG, "[D1] State changed: %d", d1_state);
        }

        // Check for debounced button press (HIGH state for D1)
        if (d1_state == HIGH && !d1_pressed && (now - d1_press_time) >= BUTTON_DEBOUNCE_MS) {
            d1_pressed = true;
            ESP_LOGI(TAG, "[Button] D1: Display mode cycled!");
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
            ESP_LOGI(TAG, "[D2] State changed: %d", d2_state);
        }

        // Check for debounced button press (HIGH state for D2)
        if (d2_state == HIGH && !d2_pressed && (now - d2_press_time) >= BUTTON_DEBOUNCE_MS) {
            d2_pressed = true;
            // Mark event - log regardless of pause state
            ESP_LOGI(TAG, "Event");
            if (m_rt_logger != nullptr && !m_rt_logger->is_storage_paused()) {
                m_rt_logger->mark_event();
                ESP_LOGI(TAG, "[Button] D2: Event marked in storage!");
            }
        }
        d2_last_state = d2_state;

        // ===== Auto-Start/Stop Logic =====
        // Separate auto-start/stop for dynamics (GPS/IMU) vs data (OBD)
        static uint32_t dynamics_stopped_since = 0;  // When vehicle stopped (for timeout)
        static uint32_t data_stopped_since = 0;      // When engine stopped (for timeout)
        static bool dynamics_was_moving = false;     // Track if we were moving
        static bool data_was_running = false;        // Track if engine was running

        logging_config_t config = ConfigManager::get_current();

        if (m_rt_logger != nullptr) {
            // Get current sensor data
            gps_data_t gps = m_rt_logger->get_last_gps();
            obd_data_t obd = IcarBleDriver::get_data();

            // === Dynamics Auto-Start/Stop (Speed-based) ===
            if (config.dynamics_auto_enabled) {
                float speed_mph = gps.speed * 0.621371f;  // Convert km/h to mph
                bool is_moving = (speed_mph > config.dynamics_start_speed_mph);

                if (is_moving && !dynamics_was_moving) {
                    // Started moving - auto-resume if paused
                    if (m_rt_logger->is_storage_paused()) {
                        m_rt_logger->resume_storage();
                        ESP_LOGI(TAG, "[Auto] Dynamics auto-started (speed: %.1f mph)", speed_mph);
                    }
                    dynamics_stopped_since = 0;
                    dynamics_was_moving = true;

                } else if (!is_moving && dynamics_was_moving) {
                    // Stopped - start timeout
                    if (dynamics_stopped_since == 0) {
                        dynamics_stopped_since = now;
                        ESP_LOGD(TAG, "[Auto] Vehicle stopped, starting %ds timeout", config.dynamics_stop_timeout_sec);
                    }
                    dynamics_was_moving = false;

                } else if (!is_moving && dynamics_stopped_since > 0) {
                    // Still stopped - check timeout
                    uint32_t stopped_duration_sec = (now - dynamics_stopped_since) / 1000;
                    if (stopped_duration_sec >= config.dynamics_stop_timeout_sec) {
                        // Timeout expired - auto-pause
                        if (!m_rt_logger->is_storage_paused()) {
                            m_rt_logger->pause_storage();
                            ESP_LOGI(TAG, "[Auto] Dynamics auto-stopped (stopped for %ds)", stopped_duration_sec);
                        }
                        dynamics_stopped_since = 0;
                    }
                }
            }

            // === Data Auto-Start/Stop (Engine-based) ===
            if (config.data_auto_enabled && obd.valid) {
                // Engine running if RPM > 0 or speed > 0
                bool engine_running = (obd.engine_rpm > 0 || obd.vehicle_speed > 0);

                if (engine_running && !data_was_running) {
                    // Engine started - auto-resume if paused
                    if (m_rt_logger->is_storage_paused()) {
                        m_rt_logger->resume_storage();
                        ESP_LOGI(TAG, "[Auto] Data auto-started (engine running: RPM=%.0f)", obd.engine_rpm);
                    }
                    data_stopped_since = 0;
                    data_was_running = true;

                } else if (!engine_running && data_was_running) {
                    // Engine stopped - start timeout
                    if (data_stopped_since == 0) {
                        data_stopped_since = now;
                        ESP_LOGD(TAG, "[Auto] Engine stopped, starting %ds timeout", config.data_stop_timeout_sec);
                    }
                    data_was_running = false;

                } else if (!engine_running && data_stopped_since > 0) {
                    // Engine still off - check timeout
                    uint32_t stopped_duration_sec = (now - data_stopped_since) / 1000;
                    if (stopped_duration_sec >= config.data_stop_timeout_sec) {
                        // Timeout expired - auto-pause
                        if (!m_rt_logger->is_storage_paused()) {
                            m_rt_logger->pause_storage();
                            ESP_LOGI(TAG, "[Auto] Data auto-stopped (engine off for %ds)", stopped_duration_sec);
                        }
                        data_stopped_since = 0;
                    }
                }
            }
        }

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
            ESP_LOGI(TAG, "[Power DEBUG] GPIO19=%d, BattV=%.2fV, gpio_usb=%d, voltage_usb=%d, final_usb=%d, shutdown=%d",
                         digitalRead(VBUS_DETECT_PIN), battery_voltage, gpio_usb, voltage_usb, usb_present, m_shutdown_pending);
            last_debug_time = now;
        }
        
        if (!m_shutdown_pending) {
            if (usb_present && !m_usb_powered) {
                // USB power restored
                ESP_LOGI(TAG, "[Power] USB power restored!");
                m_usb_powered = true;
                m_usb_loss_time = 0;
            } else if (!usb_present && m_usb_powered) {
                // USB power lost - start countdown
                ESP_LOGI(TAG, "[Power] USB power lost! Starting 60-second countdown...");
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
                ESP_LOGI(TAG, "[Power] USB restored - shutdown canceled!");
                m_shutdown_pending = false;
                m_usb_powered = true;
                m_usb_loss_time = 0;

                // Restore previous NeoPixel state (will be updated in next status update)
            } else {
                // Check if timeout expired
                uint32_t time_since_loss = now - m_usb_loss_time;
                if (time_since_loss >= USB_TIMEOUT_MS) {
                    // Timeout expired - initiate 2-stage sleep
                    ESP_LOGI(TAG, "[Power] Countdown complete - preparing for light sleep...");
                    m_shutdown_initiated = true;

                    // === Stage 1: Prepare for Light Sleep ===

                    // 1. Flush any pending flash writes
                    if (m_flash_storage != nullptr) {
                        ESP_LOGI(TAG, "[Power] Flushing pending flash writes...");
                        // Give flash writer time to finish current operation
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }

                    // 2. Disconnect BLE to save power
                    ESP_LOGI(TAG, "[Power] Disconnecting BLE...");
                    IcarBleDriver::disconnect();

                    // 3. Show sleep message on display
                    ST7789Display::show_shutdown_screen(0);  // Show "sleeping" state

                    // Note: WiFi stays on during light sleep - it will automatically reconnect
                    // Light sleep keeps WiFi/BT peripherals powered (~0.8mA vs 80mA active)

                    // 4. Configure wake sources
                    // Wake on USB power restore (GPIO19 goes HIGH)
                    esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_DETECT_PIN, 1);  // Wake on HIGH

                    // Wake after 5 minutes if USB not restored
                    esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_DURATION_US);

                    ESP_LOGI(TAG, "[Power] Entering light sleep for 5 minutes...");
                    ESP_LOGI(TAG, "[Power] Wake sources: USB restore (GPIO19) or 5-minute timer");

                    // Small delay to ensure logs are flushed
                    vTaskDelay(pdMS_TO_TICKS(100));

                    // === Enter Light Sleep ===
                    esp_light_sleep_start();

                    // === Woke Up from Light Sleep ===
                    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

                    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
                        // Woke up because USB was restored
                        ESP_LOGI(TAG, "[Power] ✓ Woke from light sleep - USB power restored!");
                        ESP_LOGI(TAG, "[Power] Resuming normal operation...");

                        // Cancel shutdown and resume
                        m_shutdown_pending = false;
                        m_shutdown_initiated = false;
                        m_usb_powered = true;
                        m_usb_loss_time = 0;

                        // WiFi automatically stays connected during light sleep

                        // Restart BLE scanning if needed
                        ESP_LOGI(TAG, "[Power] Restarting BLE scanning...");
                        IcarBleDriver::start_scan();

                        // Update NeoPixel state (resume normal operation)
                        NeoPixelStatus::setState(NeoPixelStatus::State::NO_GPS_FIX);

                    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
                        // Timer expired - USB not restored in 5 minutes
                        ESP_LOGI(TAG, "[Power] Light sleep timer expired (5 minutes)");
                        ESP_LOGI(TAG, "[Power] USB still not present - entering deep sleep...");

                        // === Stage 2: Deep Sleep ===

                        // Configure wake on USB restore only
                        esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_DETECT_PIN, 1);

                        ESP_LOGI(TAG, "[Power] Entering deep sleep (wake on USB restore only)");
                        ESP_LOGI(TAG, "[Power] Device will reboot when USB power restored");

                        // Small delay for logs
                        vTaskDelay(pdMS_TO_TICKS(100));

                        // Enter deep sleep (device will reboot on wake)
                        esp_deep_sleep_start();

                        // Never reaches here
                    } else {
                        // Unknown wake reason
                        ESP_LOGW(TAG, "[Power] Woke from light sleep - unknown reason: %d", wakeup_reason);
                        ESP_LOGI(TAG, "[Power] Resuming normal operation...");

                        m_shutdown_pending = false;
                        m_shutdown_initiated = false;
                    }

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
        // Update OBD at 10Hz (100ms) for fast-changing PIDs (RPM, throttle, speed)
        static uint32_t last_obd_update = 0;
        if (now - last_obd_update >= 100) { // 10 Hz on Core 0 for fast PID updates
            if (IcarBleDriver::update()) {
                // OBD data was updated, queue sample with accurate timestamp directly to flash
                if (m_flash_storage != nullptr && IcarBleDriver::is_connected()) {
                    obd_data_t obd = IcarBleDriver::get_data();
                    // Queue OBD sample with microsecond timestamp captured when data arrived
                    m_flash_storage->queue_obd_sample(obd);
                }
            }
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

        // TEMPORARILY DISABLED: Display updates causing 130KB allocation (65KB DRAM + 65KB PSRAM)
        // TODO: Fix GFXcanvas16 double allocation issue
        static uint32_t last_display_update = 0;
        if (false && m_rt_logger != nullptr && now - last_display_update >= 1000) {
            ESP_LOGI(TAG, "[DEBUG] Starting 1Hz display update...");

            DisplayMode current_mode = ST7789Display::get_display_mode();
            bool is_paused = m_rt_logger->is_storage_paused();

            if (current_mode == DisplayMode::MAIN_SCREEN) {
                ESP_LOGI(TAG, "[DEBUG] Getting sensor data...");
                // Get latest sensor data for display
                gps_data_t gps = m_rt_logger->get_last_gps();
                accel_data_t accel = m_rt_logger->get_last_accel();
                gyro_data_t gyro = m_rt_logger->get_last_gyro();
                battery_data_t battery = m_rt_logger->get_last_battery();
                uint32_t sample_count = m_rt_logger->get_sample_count();
                uint32_t uptime_sec = now / 1000;
                float sample_hz = sample_count > 0 && uptime_sec > 0 ? (float)sample_count / uptime_sec : 0.0f;
                if (!isfinite(sample_hz) || sample_hz < 0.0f) sample_hz = 0.0f;

                ESP_LOGI(TAG, "[DEBUG] Calling ST7789Display::update()...");
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
                ESP_LOGI(TAG, "[DEBUG] Display update returned successfully");

                if (DebugFlags::ENABLE_DISPLAY_TIMING) {
                    uint32_t display_elapsed = millis() - display_start;
                    if (display_elapsed > 10) {
                        ESP_LOGI(TAG, "[Display] Update took %ums", display_elapsed);
                    }
                }
            } else if (current_mode == DisplayMode::INFO_SCREEN) {
                ESP_LOGI(TAG, "[DEBUG] Showing info screen...");
                ST7789Display::show_info_screen("192.168.4.1", "OpenPonyLogger");
            }
            // DisplayMode::DARK - do nothing

            // Print 1Hz serial monitor update showing total samples, RTLogger Hz, and write count
            ESP_LOGI(TAG, "[Monitor] Samples: %u | RTLogger: %.1f Hz | Writes: %u",
                     sample_count, sample_hz, m_write_count);
            ESP_LOGI(TAG, "[DEBUG] 1Hz update complete");

            last_display_update = now;

            // Yield after display update to prevent watchdog starvation
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        // Memory and task monitoring every 5 seconds
        static uint32_t last_memory_report = 0;
        if (now - last_memory_report >= 5000) {
            ESP_LOGI(TAG, "[DEBUG] Starting 5s memory report...");

            // ========== MEMORY STATS ==========
            size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            size_t total_dram = heap_caps_get_total_size(MALLOC_CAP_8BIT);
            size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

            uint8_t dram_used_pct = (uint8_t)(((total_dram - free_dram) * 100) / total_dram);
            uint8_t psram_used_pct = (total_psram > 0) ? (uint8_t)(((total_psram - free_psram) * 100) / total_psram) : 0;

            // ========== TASK STATS ==========
            // Get task count as system activity indicator
            // Note: Per-core CPU stats require ESP-IDF trace facility (not available in Arduino framework by default)
            UBaseType_t task_count = uxTaskGetNumberOfTasks();

            // Print memory + task stats
            ESP_LOGI(TAG, "[System] DRAM: %u/%u KB (%u%%) | PSRAM: %u/%u KB (%u%%) | Tasks: %u",
                     (total_dram - free_dram) / 1024, total_dram / 1024, dram_used_pct,
                     (total_psram - free_psram) / 1024, total_psram / 1024, psram_used_pct,
                     task_count);

            last_memory_report = now;
            ESP_LOGI(TAG, "[DEBUG] 5s memory report complete");
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
                ESP_LOGI(TAG, "[10s] GPS:%u IMU:%u Gyro:%u OBD:%u | Paused:%d | Heap:%u",
                    gps_samples, accel_samples, gyro_samples, obd_samples, is_paused, ESP.getFreeHeap());
                last_sample_count_print = now;
            }
            
            // Full status report if enabled
            if (DebugFlags::ENABLE_STATUS_REPORT) {
                ESP_LOGI(TAG, "[StatusMonitor] STATUS REPORT #%u (loops=%u, broadcasts=%u, yields=%u)",
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
        ESP_LOGI(TAG, "[Power] Executing graceful shutdown sequence...");

        // Close current logging session
        if (m_rt_logger != nullptr) {
            ESP_LOGI(TAG, "[Power] Closing logging session...");
            // The RTLogger thread will write the final header when stopped
        }

        // Configure wake sources
        ESP_LOGI(TAG, "[Power] Configuring wake sources...");

        // Wake ONLY on GPIO19 (VBUS) rising edge - USB power restored
        // NOTE: ESP32-S3 can use EITHER ext0 OR ext1, not both!
        // Using ext0 for single GPIO wake on USB power restore
        esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_DETECT_PIN, 1);

        ESP_LOGI(TAG, "[Power] Wake source configured: USB power restore only (GPIO19)");

        // Turn off display
        ST7789Display::off();

        // Final message
        ESP_LOGI(TAG, "[Power] Entering deep sleep...");
        ESP_LOGI(TAG, "[Power] Will wake when USB power restored (GPIO19)");

        delay(100);

        // Enter deep sleep
        esp_deep_sleep_start();
    }
}
