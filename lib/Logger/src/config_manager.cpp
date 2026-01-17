#include "config_manager.h"
#include <Preferences.h>

// Static member initialization
bool ConfigManager::m_initialized = false;
logging_config_t ConfigManager::m_current_config;

// NVS keys
const char* ConfigManager::NVS_NAMESPACE = "ponylogger";
const char* ConfigManager::KEY_MAIN_LOOP_HZ = "main_loop_hz";
const char* ConfigManager::KEY_GPS_HZ = "gps_hz";
const char* ConfigManager::KEY_IMU_HZ = "imu_hz";
const char* ConfigManager::KEY_OBD_HZ = "obd_hz";

bool ConfigManager::init() {
    if (m_initialized) {
        return true;
    }
    
    Serial.println("[Config] Initializing configuration manager...");
    
    // Load configuration from NVS
    m_current_config = load();
    
    // Validate loaded configuration
    if (!validate(m_current_config)) {
        Serial.println("[Config] WARNING: Invalid configuration loaded, using defaults");
        m_current_config = logging_config_t();  // Reset to defaults
        save(m_current_config);  // Save defaults
    }
    
    Serial.printf("[Config] ✓ Configuration loaded - Main: %dHz, GPS: %dHz, IMU: %dHz, OBD: %dHz\n",
                  m_current_config.main_loop_hz,
                  m_current_config.gps_hz,
                  m_current_config.imu_hz,
                  m_current_config.obd_hz);
    
    m_initialized = true;
    return true;
}

logging_config_t ConfigManager::load() {
    Preferences prefs;
    logging_config_t config;
    
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // true = read-only
        Serial.println("[Config] No saved configuration found, using defaults");
        return config;  // Return defaults
    }
    
    // Load each value, using defaults if not found
    config.main_loop_hz = prefs.getUShort(KEY_MAIN_LOOP_HZ, 10);
    config.gps_hz = prefs.getUShort(KEY_GPS_HZ, 10);
    config.imu_hz = prefs.getUShort(KEY_IMU_HZ, 10);
    config.obd_hz = prefs.getUShort(KEY_OBD_HZ, 10);
    
    prefs.end();
    
    Serial.println("[Config] Configuration loaded from NVS");
    return config;
}

bool ConfigManager::save(const logging_config_t& config) {
    if (!validate(config)) {
        Serial.println("[Config] ERROR: Cannot save invalid configuration");
        return false;
    }
    
    Preferences prefs;
    
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // false = read/write
        Serial.println("[Config] ERROR: Failed to open NVS for writing");
        return false;
    }
    
    // Save each value
    prefs.putUShort(KEY_MAIN_LOOP_HZ, config.main_loop_hz);
    prefs.putUShort(KEY_GPS_HZ, config.gps_hz);
    prefs.putUShort(KEY_IMU_HZ, config.imu_hz);
    prefs.putUShort(KEY_OBD_HZ, config.obd_hz);
    
    prefs.end();
    
    Serial.printf("[Config] Configuration saved to NVS - Main: %dHz, GPS: %dHz, IMU: %dHz, OBD: %dHz\n",
                  config.main_loop_hz, config.gps_hz, config.imu_hz, config.obd_hz);
    
    return true;
}

logging_config_t ConfigManager::get_current() {
    return m_current_config;
}

bool ConfigManager::update(const logging_config_t& config) {
    if (!validate(config)) {
        Serial.println("[Config] ERROR: Invalid configuration provided");
        return false;
    }
    
    if (save(config)) {
        m_current_config = config;
        Serial.println("[Config] Configuration updated successfully");
        return true;
    }
    
    return false;
}

bool ConfigManager::validate(const logging_config_t& config) {
    // Valid main loop rates: 5, 10, 20, 50, 100 Hz
    bool valid_main = (config.main_loop_hz == 5 || 
                       config.main_loop_hz == 10 || 
                       config.main_loop_hz == 20 || 
                       config.main_loop_hz == 50 || 
                       config.main_loop_hz == 100);
    
    if (!valid_main) {
        Serial.printf("[Config] ERROR: Invalid main_loop_hz: %d (must be 5, 10, 20, 50, or 100)\n", 
                     config.main_loop_hz);
        return false;
    }
    
    // Sensor rates must be <= main loop rate and within valid range (1-100 Hz)
    if (config.gps_hz < 1 || config.gps_hz > config.main_loop_hz || config.gps_hz > 100) {
        Serial.printf("[Config] ERROR: Invalid gps_hz: %d (must be 1-%d)\n", 
                     config.gps_hz, config.main_loop_hz);
        return false;
    }
    
    if (config.imu_hz < 1 || config.imu_hz > config.main_loop_hz || config.imu_hz > 100) {
        Serial.printf("[Config] ERROR: Invalid imu_hz: %d (must be 1-%d)\n", 
                     config.imu_hz, config.main_loop_hz);
        return false;
    }
    
    if (config.obd_hz < 1 || config.obd_hz > config.main_loop_hz || config.obd_hz > 100) {
        Serial.printf("[Config] ERROR: Invalid obd_hz: %d (must be 1-%d)\n", 
                     config.obd_hz, config.main_loop_hz);
        return false;
    }
    
    return true;
}

bool ConfigManager::reset_to_defaults() {
    logging_config_t defaults;
    return update(defaults);
}
