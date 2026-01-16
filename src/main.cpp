#include <Arduino.h>
#include "sensor_hal.h"
#include "pa1010d_driver.h"
#include "icm20948_driver.h"
#include "icm20948_gyro_wrapper.h"
#include "icm20948_compass_wrapper.h"
#include "max17048_driver.h"
#include "rt_logger_thread.h"
#include "storage_reporter.h"
#include "status_monitor.h"
#include "st7789_display.h"

// Hardware configuration
#define GPS_TX_PIN          17
#define GPS_RX_PIN          16
#define I2C_SDA_PIN         3   // ESP32-S3 Feather TFT STEMMA QT SDA
#define I2C_SCL_PIN         4   // ESP32-S3 Feather TFT STEMMA QT SCL
#define I2C_PWR_PIN         7   // Power enable for I2C/STEMMA (hold HIGH)
#define GPS_I2C_ADDR        0x10
#define IMU_I2C_ADDR        0x69
#define BATTERY_I2C_ADDR    0x36

// GPS Communication Mode Selection
// Set to true for I2C (default), false for UART
#define GPS_USE_I2C         true

// Logging tag
static const char* TAG = "MAIN";

// Global instances
SensorManager sensor_manager;
RTLoggerThread* rt_logger = nullptr;
StatusMonitor* status_monitor = nullptr;
StorageReporter reporter;

// PA1010D GPS driver instance
PA1010DDriver* gps_driver = nullptr;

// ICM20948 IMU driver instance
ICM20948Driver* imu_driver = nullptr;

// ICM20948 IMU wrappers for gyro and compass
ICM20948GyroWrapper* gyro_wrapper = nullptr;
ICM20948CompassWrapper* compass_wrapper = nullptr;

// MAX17048 Battery monitor driver instance
MAX17048Driver* battery_driver = nullptr;

/**
 * @brief Callback function for storage write events
 */
void on_storage_write(const gps_data_t& gps, const accel_data_t& accel,
                      const gyro_data_t& gyro, const compass_data_t& compass,
                      const battery_data_t& battery) {
    reporter.report_storage_write(gps, accel, gyro, compass, battery);
    
    // Increment write counter in status monitor
    if (status_monitor != nullptr) {
        status_monitor->increment_write_count();
    }
}

/**
 * @brief Initialize sensors
 */
bool init_sensors() {
    reporter.printf_debug("  → Powering I2C bus (GPIO%d)...", I2C_PWR_PIN);
    
    // Enable I2C power on ESP32-S3 Feather TFT
    pinMode(I2C_PWR_PIN, OUTPUT);
    digitalWrite(I2C_PWR_PIN, HIGH);
    delay(50);  // Allow power to stabilize
    reporter.print_debug("  ✓ I2C power enabled");
    
    reporter.printf_debug("  → Initializing I2C (SDA: GPIO%d, SCL: GPIO%d)...", I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Initialize I2C for IMU and GPS (if using I2C)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);  // 400kHz I2C clock
    delay(100);  // Give I2C time to stabilize
    reporter.print_debug("  ✓ I2C ready at 400kHz");
    
    // Create and initialize GPS driver
    reporter.printf_debug("  → Initializing GPS (PA1010D @ 0x%02X, %s mode)...",
                        GPS_I2C_ADDR, GPS_USE_I2C ? "I2C" : "UART");
    
    if (GPS_USE_I2C) {
        // I2C mode (default)
        gps_driver = new PA1010DDriver(Wire, GPS_I2C_ADDR);
        if (!gps_driver->init()) {
            reporter.print_debug("  ✗ ERROR: Failed to initialize GPS (I2C mode)");
            return false;
        }
        reporter.print_debug("  ✓ GPS initialized (I2C)");
    } else {
        // UART mode
        gps_driver = new PA1010DDriver(Serial1, GPS_TX_PIN, GPS_RX_PIN, 9600);
        if (!gps_driver->init()) {
            reporter.print_debug("  ✗ ERROR: Failed to initialize GPS (UART mode)");
            return false;
        }
        reporter.print_debug("  ✓ GPS initialized (UART)");
    }
    
    // Create and initialize IMU driver
    reporter.printf_debug("  → Initializing IMU (ICM20948 @ 0x%02X)...", IMU_I2C_ADDR);
    imu_driver = new ICM20948Driver(Wire, IMU_I2C_ADDR);
    if (!imu_driver->init()) {
        reporter.print_debug("  ✗ ERROR: Failed to initialize IMU");
        return false;
    }
    reporter.print_debug("  ✓ IMU initialized (Accel + Gyro + Compass)");
    
    // Create and initialize battery monitor driver
    reporter.printf_debug("  → Initializing Battery Monitor (MAX17048 @ 0x%02X)...", BATTERY_I2C_ADDR);
    battery_driver = new MAX17048Driver(Wire, BATTERY_I2C_ADDR);
    if (!battery_driver->init()) {
        reporter.print_debug("  ✗ ERROR: Failed to initialize battery monitor");
        return false;
    }
    reporter.print_debug("  ✓ Battery monitor initialized");
    
    // Create wrappers for gyroscope and compass that delegate to the IMU driver
    reporter.print_debug("  → Creating sensor HAL wrappers...");
    gyro_wrapper = new ICM20948GyroWrapper(imu_driver);
    compass_wrapper = new ICM20948CompassWrapper(imu_driver);
    reporter.print_debug("  ✓ Wrappers created");
    
    // Initialize sensor manager with the drivers
    reporter.print_debug("  → Initializing Sensor Manager...");
    if (!sensor_manager.init(gps_driver, imu_driver, gyro_wrapper, compass_wrapper, battery_driver)) {
        reporter.print_debug("  ✗ ERROR: Failed to initialize sensor manager");
        return false;
    }
    reporter.print_debug("  ✓ Sensor Manager ready");
    
    return true;
}

/**
 * @brief Setup function (Arduino standard)
 */
void setup() {
    // Very early serial output before anything else
    // ESP32-S3 uses USB CDC for Serial
    Serial.begin(115200);
    delay(2000);  // Longer delay for USB CDC to stabilize
    
    // Send simple test without any formatting
    Serial.write("BOOT\n");
    Serial.write("BOOT\n");
    Serial.write("BOOT\n");
    Serial.flush();
    delay(500);
    
    Serial.println("\n\n\n=== BOOT START ===");
    Serial.flush();
    
    // Initialize serial communication for debugging
    // ESP32-S3 needs extra time to establish USB JTAG connection
    reporter.init(115200);
    
    // Wait longer for USB JTAG to fully establish
    Serial.println("Waiting for USB JTAG...");
    for (int i = 0; i < 20; i++) {
        delay(100);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("After dots - about to print header");
    Serial.flush();
    
    Serial.println("╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║        OpenPonyLogger - Real-Time Data Logger              ║");
    Serial.println("║              ESP32-S3 Feather TFT                          ║");
    Serial.println("╚═══════════════════════════════════════════════════════════╝");
    Serial.flush();
    
    Serial.println("▶ Initializing hardware...");
    Serial.flush();
    
    // Initialize display
    Serial.println("▶ Initializing ST7789 Display...");
    Serial.flush();
    if (!ST7789Display::init()) {
        Serial.println("⚠ WARNING: Display initialization failed, continuing with serial output only");
        Serial.flush();
    } else {
        Serial.println("✓ Display initialized");
        Serial.flush();
    }
    
    // Initialize NeoPixel status indicator
    Serial.println("▶ Initializing NeoPixel Status Indicator...");
    Serial.flush();
    if (!NeoPixelStatus::init()) {
        Serial.println("⚠ WARNING: NeoPixel initialization failed");
        Serial.flush();
    } else {
        Serial.println("✓ NeoPixel initialized (Booting - Red)");
        Serial.flush();
    }
    
    // Initialize sensors
    Serial.println("About to call init_sensors()");
    Serial.flush();
    if (!init_sensors()) {
        Serial.println("✗ FATAL ERROR: Sensor initialization failed!");
        Serial.println("System halted.");
        Serial.flush();
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("init_sensors() completed successfully");
    Serial.flush();
    
    Serial.println("▶ Starting Real-Time Logger Thread (Core 1)...");
    Serial.flush();
    
    // Create and start the RT logger thread on core 1
    Serial.println("  → Creating RTLoggerThread object...");
    Serial.flush();
    rt_logger = new RTLoggerThread(&sensor_manager, 100);  // 100ms update rate
    Serial.println("  ✓ RTLoggerThread object created");
    Serial.flush();
    
    // Register the storage write callback
    Serial.println("  → Registering storage callback...");
    Serial.flush();
    rt_logger->set_storage_write_callback(on_storage_write);
    Serial.println("  ✓ Callback registered");
    Serial.flush();
    
    Serial.println("  → Calling start() on RT logger...");
    Serial.flush();
    if (!rt_logger->start()) {
        Serial.println("✗ ERROR: Failed to start RT logger thread");
        Serial.println("System halted.");
        Serial.flush();
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("✓ RT Logger thread started");
    Serial.flush();
    
    Serial.println("▶ Starting Status Monitor Thread (Core 0)...");
    Serial.flush();
    
    // Create and start status monitor on core 0
    Serial.println("  → Creating StatusMonitor object...");
    Serial.flush();
    status_monitor = new StatusMonitor(rt_logger, 1000);  // Report every 1 second for debugging
    Serial.println("  ✓ StatusMonitor object created");
    Serial.flush();
    
    Serial.println("  → Calling start() on status monitor...");
    Serial.flush();
    if (!status_monitor->start()) {
        Serial.println("✗ ERROR: Failed to start status monitor thread");
        Serial.println("System halted.");
        Serial.flush();
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("✓ Status monitor started");
    Serial.flush();
    
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║              ✓ SYSTEM READY - LOGGING ACTIVE              ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════╝");
}

/**
 * @brief Main loop (Arduino standard)
 */
void loop() {
    // Main loop handles other tasks or command processing
    // Status monitor runs on core 0, RT logger on core 1
    // Storage writes triggered every 5 seconds
    
    static uint32_t last_write_time = 0;
    uint32_t now = millis();
    
    // Trigger storage write every 5 seconds
    if (now - last_write_time >= 5000) {
        rt_logger->trigger_storage_write();
        last_write_time = now;
    }
    
    delay(100);
}
