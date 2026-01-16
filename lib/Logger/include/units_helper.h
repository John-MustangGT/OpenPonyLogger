#ifndef UNITS_HELPER_H
#define UNITS_HELPER_H

/**
 * @brief Unit conversion helper for Imperial/Metric display
 * Controlled by USE_IMPERIAL compile flag (1 = imperial, 0 = metric)
 */

// Default to Imperial units if not specified
#ifndef USE_IMPERIAL
#define USE_IMPERIAL 1
#endif

/**
 * @brief Convert temperature from Celsius to display units
 * @param celsius Temperature in Celsius
 * @return Temperature in °F if USE_IMPERIAL=1, otherwise °C
 */
inline float convert_temperature(float celsius) {
#if USE_IMPERIAL
    return (celsius * 9.0f / 5.0f) + 32.0f;  // °F
#else
    return celsius;  // °C
#endif
}

/**
 * @brief Get temperature unit string
 * @return "F" or "C"
 */
inline const char* get_temp_unit() {
#if USE_IMPERIAL
    return "F";
#else
    return "C";
#endif
}

/**
 * @brief Convert speed from knots to display units
 * @param knots Speed in knots
 * @return Speed in mph if USE_IMPERIAL=1, otherwise km/h
 */
inline float convert_speed(float knots) {
#if USE_IMPERIAL
    return knots * 1.15078f;  // knots to mph
#else
    return knots * 1.852f;    // knots to km/h
#endif
}

/**
 * @brief Get speed unit string
 * @return "mph" or "km/h"
 */
inline const char* get_speed_unit() {
#if USE_IMPERIAL
    return "mph";
#else
    return "km/h";
#endif
}

#endif // UNITS_HELPER_H
