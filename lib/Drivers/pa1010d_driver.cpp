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
    if (m_serial) {
        while (m_serial->available()) {
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
    }
    
    // Always return true - either we got data or waiting for it is OK
    return true;
}

gps_data_t PA1010DDriver::get_data() const {
    return m_data;
}

bool PA1010DDriver::is_valid() const {
    return m_valid;
}

bool PA1010DDriver::parse_nmea_sentence(const char* sentence) {
    // Debug: Print last GPS sentence received
    static uint32_t last_print_time = 0;
    uint32_t now = millis();
    if (now - last_print_time >= 2000) {  // Print every 2 seconds
        Serial.printf("[GPS] Last sentence: %s\n", sentence);
        last_print_time = now;
    }
    
    // Process RMC sentences for time, date, and speed
    if (strncmp(sentence, "$GNRMC", 6) == 0 || strncmp(sentence, "$GPRMC", 6) == 0) {
        return parse_gprmc(sentence);
    }
    
    // Process GGA sentences for position and satellite count
    if (strncmp(sentence, "$GNGGA", 6) == 0) {
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
    
    // Time: hhmmss.ss (we extract just hhmmss)
    pos++;
    int parsed = sscanf(pos, "%2hhu%2hhu%2hhu", &m_data.hour, &m_data.minute, &m_data.second);
    if (parsed != 3) return false;
    
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
    m_data.valid = m_valid;  // Update the struct field too
    
    // Debug: Print parsing result
    static uint32_t last_parse_debug = 0;
    uint32_t now = millis();
    if (now - last_parse_debug >= 2000) {
        Serial.printf("[GPS] GPRMC: valid=%d, status=%c, lat=%.6f, lon=%.6f, speed=%.1f\n", 
                      m_valid, status, m_data.latitude, m_data.longitude, m_data.speed);
        last_parse_debug = now;
    }
    
    return m_valid;
}

bool PA1010DDriver::parse_gpgga(const char* sentence) {
    // Format: $GNGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,,*hh
    // Example: $GNGGA,014153.377,4219.9872,N,07126.1921,W,1,4,1.69,69.9,M,-33.8,M,,*4B
    //          0      1          2         3  4         5  6 7  8    9    10
    
    // For now, just check if we can parse the basic structure
    // Focus on: time, lat, lon, fix_quality, sats, altitude
    
    if (!sentence || sentence[0] != '$') {
        return false;
    }
    
    // Skip the sentence header
    const char* pos = strchr(sentence, ',');
    if (!pos) return false;
    
    // Time field - hhmmss.ss format
    pos++;
    int time_parsed = sscanf(pos, "%2hhu%2hhu%2hhu", &m_data.hour, &m_data.minute, &m_data.second);
    // Continue even if time parsing fails, since we need the position data
    
    // Latitude
    double tmp_lat = 0;
    pos = strchr(pos, ',');
    if (!pos) return false;
    pos++;
    int lat_scan = sscanf(pos, "%lf", &tmp_lat);
    if (lat_scan != 1) return false;
    
    // Latitude direction (N/S)
    pos = strchr(pos, ',');
    if (!pos) return false;
    pos++;
    char tmp_lat_dir = *pos;
    
    // Longitude
    double tmp_lon = 0;
    pos = strchr(pos, ',');
    if (!pos) return false;
    pos++;
    int lon_scan = sscanf(pos, "%lf", &tmp_lon);
    if (lon_scan != 1) return false;
    
    // Longitude direction (E/W)
    pos = strchr(pos, ',');
    if (!pos) return false;
    pos++;
    char tmp_lon_dir = *pos;
    
    // Fix quality (critical field)
    int fix_quality = 0;
    pos = strchr(pos, ',');
    if (!pos) return false;
    pos++;
    int fix_scan = sscanf(pos, "%d", &fix_quality);
    if (fix_scan != 1) return false;
    
    // Satellite count
    uint8_t sats = 0;
    pos = strchr(pos, ',');
    if (!pos) return false;
    pos++;
    sscanf(pos, "%hhu", &sats);
    
    // Skip dilution (HDOP)
    pos = strchr(pos, ',');
    if (!pos) return false;
    
    // Altitude
    double alt = 0;
    pos = strchr(pos, ',');
    if (!pos) return false;
    pos++;
    sscanf(pos, "%lf", &alt);
    
    // Convert latitude and longitude from NMEA format (ddmm.mmmm)
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
    
    m_data.altitude = alt;
    m_data.satellites = sats;
    m_valid = (fix_quality > 0);
    m_data.valid = m_valid;
    
    // Debug: Print parsing result
    static uint32_t last_parse_debug_gga = 0;
    uint32_t now = millis();
    if (now - last_parse_debug_gga >= 2000) {
        Serial.printf("[GPS] GNGGA: valid=%d, fix_quality=%d, sats=%d, alt=%.1f, lat=%.6f, lon=%.6f\n", 
                      m_valid, fix_quality, sats, alt, m_data.latitude, m_data.longitude);
        last_parse_debug_gga = now;
    }
    
    return m_valid;
}

bool PA1010DDriver::read_i2c_nmea_buffer() {
    // PA1010D I2C protocol: Simple streaming read
    // The GPS continuously outputs NMEA sentences over I2C
    // Just read available bytes directly - no register addressing needed
    
    if (!m_wire) return false;
    
    static EXT_RAM_BSS_ATTR char sentence_buffer[256];
    static int buffer_pos = 0;
    static uint32_t last_i2c_debug = 0;
    static uint32_t read_attempts = 0;
    static uint32_t successful_sentences = 0;
    
    read_attempts++;
    
    // Request up to 32 bytes at a time from the GPS
    size_t bytes_available = m_wire->requestFrom(m_i2c_addr, (size_t)32);
    
    if (bytes_available == 0) {
        uint32_t now = millis();
        if (now - last_i2c_debug >= 2000) {
            Serial.printf("[GPS-I2C] No bytes available (attempts=%u, sentences=%u)\n", 
                          read_attempts, successful_sentences);
            last_i2c_debug = now;
        }
        return true;  // No data available yet
    }
    
    // Read available bytes into our sentence buffer
    while (m_wire->available() && buffer_pos < 255) {
        char c = m_wire->read();
        
        // Look for sentence start
        if (c == '$') {
            buffer_pos = 0;  // Start new sentence
        }
        
        sentence_buffer[buffer_pos++] = c;
        
        // Check for sentence end (newline or carriage return)
        if (c == '\n' || c == '\r') {
            if (buffer_pos > 1) {  // We have a complete sentence
                sentence_buffer[buffer_pos] = '\0';
                
                // Remove any trailing CR/LF
                while (buffer_pos > 0 && (sentence_buffer[buffer_pos-1] == '\n' || 
                                          sentence_buffer[buffer_pos-1] == '\r')) {
                    sentence_buffer[--buffer_pos] = '\0';
                }
                
                uint32_t now = millis();
                if (now - last_i2c_debug >= 2000) {
                    Serial.printf("[GPS-I2C] Read sentence: %d chars (attempts=%u, sentences=%u)\n", 
                                  buffer_pos, read_attempts, successful_sentences);
                    last_i2c_debug = now;
                }
                
                // Parse the NMEA sentence if it starts with '$'
                if (sentence_buffer[0] == '$') {
                    successful_sentences++;
                    parse_nmea_sentence(sentence_buffer);
                }
                
                buffer_pos = 0;  // Reset for next sentence
            } else {
                buffer_pos = 0;  // Empty line, reset
            }
        }
    }
    
    // Prevent buffer overflow
    if (buffer_pos >= 255) {
        uint32_t now = millis();
        if (now - last_i2c_debug >= 2000) {
            Serial.printf("[GPS-I2C] Buffer overflow, resetting (attempts=%u, sentences=%u)\n", 
                          read_attempts, successful_sentences);
            last_i2c_debug = now;
        }
        buffer_pos = 0;
    }
    
    return true;  // Successfully queried I2C
}
