#include "icar_ble_driver.h"
#include <cstring>
#include <algorithm>

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

// vgate iCar 2 Pro BLE UUIDs
static const char* SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
static const char* RX_CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";  // Read/Notify from device
static const char* TX_CHAR_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb";  // Write to device

bool IcarBleDriver::init() {
    Serial.println("[OBD] Initializing NimBLE central...");
    
    // Initialize BLE device
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Max power for range
    
    Serial.println("[OBD] NimBLE initialized successfully");
    return true;
}

bool IcarBleDriver::start_scan() {
    Serial.println("[OBD] Starting BLE scan for iCar device...");
    
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (!scan) {
        Serial.println("[OBD] Failed to get scan instance");
        return false;
    }
    
    // Configure scan parameters
    scan->setInterval(97);   // Interval in 0.625ms units (~60ms)
    scan->setWindow(32);     // Window in 0.625ms units (~20ms)
    scan->setActiveScan(true);
    scan->setDuplicateFilter(true);
    
    // Start scan and get results
    scan->start(10, nullptr, false);
    
    return true;
}

void IcarBleDriver::stop_scan() {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan) {
        scan->stop();
    }
}

bool IcarBleDriver::connect(const char* address) {
    if (!address) return false;
    
    Serial.printf("[OBD] Attempting to connect to %s\n", address);
    
    // Create client
    NimBLEClient* pClient = NimBLEDevice::createClient();
    if (!pClient) {
        Serial.println("[OBD] Failed to create client");
        return false;
    }
    
    // Connect to address
    NimBLEAddress addr(address);
    bool connected = pClient->connect(addr);
    
    if (!connected) {
        Serial.println("[OBD] Failed to connect to remote device");
        NimBLEDevice::deleteClient(pClient);
        return false;
    }
    
    // Get the service
    NimBLERemoteService* pRemoteSvc = pClient->getService(SERVICE_UUID);
    if (!pRemoteSvc) {
        Serial.println("[OBD] Service not found, disconnecting");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }
    
    // Get RX characteristic (read/notify)
    m_rx_char = pRemoteSvc->getCharacteristic(RX_CHAR_UUID);
    if (!m_rx_char) {
        Serial.println("[OBD] RX characteristic not found");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }
    
    // Get TX characteristic (write)
    m_tx_char = pRemoteSvc->getCharacteristic(TX_CHAR_UUID);
    if (!m_tx_char) {
        Serial.println("[OBD] TX characteristic not found");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }
    
    // Subscribe to notifications if available
    if (m_rx_char->canNotify()) {
        // Use lambda function instead of callback class
        m_rx_char->subscribe(true, [](NimBLERemoteCharacteristic* pChar, 
                                      uint8_t* pData, size_t length, bool isNotify) {
            if (length > 0) {
                Serial.printf("[OBD] Received %d bytes: ", length);
                for (int i = 0; i < length && i < 16; i++) {
                    Serial.printf("%02X ", pData[i]);
                }
                Serial.println();
            }
        });
        Serial.println("[OBD] Subscribed to RX notifications");
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
    
    Serial.printf("[OBD] Connected to %s successfully!\n", m_device_name);
    
    // Request VIN and ECM name once connected
    request_vehicle_info();
    
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
    Serial.println("[OBD] Disconnected from device");
}

bool IcarBleDriver::is_connected() {
    return m_connected;
}

bool IcarBleDriver::update() {
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
        Serial.println("[OBD] Not connected, cannot request PID");
        return false;
    }
    
    // Construct OBD-II request: 62 01 <PID>
    // 62 = Service 01 (read data by identifier)
    // 01 = PID mode
    uint8_t request[3] = {0x62, 0x01, pid};
    
    try {
        m_tx_char->writeValue(request, sizeof(request), false);
        Serial.printf("[OBD] Requested PID 0x%02X\n", pid);
        return true;
    } catch (const std::exception& e) {
        Serial.printf("[OBD] Write failed: %s\n", e.what());
        return false;
    }
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
            Serial.printf("[OBD] Updated PID 0x%02X polling interval to %lu ms\n", pid, poll_interval_ms);
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
    Serial.printf("[OBD] Added PID 0x%02X (%s) with interval %lu ms\n", pid, description, poll_interval_ms);
    return true;
}

void IcarBleDriver::remove_pid(uint8_t pid) {
    auto it = std::find_if(m_configured_pids.begin(), m_configured_pids.end(),
                          [pid](const obd_pid_config_t& config) { return config.pid == pid; });
    
    if (it != m_configured_pids.end()) {
        Serial.printf("[OBD] Removed PID 0x%02X\n", pid);
        m_configured_pids.erase(it);
    }
}

const std::vector<obd_pid_config_t>& IcarBleDriver::get_configured_pids() {
    return m_configured_pids;
}

void IcarBleDriver::clear_all_pids() {
    m_configured_pids.clear();
    Serial.println("[OBD] Cleared all configured PIDs");
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
        Serial.println("[OBD] Cannot request vehicle info - not connected");
        return;
    }
    
    Serial.println("[OBD] Requesting vehicle VIN and ECM name...");
    
    // Initialize to empty
    m_vin[0] = '\0';
    m_ecm_name[0] = '\0';
    
    static EXT_RAM_BSS_ATTR char response[512];
    
    // Request VIN (Mode 09, PID 02)
    Serial.println("[OBD] Sending VIN request (09 02)...");
    if (send_obd_command("09 02\r", response, sizeof(response), 3000)) {
        Serial.printf("[OBD] VIN response: %s\n", response);
        if (parse_vin_response(response, m_vin)) {
            Serial.printf("[OBD] VIN retrieved: %s\n", m_vin);
        } else {
            Serial.println("[OBD] Failed to parse VIN");
            strncpy(m_vin, "N/A", sizeof(m_vin) - 1);
        }
    } else {
        Serial.println("[OBD] No response for VIN request");
        strncpy(m_vin, "N/A", sizeof(m_vin) - 1);
    }
    
    // Small delay between requests
    delay(500);
    
    // Request ECM name (Mode 09, PID 0A)
    Serial.println("[OBD] Sending ECM name request (09 0A)...");
    if (send_obd_command("09 0A\r", response, sizeof(response), 3000)) {
        Serial.printf("[OBD] ECM response: %s\n", response);
        if (parse_ecm_response(response, m_ecm_name)) {
            Serial.printf("[OBD] ECM name retrieved: %s\n", m_ecm_name);
        } else {
            Serial.println("[OBD] Failed to parse ECM name");
            strncpy(m_ecm_name, "N/A", sizeof(m_ecm_name) - 1);
        }
    } else {
        Serial.println("[OBD] No response for ECM request");
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
