#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <cstdint>
#include <time.h>
#include <Wire.h>

/**
 * @brief PCF8523 Real-Time Clock Manager
 * 
 * Handles interaction with Adafruit Adalogger's PCF8523 RTC chip.
 * Features:
 * - I2C detection at 0x68
 * - Read/write time to PCF8523 (battery-backed)
 * - Fallback to NVS-stored GPS time if RTC unavailable
 * - Restore ESP32-S3 system time on boot
 */
class RTCManager {
public:
    static const uint8_t I2C_ADDRESS = 0x68;

    RTCManager();

    /**
     * @brief Initialize RTC manager and detect PCF8523
     * @param wire Reference to TwoWire (I2C) bus
     * @return true if PCF8523 detected and readable, false otherwise
     */
    bool init(TwoWire& wire);

    /**
     * @brief Check if PCF8523 is available
     * @return true if RTC was detected and is functional
     */
    bool is_available() const { return m_available; }

    /**
     * @brief Read current time from PCF8523
     * @return Unix timestamp (time_t) or 0 if unavailable
     */
    time_t read_time();

    /**
     * @brief Write time to PCF8523
     * @param time_val Unix timestamp to write (as long/time_t)
     * @return true if write successful, false if RTC unavailable
     */
    bool write_time(long time_val);

    /**
     * @brief Restore system time from PCF8523 or NVS on boot
     * Sets the ESP32-S3 internal RTC using:
     * 1. PCF8523 (if available and has valid time)
     * 2. NVS "gps_time" key (fallback if RTC absent)
     * 3. Compile-time default (final fallback)
     */
    void restore_system_time_on_boot();

private:
    TwoWire* m_wire;
    bool m_available;

    /**
     * @brief Internal: Convert BCD (Binary-Coded Decimal) to decimal
     */
    static uint8_t bcd_to_decimal(uint8_t bcd);

    /**
     * @brief Internal: Convert decimal to BCD
     */
    static uint8_t decimal_to_bcd(uint8_t decimal);
};

#endif // RTC_MANAGER_H
