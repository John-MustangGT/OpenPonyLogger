#include "pa1010d_driver.h"
#include <cstring>
#include <cstdlib>

PA1010DDriver::PA1010DDriver(HardwareSerial& serial, int tx_pin, int rx_pin, unsigned long baud)
    : m_serial(serial), m_tx_pin(tx_pin), m_rx_pin(rx_pin), m_baud(baud), m_valid(false) {
    memset(&m_data, 0, sizeof(m_data));
}

PA1010DDriver::~PA1010DDriver() {
}

bool PA1010DDriver::init() {
    // Initialize serial communication with the GPS module
    m_serial.begin(m_baud, SERIAL_8N1, m_rx_pin, m_tx_pin);
    return true;
}

bool PA1010DDriver::update() {
    static char buffer[128];
    static int buffer_idx = 0;
    
    // Read available data from GPS module
    while (m_serial.available()) {
        char c = m_serial.read();
        
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
