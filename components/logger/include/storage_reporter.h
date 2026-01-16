#ifndef STORAGE_REPORTER_H
#define STORAGE_REPORTER_H

#include "sensor_hal.h"
#include <cstdio>

/**
 * @brief Storage Write Reporter
 * Reports storage write events and current sensor values to serial line
 */
class StorageReporter {
public:
    /**
     * @brief Constructor
     * @param serial_port Serial port for output (default Serial)
     */
    StorageReporter();
    
    ~StorageReporter();
    
    /**
     * @brief Initialize the reporter
     * @param baud_rate Serial baud rate (default 115200)
     */
    void init(unsigned long baud_rate = 115200);
    
    /**
     * @brief Report a storage write event with current sensor values
     */
    void report_storage_write(const gps_data_t& gps, const accel_data_t& accel,
                              const gyro_data_t& gyro, const compass_data_t& compass);
    
    /**
     * @brief Print a debug message
     */
    void print_debug(const char* message);
    
    /**
     * @brief Print formatted message
     */
    void printf_debug(const char* format, ...);

private:
    void print_gps_data(const gps_data_t& gps);
    void print_accel_data(const accel_data_t& accel);
    void print_gyro_data(const gyro_data_t& gyro);
    void print_compass_data(const compass_data_t& compass);
};

#endif // STORAGE_REPORTER_H
