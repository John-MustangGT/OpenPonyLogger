#include "sensor_hal.h"
#include <Arduino.h>

SensorManager::SensorManager()
    : m_gps(nullptr), m_accel(nullptr), m_gyro(nullptr), m_compass(nullptr), m_battery(nullptr) {
}

SensorManager::~SensorManager() {
}

bool SensorManager::init(IGPSSensor* gps, IAccelSensor* accel, IGyroSensor* gyro, 
              ICompassSensor* compass, IBatterySensor* battery) {
    m_gps = gps;
    m_accel = accel;
    m_gyro = gyro;
    m_compass = compass;
    m_battery = battery;

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
    if (m_battery && !m_battery->init()) {
        success = false;
    }

    return success;
}

bool SensorManager::update_all() {
    bool success = true;
    
    // Debug tracking
    static bool first_run = true;
    static uint32_t last_debug = 0;
    uint32_t now = millis();
    bool should_debug = first_run || (now - last_debug > 5000);

    if (m_gps) {
        bool gps_ok = m_gps->update();
        if (!gps_ok) {
            if (should_debug) Serial.println("  GPS update failed");
            success = false;
        }
    }
    if (m_accel) {
        bool accel_ok = m_accel->update();
        if (!accel_ok) {
            if (should_debug) Serial.println("  Accel update failed");
            success = false;
        }
    }
    if (m_gyro) {
        bool gyro_ok = m_gyro->update();
        if (!gyro_ok) {
            if (should_debug) Serial.println("  Gyro update failed");
            success = false;
        }
    }
    if (m_compass) {
        bool compass_ok = m_compass->update();
        if (!compass_ok) {
            if (should_debug) Serial.println("  Compass update failed");
            success = false;
        }
    }
    if (m_battery) {
        bool battery_ok = m_battery->update();
        if (!battery_ok) {
            if (should_debug) Serial.println("  Battery update failed");
            success = false;
        }
    }
    
    if (should_debug && !success) {
        Serial.flush();
        last_debug = now;
        first_run = false;
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

battery_data_t SensorManager::get_battery() const {
    if (m_battery) {
        return m_battery->get_data();
    }
    return battery_data_t{};
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

bool SensorManager::battery_valid() const {
    return m_battery && m_battery->is_valid();
}

