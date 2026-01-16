#ifndef MAX17048_BATTERY_MONITOR_H
#define MAX17048_BATTERY_MONITOR_H

#include "sensor_hal.h"
#include <Wire.h>

/**
 * @brief MAX17048 Battery Fuel Gauge Driver
 * Implements IBatterySensor interface for the MAX17048 I2C fuel gauge
 * Provides voltage, state of charge, current, and temperature monitoring
 */
class MAX17048Driver : public IBatterySensor {
public:
    /**
     * @brief Constructor
     * @param wire I2C bus reference
     * @param i2c_addr I2C address (default 0x36 for MAX17048)
     */
    MAX17048Driver(TwoWire& wire, uint8_t i2c_addr = 0x36);
    
    ~MAX17048Driver();
    
    // IBatterySensor implementation
    bool init() override;
    bool update() override;
    battery_data_t get_data() const override;
    bool is_valid() const override;

private:
    TwoWire& m_wire;
    uint8_t m_i2c_addr;
    
    battery_data_t m_data;
    bool m_valid;
    
    // Low-level I2C operations
    bool write_register(uint8_t reg, uint8_t high_byte, uint8_t low_byte);
    bool read_register(uint8_t reg, uint8_t& high_byte, uint8_t& low_byte);
    bool read_registers(uint8_t reg, uint8_t* data, uint8_t len);
    
    // Device-specific functions
    bool read_voltage();
    bool read_soc();
    
    // Helper functions
    uint16_t combine_bytes(uint8_t high, uint8_t low) const;
};

#endif // MAX17048_BATTERY_MONITOR_H
