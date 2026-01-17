#ifndef OBD_DATA_H
#define OBD_DATA_H

#include <cstdint>

/**
 * @brief OBD-II data from vgate iCar 2 Pro BLE interface
 * Supports common PIDs (Parameter IDs) used in automotive logging
 */
struct obd_data_t {
    // Engine parameters
    float engine_rpm = 0.0f;           // Revolutions per minute
    float engine_load = 0.0f;          // Engine load (%)
    float coolant_temp = 0.0f;         // Coolant temperature (°C)
    float intake_temp = 0.0f;          // Intake air temperature (°C)
    float fuel_pressure = 0.0f;        // Fuel pressure (kPa)
    
    // Vehicle motion
    float vehicle_speed = 0.0f;        // Vehicle speed (km/h)
    float throttle_position = 0.0f;    // Throttle position (%)
    float timing_advance = 0.0f;       // Timing advance (°)
    
    // Air/Fuel
    float o2_sensor_voltage = 0.0f;    // O2 sensor voltage (V)
    float o2_sensor_trim = 0.0f;       // O2 sensor trim (%)
    float fuel_consumption = 0.0f;     // Fuel consumption (L/h estimated)
    
    // Diagnostic
    float maf_flow = 0.0f;             // Mass air flow (g/s)
    float barometric_pressure = 0.0f;  // Barometric pressure (kPa)
    
    // DTC (Diagnostic Trouble Code)
    uint32_t dtc_count = 0;            // Number of active DTCs
    
    // Connection state
    bool connected = false;            // Connected to OBD device
    bool valid = false;                // Data is valid/fresh
    uint32_t last_update_ms = 0;       // Last successful update time
};

#endif // OBD_DATA_H
