#ifndef ICM20948_DRIVER_H
#define ICM20948_DRIVER_H

#include "sensor_hal.h"
#include <Wire.h>

/**
 * @brief ICM20948 9-DOF IMU Driver
 * Implements IAccelSensor, IGyroSensor, and ICompassSensor interfaces
 * The ICM20948 contains accelerometer, gyroscope, and magnetometer
 */
class ICM20948Driver : public IAccelSensor, public IGyroSensor, public ICompassSensor {
public:
    /**
     * @brief Constructor
     * @param wire I2C bus reference
     * @param i2c_addr I2C address (default 0x68)
     */
    ICM20948Driver(TwoWire& wire, uint8_t i2c_addr = 0x68);
    
    ~ICM20948Driver();
    
    // IAccelSensor implementation
    bool init() override;
    bool update() override;
    accel_data_t get_data() const override;
    bool is_valid() const override;
    
    // IGyroSensor implementation
    gyro_data_t get_gyro_data() const;
    bool get_gyro_is_valid() const;
    
    // ICompassSensor implementation
    compass_data_t get_compass_data() const;
    bool get_compass_is_valid() const;

private:
    TwoWire& m_wire;
    uint8_t m_addr;
    
    accel_data_t m_accel_data;
    gyro_data_t m_gyro_data;
    compass_data_t m_compass_data;
    
    bool m_accel_valid;
    bool m_gyro_valid;
    bool m_compass_valid;
    
    // Low-level I2C operations
    bool write_register(uint8_t reg, uint8_t value);
    uint8_t read_register(uint8_t reg);
    bool read_registers(uint8_t reg, uint8_t* data, uint8_t len);
    
    // Device-specific initialization
    bool configure_accel();
    bool configure_gyro();
    bool configure_compass();
    
    // Data reading
    bool read_accel_raw();
    bool read_gyro_raw();
    bool read_compass_raw();
    
    // Raw data conversion
    void convert_accel_data(int16_t raw_x, int16_t raw_y, int16_t raw_z);
    void convert_gyro_data(int16_t raw_x, int16_t raw_y, int16_t raw_z);
    void convert_compass_data(int16_t raw_x, int16_t raw_y, int16_t raw_z);
};

#endif // ICM20948_DRIVER_H
