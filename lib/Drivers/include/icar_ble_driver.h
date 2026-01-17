#ifndef ICAR_BLE_DRIVER_H
#define ICAR_BLE_DRIVER_H

#include <NimBLEDevice.h>
#include "obd_data.h"

/**
 * @brief vgate iCar 2 Pro BLE Central interface
 * Connects to the BLE OBD-II scanner and reads OBD parameters
 * 
 * BLE Service UUIDs for vgate iCar 2 Pro:
 * - Device Name: "vgate iCar2Pro" (typically)
 * - Service UUID: 0xFFE0
 * - RX Characteristic: 0xFFE1 (read/notify)
 * - TX Characteristic: 0xFFE2 (write)
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
     * @return true if scan started successfully
     */
    static bool start_scan();

    /**
     * @brief Stop BLE scanning
     */
    static void stop_scan();

    /**
     * @brief Connect to a specific device by address
     * @param address BLE device address string
     * @return true if connection attempt made
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
     * @brief Update OBD data by querying device
     * Queries engine RPM, speed, temperature, etc.
     * @return true if update successful
     */
    static bool update();

    /**
     * @brief Get latest OBD data
     */
    static obd_data_t get_data();

    /**
     * @brief Request a specific PID from the device
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

private:
    static obd_data_t m_data;
    static bool m_connected;
    static char m_device_address[18];
    static NimBLERemoteCharacteristic* m_rx_char;
    static NimBLERemoteCharacteristic* m_tx_char;
};

#endif // ICAR_BLE_DRIVER_H
