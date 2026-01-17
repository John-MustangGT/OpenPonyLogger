#ifndef SENSOR_HAL_H
#define SENSOR_HAL_H

#include <cstdint>
#include "obd_data.h"

/**
 * @brief GPS Data Structure
 */
struct gps_data_t {
    double latitude;
    double longitude;
    double altitude;
    float speed;           // in knots
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool valid;
    uint8_t satellites;
};

/**
 * @brief Accelerometer Data Structure (includes IMU temperature)
 */
struct accel_data_t {
    float x;
    float y;
    float z;
    float temperature;  // IMU die temperature in °C
};

/**
 * @brief Gyroscope Data Structure
 */
struct gyro_data_t {
    float x;
    float y;
    float z;
};

/**
 * @brief Compass/Magnetometer Data Structure
 */
struct compass_data_t {
    float x;
    float y;
    float z;
};

/**
 * @brief Battery Data Structure
 */
struct battery_data_t {
    float voltage;         // in volts (V)
    float state_of_charge; // in percent (0-100%)
    float current;         // in mA (positive = charging, negative = discharging)
    int16_t temperature;   // in Celsius * 100 (e.g., 2500 = 25.00°C)
    bool valid;            // true if battery monitor is functioning
};

/**
 * @brief GPS Sensor HAL Interface
 */
class IGPSSensor {
public:
    virtual ~IGPSSensor() = default;
    virtual bool init() = 0;
    virtual bool update() = 0;
    virtual gps_data_t get_data() const = 0;
    virtual bool is_valid() const = 0;
};

/**
 * @brief Accelerometer HAL Interface
 */
class IAccelSensor {
public:
    virtual ~IAccelSensor() = default;
    virtual bool init() = 0;
    virtual bool update() = 0;
    virtual accel_data_t get_data() const = 0;
    virtual bool is_valid() const = 0;
};

/**
 * @brief Gyroscope HAL Interface
 */
class IGyroSensor {
public:
    virtual ~IGyroSensor() = default;
    virtual bool init() = 0;
    virtual bool update() = 0;
    virtual gyro_data_t get_data() const = 0;
    virtual bool is_valid() const = 0;
};

/**
 * @brief Compass/Magnetometer HAL Interface
 */
class ICompassSensor {
public:
    virtual ~ICompassSensor() = default;
    virtual bool init() = 0;
    virtual bool update() = 0;
    virtual compass_data_t get_data() const = 0;
    virtual bool is_valid() const = 0;
};

/**
 * @brief Battery Monitor HAL Interface
 */
class IBatterySensor {
public:
    virtual ~IBatterySensor() = default;
    virtual bool init() = 0;
    virtual bool update() = 0;
    virtual battery_data_t get_data() const = 0;
    virtual bool is_valid() const = 0;
};

/**
 * @brief Sensor Manager - Central HAL interface for all sensors
 */
class SensorManager {
public:
    SensorManager();
    ~SensorManager();

    bool init(IGPSSensor* gps, IAccelSensor* accel, IGyroSensor* gyro, 
              ICompassSensor* compass, IBatterySensor* battery = nullptr);
    
    bool update_all();
    
    // Generic sensor access functions
    gps_data_t get_gps() const;
    accel_data_t get_accel() const;
    gyro_data_t get_gyro() const;
    compass_data_t get_comp() const;
    battery_data_t get_battery() const;
    
    // Validity checks
    bool gps_valid() const;
    bool accel_valid() const;
    bool gyro_valid() const;
    bool compass_valid() const;
    bool battery_valid() const;

private:
    IGPSSensor* m_gps;
    IAccelSensor* m_accel;
    IGyroSensor* m_gyro;
    ICompassSensor* m_compass;
    IBatterySensor* m_battery;
};

#endif // SENSOR_HAL_H
