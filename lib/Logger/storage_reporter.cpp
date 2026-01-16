#include "storage_reporter.h"
#include <Arduino.h>
#include <cstdarg>
#include <esp_log.h>

static const char* TAG = "STORAGE";

StorageReporter::StorageReporter() {
}

StorageReporter::~StorageReporter() {
}

void StorageReporter::init(unsigned long baud_rate) {
    Serial.begin(baud_rate);
    // Extra wait for USB JTAG to be ready on ESP32-S3
    delay(500);
    
    // Additional delay to ensure USB is ready
    uint32_t start = millis();
    while (!Serial && (millis() - start) < 1000) {
        delay(50);
    }
}

void StorageReporter::report_storage_write(const gps_data_t& gps, const accel_data_t& accel,
                                           const gyro_data_t& gyro, const compass_data_t& compass,
                                           const battery_data_t& battery) {
    ESP_LOGI(TAG, "=== STORAGE WRITE EVENT ===");
    ESP_LOGI(TAG, "Timestamp: %04d-%02d-%02d %02d:%02d:%02d", 
             gps.year, gps.month, gps.day, gps.hour, gps.minute, gps.second);
    
    ESP_LOGI(TAG, "--- GPS Data ---");
    print_gps_data(gps);
    
    ESP_LOGI(TAG, "--- Accelerometer Data ---");
    print_accel_data(accel);
    
    ESP_LOGI(TAG, "--- Gyroscope Data ---");
    print_gyro_data(gyro);
    
    ESP_LOGI(TAG, "--- Compass Data ---");
    print_compass_data(compass);
    
    ESP_LOGI(TAG, "--- Battery Data ---");
    print_battery_data(battery);
    
    ESP_LOGI(TAG, "===========================");
}

void StorageReporter::print_gps_data(const gps_data_t& gps) {
    if (gps.valid) {
        ESP_LOGI(TAG, "  Latitude:  %.6f", gps.latitude);
        ESP_LOGI(TAG, "  Longitude: %.6f", gps.longitude);
        ESP_LOGI(TAG, "  Altitude:  %.2f m", gps.altitude);
        ESP_LOGI(TAG, "  Speed:     %.2f kts", gps.speed);
        ESP_LOGI(TAG, "  Satellites: %d", gps.satellites);
        ESP_LOGI(TAG, "  Status: VALID");
    } else {
        ESP_LOGW(TAG, "  Status: INVALID - No GPS fix");
    }
}

void StorageReporter::print_accel_data(const accel_data_t& accel) {
    ESP_LOGI(TAG, "  X: %.4f g", accel.x);
    ESP_LOGI(TAG, "  Y: %.4f g", accel.y);
    ESP_LOGI(TAG, "  Z: %.4f g", accel.z);
    ESP_LOGI(TAG, "  Magnitude: %.4f g", 
             sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z));
}

void StorageReporter::print_gyro_data(const gyro_data_t& gyro) {
    ESP_LOGI(TAG, "  X: %.2f dps", gyro.x);
    ESP_LOGI(TAG, "  Y: %.2f dps", gyro.y);
    ESP_LOGI(TAG, "  Z: %.2f dps", gyro.z);
    ESP_LOGI(TAG, "  Magnitude: %.2f dps", 
             sqrtf(gyro.x * gyro.x + gyro.y * gyro.y + gyro.z * gyro.z));
}

void StorageReporter::print_compass_data(const compass_data_t& compass) {
    ESP_LOGI(TAG, "  X: %.2f uT", compass.x);
    ESP_LOGI(TAG, "  Y: %.2f uT", compass.y);
    ESP_LOGI(TAG, "  Z: %.2f uT", compass.z);
    ESP_LOGI(TAG, "  Magnitude: %.2f uT", 
             sqrtf(compass.x * compass.x + compass.y * compass.y + compass.z * compass.z));
}

void StorageReporter::print_battery_data(const battery_data_t& battery) {
    if (battery.valid) {
        ESP_LOGI(TAG, "  Voltage:     %.3f V", battery.voltage);
        ESP_LOGI(TAG, "  State of Charge: %.2f %%", battery.state_of_charge);
        ESP_LOGI(TAG, "  Current:     %.2f mA", battery.current);
        ESP_LOGI(TAG, "  Temperature: %.2f °C", battery.temperature / 100.0f);
        ESP_LOGI(TAG, "  Status: VALID");
    } else {
        ESP_LOGW(TAG, "  Status: INVALID - Battery monitor not responding");
    }
}

void StorageReporter::print_debug(const char* message) {
    ESP_LOGI(TAG, "%s", message);
}

void StorageReporter::printf_debug(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    ESP_LOGI(TAG, "%s", buffer);
}
