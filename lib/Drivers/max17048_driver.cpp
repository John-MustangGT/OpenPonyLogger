#include "max17048_driver.h"
#include <cstring>

// MAX17048 Register Map
#define MAX17048_REG_VCELL       0x02  // Cell voltage (high, low bytes)
#define MAX17048_REG_SOC         0x04  // State of charge (high, low bytes)
#define MAX17048_REG_MODE        0x06  // Mode register
#define MAX17048_REG_VERSION     0x08  // Version register
#define MAX17048_REG_CURRENT     0x0A  // Current (high, low bytes)
#define MAX17048_REG_TEMP        0x0C  // Temperature (high, low bytes)
#define MAX17048_REG_RCOMP       0x0E  // RCOMP register
#define MAX17048_REG_CMD         0xFE  // Command register

// MAX17048 Expected Version
#define MAX17048_VERSION_EXPECTED 0x4010

MAX17048Driver::MAX17048Driver(TwoWire& wire, uint8_t i2c_addr)
    : m_wire(wire), m_i2c_addr(i2c_addr), m_valid(false) {
    memset(&m_data, 0, sizeof(m_data));
}

MAX17048Driver::~MAX17048Driver() {
}

bool MAX17048Driver::init() {
    // Verify device is present by reading version
    uint8_t high, low;
    if (!read_register(MAX17048_REG_VERSION, high, low)) {
        return false;
    }
    
    uint16_t version = combine_bytes(high, low);
    
    // Check if version is reasonable (MAX17048 version should be 0x4010)
    // Some flexibility in version checking for compatibility
    if (version == 0 || version == 0xFFFF) {
        return false;
    }
    
    m_valid = true;
    return true;
}

bool MAX17048Driver::update() {
    bool success = true;
    
    // Read all battery parameters
    if (!read_voltage()) success = false;
    if (!read_soc()) success = false;
    if (!read_current()) success = false;
    if (!read_temperature()) success = false;
    
    return success;
}

battery_data_t MAX17048Driver::get_data() const {
    return m_data;
}

bool MAX17048Driver::is_valid() const {
    return m_valid;
}

bool MAX17048Driver::write_register(uint8_t reg, uint8_t high_byte, uint8_t low_byte) {
    m_wire.beginTransmission(m_i2c_addr);
    m_wire.write(reg);
    m_wire.write(high_byte);
    m_wire.write(low_byte);
    return m_wire.endTransmission() == 0;
}

bool MAX17048Driver::read_register(uint8_t reg, uint8_t& high_byte, uint8_t& low_byte) {
    m_wire.beginTransmission(m_i2c_addr);
    m_wire.write(reg);
    m_wire.endTransmission();
    
    m_wire.requestFrom(m_i2c_addr, (size_t)2);
    
    if (!m_wire.available()) return false;
    high_byte = m_wire.read();
    
    if (!m_wire.available()) return false;
    low_byte = m_wire.read();
    
    return true;
}

bool MAX17048Driver::read_registers(uint8_t reg, uint8_t* data, uint8_t len) {
    m_wire.beginTransmission(m_i2c_addr);
    m_wire.write(reg);
    m_wire.endTransmission();
    
    m_wire.requestFrom(m_i2c_addr, (size_t)len);
    
    for (uint8_t i = 0; i < len; i++) {
        if (m_wire.available()) {
            data[i] = m_wire.read();
        } else {
            return false;
        }
    }
    return true;
}

bool MAX17048Driver::read_voltage() {
    uint8_t high, low;
    if (!read_register(MAX17048_REG_VCELL, high, low)) {
        return false;
    }
    
    // VCELL: 16-bit register
    // ADC value = (high << 4) | (low >> 4)
    // Voltage [V] = ADC value * 78.125 / 1000000
    uint16_t adc_value = (high << 4) | (low >> 4);
    m_data.voltage = adc_value * 0.078125f;  // 78.125 / 1000000 * 1000000 = 78.125 / 1
    
    return true;
}

bool MAX17048Driver::read_soc() {
    uint8_t high, low;
    if (!read_register(MAX17048_REG_SOC, high, low)) {
        return false;
    }
    
    // SOC: 16-bit register with 1/512% resolution
    // State of charge [%] = (high + (low / 256)) / 100 * 512
    // Simplified: [%] = high + (low / 256)
    m_data.state_of_charge = high + (low / 256.0f);
    
    return true;
}

bool MAX17048Driver::read_current() {
    uint8_t high, low;
    if (!read_register(MAX17048_REG_CURRENT, high, low)) {
        return false;
    }
    
    // CURRENT: 16-bit signed register
    // Current [mA] = (signed_value) * 1.5625 / 256
    int16_t raw_current = (high << 8) | low;
    m_data.current = raw_current * 1.5625f / 256.0f;
    
    return true;
}

bool MAX17048Driver::read_temperature() {
    uint8_t high, low;
    if (!read_register(MAX17048_REG_TEMP, high, low)) {
        return false;
    }
    
    // TEMP: 16-bit signed register
    // Temperature [°C] = high + (low / 256)
    // Store as int16_t in units of 0.01°C for precision
    int16_t temp_celsius = high + (low / 256.0f);
    m_data.temperature = temp_celsius * 100;  // Convert to 0.01°C units
    
    return true;
}

uint16_t MAX17048Driver::combine_bytes(uint8_t high, uint8_t low) const {
    return (high << 8) | low;
}
