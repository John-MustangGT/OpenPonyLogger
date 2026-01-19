# OpenPonyLogger

## Project Overview

OpenPonyLogger is an open-source automotive telemetry system designed for track day data logging and analysis. Built on the ESP32-S3 platform, it provides professional-grade data acquisition at a fraction of commercial system costs.

## Features

### Hardware
- **ESP32-S3 Feather TFT** - Dual-core processor, built-in 1.14" IPS display, USB-C
- **9-DOF IMU** - ICM20948 (accelerometer, gyroscope, magnetometer)
- **GPS** - PA1010D with 10Hz update rate
- **Battery Management** - MAX17048 fuel gauge with charge/discharge monitoring
- **Storage** - 8MB flash partition for logging (expandable)
- **Display** - 240×135 ST7789 TFT with NeoPixel status LED

### Capabilities
- **Real-time logging** - Configurable 5-100Hz sample rates
- **Dual-core architecture** - Core 0: display/storage, Core 1: sensors
- **WiFi access point** - Download data via web browser (no internet required)
- **Power management** - USB power detection with automatic deep sleep (~50µA)
- **Button control** - Pause/resume, display modes, event markers
- **Visual feedback** - Color-coded NeoPixel status (GPS fix, paused, shutdown)
- **Binary format** - Efficient .opl files with session headers

### Data Collection
- 3-axis acceleration (±16g)
- 3-axis gyroscope (±2000 dps)
- 3-axis magnetometer/compass
- GPS position, altitude, speed, time
- Battery voltage, current, state-of-charge
- Configurable units (Imperial/Metric)
- Event markers for lap timing

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

For contributor workflow, build steps, and repo hygiene, see [CONTRIBUTING.md](CONTRIBUTING.md).

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

### Getting Started
- **[Quick Start Guide](QUICK_START.md)** - Build, upload, and monitor the device
- **[Display Guide](docs/DISPLAY_GUIDE.md)** - Display layout, units configuration, and NeoPixel status indicators
- **[Button Control](docs/BUTTON_CONTROL.md)** - Button functions, debouncing, and event marking

### Hardware Reference
- **[GPIO Pin Mapping](docs/GPIO_PIN_MAPPING.md)** - Complete ESP32-S3 pin allocation and peripheral connections
- **[Power Management](docs/POWER_MANAGEMENT.md)** - USB power detection, deep sleep, and wake-up behavior
- **[Bill of Materials](docs/BOM.md)** - Hardware components and specifications

### Software Architecture
- **[RT Logger Architecture](docs/RT_LOGGER_ARCHITECTURE.md)** - Real-time logger thread design and dual-core architecture
- **[Design Overview](docs/Design.md)** - Overall system design and component interaction

### Data Format
- **[Log Format Specification](docs/LOG_FORMAT.md)** - Logging partition structure, block headers, and session management
- **[OpenPony Binary Format](docs/OPENPONY_BINARY_FORMAT.md)** - .opl file format for data export
- **[Record Schema](docs/RECORD_SCHEMA.md)** - Binary record structures (IMU, GPS, CAN, COMPASS)

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

**Document Version:** 2.0  
**Last Updated:** January 19, 2026  
**Status:** Active Development - V2 Prototype Functional
