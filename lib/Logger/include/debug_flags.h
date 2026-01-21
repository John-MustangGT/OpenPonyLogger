#ifndef DEBUG_FLAGS_H
#define DEBUG_FLAGS_H

/**
 * @brief Debug output control flags
 * Set to false to disable verbose debug logging for specific subsystems
 */
namespace DebugFlags {
    // Core system logging
    constexpr bool ENABLE_STATUS_REPORT = false;   // Periodic sensor status boxes (disabled - too verbose)
    constexpr bool ENABLE_SAMPLE_COUNTS = true;    // 1Hz sensor sample rate updates (main logging)
    
    // Sensor subsystems (verbose logging)
    constexpr bool ENABLE_GPS_DEBUG = false;       // GPS I2C read attempts, sentence parsing
    constexpr bool ENABLE_IMU_DEBUG = false;       // IMU register reads, calibration
    constexpr bool ENABLE_BATTERY_DEBUG = false;   // MAX17048 register dumps
    constexpr bool ENABLE_OBD_DEBUG = false;       // BLE OBD connection, data rx/tx - DISABLED to reduce crashes
    
    // WiFi/WebSocket
    constexpr bool ENABLE_WEBSOCKET_DEBUG = false; // WebSocket broadcast details
    
    // Power management
    constexpr bool ENABLE_POWER_DEBUG = false;     // USB detect, shutdown countdown
    
    // Display updates
    constexpr bool ENABLE_DISPLAY_TIMING = false;  // Display update duration warnings
}

#endif // DEBUG_FLAGS_H
