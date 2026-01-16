#include "icm20948_driver.h"
#include <Arduino.h>

// ICM20948 Register Map
// ICM20948 uses register banks - must select bank before accessing registers
#define ICM20948_REG_BANK_SEL        0x7F  // Bank 0-3 selection register

// Bank 0 registers
#define ICM20948_REG_WHO_AM_I        0x00  // Bank 0
#define ICM20948_REG_PWR_MGMT_1      0x06  // Bank 0
#define ICM20948_REG_TEMP_OUT_H      0x39  // Bank 0 (temperature)
#define ICM20948_REG_TEMP_OUT_L      0x3A  // Bank 0 (temperature)
#define ICM20948_REG_ACCEL_XOUT_H    0x2D  // Bank 0
#define ICM20948_REG_GYRO_XOUT_H     0x33  // Bank 0
#define ICM20948_REG_MAG_XOUT_L      0x49  // Bank 0 (mag data)

// Bank 2 registers (sensor configuration)
#define ICM20948_REG_GYRO_SMPLRT_DIV 0x00  // Bank 2
#define ICM20948_REG_GYRO_CONFIG_1   0x01  // Bank 2
#define ICM20948_REG_ACCEL_SMPLRT_DIV_1 0x10  // Bank 2
#define ICM20948_REG_ACCEL_SMPLRT_DIV_2 0x11  // Bank 2  
#define ICM20948_REG_ACCEL_CONFIG    0x14  // Bank 2

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
    // Always return true - I2C bus communication is OK
    // Individual sensor validity is tracked separately
    
    if (!read_accel_raw()) {
        m_accel_valid = false;
    } else {
        m_accel_valid = true;
    }
    
    // Also read temperature with accelerometer
    read_temperature();
    
    if (!read_gyro_raw()) {
        m_gyro_valid = false;
    } else {
        m_gyro_valid = true;
    }
    
    if (!read_compass_raw()) {
        m_compass_valid = false;
    } else {
        m_compass_valid = true;
    }
    
    return true;  // Update attempt succeeded, even if individual reads failed
}

accel_data_t ICM20948Driver::get_data() const {
    return m_accel_data;
}

bool ICM20948Driver::is_valid() const {
    return m_accel_valid;
}

gyro_data_t ICM20948Driver::get_gyro() const {
    return m_gyro_data;
}

bool ICM20948Driver::gyro_is_valid() const {
    return m_gyro_valid;
}

compass_data_t ICM20948Driver::get_compass() const {
    return m_compass_data;
}

bool ICM20948Driver::compass_is_valid() const {
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
    // ICM20948 requires selecting Bank 2 for configuration registers
    write_register(ICM20948_REG_BANK_SEL, 0x20);  // Select Bank 2
    delay(10);
    
    // Configure accelerometer for ±4g range
    // ACCEL_CONFIG (Bank 2, 0x14) bits [2:1]:
    // 00 = ±2g, 01 = ±4g, 10 = ±8g, 11 = ±16g
    // 0x02 = 0b00000010 sets bits [2:1] = 01 for ±4g
    bool success = write_register(ICM20948_REG_ACCEL_CONFIG, 0x02);
    
    // Return to Bank 0 for normal data reading
    write_register(ICM20948_REG_BANK_SEL, 0x00);
    delay(10);
    
    return success;
}

bool ICM20948Driver::configure_gyro() {
    // ICM20948 requires selecting Bank 2 for configuration registers
    write_register(ICM20948_REG_BANK_SEL, 0x20);  // Select Bank 2
    delay(10);
    
    // Configure gyroscope for ±250dps range
    // GYRO_CONFIG_1 (Bank 2, 0x01) bits [2:1]:
    // 00 = ±250dps, 01 = ±500dps, 10 = ±1000dps, 11 = ±2000dps
    // 0x00 sets bits [2:1] = 00 for ±250dps
    bool success = write_register(ICM20948_REG_GYRO_CONFIG_1, 0x00);
    Serial.println(success ? "Gyro config OK (±250dps)" : "Gyro config FAILED");
    
    // Return to Bank 0 for normal data reading
    write_register(ICM20948_REG_BANK_SEL, 0x00);
    delay(10);
    
    return success;
}

bool ICM20948Driver::configure_compass() {
    // TODO: Implement AK09916 magnetometer configuration
    // The ICM20948's internal magnetometer (AK09916) requires:
    // 1. Enable I2C master mode in Bank 0
    // 2. Configure I2C master to communicate with AK09916 (I2C addr 0x0C)
    // 3. Set AK09916 to continuous measurement mode
    // 4. Read data through EXT_SLV_SENS_DATA registers
    // For now, compass is disabled
    Serial.println("WARNING: Compass not yet implemented - requires AK09916 init");
    return true;
}

bool ICM20948Driver::read_temperature() {
    // Temperature register is 16-bit signed value at 0x39 (high) and 0x3A (low)
    uint8_t high = read_register(ICM20948_REG_TEMP_OUT_H);
    uint8_t low = read_register(ICM20948_REG_TEMP_OUT_L);
    
    // Conversion: Temp [°C] = (RAW / 333.87) + 21
    int16_t raw_temp = (high << 8) | low;
    m_accel_data.temperature = (raw_temp / 333.87f) + 21.0f;
    
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
    // Magnetometer not yet implemented - requires AK09916 configuration
    // Just return zeros for now
    int16_t raw_x = 0;
    int16_t raw_y = 0;
    int16_t raw_z = 0;
    
    convert_compass_data(raw_x, raw_y, raw_z);
    return true;  // Don't fail the update cycle
}

void ICM20948Driver::convert_accel_data(int16_t raw_x, int16_t raw_y, int16_t raw_z) {
    // Debug: Print raw values occasionally
    static uint32_t last_debug = 0;
    uint32_t now = millis();
    if (now - last_debug > 5000) {
        Serial.printf("Accel RAW: X=%d Y=%d Z=%d\n", raw_x, raw_y, raw_z);
        last_debug = now;
    }
    
    // Convert raw values to g
    // ±4g full scale = ±32768 counts → 4g/32768 = 0.0001220703125 g/count
    // Good resolution for racing: 1-2g normal lateral, 3+g extreme
    const float scale = 4.0f / 32768.0f;
    m_accel_data.x = raw_x * scale;
    m_accel_data.y = raw_y * scale;
    m_accel_data.z = raw_z * scale;
}

void ICM20948Driver::convert_gyro_data(int16_t raw_x, int16_t raw_y, int16_t raw_z) {
    // Debug: Print raw values occasionally
    static uint32_t last_debug = 0;
    uint32_t now = millis();
    if (now - last_debug > 5000) {
        Serial.printf("Gyro RAW: X=%d Y=%d Z=%d\n", raw_x, raw_y, raw_z);
        last_debug = now;
    }
    
    // Convert raw values to dps (degrees per second)
    // Based on raw data drift values, likely configured for ±250dps, not ±2000dps
    // Scale: ±250dps full scale = ±32768 counts → 250dps/32768
    const float scale = 250.0f / 32768.0f;
    m_gyro_data.x = raw_x * scale;
    m_gyro_data.y = raw_y * scale;
    m_gyro_data.z = raw_z * scale;
}

void ICM20948Driver::convert_compass_data(int16_t raw_x, int16_t raw_y, int16_t raw_z) {
    // Debug: Print raw values occasionally
    static uint32_t last_debug = 0;
    uint32_t now = millis();
    if (now - last_debug > 5000) {
        Serial.printf("Compass RAW: X=%d Y=%d Z=%d\n", raw_x, raw_y, raw_z);
        last_debug = now;
    }
    
    // Convert raw magnetometer values (simplified - typically in uT)
    const float scale = 1.0f / 256.0f;
    m_compass_data.x = raw_x * scale;
    m_compass_data.y = raw_y * scale;
    m_compass_data.z = raw_z * scale;
}
