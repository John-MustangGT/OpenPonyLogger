#include "pa1010d_driver.h"
#include <cstring>
#include <cstdlib>

/**
 * @brief I2C Constructor (default)
 */
PA1010DDriver::PA1010DDriver(TwoWire& wire, uint8_t i2c_addr)
    : m_comm_mode(CommInterface::I2C),
      m_serial(nullptr), m_tx_pin(0), m_rx_pin(0), m_baud(0),
      m_wire(&wire), m_i2c_addr(i2c_addr), m_valid(false) {
    memset(&m_data, 0, sizeof(m_data));
}

/**
 * @brief UART Constructor
 */
PA1010DDriver::PA1010DDriver(HardwareSerial& serial, int tx_pin, int rx_pin, unsigned long baud)
    : m_comm_mode(CommInterface::UART),
      m_serial(&serial), m_tx_pin(tx_pin), m_rx_pin(rx_pin), m_baud(baud),
      m_wire(nullptr), m_i2c_addr(0), m_valid(false) {
    memset(&m_data, 0, sizeof(m_data));
}

PA1010DDriver::~PA1010DDriver() {
}

bool PA1010DDriver::init() {
    if (m_comm_mode == CommInterface::UART) {
        // Initialize UART communication with the GPS module
        if (!m_serial) return false;
        m_serial->begin(m_baud, SERIAL_8N1, m_rx_pin, m_tx_pin);
        return true;
    } else {
        // Initialize I2C communication
        if (!m_wire) return false;
        // I2C bus should already be initialized by the application
        // Just verify we can communicate with the device
        m_wire->beginTransmission(m_i2c_addr);
        return m_wire->endTransmission() == 0;
    }
}

bool PA1010DDriver::update() {
    if (m_comm_mode == CommInterface::UART) {
        return read_uart_nmea_buffer();
    } else {
        return read_i2c_nmea_buffer();
    }
}

bool PA1010DDriver::read_uart_nmea_buffer() {
    static char buffer[128];
    static int buffer_idx = 0;
    
    // Read available data from GPS module
    while (m_serial && m_serial->available()) {
        char c = m_serial->read();
        
        if (c == '\n') {
            buffer[buffer_idx] = '\0';
            
            // Parse the complete NMEA sentence
            if (buffer[0] == '$') {
                parse_nmea_sentence(buffer);
            }
            
            buffer_idx = 0;
        } else if (c != '\r' && buffer_idx < sizeof(buffer) - 1) {
            buffer[buffer_idx++] = c;
        }
    }
    
    return m_valid;
}

gps_data_t PA1010DDriver::get_data() const {
    return m_data;
}

bool PA1010DDriver::is_valid() const {
    return m_valid;
}

bool PA1010DDriver::parse_nmea_sentence(const char* sentence) {
    if (strncmp(sentence, "$GPRMC", 6) == 0) {
        return parse_gprmc(sentence);
    } else if (strncmp(sentence, "$GPGGA", 6) == 0) {
        return parse_gpgga(sentence);
    }
    return false;
}

bool PA1010DDriver::parse_gprmc(const char* sentence) {
    // Format: $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
    // Example: $GPRMC,064951.734,A,2307.038,N,12042.457,E,2.3,90.0,051200,,,A*22
    
    int values = 0;
    double tmp_lat = 0, tmp_lon = 0;
    char tmp_lat_dir = 'N', tmp_lon_dir = 'E';
    int tmp_date = 0;
    float tmp_speed = 0;
    
    // Parse comma-separated values
    const char* pos = strchr(sentence, ',');
    
    if (!pos) return false;
    
    // Time: hhmmss
    pos++;
    sscanf(pos, "%2hhu%2hhu%2hhu", &m_data.hour, &m_data.minute, &m_data.second);
    
    pos = strchr(pos, ',') + 1;
    char status = *pos;  // A=valid, V=invalid
    
    pos = strchr(pos, ',') + 1;
    sscanf(pos, "%lf", &tmp_lat);
    
    pos = strchr(pos, ',') + 1;
    tmp_lat_dir = *pos;
    
    pos = strchr(pos, ',') + 1;
    sscanf(pos, "%lf", &tmp_lon);
    
    pos = strchr(pos, ',') + 1;
    tmp_lon_dir = *pos;
    
    pos = strchr(pos, ',') + 1;
    sscanf(pos, "%f", &tmp_speed);
    
    // Skip course
    pos = strchr(pos, ',') + 1;
    pos = strchr(pos, ',') + 1;
    
    // Date: ddmmyy
    sscanf(pos, "%2hhu%2hhu%2hu", &m_data.day, &m_data.month, &m_data.year);
    m_data.year += 2000;
    
    // Convert latitude and longitude from NMEA format
    int lat_deg = (int)(tmp_lat / 100.0);
    m_data.latitude = lat_deg + (tmp_lat - lat_deg * 100.0) / 60.0;
    if (tmp_lat_dir == 'S') {
        m_data.latitude = -m_data.latitude;
    }
    
    int lon_deg = (int)(tmp_lon / 100.0);
    m_data.longitude = lon_deg + (tmp_lon - lon_deg * 100.0) / 60.0;
    if (tmp_lon_dir == 'W') {
        m_data.longitude = -m_data.longitude;
    }
    
    m_data.speed = tmp_speed;
    m_valid = (status == 'A');
    
    return m_valid;
}

bool PA1010DDriver::parse_gpgga(const char* sentence) {
    // Format: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,,*hh
    // Example: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*42
    
    const char* pos = strchr(sentence, ',');
    
    if (!pos) return false;
    
    // Skip time
    pos = strchr(pos + 1, ',');
    
    // Skip latitude
    pos = strchr(pos + 1, ',');
    pos = strchr(pos + 1, ',');
    
    // Skip longitude
    pos = strchr(pos + 1, ',');
    pos = strchr(pos + 1, ',');
    
    // Fix quality
    pos++;
    int fix_quality = atoi(pos);
    
    // Skip to satellite count
    pos = strchr(pos, ',') + 1;
    sscanf(pos, "%hhu", &m_data.satellites);
    
    // Skip dilution
    pos = strchr(pos, ',') + 1;
    
    // Altitude
    pos = strchr(pos, ',') + 1;
    sscanf(pos, "%lf", &m_data.altitude);
    
    m_valid = (fix_quality > 0);
    
    return m_valid;
}

bool PA1010DDriver::read_i2c_nmea_buffer() {
    // PA1010D I2C protocol: Read from I2C buffer
    // Device stores NMEA sentences in an internal buffer
    // Read format: 2 bytes length + up to 255 bytes NMEA data
    
    if (!m_wire) return false;
    
    static char sentence_buffer[256];
    
    // Request data from PA1010D
    // Address pointer at 0xFF indicates we want the NMEA buffer
    m_wire->beginTransmission(m_i2c_addr);
    m_wire->write(0xFF);  // NMEA buffer register
    m_wire->endTransmission();
    
    // Read the length (2 bytes, big-endian)
    m_wire->requestFrom(m_i2c_addr, (size_t)2);
    if (m_wire->available() < 2) {
        return false;
    }
    
    uint16_t length = (m_wire->read() << 8) | m_wire->read();
    
    // Limit length to avoid buffer overflow
    if (length == 0 || length > 255) {
        return false;
    }
    
    // Read the NMEA sentence
    m_wire->requestFrom(m_i2c_addr, (size_t)length);
    
    int bytes_read = 0;
    while (m_wire->available() && bytes_read < length) {
        sentence_buffer[bytes_read++] = m_wire->read();
    }
    
    if (bytes_read != length) {
        return false;
    }
    
    sentence_buffer[bytes_read] = '\0';
    
    // Parse the NMEA sentence if it's valid
    if (sentence_buffer[0] == '$') {
        return parse_nmea_sentence(sentence_buffer);
    }
    
    return false;
}
