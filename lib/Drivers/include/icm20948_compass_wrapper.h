#ifndef ICM20948_COMPASS_WRAPPER_H
#define ICM20948_COMPASS_WRAPPER_H

#include "sensor_hal.h"

// Forward declaration
class ICM20948Driver;

/**
 * @brief Wrapper to expose ICM20948Driver's compass as ICompassSensor
 */
class ICM20948CompassWrapper : public ICompassSensor {
public:
    explicit ICM20948CompassWrapper(ICM20948Driver* imu_driver);
    ~ICM20948CompassWrapper();
    
    bool init() override;
    bool update() override;
    compass_data_t get_data() const override;
    bool is_valid() const override;
    
private:
    ICM20948Driver* m_imu_driver;
};

#endif // ICM20948_COMPASS_WRAPPER_H
