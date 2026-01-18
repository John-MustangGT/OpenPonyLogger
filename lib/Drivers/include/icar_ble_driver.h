#ifndef ICAR_BLE_DRIVER_H
#define ICAR_BLE_DRIVER_H

#include <NimBLEDevice.h>
#include "obd_data.h"
#include <vector>

/**
 * @brief OBD PID configuration with polling rate
 */
struct obd_pid_config_t {
    uint8_t pid;                  // Parameter ID (0x00-0xFF)
    uint32_t poll_interval_ms;    // How often to poll this PID (milliseconds)
    uint32_t last_poll_ms;        // Last time this PID was polled
    const char* description;      // Human-readable description
};

/**
 * @brief vgate iCar 2 Pro BLE Central interface
 * Connects to the BLE OBD-II scanner and reads OBD parameters
 * 
 * Device Information:
 * - Device Name: "vgate iCar2Pro" or similar (searches for "iCar" or "vgate")
 * - Typical MAC: AA:BB:CC:DD:EE:FF (random)
 * - BLE Standard: BLE 4.0/4.1
 * 
 * BLE Service/Characteristic UUIDs (reversed notation):
 * - Service UUID: 0xFFE0 (proprietary vgate service)
 * - RX Char: 0xFFE1 (read/notify from device) 
 * - TX Char: 0xFFE2 (write to device)
 * 
 * Connection Notes:
 * - Device advertises with local name containing "iCar" or "vgate"
 * - No authentication required (open connection)
 * - Connection is stable at 1-2 MTU (default 23 bytes)
 * - Requires 500ms+ between writes for reliable communication
 */
class IcarBleDriver {
public:
    /**
     * @brief Initialize the BLE central interface
     * @return true if initialization successful
     */
    static bool init();

    /**
     * @brief Start scanning for vgate iCar 2 Pro device
     * Automatically connects when device is found
     * @return true if device found and connected
     */
    static bool start_scan();

    /**
     * @brief Stop BLE scanning
     */
    static void stop_scan();

    /**
     * @brief Connect to a specific device by address
     * @param address BLE device address string (format: "AA:BB:CC:DD:EE:FF")
     * @return true if connection successful
     */
    static bool connect(const char* address);

    /**
     * @brief Disconnect from the device
     */
    static void disconnect();

    /**
     * @brief Check if connected to OBD device
     */
    static bool is_connected();

    /**
     * @brief Update OBD data by polling configured PIDs
     * Only polls PIDs whose interval has elapsed
     * @return true if any PID was updated
     */
    static bool update();

    /**
     * @brief Get latest OBD data
     */
    static obd_data_t get_data();

    /**
     * @brief Configure PID polling
     * @param pid Parameter ID
     * @param poll_interval_ms Polling interval in milliseconds
     * @param description Human-readable description
     * @return true if PID added/updated
     */
    static bool add_pid(uint8_t pid, uint32_t poll_interval_ms, const char* description);

    /**
     * @brief Remove a PID from polling list
     * @param pid Parameter ID to remove
     */
    static void remove_pid(uint8_t pid);

    /**
     * @brief Get all configured PIDs
     */
    static const std::vector<obd_pid_config_t>& get_configured_pids();

    /**
     * @brief Clear all configured PIDs
     */
    static void clear_all_pids();

    /**
     * @brief Request a specific PID from the device (manual query)
     * @param pid Parameter ID (0x01-0xFF for standard PIDs)
     * @return true if request sent
     */
    static bool request_pid(uint8_t pid);

    /**
     * @brief Get device address (for remembering last connection)
     */
    static const char* get_device_address();

    /**
     * @brief Set device address for auto-reconnection
     */
    static void set_device_address(const char* address);

    /**
     * @brief Get connected device name
     */
    static const char* get_device_name();

    /**
     * @brief Get vehicle VIN (Vehicle Identification Number)
     * Retrieves VIN from OBD-II Mode 09 PID 02
     */
    static const char* get_vin();

    /**
     * @brief Get ECU/ECM name
     * Retrieves ECU name from OBD-II Mode 09 PID 0A
     */
    static const char* get_ecm_name();

    /**
     * @brief Request VIN and ECM name from vehicle
     * Should be called once after connection established
     */
    static void request_vehicle_info();

private:
    static obd_data_t m_data;
    static bool m_connected;
    static char m_device_address[18];
    static char m_device_name[32];
    static char m_vin[18];
    static char m_ecm_name[20];
    static NimBLERemoteCharacteristic* m_rx_char;
    static NimBLERemoteCharacteristic* m_tx_char;
    static std::vector<obd_pid_config_t> m_configured_pids;
    
    /**
     * @brief Send OBD command and wait for response
     * @param command Command string to send (e.g., "09 02\r")
     * @param response Buffer to store response
     * @param max_len Maximum response buffer size
     * @param timeout_ms Timeout in milliseconds
     * @return true if response received
     */
    static bool send_obd_command(const char* command, char* response, size_t max_len, uint32_t timeout_ms = 2000);
    
    /**
     * @brief Parse VIN from Mode 09 PID 02 response
     * @param response Raw OBD response string
     * @param vin Output buffer for VIN (must be at least 18 bytes)
     * @return true if VIN parsed successfully
     */
    static bool parse_vin_response(const char* response, char* vin);
    
    /**
     * @brief Parse ECM name from Mode 09 PID 0A response
     * @param response Raw OBD response string
     * @param ecm_name Output buffer for ECM name (must be at least 20 bytes)
     * @return true if ECM name parsed successfully
     */
    static bool parse_ecm_response(const char* response, char* ecm_name);
};

#endif // ICAR_BLE_DRIVER_H
