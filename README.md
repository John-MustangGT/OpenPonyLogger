# OpenPonyLogger

## Project Overview

OpenPonyLogger is an open-source automotive telemetry system designed for track day data logging and analysis. Built on a ESP32-S3 platform, it provides professional-grade data acquisition at a fraction of commercial system costs.

## Safety and Legal Considerations

### Automotive Safety
- Device should not interfere with vehicle operation
- Non-invasive OBD-II connection (read-only)
- Secure mounting to prevent projectile hazard
- Heat management in enclosed cabin environment
- Fuse protection on power input

### Data Privacy
- All data stored locally on device
- No cloud upload without user consent
- WiFi AP mode (no internet connectivity)
- User controls all data export and deletion

### Track Day Usage
- Verify track rules permit data logging devices
- Ensure mounting does not obstruct driver visibility
- Secure all wiring to prevent pedal interference
- Easy removal if required by event officials

## Contributing

This is an open-source project. Contributions welcome:
- Hardware improvements and alternatives
- Code optimization and bug fixes
- Additional sensor support
- Post-processing tools
- Documentation improvements
- Vehicle-specific CAN definitions

## License

MIT 

## Project Repository

GitHub: https://github.com/John-MustangGT/OpenPonyLogger

## Credits

**Project Lead:** John Orthoefer  
**Target Vehicle:** 2014 Ford Mustang GT (S197) "Ciara"  
**Inspiration:** Carroll Shelby's "foundation first" philosophy applied to data acquisition

## Appendix A: Message Type Definitions

### GPS NMEA Message (0x01)
```c
// Raw NMEA sentence stored as null-terminated string
// Example: "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
// Length varies: typically 60-82 bytes
```

### Accelerometer XYZ Message (0x02)
```c
typedef struct {
    float accel_x;  // G-force lateral
    float accel_y;  // G-force longitudinal  
    float accel_z;  // G-force vertical
    float gyro_x;   // Degrees/sec roll
    float gyro_y;   // Degrees/sec pitch
    float gyro_z;   // Degrees/sec yaw
} __attribute__((packed)) accel_data_t;
// Fixed length: 24 bytes
```

### OBD-II PID Message (0x03)
```c
typedef struct {
    uint8_t mode;      // OBD mode (typically 0x01)
    uint8_t pid;       // Parameter ID
    uint8_t data_len;  // Response data length
    uint8_t data[8];   // Response data (max 8 bytes)
} __attribute__((packed)) obd_data_t;
// Variable length: 11-19 bytes
```

### Session Start Message (0xF0)
```c
typedef struct {
    char session_id[32];  // ISO timestamp or UUID
    uint8_t gps_module;   // 0=NEO-6M, 1=NEO-8M
    uint8_t version[3];   // Firmware version [major.minor.patch]
} __attribute__((packed)) session_start_t;
// Fixed length: 35 bytes
```

### Session End Message (0xF1)
```c
typedef struct {
    uint32_t total_messages;  // Total messages logged
    uint32_t dropped_messages; // Messages lost (buffer overflow)
    uint32_t duration_sec;    // Session duration
} __attribute__((packed)) session_end_t;
// Fixed length: 12 bytes
```

## Appendix B: Ford Mustang S197 CAN Bus Information

### OBD-II Standard PIDs (Mode 0x01)

| PID | Description | Units | Formula |
|-----|-------------|-------|---------|
| 0x0C | Engine RPM | rpm | ((A*256)+B)/4 |
| 0x0D | Vehicle Speed | km/h | A |
| 0x11 | Throttle Position | % | A*100/255 |
| 0x05 | Coolant Temperature | °C | A-40 |
| 0x0F | Intake Air Temperature | °C | A-40 |
| 0x2F | Fuel Tank Level | % | A*100/255 |
| 0x46 | Ambient Air Temperature | °C | A-40 |

### Ford-Specific PIDs (Mode 0x22)

Research required - community databases available through:
- FORScan software
- OpenXC Ford platform
- Mustang6G forums
- S197 tuning communities

### High-Speed CAN (500 kbps) Expected Messages

**Engine Control:**
- Engine RPM, load, timing
- Fuel injection parameters
- Air/fuel ratio (if wideband equipped)

**Transmission:**
- Current gear
- Transmission temperature
- Torque converter lock status

**ABS/Stability:**
- Individual wheel speeds
- Brake pressure
- Yaw rate sensor
- Lateral acceleration sensor (if equipped)

**Steering:**
- Steering angle
- Steering rate
- Power steering pressure

## Appendix C: Useful Resources

### Pico Development
- [Raspberry Pi Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [Getting Started with Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)

### GPS Modules
- [NEO-6M Datasheet](https://www.u-blox.com/sites/default/files/products/documents/NEO-6_DataSheet_%28GPS.G6-HW-09005%29.pdf)
- [NMEA 0183 Protocol](https://www.nmea.org/content/STANDARDS/NMEA_0183_Standard)

### OBD-II & CAN
- [OBD-II PIDs Wikipedia](https://en.wikipedia.org/wiki/OBD-II_PIDs)
- [CAN Bus Tutorial](https://www.csselectronics.com/pages/can-bus-simple-intro-tutorial)
- [ISO 15765-2 (CAN for Diagnostics)](https://en.wikipedia.org/wiki/ISO_15765-2)

### Web Technologies
- [Bootstrap 5 Documentation](https://getbootstrap.com/docs/5.0/)
- [Plotly.js Documentation](https://plotly.com/javascript/)
- [lwIP TCP/IP Stack](https://www.nongnu.org/lwip/)

### Ford Mustang Resources
- [Mustang6G Forums](https://www.mustang6g.com/)
- [FORScan Official Site](https://forscan.org/)
- [S197 Forums](https://www.s197forum.com/)

---

**Document Version:** 1.1  
**Last Updated:** 2026-01-15  
**Status:** Planning Phase
