#ifndef ICM20948_GYRO_WRAPPER_H
#define ICM20948_GYRO_WRAPPER_H

#include "sensor_hal.h"

// Forward declaration
class ICM20948Driver;

/**
 * @brief Wrapper to expose ICM20948Driver's gyroscope as IGyroSensor
 */
class ICM20948GyroWrapper : public IGyroSensor {
public:
    explicit ICM20948GyroWrapper(ICM20948Driver* imu_driver);
    ~ICM20948GyroWrapper();
    
    bool init() override;
    bool update() override;
    gyro_data_t get_data() const override;
    bool is_valid() const override;
    
private:
    ICM20948Driver* m_imu_driver;
};

#endif // ICM20948_GYRO_WRAPPER_H
