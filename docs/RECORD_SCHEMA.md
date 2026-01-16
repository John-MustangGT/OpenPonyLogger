# Record Schema

This file defines the canonical C structs used for log record payloads. All records are little-endian and start with a common prefix: `uint8_t msg_type; uint64_t timestamp_offset_us;` 

// IMU record (0x01) - fixed size, 6 floats: accel_x/y/z (m/s^2) and gyro_x/y/z (deg/s)
```c
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;            // 0x01
    uint64_t timestamp_offset_us; // microseconds since session start
    float    accel_x;
    float    accel_y;
    float    accel_z;
    float    gyro_x;
    float    gyro_y;
    float    gyro_z;
} imu_record_t; // total: 1 + 8 + 24 = 33 bytes (packed)
```

// GPS record (0x02) - parsed fields
```c
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;            // 0x02
    uint64_t timestamp_offset_us; // microseconds since session start
    double   latitude;            // degrees (float64)
    double   longitude;           // degrees (float64)
    float    altitude_m;          // meters
    uint8_t  fix_type;            // 0=none,1=2D,2=3D
    uint8_t  num_sats;
    float    hdop;
} gps_record_t; // variable but fixed-size parsed format
```

// CAN frame record (0x03)
```c
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;            // 0x03
    uint64_t timestamp_offset_us; // microseconds since session start
    uint32_t can_id;              // 11- or 29-bit identifier (stored in 32-bit)
    uint8_t  dlc;                 // data length (0-8)
    uint8_t  flags;               // bitflags: bit0=extended_id, bit1=remote_frame, etc.
    uint8_t  data[8];             // CAN payload (0..8 valid bytes)
} can_record_t;
```

// Compass record (0x04)
```c
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;            // 0x04
    uint64_t timestamp_offset_us; // microseconds since session start
    float    bearing_deg;         // float32 degrees 0.0 - 360.0
} compass_record_t;
```

Notes:
- The record prefix (msg_type + timestamp_offset_us) allows the reader to interpret the payload.
- For storage efficiency an alternative is to use fixed-point integer encodings instead of float; stick with IEEE754 floats for simplicity in prototype.
