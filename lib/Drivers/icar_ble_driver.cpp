#include "icar_ble_driver.h"
#include <cstring>
#include <algorithm>

// Static member initialization
obd_data_t IcarBleDriver::m_data = {};
bool IcarBleDriver::m_connected = false;
char IcarBleDriver::m_device_address[18] = "";
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
    
    m_connected = true;
    m_data.connected = true;
    m_data.last_update_ms = millis();
    
    Serial.println("[OBD] Connected to iCar device successfully!");
    return true;
}

void IcarBleDriver::disconnect() {
    m_connected = false;
    m_data.connected = false;
    m_rx_char = nullptr;
    m_tx_char = nullptr;
    
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
