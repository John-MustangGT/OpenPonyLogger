#include "icm20948_driver.h"
#include "../Logger/include/debug_flags.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>

static const char* TAG = "IMU";
// ICM20948 uses register banks - must select bank before accessing registers
#define ICM20948_REG_BANK_SEL        0x7F  // Bank 0-3 selection register

// Bank 0 registers
#define ICM20948_REG_WHO_AM_I        0x00  // Bank 0
#define ICM20948_REG_USER_CTRL       0x03  // Bank 0 - I2C master enable
#define ICM20948_REG_PWR_MGMT_1      0x06  // Bank 0
#define ICM20948_REG_TEMP_OUT_H      0x39  // Bank 0 (temperature)
#define ICM20948_REG_TEMP_OUT_L      0x3A  // Bank 0 (temperature)
#define ICM20948_REG_ACCEL_XOUT_H    0x2D  // Bank 0
#define ICM20948_REG_GYRO_XOUT_H     0x33  // Bank 0
#define ICM20948_REG_EXT_SLV_SENS_DATA_00 0x3B  // Bank 0 - External sensor data
#define ICM20948_REG_MAG_XOUT_L      0x49  // Bank 0 (mag data - alias for backwards compat)

// Bank 2 registers (sensor configuration)
#define ICM20948_REG_GYRO_SMPLRT_DIV 0x00  // Bank 2
#define ICM20948_REG_GYRO_CONFIG_1   0x01  // Bank 2
#define ICM20948_REG_ACCEL_SMPLRT_DIV_1 0x10  // Bank 2
#define ICM20948_REG_ACCEL_SMPLRT_DIV_2 0x11  // Bank 2
#define ICM20948_REG_ACCEL_CONFIG    0x14  // Bank 2

// Bank 3 registers (I2C master configuration)
#define ICM20948_REG_I2C_MST_CTRL    0x01  // Bank 3 - I2C master control
#define ICM20948_REG_I2C_SLV0_ADDR   0x03  // Bank 3 - I2C slave 0 address
#define ICM20948_REG_I2C_SLV0_REG    0x04  // Bank 3 - I2C slave 0 register
#define ICM20948_REG_I2C_SLV0_CTRL   0x05  // Bank 3 - I2C slave 0 control
#define ICM20948_REG_I2C_SLV0_DO     0x06  // Bank 3 - I2C slave 0 data out

// AK09916 magnetometer registers (accessed via I2C master)
#define AK09916_I2C_ADDR             0x0C  // AK09916 I2C address
#define AK09916_REG_WIA2             0x01  // Who Am I register (should be 0x09)
#define AK09916_REG_ST1              0x10  // Status 1
#define AK09916_REG_HXL              0x11  // X-axis data low byte
#define AK09916_REG_HXH              0x12  // X-axis data high byte
#define AK09916_REG_HYL              0x13  // Y-axis data low byte
#define AK09916_REG_HYH              0x14  // Y-axis data high byte
#define AK09916_REG_HZL              0x15  // Z-axis data low byte
#define AK09916_REG_HZH              0x16  // Z-axis data high byte
#define AK09916_REG_ST2              0x18  // Status 2
#define AK09916_REG_CNTL2            0x31  // Control 2
#define AK09916_REG_CNTL3            0x32  // Control 3

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
    ESP_LOGI(TAG, "%s", success ? "Gyro config OK (±250dps)" : "Gyro config FAILED");
    
    // Return to Bank 0 for normal data reading
    write_register(ICM20948_REG_BANK_SEL, 0x00);
    delay(10);
    
    return success;
}

bool ICM20948Driver::configure_compass() {
    // The ICM20948's internal magnetometer (AK09916) requires I2C master configuration
    ESP_LOGI(TAG, "Configuring AK09916 magnetometer...");

    // Step 1: Enable I2C master mode (Bank 0)
    write_register(ICM20948_REG_BANK_SEL, 0x00);  // Select Bank 0
    delay(10);
    write_register(ICM20948_REG_USER_CTRL, 0x20);  // Enable I2C master mode
    delay(10);

    // Step 2: Configure I2C master (Bank 3)
    write_register(ICM20948_REG_BANK_SEL, 0x30);  // Select Bank 3
    delay(10);
    write_register(ICM20948_REG_I2C_MST_CTRL, 0x07);  // I2C master clock = 345.6 kHz
    delay(10);

    // Step 3: Reset AK09916 (write 0x01 to CNTL3)
    // Configure slave 0 to write reset command
    write_register(ICM20948_REG_I2C_SLV0_ADDR, AK09916_I2C_ADDR);  // AK09916 address (write mode)
    write_register(ICM20948_REG_I2C_SLV0_REG, AK09916_REG_CNTL3);  // CNTL3 register
    write_register(ICM20948_REG_I2C_SLV0_DO, 0x01);  // Reset command
    write_register(ICM20948_REG_I2C_SLV0_CTRL, 0x81);  // Enable, 1 byte
    delay(100);  // Wait for reset to complete

    // Step 4: Set AK09916 to continuous measurement mode 4 (100Hz)
    // Configure slave 0 to write continuous mode command
    write_register(ICM20948_REG_I2C_SLV0_ADDR, AK09916_I2C_ADDR);  // AK09916 address (write mode)
    write_register(ICM20948_REG_I2C_SLV0_REG, AK09916_REG_CNTL2);  // CNTL2 register
    write_register(ICM20948_REG_I2C_SLV0_DO, 0x08);  // Continuous mode 4 (100Hz, 16-bit)
    write_register(ICM20948_REG_I2C_SLV0_CTRL, 0x81);  // Enable, 1 byte
    delay(10);

    // Step 5: Configure slave 0 to read magnetometer data (8 bytes: ST1 + HXL/H + HYL/H + HZL/H + ST2)
    write_register(ICM20948_REG_I2C_SLV0_ADDR, AK09916_I2C_ADDR | 0x80);  // AK09916 address with read bit
    write_register(ICM20948_REG_I2C_SLV0_REG, AK09916_REG_ST1);  // Start reading from ST1
    write_register(ICM20948_REG_I2C_SLV0_CTRL, 0x88);  // Enable, 8 bytes
    delay(10);

    // Return to Bank 0 for normal data reading
    write_register(ICM20948_REG_BANK_SEL, 0x00);
    delay(10);

    ESP_LOGI(TAG, "✓ AK09916 magnetometer configured (100Hz continuous mode)");
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
    // Read magnetometer data from external sensor data registers
    // Data format: ST1(1) + HXL(1) + HXH(1) + HYL(1) + HYH(1) + HZL(1) + HZH(1) + ST2(1) = 8 bytes
    uint8_t data[8];

    if (!read_registers(ICM20948_REG_EXT_SLV_SENS_DATA_00, data, 8)) {
        ESP_LOGW(TAG, "Failed to read magnetometer data");
        return false;
    }

    // Check ST1 data ready bit (bit 0)
    if ((data[0] & 0x01) == 0) {
        // Data not ready
        return false;
    }

    // Check ST2 overflow bit (bit 3) - if set, data is invalid
    if (data[7] & 0x08) {
        ESP_LOGW(TAG, "Magnetometer overflow detected");
        return false;
    }

    // Extract 16-bit signed values (little-endian)
    int16_t raw_x = (int16_t)((data[2] << 8) | data[1]);  // HXH, HXL
    int16_t raw_y = (int16_t)((data[4] << 8) | data[3]);  // HYH, HYL
    int16_t raw_z = (int16_t)((data[6] << 8) | data[5]);  // HZH, HZL

    convert_compass_data(raw_x, raw_y, raw_z);
    return true;
}

void ICM20948Driver::convert_accel_data(int16_t raw_x, int16_t raw_y, int16_t raw_z) {
    // Debug: Print raw values occasionally
    if (DebugFlags::ENABLE_IMU_DEBUG) {
        static uint32_t last_debug = 0;
        uint32_t now = millis();
        if (now - last_debug > 5000) {
            ESP_LOGI(TAG, "Accel RAW: X=%d Y=%d Z=%d", raw_x, raw_y, raw_z);
            last_debug = now;
        }
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
    if (DebugFlags::ENABLE_IMU_DEBUG) {
        static uint32_t last_debug = 0;
        uint32_t now = millis();
        if (now - last_debug > 5000) {
            Serial.printf("Gyro RAW: X=%d Y=%d Z=%d\n", raw_x, raw_y, raw_z);
            last_debug = now;
        }
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
    if (DebugFlags::ENABLE_IMU_DEBUG) {
        static uint32_t last_debug = 0;
        uint32_t now = millis();
        if (now - last_debug > 5000) {
            ESP_LOGI(TAG, "Compass RAW: X=%d Y=%d Z=%d", raw_x, raw_y, raw_z);
            last_debug = now;
        }
    }

    // Convert raw magnetometer values to microTeslas (µT)
    // AK09916 sensitivity: 0.15 µT/LSB in 16-bit mode
    const float scale = 0.15f;
    m_compass_data.x = raw_x * scale;
    m_compass_data.y = raw_y * scale;
    m_compass_data.z = raw_z * scale;

    // Debug: Print converted values and calculated heading
    if (DebugFlags::ENABLE_IMU_DEBUG) {
        static uint32_t last_debug = 0;
        uint32_t now = millis();
        if (now - last_debug > 5000) {
            // Calculate heading for debugging (0-360 degrees)
            // Assuming X points forward, Y points right (adjust based on mounting)
            float heading = atan2(m_compass_data.y, m_compass_data.x) * 180.0f / M_PI;
            if (heading < 0) heading += 360.0f;
            ESP_LOGI(TAG, "Compass: X=%.2f Y=%.2f Z=%.2f µT, Heading=%.1f°",
                     m_compass_data.x, m_compass_data.y, m_compass_data.z, heading);
            last_debug = now;
        }
    }
}
