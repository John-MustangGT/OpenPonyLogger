#include "config_manager.h"
#include <Preferences.h>
#include <esp_crc.h>

// Static member initialization
bool ConfigManager::m_initialized = false;
logging_config_t ConfigManager::m_current_config;

// NVS keys
const char* ConfigManager::NVS_NAMESPACE = "ponylogger";
const char* ConfigManager::KEY_MAIN_LOOP_HZ = "main_loop_hz";
const char* ConfigManager::KEY_GPS_HZ = "gps_hz";
const char* ConfigManager::KEY_IMU_HZ = "imu_hz";
const char* ConfigManager::KEY_OBD_HZ = "obd_hz";
const char* ConfigManager::KEY_NET_SSID = "net_ssid";
const char* ConfigManager::KEY_NET_PASSWORD = "net_password";
const char* ConfigManager::KEY_NET_IP = "net_ip";
const char* ConfigManager::KEY_NET_SUBNET = "net_subnet";
const char* ConfigManager::KEY_CHECKSUM = "checksum";

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
    
    // Load network configuration with safety checks
    size_t ssid_len = prefs.getString(KEY_NET_SSID, config.network.ssid, sizeof(config.network.ssid));
    if (ssid_len == 0 || config.network.ssid[0] == '\0') {
        // No SSID saved or empty, use default
        strncpy(config.network.ssid, "PonyLogger", sizeof(config.network.ssid) - 1);
        config.network.ssid[sizeof(config.network.ssid) - 1] = '\0';
    }
    
    size_t pwd_len = prefs.getString(KEY_NET_PASSWORD, config.network.password, sizeof(config.network.password));
    if (pwd_len > 0) {
        config.network.password[sizeof(config.network.password) - 1] = '\0';  // Ensure null termination
    }
    
    size_t ip_len = prefs.getBytes(KEY_NET_IP, config.network.ip, sizeof(config.network.ip));
    if (ip_len != sizeof(config.network.ip)) {
        // Invalid or missing IP, use default
        config.network.ip[0] = 192; config.network.ip[1] = 168;
        config.network.ip[2] = 4; config.network.ip[3] = 1;
    }
    
    size_t subnet_len = prefs.getBytes(KEY_NET_SUBNET, config.network.subnet, sizeof(config.network.subnet));
    if (subnet_len != sizeof(config.network.subnet)) {
        // Invalid or missing subnet, use default
        config.network.subnet[0] = 255; config.network.subnet[1] = 255;
        config.network.subnet[2] = 255; config.network.subnet[3] = 0;
    }
    
    // Verify checksum
    uint32_t stored_checksum = prefs.getUInt(KEY_CHECKSUM, 0);
    uint32_t calculated_checksum = calculate_checksum(config);
    
    prefs.end();
    
    if (stored_checksum == 0) {
        Serial.println("[Config] No checksum found in NVS, using defaults");
        return logging_config_t();  // Return defaults
    }
    
    if (stored_checksum != calculated_checksum) {
        Serial.printf("[Config] WARNING: Checksum mismatch! Stored: 0x%08X, Calculated: 0x%08X\n", 
                     stored_checksum, calculated_checksum);
        Serial.println("[Config] NVS data corrupted, using defaults");
        return logging_config_t();  // Return defaults
    }
    
    Serial.printf("[Config] Configuration loaded from NVS (checksum: 0x%08X)\n", stored_checksum);
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
    
    // Save network configuration
    prefs.putString(KEY_NET_SSID, config.network.ssid);
    prefs.putString(KEY_NET_PASSWORD, config.network.password);
    prefs.putBytes(KEY_NET_IP, config.network.ip, sizeof(config.network.ip));
    prefs.putBytes(KEY_NET_SUBNET, config.network.subnet, sizeof(config.network.subnet));
    
    // Calculate and save checksum
    uint32_t checksum = calculate_checksum(config);
    prefs.putUInt(KEY_CHECKSUM, checksum);
    
    prefs.end();
    
    Serial.printf("[Config] Configuration saved to NVS - Main: %dHz, GPS: %dHz, IMU: %dHz, OBD: %dHz (checksum: 0x%08X)\n",
                  config.main_loop_hz, config.gps_hz, config.imu_hz, config.obd_hz, checksum);
    
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

uint32_t ConfigManager::calculate_checksum(const logging_config_t& config) {
    // Create a buffer with the configuration values to checksum
    struct {
        uint16_t main_loop_hz;
        uint16_t gps_hz;
        uint16_t imu_hz;
        uint16_t obd_hz;
        char ssid[32];
        char password[64];
        uint8_t ip[4];
        uint8_t subnet[4];
    } data;
    
    data.main_loop_hz = config.main_loop_hz;
    data.gps_hz = config.gps_hz;
    data.imu_hz = config.imu_hz;
    data.obd_hz = config.obd_hz;
    memcpy(data.ssid, config.network.ssid, sizeof(data.ssid));
    memcpy(data.password, config.network.password, sizeof(data.password));
    memcpy(data.ip, config.network.ip, sizeof(data.ip));
    memcpy(data.subnet, config.network.subnet, sizeof(data.subnet));
    
    // Calculate CRC32 checksum
    return esp_crc32_le(0, (uint8_t*)&data, sizeof(data));
}
