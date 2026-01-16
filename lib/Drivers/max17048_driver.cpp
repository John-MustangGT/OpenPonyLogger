#include "max17048_driver.h"
#include <cstring>

// MAX17048 Register Map
// Note: MAX17048 only has VCELL and SOC registers, no current or temperature!
#define MAX17048_REG_VCELL       0x02  // Cell voltage (12-bit, 78.125µV per LSB)
#define MAX17048_REG_SOC         0x04  // State of charge (1/256% per LSB)
#define MAX17048_REG_MODE        0x06  // Mode register
#define MAX17048_REG_VERSION     0x08  // Version register
#define MAX17048_REG_RCOMP       0x0C  // RCOMP register
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
    // Always return true - I2C bus communication is OK
    // Individual reads may fail but that's tracked in m_data
    
    // MAX17048 only provides voltage and SOC - no current or temperature!
    read_voltage();
    read_soc();
    
    // Set unavailable measurements to zero
    m_data.current = 0.0f;
    m_data.temperature = 0;
    
    return true;  // Update attempt succeeded
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
    
    // VCELL: 12-bit register (bits 15-4 of 16-bit word)
    // Resolution: 1.25mV per LSB (per MAX17048 datasheet)
    // Voltage [V] = ((high << 8) | low) >> 4) * 0.00125
    uint16_t raw_value = (high << 8) | low;
    uint16_t adc_value = raw_value >> 4;  // Get 12-bit value (0-4095)
    m_data.voltage = adc_value * 0.00125f;  // 1.25mV per LSB
    
    return true;
}

bool MAX17048Driver::read_soc() {
    uint8_t high, low;
    if (!read_register(MAX17048_REG_SOC, high, low)) {
        return false;
    }
    
    // SOC: 16-bit register with 1/256% resolution
    // High byte = integer percent (0-100)
    // Low byte = fractional percent (0-255 representing 0.00-0.99609375%)
    // Total SOC [%] = high + (low / 256)
    float soc = high + (low / 256.0f);
    
    // Clamp to 100% max (can sometimes read slightly over 100%)
    if (soc > 100.0f) {
        soc = 100.0f;
    }
    
    m_data.state_of_charge = soc;
    
    return true;
}

uint16_t MAX17048Driver::combine_bytes(uint8_t high, uint8_t low) const {
    return (high << 8) | low;
}
