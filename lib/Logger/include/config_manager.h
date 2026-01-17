#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <cstdint>
#include <Arduino.h>

/**
 * @brief Logging configuration
 * Stores configurable logging rates for different subsystems
 * Rates are in Hz (samples per second)
 */
struct logging_config_t {
    // Main loop rate (5, 10, 20, 50, 100 Hz)
    uint16_t main_loop_hz;
    
    // Individual sensor rates (can be lower than or equal to main loop rate)
    uint16_t gps_hz;        // GPS update rate
    uint16_t imu_hz;        // IMU (accel/gyro/compass) update rate
    uint16_t obd_hz;        // OBD/PID update rate
    
    // Default constructor with 10Hz across the board
    logging_config_t() 
        : main_loop_hz(10), gps_hz(10), imu_hz(10), obd_hz(10) {}
};

/**
 * @brief Configuration Manager for OpenPonyLogger
 * Handles loading/saving configuration to NVS (Non-Volatile Storage)
 * Provides default values if no configuration exists
 */
class ConfigManager {
public:
    /**
     * @brief Initialize NVS and load configuration
     * @return true if successful
     */
    static bool init();
    
    /**
     * @brief Load configuration from NVS
     * Uses defaults if no saved configuration exists
     * @return Configuration structure
     */
    static logging_config_t load();
    
    /**
     * @brief Save configuration to NVS
     * @param config Configuration to save
     * @return true if successful
     */
    static bool save(const logging_config_t& config);
    
    /**
     * @brief Get current active configuration
     * @return Current configuration
     */
    static logging_config_t get_current();
    
    /**
     * @brief Update current configuration (saves to NVS)
     * @param config New configuration
     * @return true if successful
     */
    static bool update(const logging_config_t& config);
    
    /**
     * @brief Validate configuration values
     * Ensures rates are within acceptable ranges
     * @param config Configuration to validate
     * @return true if valid
     */
    static bool validate(const logging_config_t& config);
    
    /**
     * @brief Reset to default configuration
     * @return true if successful
     */
    static bool reset_to_defaults();

private:
    static bool m_initialized;
    static logging_config_t m_current_config;
    
    // NVS keys
    static const char* NVS_NAMESPACE;
    static const char* KEY_MAIN_LOOP_HZ;
    static const char* KEY_GPS_HZ;
    static const char* KEY_IMU_HZ;
    static const char* KEY_OBD_HZ;
};

#endif // CONFIG_MANAGER_H
