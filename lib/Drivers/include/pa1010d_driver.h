#ifndef PA1010D_GPS_DRIVER_H
#define PA1010D_GPS_DRIVER_H

#include "sensor_hal.h"
#include <HardwareSerial.h>
#include <Wire.h>

/**
 * @brief PA1010D GPS Module Driver
 * Implements IGPSSensor interface for the PA1010D GNSS module
 * Supports both UART (Serial) and I2C communication modes
 */
class PA1010DDriver : public IGPSSensor {
public:
    /**
     * @brief Communication interface type
     */
    enum class CommInterface {
        UART,  // Serial/UART interface
        I2C    // I2C interface (default)
    };

    /**
     * @brief Constructor for I2C communication (default)
     * @param wire I2C bus reference
     * @param i2c_addr I2C address (default 0x10 for PA1010D)
     */
    PA1010DDriver(TwoWire& wire, uint8_t i2c_addr = 0x10);
    
    /**
     * @brief Constructor for UART communication
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
    // Communication mode
    CommInterface m_comm_mode;
    
    // UART members (Serial)
    HardwareSerial* m_serial;
    int m_tx_pin;
    int m_rx_pin;
    unsigned long m_baud;
    
    // I2C members
    TwoWire* m_wire;
    uint8_t m_i2c_addr;
    
    // Common data
    gps_data_t m_data;
    bool m_valid;
    
    // Helper functions
    bool parse_nmea_sentence(const char* sentence);
    bool parse_gprmc(const char* sentence);
    bool parse_gpgga(const char* sentence);
    
    // UART specific
    bool read_uart_nmea_buffer();
    
    // I2C specific
    bool read_i2c_nmea_buffer();
};

#endif // PA1010D_GPS_DRIVER_H
