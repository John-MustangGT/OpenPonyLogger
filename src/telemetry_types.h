/**
 * @file telemetry_types.h
 * @brief Core data structures for OpenPonyLogger telemetry system
 * 
 * Defines the message types, sensor identifiers, and data structures
 * used in the inter-core ring buffer communication.
 */

#ifndef TELEMETRY_TYPES_H
#define TELEMETRY_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Maximum sizes for variable-length data
#define MAX_NMEA_LENGTH 82
#define MAX_OBD_DATA_LENGTH 8
#define MAX_SESSION_ID_LENGTH 32

/**
 * Sensor source identifier
 */
typedef enum {
    SENSOR_GPS = 0x01,
    SENSOR_ACCELEROMETER = 0x02,
    SENSOR_OBD_II = 0x03,
    SENSOR_SYSTEM = 0xFF
} sensor_type_t;

/**
 * Data type within each sensor category
 */
typedef enum {
    // GPS data types
    DATA_GPS_NMEA = 0x01,      // Raw NMEA sentence
    DATA_GPS_FIX = 0x02,       // Parsed fix data
    DATA_GPS_SATS = 0x03,      // Satellite info
    
    // Accelerometer data types
    DATA_ACCEL_XYZ = 0x10,     // Raw XYZ acceleration
    DATA_ACCEL_GYRO = 0x11,    // Gyroscope data
    DATA_ACCEL_COMBINED = 0x12, // Both accel + gyro
    
    // OBD-II data types
    DATA_OBD_PID = 0x20,       // Single PID response
    DATA_OBD_BATCH = 0x21,     // Multiple PID responses
    
    // System events
    DATA_SESSION_START = 0xF0,
    DATA_SESSION_END = 0xF1,
    DATA_ERROR = 0xFE
} data_type_t;

/**
 * Timestamp source flag
 */
typedef enum {
    TIME_SOURCE_UPTIME = 0,    // Microseconds since boot
    TIME_SOURCE_GPS = 1,       // GPS time (when available)
    TIME_SOURCE_RTC = 2        // Real-time clock (when available)
} time_source_t;

/**
 * GPS fix data (parsed from NMEA)
 */
typedef struct {
    float latitude;            // Decimal degrees
    float longitude;           // Decimal degrees
    float altitude;            // Meters
    float speed;               // Meters per second
    float heading;             // Degrees (0-359)
    uint8_t fix_quality;       // 0=no fix, 1=GPS, 2=DGPS
    uint8_t satellites;        // Number of satellites
    float hdop;                // Horizontal dilution of precision
} __attribute__((packed)) gps_fix_t;

/**
 * GPS satellite information
 */
typedef struct {
    uint8_t prn;               // Satellite PRN number
    uint8_t elevation;         // Elevation in degrees
    uint16_t azimuth;          // Azimuth in degrees
    uint8_t snr;               // Signal-to-noise ratio (dB)
} __attribute__((packed)) gps_satellite_t;

/**
 * Accelerometer + Gyroscope data
 */
typedef struct {
    float accel_x;             // G-force lateral (left/right)
    float accel_y;             // G-force longitudinal (fwd/back)
    float accel_z;             // G-force vertical (up/down)
    float gyro_x;              // Roll rate (deg/sec)
    float gyro_y;              // Pitch rate (deg/sec)
    float gyro_z;              // Yaw rate (deg/sec)
} __attribute__((packed)) accel_data_t;

/**
 * OBD-II PID response
 */
typedef struct {
    uint8_t mode;              // OBD mode (typically 0x01)
    uint8_t pid;               // Parameter ID
    uint8_t data_len;          // Response data length (1-8 bytes)
    uint8_t data[MAX_OBD_DATA_LENGTH]; // Response data
} __attribute__((packed)) obd_pid_t;

/**
 * Session start metadata
 */
typedef struct {
    char session_id[MAX_SESSION_ID_LENGTH]; // ISO timestamp or UUID
    uint8_t firmware_version[3]; // [major, minor, patch]
    uint8_t gps_module_type;   // 0=ATGM336H, 1=NEO-6M, 2=NEO-8M
    uint8_t accel_module_type; // 0=LIS3DH, 1=MPU6050
    uint16_t config_flags;     // Bitfield of enabled features
} __attribute__((packed)) session_start_t;

/**
 * Session end statistics
 */
typedef struct {
    uint32_t total_messages;   // Total messages logged
    uint32_t dropped_messages; // Messages lost (overflow)
    uint32_t duration_sec;     // Session duration in seconds
    uint32_t file_size_bytes;  // Total bytes written
} __attribute__((packed)) session_end_t;

/**
 * Error event
 */
typedef struct {
    uint8_t error_code;        // Error code identifier
    uint8_t sensor_source;     // Which sensor generated error
    char error_message[64];    // Human-readable error
} __attribute__((packed)) error_event_t;

/**
 * Main telemetry message structure
 * 
 * This is the unified message format that goes into the ring buffer
 * and gets written to the SD card log file.
 */
typedef struct {
    uint64_t timestamp_us;     // Microsecond timestamp
    uint8_t time_source;       // TIME_SOURCE_* flag
    uint8_t sensor;            // SENSOR_* identifier
    uint8_t data_type;         // DATA_* type
    uint16_t length;           // Payload length in bytes
    
    // Flexible payload - actual data follows this header
    // The payload will be one of the above data structures
    // or raw data (like NMEA string)
    uint8_t payload[];         
} __attribute__((packed)) telemetry_msg_t;

/**
 * Ring buffer metadata structure
 * 
 * Used for inter-core communication. Core 0 writes, Core 1 reads.
 */
typedef struct {
    uint32_t write_index;      // Current write position
    uint32_t read_index;       // Current read position
    uint32_t capacity;         // Buffer size in bytes
    uint32_t dropped_count;    // Messages dropped due to overflow
    volatile bool overflow;    // Flag indicating buffer overflow
} ring_buffer_meta_t;

// Helper macro to get message header size
#define TELEMETRY_MSG_HEADER_SIZE (sizeof(uint64_t) + sizeof(uint8_t) * 3 + sizeof(uint16_t))

// Calculate total message size including payload
#define TELEMETRY_MSG_TOTAL_SIZE(payload_len) (TELEMETRY_MSG_HEADER_SIZE + (payload_len))

#endif // TELEMETRY_TYPES_H
