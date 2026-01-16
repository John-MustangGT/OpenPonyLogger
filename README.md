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

## Technical Documentation

For detailed information about the logging system, please refer to:

- **[Log Format Specification](docs/LOG_FORMAT.md)** - Complete logging format including block headers, session headers, NVS schema, and write/recovery semantics
- **[Record Schema](docs/RECORD_SCHEMA.md)** - Canonical C struct definitions for all record types (IMU, GPS, CAN, COMPASS)
- **[Project Checklist](docs/PROJECT_CHECKLIST.md)** - V2 prototype implementation tasks and design decisions

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
