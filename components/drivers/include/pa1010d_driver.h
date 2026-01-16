#ifndef PA1010D_GPS_DRIVER_H
#define PA1010D_GPS_DRIVER_H

#include "sensor_hal.h"
#include <HardwareSerial.h>

/**
 * @brief PA1010D GPS Module Driver
 * Implements IGPSSensor interface for the PA1010D GNSS module
 */
class PA1010DDriver : public IGPSSensor {
public:
    /**
     * @brief Constructor
     * @param serial Hardware serial port (typically Serial1)
     * @param tx_pin TX pin for the serial connection
     * @param rx_pin RX pin for the serial connection
     * @param baud Baud rate (default 9600)
     */
    PA1010DDriver(HardwareSerial& serial, int tx_pin, int rx_pin, unsigned long baud = 9600);
    
    ~PA1010DDriver();
    
    // IGPSSensor implementation
    bool init() override;
    bool update() override;
    gps_data_t get_data() const override;
    bool is_valid() const override;

private:
    HardwareSerial& m_serial;
    int m_tx_pin;
    int m_rx_pin;
    unsigned long m_baud;
    gps_data_t m_data;
    bool m_valid;
    
    bool parse_nmea_sentence(const char* sentence);
    bool parse_gprmc(const char* sentence);
    bool parse_gpgga(const char* sentence);
};

#endif // PA1010D_GPS_DRIVER_H
