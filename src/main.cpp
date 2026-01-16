#include <Arduino.h>
#include "sensor_hal.h"
#include "pa1010d_driver.h"
#include "icm20948_driver.h"
#include "icm20948_gyro_wrapper.h"
#include "icm20948_compass_wrapper.h"
#include "max17048_driver.h"
#include "rt_logger_thread.h"
#include "storage_reporter.h"

// Hardware configuration
#define GPS_TX_PIN          17
#define GPS_RX_PIN          16
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define GPS_I2C_ADDR        0x10
#define IMU_I2C_ADDR        0x69
#define BATTERY_I2C_ADDR    0x36

// GPS Communication Mode Selection
// Set to true for I2C (default), false for UART
#define GPS_USE_I2C         true

// Global instances
SensorManager sensor_manager;
RTLoggerThread* rt_logger = nullptr;
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
}

/**
 * @brief Initialize sensors
 */
bool init_sensors() {
    // Initialize I2C for IMU and GPS (if using I2C)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);  // 400kHz I2C clock
    
    // Create and initialize GPS driver
    if (GPS_USE_I2C) {
        // I2C mode (default)
        gps_driver = new PA1010DDriver(Wire, GPS_I2C_ADDR);
        if (!gps_driver->init()) {
            reporter.print_debug("ERROR: Failed to initialize GPS (I2C mode)");
            return false;
        }
        reporter.print_debug("GPS initialized (I2C mode)");
    } else {
        // UART mode
        gps_driver = new PA1010DDriver(Serial1, GPS_TX_PIN, GPS_RX_PIN, 9600);
        if (!gps_driver->init()) {
            reporter.print_debug("ERROR: Failed to initialize GPS (UART mode)");
            return false;
        }
        reporter.print_debug("GPS initialized (UART mode)");
    }
    
    // Create and initialize IMU driver
    imu_driver = new ICM20948Driver(Wire, IMU_I2C_ADDR);
    if (!imu_driver->init()) {
        reporter.print_debug("ERROR: Failed to initialize IMU");
        return false;
    }
    reporter.print_debug("IMU initialized");
    
    // Create and initialize battery monitor driver
    battery_driver = new MAX17048Driver(Wire, BATTERY_I2C_ADDR);
    if (!battery_driver->init()) {
        reporter.print_debug("ERROR: Failed to initialize battery monitor");
        return false;
    }
    reporter.print_debug("Battery monitor initialized");
    
    // Create wrappers for gyroscope and compass that delegate to the IMU driver
    gyro_wrapper = new ICM20948GyroWrapper(imu_driver);
    compass_wrapper = new ICM20948CompassWrapper(imu_driver);
    
    // Initialize sensor manager with the drivers
    if (!sensor_manager.init(gps_driver, imu_driver, gyro_wrapper, compass_wrapper, battery_driver)) {
        reporter.print_debug("ERROR: Failed to initialize sensor manager");
        return false;
    }
    reporter.print_debug("Sensor manager initialized");
    
    return true;
}

/**
 * @brief Setup function (Arduino standard)
 */
void setup() {
    // Initialize serial communication for debugging
    reporter.init(115200);
    delay(500);
    
    reporter.print_debug("\n=== OpenPonyLogger RT Logger Starting ===");
    reporter.print_debug("Initializing sensors...");
    
    // Initialize sensors
    if (!init_sensors()) {
        reporter.print_debug("ERROR: Sensor initialization failed");
        while (1) {
            delay(1000);
        }
    }
    
    // Create and start the RT logger thread
    rt_logger = new RTLoggerThread(&sensor_manager, 100);  // 100ms update rate
    
    // Register the storage write callback
    rt_logger->set_storage_write_callback(on_storage_write);
    
    if (!rt_logger->start()) {
        reporter.print_debug("ERROR: Failed to start RT logger thread");
        while (1) {
            delay(1000);
        }
    }
    
    reporter.print_debug("RT Logger thread started");
    reporter.print_debug("=== System Ready ===\n");
}

/**
 * @brief Main loop (Arduino standard)
 */
void loop() {
    // Main loop can handle other tasks or be used for command processing
    // For now, we demonstrate triggering storage writes periodically
    
    static uint32_t last_write_time = 0;
    uint32_t now = millis();
    
    // Trigger storage write every 5 seconds
    if (now - last_write_time >= 5000) {
        rt_logger->trigger_storage_write();
        last_write_time = now;
    }
    
    delay(100);
}
