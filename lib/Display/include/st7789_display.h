#ifndef ST7789_DISPLAY_H
#define ST7789_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>

/**
 * @brief ST7789 TFT Display driver for Adafruit ESP32-S3 Feather Reverse TFT
 * Display: 1.14" IPS LCD, 240x135 pixels, SPI interface
 * 
 * Pin Configuration (Adafruit ESP32-S3 Feather Reverse TFT):
 * - SPI CLK: Default hardware SPI (GPIO36 for HSPI)
 * - SPI MOSI: Default hardware SPI (GPIO35 for HSPI)
 * - TFT_CS: GPIO42
 * - TFT_DC (Data/Command): GPIO40
 * - TFT_RST (Reset): GPIO41
 * - TFT_BACKLITE (Backlight): GPIO45
 * - TFT_I2C_POWER: GPIO7 (Already pulled high, shared with STEMMA I2C power)
 * 
 * Note: The Adafruit_ST7789 library automatically uses dedicated hardware SPI pins.
 */

class ST7789Display {
public:
    /**
     * @brief Initialize the ST7789 display
     * @return true if successful, false otherwise
     */
    static bool init();

    /**
     * @brief Update the display with latest sensor data
     * Compact format for 240x135 pixels
     * 
     * @param uptime_ms Uptime in milliseconds
     * @param temp Temperature in Celsius (will be converted based on USE_IMPERIAL)
     * @param accel_x X acceleration in g
     * @param accel_y Y acceleration in g
     * @param accel_z Z acceleration in g
     * @param gyro_x X rotation in dps
     * @param gyro_y Y rotation in dps
     * @param gyro_z Z rotation in dps
     * @param battery_soc Battery state of charge (0-100%)
     * @param battery_voltage Battery voltage in V
     * @param gps_valid GPS has valid fix
     * @param sample_count Number of samples logged
     * @param gps_speed Speed in knots (if valid)
     */
    static void update(uint32_t uptime_ms,
                      float temp,
                      float accel_x, float accel_y, float accel_z,
                      float gyro_x, float gyro_y, float gyro_z,
                      float battery_soc, float battery_voltage,
                      bool gps_valid, uint32_t sample_count,
                      float gps_speed = 0.0f);

    /**
     * @brief Turn display on
     */
    static void on();

    /**
     * @brief Turn display off
     */
    static void off();

private:
    static Adafruit_ST7789* m_tft;
    static bool m_initialized;
};

/**
 * @brief NeoPixel status indicator
 * Uses the built-in NeoPixel on Adafruit ESP32-S3 Feather (GPIO33)
 * 
 * States:
 * - Red (solid): Booting
 * - Yellow (1Hz flash): Searching for GPS fix
 * - Green (solid): GPS 3D fix acquired
 */
class NeoPixelStatus {
public:
    enum class State {
        BOOTING,      // Red
        NO_GPS_FIX,   // Yellow flashing
        GPS_3D_FIX    // Green
    };

    /**
     * @brief Initialize the NeoPixel
     * @return true if successful
     */
    static bool init();

    /**
     * @brief Set NeoPixel state
     * @param state The desired state
     */
    static void setState(State state);

    /**
     * @brief Update NeoPixel (must be called periodically for flashing)
     * @param current_ms Current time in milliseconds
     */
    static void update(uint32_t current_ms);

    /**
     * @brief Deinitialize NeoPixel
     */
    static void deinit();

private:
    static Adafruit_NeoPixel* m_pixel;
    static State m_current_state;
    static uint32_t m_last_flash_time;
    static bool m_pixel_on;
    static bool m_initialized;
    static constexpr uint32_t FLASH_INTERVAL_MS = 500;  // 1Hz flash (500ms on, 500ms off)
};

#endif // ST7789_DISPLAY_H
