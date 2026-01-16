#include "icm20948_compass_wrapper.h"
#include "icm20948_driver.h"

ICM20948CompassWrapper::ICM20948CompassWrapper(ICM20948Driver* imu_driver)
    : m_imu_driver(imu_driver) {
}

ICM20948CompassWrapper::~ICM20948CompassWrapper() {
}

bool ICM20948CompassWrapper::init() {
    // Initialization is handled by the main ICM20948Driver
    return m_imu_driver != nullptr;
}

bool ICM20948CompassWrapper::update() {
    // Update is handled by the main ICM20948Driver
    return true;
}

compass_data_t ICM20948CompassWrapper::get_data() const {
    if (m_imu_driver) {
        return m_imu_driver->get_compass();
    }
    return compass_data_t{};
}

bool ICM20948CompassWrapper::is_valid() const {
    if (m_imu_driver) {
        return m_imu_driver->compass_is_valid();
    }
    return false;
}
