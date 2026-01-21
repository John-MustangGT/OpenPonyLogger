#include "icar_ble_driver.h"
#include "../Logger/include/debug_flags.h"
#include "../Config/include/config_manager.h"
#include <cstring>
#include <algorithm>
#include <esp_log.h>

// Forward declaration
class IcarBleDriver;

static const char* TAG = "OBD-BLE";

// Global variables for deferred BLE connection (to avoid callback context crash)
static bool g_pending_connection = false;
static uint32_t g_pending_connection_time = 0;

// Scan callback implementation
void OBDScanCallback::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    std::string name = advertisedDevice->getName();
    std::string addr = advertisedDevice->getAddress().toString();
    int rssi = advertisedDevice->getRSSI();
    
    // Log every device found for debugging
    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Found device: '%s' (%s) RSSI=%d",
                 name.c_str(), addr.c_str(), rssi);
    }
    
    // Add to recent devices list
    uint32_t now = millis();
    
    // Check if we already have this device in recent list
    bool found = false;
    for (auto& device : IcarBleDriver::m_recent_devices) {
        if (strcmp(device.address, addr.c_str()) == 0) {
            // Update existing entry
            device.rssi = rssi;
            device.last_seen_ms = now;
            strncpy(device.name, name.c_str(), sizeof(device.name) - 1);
            device.name[sizeof(device.name) - 1] = '\0';
            found = true;
            break;
        }
    }
    
    // Add new device if not found
    if (!found && IcarBleDriver::m_recent_devices.size() < 20) {  // Keep max 20 devices
        recent_ble_device_t new_device;
        strncpy(new_device.name, name.c_str(), sizeof(new_device.name) - 1);
        new_device.name[sizeof(new_device.name) - 1] = '\0';
        strncpy(new_device.address, addr.c_str(), sizeof(new_device.address) - 1);
        new_device.address[sizeof(new_device.address) - 1] = '\0';
        new_device.rssi = rssi;
        new_device.last_seen_ms = now;
        IcarBleDriver::m_recent_devices.push_back(new_device);
    }
    
    // Check if device name matches known ELM327 adapters
    if (name.find("iCar") != std::string::npos ||
        name.find("vgate") != std::string::npos ||
        name.find("vlink") != std::string::npos ||
        name.find("vLink") != std::string::npos ||
        name.find("VLINK") != std::string::npos ||
        name.find("IOS-Vlink") != std::string::npos ||
        name.find("OBD") != std::string::npos ||
        name.find("ELM") != std::string::npos) {
        
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGD(TAG, "Matching device found: '%s' (%s) RSSI=%d", name.c_str(), addr.c_str(), rssi);
        }
        
        // Only trigger connection if not already pending
        if (!g_pending_connection) {
            // Stop scanning before connecting
            NimBLEDevice::getScan()->stop();
            if (DebugFlags::ENABLE_OBD_DEBUG) {
                ESP_LOGD(TAG, "Scan stopped");
            }
            
            // Store device name and address for later connection
            strncpy(IcarBleDriver::m_device_name, name.c_str(), sizeof(IcarBleDriver::m_device_name) - 1);
            IcarBleDriver::m_device_name[sizeof(IcarBleDriver::m_device_name) - 1] = '\0';
            
            strncpy(IcarBleDriver::m_device_address, addr.c_str(), sizeof(IcarBleDriver::m_device_address) - 1);
            IcarBleDriver::m_device_address[sizeof(IcarBleDriver::m_device_address) - 1] = '\0';
            
            // Set flag to connect from main loop (avoid callback context issues)
            g_pending_connection = true;
            g_pending_connection_time = millis();
            if (DebugFlags::ENABLE_OBD_DEBUG) {
                ESP_LOGD(TAG, "Pending connection flag set for %s at time %ums",
                         IcarBleDriver::m_device_address, (uint32_t)g_pending_connection_time);
            }
        } else {
            if (DebugFlags::ENABLE_OBD_DEBUG) {
                ESP_LOGD(TAG, "Connection already pending, ignoring duplicate match for %s",
                         name.c_str());
            }
        }
    }
}

// Static member initialization
obd_data_t IcarBleDriver::m_data = {};
bool IcarBleDriver::m_connected = false;
char IcarBleDriver::m_device_address[18] = "";
char IcarBleDriver::m_device_name[32] = "";
char IcarBleDriver::m_vin[18] = "";
char IcarBleDriver::m_ecm_name[20] = "";
NimBLERemoteCharacteristic* IcarBleDriver::m_rx_char = nullptr;
NimBLERemoteCharacteristic* IcarBleDriver::m_tx_char = nullptr;
std::vector<obd_pid_config_t> IcarBleDriver::m_configured_pids = {};
std::vector<recent_ble_device_t> IcarBleDriver::m_recent_devices = {};

// vgate iCar 2 Pro BLE UUIDs (may vary by device variant)
// Standard vgate: 0xFFE0, but some devices use custom UUIDs
static const char* SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
static const char* SERVICE_UUID_ALT = "e7810a71-73ae-499d-8c15-faa9aef0c3f2";  // IOS-Vlink custom
static const char* RX_CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";  // Read/Notify from device
static const char* TX_CHAR_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb";  // Write to device

bool IcarBleDriver::init() {
    ESP_LOGI(TAG, "Initializing NimBLE central");

    // Initialize BLE device
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Max power for range

    ESP_LOGI(TAG, "NimBLE initialized successfully");
    return true;
}

bool IcarBleDriver::start_scan() {
    ESP_LOGI(TAG, "Starting BLE scan for ELM327-compatible device (iCar/vgate/vlink/IOS-Vlink)");

    NimBLEScan* scan = NimBLEDevice::getScan();
    if (!scan) {
        ESP_LOGE(TAG, "Failed to get scan instance");
        return false;
    }

    // Configure scan parameters
    scan->setInterval(97);   // Interval in 0.625ms units (~60ms)
    scan->setWindow(32);     // Window in 0.625ms units (~20ms)
    scan->setActiveScan(true);
    scan->setDuplicateFilter(true);

    // Set scan callback to auto-connect when device found
    static OBDScanCallback callback;
    scan->setAdvertisedDeviceCallbacks(&callback);

    // Start scan (0 = scan indefinitely until match found)
    bool started = scan->start(0, nullptr, false);

    if (started) {
        ESP_LOGI(TAG, "BLE scan started successfully");
    } else {
        ESP_LOGE(TAG, "BLE scan failed to start");
    }

    return started;
}

void IcarBleDriver::stop_scan() {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan) {
        scan->stop();
    }
}

bool IcarBleDriver::connect(const char* address) {
    if (!address) {
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGE(TAG, "Address is null");
        }
        return false;
    }

    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Starting connection sequence to %s", address);
    }
    
    // Create client
    NimBLEClient* pClient = NimBLEDevice::createClient();
    if (!pClient) {
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGE(TAG, "Failed to create client");
        }
        return false;
    }

    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Client created, connecting to device");
    }
    
    // Connect to address
    NimBLEAddress addr(address);
    bool connected = pClient->connect(addr);

    if (!connected) {
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGE(TAG, "Failed to connect to remote device");
        }
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Connected");
    }
    
    // Skip discoverAttributes() - it blocks too long and triggers watchdog
    // Go straight to service lookup with standard + alternate UUIDs

    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Looking for service %s", SERVICE_UUID);
    }
    NimBLERemoteService* pRemoteSvc = pClient->getService(SERVICE_UUID);

    // If not found, try alternate UUID (IOS-Vlink uses custom UUID)
    if (!pRemoteSvc) {
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGD(TAG, "Standard service not found, trying alternate: %s", SERVICE_UUID_ALT);
        }
        pRemoteSvc = pClient->getService(SERVICE_UUID_ALT);
    }

    if (!pRemoteSvc) {
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGE(TAG, "Neither service found (%s or %s), disconnecting", SERVICE_UUID, SERVICE_UUID_ALT);
        }
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }
    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Service found");
    }
    
    // Auto-detect RX/TX characteristics by properties instead of hardcoded UUIDs
    // RX = characteristic with notify/indicate (device sends data to us)
    // TX = characteristic with write (we send data to device)
    ESP_LOGI(TAG, "Auto-detecting RX/TX characteristics");

    auto* chars = pRemoteSvc->getCharacteristics(true);  // true = force refresh
    if (!chars || chars->empty()) {
        ESP_LOGE(TAG, "No characteristics found");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }
    
    for (auto* chr : *chars) {
        if (!chr) continue;

        // RX char: can notify or indicate (device → us)
        if (!m_rx_char && (chr->canNotify() || chr->canIndicate())) {
            m_rx_char = chr;
            if (DebugFlags::ENABLE_OBD_DEBUG) {
                ESP_LOGD(TAG, "RX char: %s (notify=%d)",
                        chr->getUUID().toString().c_str(), chr->canNotify());
            }
        }

        // TX char: can write (us → device)
        if (!m_tx_char && chr->canWrite()) {
            m_tx_char = chr;
            if (DebugFlags::ENABLE_OBD_DEBUG) {
                ESP_LOGD(TAG, "TX char: %s (write=%d)",
                        chr->getUUID().toString().c_str(), chr->canWrite());
            }
        }

        if (m_rx_char && m_tx_char) break;  // Found both, stop searching
    }

    if (!m_rx_char || !m_tx_char) {
        ESP_LOGE(TAG, "Missing characteristics (RX=%d, TX=%d)",
                m_rx_char != nullptr, m_tx_char != nullptr);
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }
    
    // Subscribe to notifications if available
    if (m_rx_char->canNotify()) {
        // Use lambda to parse OBD responses (m_data is static, no capture needed)
        m_rx_char->subscribe(true, [](NimBLERemoteCharacteristic* pChar, 
                                       uint8_t* pData, size_t length, bool isNotify) {
            // Parse OBD response in callback (avoid Serial.printf - causes crashes)
            // Response format: 62 01 <PID> <DATA_BYTES...>
            if (length >= 4 && pData[0] == 0x62 && pData[1] == 0x01) {
                uint8_t pid = pData[2];
                
                switch (pid) {
                    case 0x0C: // RPM (2 bytes)
                        if (length >= 5) {
                            m_data.engine_rpm = ((pData[3] * 256) + pData[4]) / 4.0f;
                        }
                        break;
                    case 0x0D: // Speed (1 byte)
                        if (length >= 4) {
                            m_data.vehicle_speed = pData[3];
                        }
                        break;
                    case 0x05: // Coolant temp (1 byte, offset -40)
                        if (length >= 4) {
                            m_data.coolant_temp = pData[3] - 40.0f;
                        }
                        break;
                    case 0x11: // Throttle position (1 byte)
                        if (length >= 4) {
                            m_data.throttle_position = (pData[3] * 100.0f) / 255.0f;
                        }
                        break;
                    case 0x04: // Engine load (1 byte)
                        if (length >= 4) {
                            m_data.engine_load = (pData[3] * 100.0f) / 255.0f;
                        }
                        break;
                    case 0x0F: // Intake temp (1 byte, offset -40)
                        if (length >= 4) {
                            m_data.intake_temp = pData[3] - 40.0f;
                        }
                        break;
                }
                // Capture microsecond timestamp when data arrives for accurate logging
                m_data.timestamp_us = esp_timer_get_time();
                m_data.valid = true;
                m_data.last_update_ms = millis();
            }
        });
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGD(TAG, "Subscribed to RX notifications");
        }
    }
    
    // Store address for future reconnection
    strncpy(m_device_address, address, sizeof(m_device_address) - 1);
    m_device_address[sizeof(m_device_address) - 1] = '\0';
    
    // Get and store device name from address (BLE address is the identifier)
    // Most adapters advertise as "vgate iCar2Pro" or similar during scan
    // We'll store the address as the name for now
    snprintf(m_device_name, sizeof(m_device_name), "OBD2 %s", address + strlen(address) - 5);
    
    m_connected = true;
    m_data.connected = true;
    m_data.last_update_ms = millis();

    ESP_LOGI(TAG, "Connected to %s successfully", m_device_name);

    // Print heap status
    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Free heap: %u bytes", ESP.getFreeHeap());
    }

    // Request VIN and ECM info from vehicle
    request_vehicle_info();

    // Verify VIN if logger is married to a vehicle
    if (ConfigManager::is_married()) {
        if (!ConfigManager::verify_vin(m_vin)) {
            ESP_LOGE(TAG, "❌ VIN MISMATCH! This is not the married vehicle.");
            ESP_LOGE(TAG, "Expected VIN: %s", ConfigManager::get_current().married_vin);
            ESP_LOGE(TAG, "Received VIN: %s", m_vin);
            ESP_LOGE(TAG, "Disconnecting from wrong vehicle...");

            // Disconnect from wrong vehicle
            disconnect();
            return false;
        } else {
            ESP_LOGI(TAG, "✓ VIN verified - connected to married vehicle");
        }
    } else {
        // Not married, attempt to marry to this vehicle
        if (m_vin[0] != '\0' && strcmp(m_vin, "N/A") != 0) {
            ESP_LOGI(TAG, "Logger not married - marrying to VIN: %s", m_vin);
            ConfigManager::marry_to_vehicle(m_vin, m_ecm_name);
        } else {
            ESP_LOGW(TAG, "Could not retrieve VIN - logger remains unmarried");
        }
    }

    return true;
}

void IcarBleDriver::disconnect() {
    m_connected = false;
    m_data.connected = false;
    m_rx_char = nullptr;
    m_tx_char = nullptr;

    // Clear device info
    m_device_name[0] = '\0';
    m_vin[0] = '\0';
    m_ecm_name[0] = '\0';

    NimBLEDevice::deinit(false);
    ESP_LOGI(TAG, "Disconnected from device");
}

bool IcarBleDriver::is_connected() {
    return m_connected;
}

bool IcarBleDriver::update() {
    // NimBLE stack must be driven from Core 0; skip if called elsewhere to avoid crashes
    if (xPortGetCoreID() != 0) {
        return false;
    }

    // Handle pending connection from BLE scan callback
    if (g_pending_connection && !m_connected) {
        // Wait 100ms after device was found to let stack settle
        uint32_t time_since_found = millis() - g_pending_connection_time;
        if (time_since_found >= 100) {
            if (DebugFlags::ENABLE_OBD_DEBUG) {
                ESP_LOGD(TAG, "Attempting deferred connection to %s (delay=%ums)",
                        m_device_address, time_since_found);
            }
            if (connect(m_device_address)) {
                ESP_LOGI(TAG, "Connection successful");
                g_pending_connection = false;
            } else {
                ESP_LOGW(TAG, "Connection attempt failed, will retry");
                // Retry in 500ms
                g_pending_connection_time = millis();
            }
        } else {
            if (DebugFlags::ENABLE_OBD_DEBUG) {
                ESP_LOGD(TAG, "Waiting for deferred connection (%ums/%ums)",
                        time_since_found, 100);
            }
        }
        return false;
    }
    
    // Periodic debug output to show scan is alive
    static uint32_t last_scan_debug = 0;
    uint32_t now = millis();
    if (DebugFlags::ENABLE_OBD_DEBUG && now - last_scan_debug >= 10000) {  // Every 10 seconds
        ESP_LOGD(TAG, "Scan state: connected=%d, recent_devices=%zu, pending_conn=%d",
                m_connected, m_recent_devices.size(), g_pending_connection);
        last_scan_debug = now;
    }
    
    if (!m_connected) return false;
    
    // Query common PIDs
    // PID 0x0C = RPM, 0x0D = Speed, 0x05 = Coolant Temp, etc.
    return request_pid(0x0C);  // Start with RPM
}

obd_data_t IcarBleDriver::get_data() {
    return m_data;
}

bool IcarBleDriver::request_pid(uint8_t pid) {
    if (!m_connected || !m_tx_char) {
        ESP_LOGW(TAG, "Not connected, cannot request PID");
        return false;
    }

    // Construct OBD-II request: 62 01 <PID>
    // 62 = Service 01 (read data by identifier)
    // 01 = PID mode
    uint8_t request[3] = {0x62, 0x01, pid};

    try {
        m_tx_char->writeValue(request, sizeof(request), false);
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGD(TAG, "Requested PID 0x%02X", pid);
        }
        return true;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Write failed: %s", e.what());
        return false;
    }
}

bool IcarBleDriver::has_pending_connection() {
    return g_pending_connection;
}

uint32_t IcarBleDriver::pending_connection_age_ms() {
    if (!g_pending_connection) {
        return 0;
    }
    uint32_t now = millis();
    return now - g_pending_connection_time;
}

const char* IcarBleDriver::get_device_address() {
    return m_device_address;
}

void IcarBleDriver::set_device_address(const char* address) {
    if (address) {
        strncpy(m_device_address, address, sizeof(m_device_address) - 1);
        m_device_address[sizeof(m_device_address) - 1] = '\0';
    }
}

bool IcarBleDriver::add_pid(uint8_t pid, uint32_t poll_interval_ms, const char* description) {
    // Check if PID already exists
    for (auto& pid_config : m_configured_pids) {
        if (pid_config.pid == pid) {
            // Update existing PID
            pid_config.poll_interval_ms = poll_interval_ms;
            pid_config.description = description;
            ESP_LOGI(TAG, "Updated PID 0x%02X polling interval to %u ms", pid, poll_interval_ms);
            return true;
        }
    }

    // Add new PID
    obd_pid_config_t new_pid = {
        .pid = pid,
        .poll_interval_ms = poll_interval_ms,
        .last_poll_ms = 0,
        .description = description
    };

    m_configured_pids.push_back(new_pid);
    ESP_LOGI(TAG, "Added PID 0x%02X (%s) with interval %u ms", pid, description, poll_interval_ms);
    return true;
}

void IcarBleDriver::remove_pid(uint8_t pid) {
    auto it = std::find_if(m_configured_pids.begin(), m_configured_pids.end(),
                          [pid](const obd_pid_config_t& config) { return config.pid == pid; });

    if (it != m_configured_pids.end()) {
        ESP_LOGI(TAG, "Removed PID 0x%02X", pid);
        m_configured_pids.erase(it);
    }
}

const std::vector<obd_pid_config_t>& IcarBleDriver::get_configured_pids() {
    return m_configured_pids;
}

void IcarBleDriver::clear_all_pids() {
    m_configured_pids.clear();
    ESP_LOGI(TAG, "Cleared all configured PIDs");
}

const char* IcarBleDriver::get_device_name() {
    return m_device_name;
}

const char* IcarBleDriver::get_vin() {
    return m_vin;
}

const char* IcarBleDriver::get_ecm_name() {
    return m_ecm_name;
}

void IcarBleDriver::request_vehicle_info() {
    if (!m_connected || !m_tx_char || !m_rx_char) {
        ESP_LOGE(TAG, "Cannot request vehicle info - not connected");
        return;
    }

    ESP_LOGI(TAG, "Requesting vehicle VIN and ECM name");

    // Initialize to empty
    m_vin[0] = '\0';
    m_ecm_name[0] = '\0';

    static EXT_RAM_ATTR char response[512];

    // Request VIN (Mode 09, PID 02)
    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Sending VIN request (09 02)");
    }
    if (send_obd_command("09 02\r", response, sizeof(response), 3000)) {
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGD(TAG, "VIN response: %s", response);
        }
        if (parse_vin_response(response, m_vin)) {
            ESP_LOGI(TAG, "VIN retrieved: %s", m_vin);
        } else {
            ESP_LOGE(TAG, "Failed to parse VIN");
            strncpy(m_vin, "N/A", sizeof(m_vin) - 1);
        }
    } else {
        ESP_LOGW(TAG, "No response for VIN request");
        strncpy(m_vin, "N/A", sizeof(m_vin) - 1);
    }

    // Small delay between requests
    delay(500);

    // Request ECM name (Mode 09, PID 0A)
    if (DebugFlags::ENABLE_OBD_DEBUG) {
        ESP_LOGD(TAG, "Sending ECM name request (09 0A)");
    }
    if (send_obd_command("09 0A\r", response, sizeof(response), 3000)) {
        if (DebugFlags::ENABLE_OBD_DEBUG) {
            ESP_LOGD(TAG, "ECM response: %s", response);
        }
        if (parse_ecm_response(response, m_ecm_name)) {
            ESP_LOGI(TAG, "ECM name retrieved: %s", m_ecm_name);
        } else {
            ESP_LOGE(TAG, "Failed to parse ECM name");
            strncpy(m_ecm_name, "N/A", sizeof(m_ecm_name) - 1);
        }
    } else {
        ESP_LOGW(TAG, "No response for ECM request");
        strncpy(m_ecm_name, "N/A", sizeof(m_ecm_name) - 1);
    }
}

bool IcarBleDriver::send_obd_command(const char* command, char* response, size_t max_len, uint32_t timeout_ms) {
    if (!m_tx_char || !m_rx_char || !command || !response) return false;
    
    // Clear response buffer
    response[0] = '\0';
    
    // Send command - cast to const uint8_t* and specify response expected
    size_t cmd_len = strlen(command);
    m_tx_char->writeValue((const uint8_t*)command, cmd_len, true);
    
    // Wait for response with timeout
    uint32_t start_time = millis();
    size_t response_len = 0;
    bool received_data = false;
    
    while ((millis() - start_time) < timeout_ms) {
        if (m_rx_char->canRead()) {
            std::string value = m_rx_char->readValue();
            if (value.length() > 0) {
                received_data = true;
                size_t copy_len = min(value.length(), max_len - response_len - 1);
                memcpy(response + response_len, value.c_str(), copy_len);
                response_len += copy_len;
                response[response_len] = '\0';
                
                // Check if we got a complete response (ends with '>' prompt or contains error)
                if (strstr(response, ">") || strstr(response, "NO DATA") || 
                    strstr(response, "ERROR") || strstr(response, "?")) {
                    break;
                }
            }
        }
        delay(50);
    }
    
    return received_data && response_len > 0;
}

bool IcarBleDriver::parse_vin_response(const char* response, char* vin) {
    if (!response || !vin) return false;
    
    // VIN response format: "49 02 01 XX XX XX..." where XX are hex ASCII codes
    // Response is split across multiple lines for long data
    // 49 = response to mode 09, 02 = PID 02 (VIN)
    
    // Look for "49 02" in response
    const char* data_start = strstr(response, "49 02");
    if (!data_start) {
        // Try alternate format without space
        data_start = strstr(response, "4902");
        if (!data_start) return false;
    }
    
    // Extract hex bytes and convert to ASCII
    char vin_buffer[18];
    int vin_idx = 0;
    const char* ptr = data_start;
    
    // Skip past "49 02" and frame counter
    while (*ptr && vin_idx < 17) {
        // Look for hex pairs (e.g., "31" = '1', "41" = 'A')
        while (*ptr && !isxdigit(*ptr)) ptr++;
        if (!*ptr) break;
        
        // Skip the mode/PID bytes (49, 02, frame counter)
        if (ptr == data_start || (ptr - data_start) < 10) {
            while (*ptr && (isxdigit(*ptr) || *ptr == ' ')) ptr++;
            continue;
        }
        
        // Read hex pair
        char hex_str[3] = {0};
        if (isxdigit(*ptr)) {
            hex_str[0] = *ptr++;
            if (isxdigit(*ptr)) {
                hex_str[1] = *ptr++;
                
                // Convert hex to char
                int char_code = strtol(hex_str, NULL, 16);
                if (char_code >= 32 && char_code < 127) {  // Printable ASCII
                    vin_buffer[vin_idx++] = (char)char_code;
                }
            }
        }
    }
    
    if (vin_idx >= 17) {
        memcpy(vin, vin_buffer, 17);
        vin[17] = '\0';
        return true;
    }
    
    return false;
}

bool IcarBleDriver::parse_ecm_response(const char* response, char* ecm_name) {
    if (!response || !ecm_name) return false;
    
    // ECM name response format: "49 0A ..." where ... are hex ASCII codes
    const char* data_start = strstr(response, "49 0A");
    if (!data_start) {
        data_start = strstr(response, "490A");
        if (!data_start) return false;
    }
    
    // Extract hex bytes and convert to ASCII
    char ecm_buffer[20];
    int ecm_idx = 0;
    const char* ptr = data_start;
    
    // Skip past "49 0A" and frame counter
    while (*ptr && ecm_idx < 19) {
        // Look for hex pairs
        while (*ptr && !isxdigit(*ptr)) ptr++;
        if (!*ptr) break;
        
        // Skip the mode/PID bytes
        if (ptr == data_start || (ptr - data_start) < 10) {
            while (*ptr && (isxdigit(*ptr) || *ptr == ' ')) ptr++;
            continue;
        }
        
        // Read hex pair
        char hex_str[3] = {0};
        if (isxdigit(*ptr)) {
            hex_str[0] = *ptr++;
            if (isxdigit(*ptr)) {
                hex_str[1] = *ptr++;
                
                // Convert hex to char
                int char_code = strtol(hex_str, NULL, 16);
                if (char_code >= 32 && char_code < 127) {  // Printable ASCII
                    ecm_buffer[ecm_idx++] = (char)char_code;
                }
            }
        }
    }
    
    if (ecm_idx > 0) {
        memcpy(ecm_name, ecm_buffer, ecm_idx);
        ecm_name[ecm_idx] = '\0';
        return true;
    }
    
    return false;
}

const std::vector<recent_ble_device_t>& IcarBleDriver::get_recent_devices() {
    // Clean up old entries (older than 30 seconds)
    uint32_t now = millis();
    auto it = m_recent_devices.begin();
    while (it != m_recent_devices.end()) {
        if (now - it->last_seen_ms > 30000) {  // 30 seconds
            it = m_recent_devices.erase(it);
        } else {
            ++it;
        }
    }
    return m_recent_devices;
}

void IcarBleDriver::clear_recent_devices() {
    m_recent_devices.clear();
}
