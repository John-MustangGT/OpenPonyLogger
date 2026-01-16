#include "storage_reporter.h"
#include <Arduino.h>
#include <cstdarg>

StorageReporter::StorageReporter() {
}

StorageReporter::~StorageReporter() {
}

void StorageReporter::init(unsigned long baud_rate) {
    Serial.begin(baud_rate);
    // Wait for serial connection
    delay(100);
}

void StorageReporter::report_storage_write(const gps_data_t& gps, const accel_data_t& accel,
                                           const gyro_data_t& gyro, const compass_data_t& compass) {
    Serial.println("\n=== STORAGE WRITE EVENT ===");
    Serial.print("Timestamp: ");
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", 
                  gps.year, gps.month, gps.day, 
                  gps.hour, gps.minute, gps.second);
    
    Serial.println("\n--- GPS Data ---");
    print_gps_data(gps);
    
    Serial.println("\n--- Accelerometer Data ---");
    print_accel_data(accel);
    
    Serial.println("\n--- Gyroscope Data ---");
    print_gyro_data(gyro);
    
    Serial.println("\n--- Compass Data ---");
    print_compass_data(compass);
    
    Serial.println("===========================\n");
}

void StorageReporter::print_gps_data(const gps_data_t& gps) {
    if (gps.valid) {
        Serial.printf("  Latitude:  %.6f\n", gps.latitude);
        Serial.printf("  Longitude: %.6f\n", gps.longitude);
        Serial.printf("  Altitude:  %.2f m\n", gps.altitude);
        Serial.printf("  Speed:     %.2f kts\n", gps.speed);
        Serial.printf("  Satellites: %d\n", gps.satellites);
        Serial.println("  Status: VALID");
    } else {
        Serial.println("  Status: INVALID - No GPS fix");
    }
}

void StorageReporter::print_accel_data(const accel_data_t& accel) {
    Serial.printf("  X: %.4f g\n", accel.x);
    Serial.printf("  Y: %.4f g\n", accel.y);
    Serial.printf("  Z: %.4f g\n", accel.z);
    Serial.printf("  Magnitude: %.4f g\n", 
                  sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z));
}

void StorageReporter::print_gyro_data(const gyro_data_t& gyro) {
    Serial.printf("  X: %.2f dps\n", gyro.x);
    Serial.printf("  Y: %.2f dps\n", gyro.y);
    Serial.printf("  Z: %.2f dps\n", gyro.z);
    Serial.printf("  Magnitude: %.2f dps\n", 
                  sqrtf(gyro.x * gyro.x + gyro.y * gyro.y + gyro.z * gyro.z));
}

void StorageReporter::print_compass_data(const compass_data_t& compass) {
    Serial.printf("  X: %.2f uT\n", compass.x);
    Serial.printf("  Y: %.2f uT\n", compass.y);
    Serial.printf("  Z: %.2f uT\n", compass.z);
    Serial.printf("  Magnitude: %.2f uT\n", 
                  sqrtf(compass.x * compass.x + compass.y * compass.y + compass.z * compass.z));
}

void StorageReporter::print_debug(const char* message) {
    Serial.println(message);
}

void StorageReporter::printf_debug(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.println(buffer);
}
