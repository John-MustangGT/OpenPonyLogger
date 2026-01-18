#include "icar_ble_wrapper.h"
#include <Arduino.h>

IcarBleWrapper::IcarBleWrapper()
    : m_initialized(false), m_data_valid(false) {
}

bool IcarBleWrapper::init() {
    // Configure default PIDs for performance monitoring
    // Note: IcarBleDriver::init() and start_scan() must be called on Core 0 first!
    
    // RPM - poll every 100ms (fastest changing parameter)
    IcarBleDriver::add_pid(0x0C, 100, "Engine RPM");
    
    // Speed - poll every 100ms
    IcarBleDriver::add_pid(0x0D, 100, "Vehicle Speed");
    
    // Throttle position - poll every 200ms
    IcarBleDriver::add_pid(0x11, 200, "Throttle Position");
    
    // Coolant temperature - poll every 2000ms (slow changing)
    IcarBleDriver::add_pid(0x05, 2000, "Coolant Temperature");
    
    // MAF (Mass Air Flow) - poll every 200ms
    IcarBleDriver::add_pid(0x10, 200, "Mass Air Flow");
    
    // Intake air temperature - poll every 2000ms
    IcarBleDriver::add_pid(0x0F, 2000, "Intake Air Temperature");
    
    m_initialized = true;
    
    Serial.println("[OBD] IcarBleWrapper initialized with default PIDs");
    return true;
}

bool IcarBleWrapper::update() {
    // Only update if BLE is connected
    if (!IcarBleDriver::is_connected()) {
        m_data_valid = false;
        return false;
    }

    // Update polls PIDs whose interval has elapsed
    bool updated = IcarBleDriver::update();
    
    if (updated) {
        m_data_valid = true;
    }
    
    return updated;
}

obd_data_t IcarBleWrapper::get_data() const {
    return IcarBleDriver::get_data();
}

bool IcarBleWrapper::is_valid() const {
    // Data is valid if we've successfully read at least one PID
    return m_data_valid && IcarBleDriver::is_connected();
}

bool IcarBleWrapper::is_connected() const {
    return IcarBleDriver::is_connected();
}
