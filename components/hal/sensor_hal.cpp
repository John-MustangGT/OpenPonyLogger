#include "sensor_hal.h"

SensorManager::SensorManager()
    : m_gps(nullptr), m_accel(nullptr), m_gyro(nullptr), m_compass(nullptr) {
}

SensorManager::~SensorManager() {
}

bool SensorManager::init(IGPSSensor* gps, IAccelSensor* accel, IGyroSensor* gyro, ICompassSensor* compass) {
    m_gps = gps;
    m_accel = accel;
    m_gyro = gyro;
    m_compass = compass;

    bool success = true;

    if (m_gps && !m_gps->init()) {
        success = false;
    }
    if (m_accel && !m_accel->init()) {
        success = false;
    }
    if (m_gyro && !m_gyro->init()) {
        success = false;
    }
    if (m_compass && !m_compass->init()) {
        success = false;
    }

    return success;
}

bool SensorManager::update_all() {
    bool success = true;

    if (m_gps && !m_gps->update()) {
        success = false;
    }
    if (m_accel && !m_accel->update()) {
        success = false;
    }
    if (m_gyro && !m_gyro->update()) {
        success = false;
    }
    if (m_compass && !m_compass->update()) {
        success = false;
    }

    return success;
}

gps_data_t SensorManager::get_gps() const {
    if (m_gps) {
        return m_gps->get_data();
    }
    return gps_data_t{};
}

accel_data_t SensorManager::get_accel() const {
    if (m_accel) {
        return m_accel->get_data();
    }
    return accel_data_t{};
}

gyro_data_t SensorManager::get_gyro() const {
    if (m_gyro) {
        return m_gyro->get_data();
    }
    return gyro_data_t{};
}

compass_data_t SensorManager::get_comp() const {
    if (m_compass) {
        return m_compass->get_data();
    }
    return compass_data_t{};
}

bool SensorManager::gps_valid() const {
    return m_gps && m_gps->is_valid();
}

bool SensorManager::accel_valid() const {
    return m_accel && m_accel->is_valid();
}

bool SensorManager::gyro_valid() const {
    return m_gyro && m_gyro->is_valid();
}

bool SensorManager::compass_valid() const {
    return m_compass && m_compass->is_valid();
}
