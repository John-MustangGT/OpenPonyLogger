#include "config_manager.h"
#include <Preferences.h>
#include <esp_crc.h>
#include <esp_log.h>

static const char* TAG = "Config";

// Static member initialization
bool ConfigManager::m_initialized = false;
logging_config_t ConfigManager::m_current_config;

// NVS keys
const char* ConfigManager::NVS_NAMESPACE = "ponylogger";
const char* ConfigManager::KEY_MAIN_LOOP_HZ = "main_loop_hz";
const char* ConfigManager::KEY_GPS_HZ = "gps_hz";
const char* ConfigManager::KEY_IMU_HZ = "imu_hz";
const char* ConfigManager::KEY_OBD_HZ = "obd_hz";
const char* ConfigManager::KEY_OBD_BLE_ENABLED = "obd_ble_en";
const char* ConfigManager::KEY_MARRIED_VIN = "married_vin";
const char* ConfigManager::KEY_MARRIED_ECU = "married_ecu";
const char* ConfigManager::KEY_IS_MARRIED = "is_married";
const char* ConfigManager::KEY_LOG_LEVEL = "log_level";
const char* ConfigManager::KEY_DYN_AUTO_EN = "dyn_auto_en";
const char* ConfigManager::KEY_DYN_START_SPD = "dyn_start_spd";
const char* ConfigManager::KEY_DYN_STOP_TMO = "dyn_stop_tmo";
const char* ConfigManager::KEY_DATA_AUTO_EN = "data_auto_en";
const char* ConfigManager::KEY_DATA_STOP_TMO = "data_stop_tmo";
const char* ConfigManager::KEY_NET_SSID = "net_ssid";
const char* ConfigManager::KEY_NET_PASSWORD = "net_password";
const char* ConfigManager::KEY_NET_IP = "net_ip";
const char* ConfigManager::KEY_NET_SUBNET = "net_subnet";
const char* ConfigManager::KEY_CHECKSUM = "checksum";
// Build info keys
const char* ConfigManager::KEY_BUILD_VERSION = "build_ver";
const char* ConfigManager::KEY_BUILD_COMMIT = "build_sha";
const char* ConfigManager::KEY_BUILD_BRANCH = "build_branch";
const char* ConfigManager::KEY_BUILD_TIME = "build_time";
const char* ConfigManager::KEY_BUILD_STORAGE = "build_storage";
const char* ConfigManager::KEY_FIRST_BOOT = "first_boot";
// Hardware config keys
const char* ConfigManager::KEY_HW_GPS = "hw_gps";
const char* ConfigManager::KEY_HW_IMU = "hw_imu";
const char* ConfigManager::KEY_HW_COMPASS = "hw_compass";
const char* ConfigManager::KEY_HW_BATTERY = "hw_battery";
const char* ConfigManager::KEY_HW_DISPLAY = "hw_display";
const char* ConfigManager::KEY_HW_RTC = "hw_rtc";
const char* ConfigManager::KEY_HW_OBD = "hw_obd";
const char* ConfigManager::KEY_HW_OBD_NAME = "hw_obd_name";
const char* ConfigManager::KEY_LAST_BOOT = "last_boot";

bool ConfigManager::init() {
    if (m_initialized) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing configuration manager...");

    // Load configuration from NVS
    m_current_config = load();

    // Validate loaded configuration
    if (!validate(m_current_config)) {
        ESP_LOGW(TAG, "Invalid configuration loaded, using defaults");
        m_current_config = logging_config_t();  // Reset to defaults
        save(m_current_config);  // Save defaults
    }

    ESP_LOGI(TAG, "✓ Configuration loaded - Main: %dHz, GPS: %dHz, IMU: %dHz, OBD: %dHz",
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
        ESP_LOGI(TAG, "No saved configuration found, using defaults");
        return config;  // Return defaults
    }
    
    // Load each value, using defaults if not found
    config.main_loop_hz = prefs.getUShort(KEY_MAIN_LOOP_HZ, 10);
    config.gps_hz = prefs.getUShort(KEY_GPS_HZ, 10);
    config.imu_hz = prefs.getUShort(KEY_IMU_HZ, 10);
    config.obd_hz = prefs.getUShort(KEY_OBD_HZ, 10);
    config.obd_ble_enabled = prefs.getBool(KEY_OBD_BLE_ENABLED, true);

    // Load vehicle marriage info
    config.is_married = prefs.getBool(KEY_IS_MARRIED, false);
    if (config.is_married) {
        size_t vin_len = prefs.getString(KEY_MARRIED_VIN, config.married_vin, sizeof(config.married_vin));
        if (vin_len == 0 || vin_len > 17) {
            // Invalid VIN, clear marriage
            config.is_married = false;
            config.married_vin[0] = '\0';
            config.married_ecu[0] = '\0';
        } else {
            config.married_vin[sizeof(config.married_vin) - 1] = '\0';
            size_t ecu_len = prefs.getString(KEY_MARRIED_ECU, config.married_ecu, sizeof(config.married_ecu));
            if (ecu_len > 0) {
                config.married_ecu[sizeof(config.married_ecu) - 1] = '\0';
            }
        }
    }

    config.log_level = prefs.getUChar(KEY_LOG_LEVEL, 3);  // Default: INFO

    // Load per-module log levels
    uint16_t mod_count = prefs.getUShort("mod_count", 0);
    if (mod_count > 0) {
        // Load each saved module level
        const char* module_names[] = {
            "MAIN", "RTLogger", "STATUS", "FlashStorage", "GPS", "IMU",
            "OBD-BLE", "WiFi", "Config", "TimeSync", "LogFileMgr"
        };
        for (const char* module : module_names) {
            String key = String("mod_") + module;
            if (prefs.isKey(key.c_str())) {
                uint8_t level = prefs.getUChar(key.c_str(), 3);
                config.module_log_levels[module] = level;
            }
        }
    }
    // If no saved module levels, config.module_log_levels will use constructor defaults

    // Load auto-logging configuration
    config.dynamics_auto_enabled = prefs.getBool(KEY_DYN_AUTO_EN, false);
    config.dynamics_start_speed_mph = prefs.getUChar(KEY_DYN_START_SPD, 5);
    config.dynamics_stop_timeout_sec = prefs.getUShort(KEY_DYN_STOP_TMO, 30);
    config.data_auto_enabled = prefs.getBool(KEY_DATA_AUTO_EN, false);
    config.data_stop_timeout_sec = prefs.getUShort(KEY_DATA_STOP_TMO, 300);

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
        ESP_LOGI(TAG, "No checksum found in NVS, using defaults");
        return logging_config_t();  // Return defaults
    }

    if (stored_checksum != calculated_checksum) {
        ESP_LOGW(TAG, "Checksum mismatch! Stored: 0x%08X, Calculated: 0x%08X",
                 stored_checksum, calculated_checksum);
        ESP_LOGW(TAG, "NVS data corrupted, using defaults");
        return logging_config_t();  // Return defaults
    }

    ESP_LOGI(TAG, "Configuration loaded from NVS (checksum: 0x%08X)", stored_checksum);
    return config;
}

bool ConfigManager::save(const logging_config_t& config) {
    if (!validate(config)) {
        ESP_LOGE(TAG, "Cannot save invalid configuration");
        return false;
    }

    Preferences prefs;

    if (!prefs.begin(NVS_NAMESPACE, false)) {  // false = read/write
        ESP_LOGE(TAG, "Failed to open NVS for writing");
        return false;
    }
    
    // Save each value
    prefs.putUShort(KEY_MAIN_LOOP_HZ, config.main_loop_hz);
    prefs.putUShort(KEY_GPS_HZ, config.gps_hz);
    prefs.putUShort(KEY_IMU_HZ, config.imu_hz);
    prefs.putUShort(KEY_OBD_HZ, config.obd_hz);
    prefs.putBool(KEY_OBD_BLE_ENABLED, config.obd_ble_enabled);

    // Save vehicle marriage info
    prefs.putBool(KEY_IS_MARRIED, config.is_married);
    if (config.is_married) {
        prefs.putString(KEY_MARRIED_VIN, config.married_vin);
        prefs.putString(KEY_MARRIED_ECU, config.married_ecu);
    } else {
        // Clear marriage data if not married
        prefs.remove(KEY_MARRIED_VIN);
        prefs.remove(KEY_MARRIED_ECU);
    }

    prefs.putUChar(KEY_LOG_LEVEL, config.log_level);

    // Save per-module log levels
    // First, clear any old module keys
    prefs.remove("mod_count");
    for (const auto& entry : config.module_log_levels) {
        String key = "mod_" + entry.first;
        prefs.putUChar(key.c_str(), entry.second);
    }
    // Save count for easier loading
    prefs.putUShort("mod_count", config.module_log_levels.size());

    // Save auto-logging configuration
    prefs.putBool(KEY_DYN_AUTO_EN, config.dynamics_auto_enabled);
    prefs.putUChar(KEY_DYN_START_SPD, config.dynamics_start_speed_mph);
    prefs.putUShort(KEY_DYN_STOP_TMO, config.dynamics_stop_timeout_sec);
    prefs.putBool(KEY_DATA_AUTO_EN, config.data_auto_enabled);
    prefs.putUShort(KEY_DATA_STOP_TMO, config.data_stop_timeout_sec);

    // Save network configuration
    prefs.putString(KEY_NET_SSID, config.network.ssid);
    prefs.putString(KEY_NET_PASSWORD, config.network.password);
    prefs.putBytes(KEY_NET_IP, config.network.ip, sizeof(config.network.ip));
    prefs.putBytes(KEY_NET_SUBNET, config.network.subnet, sizeof(config.network.subnet));
    
    // Calculate and save checksum
    uint32_t checksum = calculate_checksum(config);
    prefs.putUInt(KEY_CHECKSUM, checksum);
    
    prefs.end();

    ESP_LOGI(TAG, "Configuration saved to NVS - Main: %dHz, GPS: %dHz, IMU: %dHz, OBD: %dHz (checksum: 0x%08X)",
             config.main_loop_hz, config.gps_hz, config.imu_hz, config.obd_hz, checksum);

    return true;
}

logging_config_t ConfigManager::get_current() {
    return m_current_config;
}

bool ConfigManager::update(const logging_config_t& config) {
    if (!validate(config)) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return false;
    }

    if (save(config)) {
        m_current_config = config;
        ESP_LOGI(TAG, "Configuration updated successfully");
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
        ESP_LOGE(TAG, "Invalid main_loop_hz: %d (must be 5, 10, 20, 50, or 100)",
                 config.main_loop_hz);
        return false;
    }

    // Sensor rates must be <= main loop rate and within valid range (1-100 Hz)
    if (config.gps_hz < 1 || config.gps_hz > config.main_loop_hz || config.gps_hz > 100) {
        ESP_LOGE(TAG, "Invalid gps_hz: %d (must be 1-%d)",
                 config.gps_hz, config.main_loop_hz);
        return false;
    }

    if (config.imu_hz < 1 || config.imu_hz > config.main_loop_hz || config.imu_hz > 100) {
        ESP_LOGE(TAG, "Invalid imu_hz: %d (must be 1-%d)",
                 config.imu_hz, config.main_loop_hz);
        return false;
    }

    if (config.obd_hz < 1 || config.obd_hz > config.main_loop_hz || config.obd_hz > 100) {
        ESP_LOGE(TAG, "Invalid obd_hz: %d (must be 1-%d)",
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
        bool obd_ble_enabled;
        uint8_t log_level;
        bool dynamics_auto_enabled;
        uint8_t dynamics_start_speed_mph;
        uint16_t dynamics_stop_timeout_sec;
        bool data_auto_enabled;
        uint16_t data_stop_timeout_sec;
        char ssid[32];
        char password[64];
        uint8_t ip[4];
        uint8_t subnet[4];
    } data;

    data.main_loop_hz = config.main_loop_hz;
    data.gps_hz = config.gps_hz;
    data.imu_hz = config.imu_hz;
    data.obd_hz = config.obd_hz;
    data.obd_ble_enabled = config.obd_ble_enabled;
    data.log_level = config.log_level;
    data.dynamics_auto_enabled = config.dynamics_auto_enabled;
    data.dynamics_start_speed_mph = config.dynamics_start_speed_mph;
    data.dynamics_stop_timeout_sec = config.dynamics_stop_timeout_sec;
    data.data_auto_enabled = config.data_auto_enabled;
    data.data_stop_timeout_sec = config.data_stop_timeout_sec;
    memcpy(data.ssid, config.network.ssid, sizeof(data.ssid));
    memcpy(data.password, config.network.password, sizeof(data.password));
    memcpy(data.ip, config.network.ip, sizeof(data.ip));
    memcpy(data.subnet, config.network.subnet, sizeof(data.subnet));

    // Calculate CRC32 checksum
    return esp_crc32_le(0, (uint8_t*)&data, sizeof(data));
}

void ConfigManager::apply_log_levels(const logging_config_t& config) {
    // Map config levels (0-5) to ESP-IDF log levels
    const esp_log_level_t level_map[] = {
        ESP_LOG_NONE,    // 0
        ESP_LOG_ERROR,   // 1
        ESP_LOG_WARN,    // 2
        ESP_LOG_INFO,    // 3
        ESP_LOG_DEBUG,   // 4
        ESP_LOG_VERBOSE  // 5
    };

    // Apply global default log level to all modules
    esp_log_level_t global_level = level_map[config.log_level > 5 ? 3 : config.log_level];
    esp_log_level_set("*", global_level);

    // Apply per-module log levels (overrides global default)
    for (const auto& entry : config.module_log_levels) {
        uint8_t level = entry.second > 5 ? 3 : entry.second;
        esp_log_level_set(entry.first.c_str(), level_map[level]);
    }

    ESP_LOGI("Config", "Applied log levels: global=%d, modules=%zu",
             config.log_level, config.module_log_levels.size());
}

bool ConfigManager::marry_to_vehicle(const char* vin, const char* ecu_name) {
    if (!m_initialized) {
        ESP_LOGE(TAG, "Config manager not initialized");
        return false;
    }

    if (vin == nullptr || strlen(vin) != 17) {
        ESP_LOGE(TAG, "Invalid VIN (must be exactly 17 characters)");
        return false;
    }

    logging_config_t config = m_current_config;

    // Store VIN and ECU info
    strncpy(config.married_vin, vin, sizeof(config.married_vin) - 1);
    config.married_vin[sizeof(config.married_vin) - 1] = '\0';

    if (ecu_name != nullptr && strlen(ecu_name) > 0) {
        strncpy(config.married_ecu, ecu_name, sizeof(config.married_ecu) - 1);
        config.married_ecu[sizeof(config.married_ecu) - 1] = '\0';
    } else {
        config.married_ecu[0] = '\0';
    }

    config.is_married = true;

    if (update(config)) {
        ESP_LOGI(TAG, "✓ Logger married to vehicle VIN: %.17s", vin);
        return true;
    }

    ESP_LOGE(TAG, "Failed to save marriage info");
    return false;
}

bool ConfigManager::divorce_from_vehicle() {
    if (!m_initialized) {
        ESP_LOGE(TAG, "Config manager not initialized");
        return false;
    }

    logging_config_t config = m_current_config;

    // Clear marriage info
    config.is_married = false;
    config.married_vin[0] = '\0';
    config.married_ecu[0] = '\0';

    if (update(config)) {
        ESP_LOGI(TAG, "✓ Logger divorced from vehicle");
        return true;
    }

    ESP_LOGE(TAG, "Failed to clear marriage info");
    return false;
}

bool ConfigManager::is_married() {
    return m_current_config.is_married;
}

bool ConfigManager::verify_vin(const char* vin) {
    if (!m_initialized) {
        ESP_LOGE(TAG, "Config manager not initialized");
        return false;
    }

    // If not married, allow any VIN
    if (!m_current_config.is_married) {
        return true;
    }

    // If married, check VIN matches
    if (vin == nullptr || strlen(vin) != 17) {
        ESP_LOGW(TAG, "Invalid VIN format");
        return false;
    }

    bool matches = (strncmp(m_current_config.married_vin, vin, 17) == 0);

    if (!matches) {
        ESP_LOGW(TAG, "VIN mismatch! Logger married to: %.17s, received: %.17s",
                 m_current_config.married_vin, vin);
    }

    return matches;
}

bool ConfigManager::save_build_info(const build_info_t& build_info) {
    Preferences prefs;

    if (!prefs.begin(NVS_NAMESPACE, false)) {  // false = read-write
        ESP_LOGE(TAG, "Failed to open NVS for build info save");
        return false;
    }

    prefs.putString(KEY_BUILD_VERSION, build_info.version);
    prefs.putString(KEY_BUILD_COMMIT, build_info.commit_sha);
    prefs.putString(KEY_BUILD_BRANCH, build_info.branch);
    prefs.putString(KEY_BUILD_TIME, build_info.build_time);
    prefs.putString(KEY_BUILD_STORAGE, build_info.storage_type);
    prefs.putUInt(KEY_FIRST_BOOT, build_info.first_boot_time);

    prefs.end();

    ESP_LOGI(TAG, "✓ Build info saved: %s (%s on %s)",
             build_info.version, build_info.commit_sha, build_info.branch);

    return true;
}

build_info_t ConfigManager::load_build_info() {
    Preferences prefs;
    build_info_t build_info;

    if (!prefs.begin(NVS_NAMESPACE, true)) {  // true = read-only
        ESP_LOGI(TAG, "No saved build info found");
        return build_info;  // Return empty
    }

    prefs.getString(KEY_BUILD_VERSION, build_info.version, sizeof(build_info.version));
    prefs.getString(KEY_BUILD_COMMIT, build_info.commit_sha, sizeof(build_info.commit_sha));
    prefs.getString(KEY_BUILD_BRANCH, build_info.branch, sizeof(build_info.branch));
    prefs.getString(KEY_BUILD_TIME, build_info.build_time, sizeof(build_info.build_time));
    prefs.getString(KEY_BUILD_STORAGE, build_info.storage_type, sizeof(build_info.storage_type));
    build_info.first_boot_time = prefs.getUInt(KEY_FIRST_BOOT, 0);

    prefs.end();

    return build_info;
}

bool ConfigManager::save_hardware_config(const hardware_config_t& hw_config) {
    Preferences prefs;

    if (!prefs.begin(NVS_NAMESPACE, false)) {  // false = read-write
        ESP_LOGE(TAG, "Failed to open NVS for hardware config save");
        return false;
    }

    prefs.putBool(KEY_HW_GPS, hw_config.gps_detected);
    prefs.putBool(KEY_HW_IMU, hw_config.imu_detected);
    prefs.putBool(KEY_HW_COMPASS, hw_config.compass_detected);
    prefs.putBool(KEY_HW_BATTERY, hw_config.battery_detected);
    prefs.putBool(KEY_HW_DISPLAY, hw_config.display_detected);
    prefs.putBool(KEY_HW_RTC, hw_config.rtc_detected);
    prefs.putBool(KEY_HW_OBD, hw_config.obd_connected);
    prefs.putString(KEY_HW_OBD_NAME, hw_config.obd_device_name);
    prefs.putUInt(KEY_LAST_BOOT, hw_config.last_boot_time);

    prefs.end();

    ESP_LOGI(TAG, "✓ Hardware config saved - GPS:%d IMU:%d Compass:%d Battery:%d Display:%d RTC:%d OBD:%d",
             hw_config.gps_detected, hw_config.imu_detected, hw_config.compass_detected,
             hw_config.battery_detected, hw_config.display_detected, hw_config.rtc_detected,
             hw_config.obd_connected);

    return true;
}

hardware_config_t ConfigManager::load_hardware_config() {
    Preferences prefs;
    hardware_config_t hw_config;

    if (!prefs.begin(NVS_NAMESPACE, true)) {  // true = read-only
        ESP_LOGI(TAG, "No saved hardware config found");
        return hw_config;  // Return empty (all false)
    }

    hw_config.gps_detected = prefs.getBool(KEY_HW_GPS, false);
    hw_config.imu_detected = prefs.getBool(KEY_HW_IMU, false);
    hw_config.compass_detected = prefs.getBool(KEY_HW_COMPASS, false);
    hw_config.battery_detected = prefs.getBool(KEY_HW_BATTERY, false);
    hw_config.display_detected = prefs.getBool(KEY_HW_DISPLAY, false);
    hw_config.rtc_detected = prefs.getBool(KEY_HW_RTC, false);
    hw_config.obd_connected = prefs.getBool(KEY_HW_OBD, false);
    prefs.getString(KEY_HW_OBD_NAME, hw_config.obd_device_name, sizeof(hw_config.obd_device_name));
    hw_config.last_boot_time = prefs.getUInt(KEY_LAST_BOOT, 0);

    prefs.end();

    return hw_config;
}
