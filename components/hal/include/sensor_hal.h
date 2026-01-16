#ifndef SENSOR_HAL_H
#define SENSOR_HAL_H

#include <cstdint>

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
 * @brief Accelerometer Data Structure
 */
struct accel_data_t {
    float x;
    float y;
    float z;
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
 * @brief Sensor Manager - Central HAL interface for all sensors
 */
class SensorManager {
public:
    SensorManager();
    ~SensorManager();

    bool init(IGPSSensor* gps, IAccelSensor* accel, IGyroSensor* gyro, ICompassSensor* compass);
    
    bool update_all();
    
    // Generic sensor access functions
    gps_data_t get_gps() const;
    accel_data_t get_accel() const;
    gyro_data_t get_gyro() const;
    compass_data_t get_comp() const;
    
    // Validity checks
    bool gps_valid() const;
    bool accel_valid() const;
    bool gyro_valid() const;
    bool compass_valid() const;

private:
    IGPSSensor* m_gps;
    IAccelSensor* m_accel;
    IGyroSensor* m_gyro;
    ICompassSensor* m_compass;
};

#endif // SENSOR_HAL_H
