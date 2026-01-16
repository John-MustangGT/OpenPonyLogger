#include "icm20948_gyro_wrapper.h"
#include "icm20948_driver.h"

ICM20948GyroWrapper::ICM20948GyroWrapper(ICM20948Driver* imu_driver)
    : m_imu_driver(imu_driver) {
}

ICM20948GyroWrapper::~ICM20948GyroWrapper() {
}

bool ICM20948GyroWrapper::init() {
    // Initialization is handled by the main ICM20948Driver
    return m_imu_driver != nullptr;
}

bool ICM20948GyroWrapper::update() {
    // Update is handled by the main ICM20948Driver
    return true;
}

gyro_data_t ICM20948GyroWrapper::get_data() const {
    if (m_imu_driver) {
        return m_imu_driver->get_gyro();
    }
    return gyro_data_t{};
}

bool ICM20948GyroWrapper::is_valid() const {
    if (m_imu_driver) {
        return m_imu_driver->gyro_is_valid();
    }
    return false;
}
