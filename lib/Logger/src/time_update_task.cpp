#include "time_update_task.h"
#include "debug_flags.h"
#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <sys/time.h>

TimeUpdateTask::TimeUpdateTask(RTCManager& rtc_manager)
    : m_rtc_manager(rtc_manager), m_task_handle(nullptr), m_time_queue(nullptr),
      m_rtc_update_interval_sec(60),            // Update ESP32 RTC every 60s
      m_pcf8523_update_interval_sec(300),       // Write PCF8523 every 5 minutes
      m_nvs_update_interval_sec(300),           // Commit NVS every 5 minutes
      m_last_rtc_update(0),
      m_last_pcf8523_update(0),
      m_last_nvs_update(0) {
}

TimeUpdateTask::~TimeUpdateTask() {
    stop();
}

bool TimeUpdateTask::start() {
    // Create queue for GPS time notifications
    m_time_queue = xQueueCreate(QUEUE_LENGTH, sizeof(time_t));
    if (m_time_queue == nullptr) {
        Serial.println("[TimeUpdate] Failed to create queue");
        return false;
    }
    
    // Create low-priority task (priority 1 = very low)
    // Stack increased: 2048 → 8192 → 16384 bytes
    // Large stack needed for NVS operations + debug builds with -O0
    BaseType_t result = xTaskCreatePinnedToCore(
        task_wrapper,
        "TimeUpdate",
        16384,  // Stack size: 16KB for safety margin
        this,
        1,  // Priority (0-25, lower = lower priority)
        &m_task_handle,
        0  // Core 0
    );
    
    if (result != pdPASS) {
        Serial.println("[TimeUpdate] Failed to create task");
        vQueueDelete(m_time_queue);
        m_time_queue = nullptr;
        return false;
    }
    
    Serial.println("[TimeUpdate] Task started (priority 1, Core 0)");
    return true;
}

void TimeUpdateTask::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_time_queue != nullptr) {
        vQueueDelete(m_time_queue);
        m_time_queue = nullptr;
    }
}

void TimeUpdateTask::notify_gps_time_available(time_t unix_time) {
    if (m_time_queue == nullptr) {
        return;
    }
    
    // Non-blocking send to queue (from interrupt/high-priority context)
    xQueueSendFromISR(m_time_queue, &unix_time, nullptr);
}

void TimeUpdateTask::task_wrapper(void* param) {
    auto* self = static_cast<TimeUpdateTask*>(param);
    self->run();
}

void TimeUpdateTask::run() {
    time_t gps_time = 0;
    
    Serial.println("[TimeUpdate] Task running, waiting for GPS time...");
    
    while (true) {
        // Wait for GPS time notification (blocking, low priority)
        if (xQueueReceive(m_time_queue, &gps_time, portMAX_DELAY) == pdTRUE) {
            if (gps_time > 0) {
                if (DebugFlags::ENABLE_GPS_DEBUG) {
                    Serial.printf("[TimeUpdate] Received GPS time: %ld\n", gps_time);
                }
                
                // Update ESP32-S3 internal RTC (rate-limited)
                if (m_last_rtc_update == 0 || (gps_time - m_last_rtc_update) >= m_rtc_update_interval_sec) {
                    timeval tv = {gps_time, 0};
                    settimeofday(&tv, nullptr);
                    m_last_rtc_update = gps_time;
                    if (DebugFlags::ENABLE_GPS_DEBUG) {
                        Serial.printf("[TimeUpdate] ESP32-S3 RTC updated to: %ld\n", gps_time);
                    }
                }
                
                // Update PCF8523 if available (rate-limited)
                if (m_rtc_manager.is_available() &&
                    (m_last_pcf8523_update == 0 || (gps_time - m_last_pcf8523_update) >= m_pcf8523_update_interval_sec)) {
                    if (m_rtc_manager.write_time(gps_time)) {
                        m_last_pcf8523_update = gps_time;
                        Serial.println("[TimeUpdate] PCF8523 RTC updated");
                    } else {
                        Serial.println("[TimeUpdate] Failed to update PCF8523");
                    }
                }
                
                // Store in NVS for boot recovery (rate-limited)
                if (m_last_nvs_update == 0 || (gps_time - m_last_nvs_update) >= m_nvs_update_interval_sec) {
                    nvs_handle_t nvs_handle;
                    esp_err_t err = nvs_open("system", NVS_READWRITE, &nvs_handle);
                    if (err == ESP_OK) {
                        err = nvs_set_u32(nvs_handle, "gps_time", (uint32_t)gps_time);
                        if (err == ESP_OK) {
                            nvs_commit(nvs_handle);
                            m_last_nvs_update = gps_time;
                            Serial.printf("[TimeUpdate] NVS 'gps_time' updated: %u\n",
                                          (unsigned)(uint32_t)gps_time);
                        } else {
                            Serial.printf("[TimeUpdate] Failed to write NVS (error: %d)\n", err);
                        }
                        nvs_close(nvs_handle);
                    } else {
                        Serial.printf("[TimeUpdate] Failed to open NVS (error: %d)\n", err);
                    }
                }
            }
        }
    }
}
