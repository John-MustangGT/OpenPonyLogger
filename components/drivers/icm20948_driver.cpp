#include "icm20948_driver.h"

// ICM20948 Register Map
#define ICM20948_REG_WHO_AM_I        0x00
#define ICM20948_REG_ACCEL_XOUT_H    0x2D
#define ICM20948_REG_GYRO_XOUT_H     0x33
#define ICM20948_REG_MAG_XOUT_L      0x49
#define ICM20948_REG_PWR_MGMT_1      0x06
#define ICM20948_REG_ACCEL_CONFIG    0x14
#define ICM20948_REG_GYRO_CONFIG     0x13

// ICM20948 Expected WHO_AM_I value
#define ICM20948_WHO_AM_I_VALUE      0xEA

ICM20948Driver::ICM20948Driver(TwoWire& wire, uint8_t i2c_addr)
    : m_wire(wire), m_addr(i2c_addr),
      m_accel_valid(false), m_gyro_valid(false), m_compass_valid(false) {
    memset(&m_accel_data, 0, sizeof(m_accel_data));
    memset(&m_gyro_data, 0, sizeof(m_gyro_data));
    memset(&m_compass_data, 0, sizeof(m_compass_data));
}

ICM20948Driver::~ICM20948Driver() {
}

bool ICM20948Driver::init() {
    // Verify device is present
    uint8_t who_am_i = read_register(ICM20948_REG_WHO_AM_I);
    if (who_am_i != ICM20948_WHO_AM_I_VALUE) {
        return false;
    }
    
    // Wake up device
    write_register(ICM20948_REG_PWR_MGMT_1, 0x01);
    delay(100);
    
    // Configure sensors
    if (!configure_accel()) return false;
    if (!configure_gyro()) return false;
    if (!configure_compass()) return false;
    
    return true;
}

bool ICM20948Driver::update() {
    bool success = true;
    
    if (!read_accel_raw()) {
        m_accel_valid = false;
        success = false;
    } else {
        m_accel_valid = true;
    }
    
    if (!read_gyro_raw()) {
        m_gyro_valid = false;
        success = false;
    } else {
        m_gyro_valid = true;
    }
    
    if (!read_compass_raw()) {
        m_compass_valid = false;
        success = false;
    } else {
        m_compass_valid = true;
    }
    
    return success;
}

accel_data_t ICM20948Driver::get_data() const {
    return m_accel_data;
}

bool ICM20948Driver::is_valid() const {
    return m_accel_valid;
}

gyro_data_t ICM20948Driver::get_gyro_data() const {
    return m_gyro_data;
}

bool ICM20948Driver::get_gyro_is_valid() const {
    return m_gyro_valid;
}

compass_data_t ICM20948Driver::get_compass_data() const {
    return m_compass_data;
}

bool ICM20948Driver::get_compass_is_valid() const {
    return m_compass_valid;
}

bool ICM20948Driver::write_register(uint8_t reg, uint8_t value) {
    m_wire.beginTransmission(m_addr);
    m_wire.write(reg);
    m_wire.write(value);
    return m_wire.endTransmission() == 0;
}

uint8_t ICM20948Driver::read_register(uint8_t reg) {
    m_wire.beginTransmission(m_addr);
    m_wire.write(reg);
    m_wire.endTransmission();
    
    m_wire.requestFrom(m_addr, (uint8_t)1);
    if (m_wire.available()) {
        return m_wire.read();
    }
    return 0;
}

bool ICM20948Driver::read_registers(uint8_t reg, uint8_t* data, uint8_t len) {
    m_wire.beginTransmission(m_addr);
    m_wire.write(reg);
    m_wire.endTransmission();
    
    m_wire.requestFrom(m_addr, len);
    
    for (uint8_t i = 0; i < len; i++) {
        if (m_wire.available()) {
            data[i] = m_wire.read();
        } else {
            return false;
        }
    }
    return true;
}

bool ICM20948Driver::configure_accel() {
    // Configure accelerometer for ±16g range, 1.1kHz sample rate
    return write_register(ICM20948_REG_ACCEL_CONFIG, 0x10);
}

bool ICM20948Driver::configure_gyro() {
    // Configure gyroscope for ±2000dps range, 1.1kHz sample rate
    return write_register(ICM20948_REG_GYRO_CONFIG, 0x10);
}

bool ICM20948Driver::configure_compass() {
    // Compass configuration is typically handled by the AK09916 sub-module
    // This is a simplified version - full implementation would configure AK09916
    return true;
}

bool ICM20948Driver::read_accel_raw() {
    uint8_t data[6];
    if (!read_registers(ICM20948_REG_ACCEL_XOUT_H, data, 6)) {
        return false;
    }
    
    int16_t raw_x = (data[0] << 8) | data[1];
    int16_t raw_y = (data[2] << 8) | data[3];
    int16_t raw_z = (data[4] << 8) | data[5];
    
    convert_accel_data(raw_x, raw_y, raw_z);
    return true;
}

bool ICM20948Driver::read_gyro_raw() {
    uint8_t data[6];
    if (!read_registers(ICM20948_REG_GYRO_XOUT_H, data, 6)) {
        return false;
    }
    
    int16_t raw_x = (data[0] << 8) | data[1];
    int16_t raw_y = (data[2] << 8) | data[3];
    int16_t raw_z = (data[4] << 8) | data[5];
    
    convert_gyro_data(raw_x, raw_y, raw_z);
    return true;
}

bool ICM20948Driver::read_compass_raw() {
    uint8_t data[6];
    // Note: Magnetometer uses different register address
    if (!read_registers(ICM20948_REG_MAG_XOUT_L, data, 6)) {
        return false;
    }
    
    int16_t raw_x = (data[1] << 8) | data[0];
    int16_t raw_y = (data[3] << 8) | data[2];
    int16_t raw_z = (data[5] << 8) | data[4];
    
    convert_compass_data(raw_x, raw_y, raw_z);
    return true;
}

void ICM20948Driver::convert_accel_data(int16_t raw_x, int16_t raw_y, int16_t raw_z) {
    // Convert raw values to g (16g range)
    const float scale = 16.0f / 32768.0f;
    m_accel_data.x = raw_x * scale;
    m_accel_data.y = raw_y * scale;
    m_accel_data.z = raw_z * scale;
}

void ICM20948Driver::convert_gyro_data(int16_t raw_x, int16_t raw_y, int16_t raw_z) {
    // Convert raw values to dps (2000dps range)
    const float scale = 2000.0f / 32768.0f;
    m_gyro_data.x = raw_x * scale;
    m_gyro_data.y = raw_y * scale;
    m_gyro_data.z = raw_z * scale;
}

void ICM20948Driver::convert_compass_data(int16_t raw_x, int16_t raw_y, int16_t raw_z) {
    // Convert raw magnetometer values (simplified - typically in uT)
    const float scale = 1.0f / 256.0f;
    m_compass_data.x = raw_x * scale;
    m_compass_data.y = raw_y * scale;
    m_compass_data.z = raw_z * scale;
}
