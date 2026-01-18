#ifndef ICAR_BLE_WRAPPER_H
#define ICAR_BLE_WRAPPER_H

#include "sensor_hal.h"
#include "icar_ble_driver.h"

/**
 * @brief Wrapper around IcarBleDriver to implement IOBDSensor interface
 * 
 * The BLE stack initialization happens on Core 0 (required by NimBLE).
 * However, the sensor polling (update()) can be called from Core 1 RT logger
 * because IcarBleDriver uses static methods with thread-safe access.
 * 
 * Usage:
 * 1. Call IcarBleDriver::init() on Core 0 during setup
 * 2. Call IcarBleDriver::start_scan() on Core 0 to begin connection
 * 3. Create IcarBleWrapper instance and pass to SensorManager
 * 4. RT logger on Core 1 calls update() which checks is_connected() first
 */
class IcarBleWrapper : public IOBDSensor {
public:
    IcarBleWrapper();
    virtual ~IcarBleWrapper() = default;

    /**
     * @brief Initialize wrapper (BLE must already be initialized on Core 0)
     * Configures default PIDs for polling
     */
    bool init() override;

    /**
     * @brief Update OBD data by polling configured PIDs
     * Safe to call from any core. Only polls if BLE is connected.
     * @return true if data was successfully updated
     */
    bool update() override;

    /**
     * @brief Get latest OBD data
     */
    obd_data_t get_data() const override;

    /**
     * @brief Check if OBD data is valid
     * @return true if at least one PID has been successfully read
     */
    bool is_valid() const override;

    /**
     * @brief Check if BLE is connected to OBD device
     * @return true if connected to vgate iCar 2 Pro
     */
    bool is_connected() const override;

private:
    bool m_initialized;
    bool m_data_valid;
};

#endif // ICAR_BLE_WRAPPER_H
