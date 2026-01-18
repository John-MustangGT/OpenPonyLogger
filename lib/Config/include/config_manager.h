#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <cstdint>
#include <Arduino.h>
#include <map>

/**
 * @brief Network Configuration
 * WiFi AP settings for the device
 */
struct network_config_t {
    char ssid[32];          // WiFi AP SSID (max 31 chars + null)
    char password[64];      // WiFi AP password (empty = open network)
    uint8_t ip[4];          // IP address (e.g., 192.168.4.1)
    uint8_t subnet[4];      // Subnet mask (e.g., 255.255.255.0)
    
    // Default constructor with sensible defaults
    network_config_t() {
        strncpy(ssid, "PonyLogger", sizeof(ssid));
        password[0] = '\0';  // Open network by default
        ip[0] = 192; ip[1] = 168; ip[2] = 4; ip[3] = 1;
        subnet[0] = 255; subnet[1] = 255; subnet[2] = 255; subnet[3] = 0;
    }
};

/**
 * @brief OBD-II PID Configuration
 * Each PID can have its own update rate
 */
struct pid_config_t {
    uint8_t pid;           // PID identifier (e.g., 0x0C for RPM)
    uint16_t rate_hz;      // Update rate in Hz
    bool enabled;          // Whether this PID is actively polled
    const char* name;      // Human-readable name
    
    pid_config_t() : pid(0), rate_hz(1), enabled(false), name("") {}
    pid_config_t(uint8_t p, uint16_t r, bool e, const char* n) 
        : pid(p), rate_hz(r), enabled(e), name(n) {}
};

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
    uint16_t obd_hz;        // OBD global max update rate
    
    // OBD-II BLE configuration
    bool obd_ble_enabled;   // Enable/disable BLE scanning for OBD-II devices
    
    // Network configuration
    network_config_t network;
    
    // Individual PID configurations
    std::map<uint8_t, pid_config_t> pid_configs;
    
    // Default constructor with 10Hz across the board
    logging_config_t() 
        : main_loop_hz(10), gps_hz(10), imu_hz(10), obd_hz(10), obd_ble_enabled(true) {
        // Initialize core PIDs (enabled by default at 10Hz)
        pid_configs[0x0C] = pid_config_t(0x0C, 10, true, "Engine RPM");
        pid_configs[0x0D] = pid_config_t(0x0D, 10, true, "Vehicle Speed");
        pid_configs[0x05] = pid_config_t(0x05, 1, true, "Coolant Temp");
        pid_configs[0x0F] = pid_config_t(0x0F, 1, true, "Intake Air Temp");
        pid_configs[0x11] = pid_config_t(0x11, 10, true, "Throttle Position");
        pid_configs[0x10] = pid_config_t(0x10, 5, true, "MAF Air Flow");
        pid_configs[0x1F] = pid_config_t(0x1F, 1, true, "Run Time");
        pid_configs[0x2F] = pid_config_t(0x2F, 1, true, "Fuel Tank Level");
        pid_configs[0x33] = pid_config_t(0x33, 1, true, "Barometric Pressure");
        pid_configs[0x21] = pid_config_t(0x21, 1, true, "Distance w/ MIL On");
        
        // Initialize mandatory PIDs (enabled by default at lower rates)
        pid_configs[0x03] = pid_config_t(0x03, 1, true, "Fuel System Status");
        pid_configs[0x04] = pid_config_t(0x04, 5, true, "Engine Load");
    }
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
    static const char* KEY_OBD_BLE_ENABLED;
    static const char* KEY_NET_SSID;
    static const char* KEY_NET_PASSWORD;
    static const char* KEY_NET_IP;
    static const char* KEY_NET_SUBNET;
    static const char* KEY_CHECKSUM;
    
    /**
     * @brief Calculate CRC32 checksum of configuration
     * @param config Configuration to checksum
     * @return CRC32 checksum value
     */
    static uint32_t calculate_checksum(const logging_config_t& config);
};

#endif // CONFIG_MANAGER_H
