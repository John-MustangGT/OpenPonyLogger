# OpenPonyLogger

## Project Overview

OpenPonyLogger is an open-source automotive telemetry system designed for track day data logging and analysis. Built on a Raspberry Pi Pico 2W, it provides professional-grade data acquisition at a fraction of commercial system costs.

## System Architecture

### Hardware Platform
- **Microcontroller:** Raspberry Pi Pico 2W (RP2350)
- **GPS Module:** ATGM336H (development) / NEO-8M (production target)
- **Accelerometer/Gyro:** LIS3DH (I2C)
- **OBD-II Interface:** VGate iCar Pro ELM327 (Bluetooth LE) - V1
- **Storage:** MicroSD card PiCownbell Adatalogger 
- **Connectivity:** WiFi 802.11n (built-in AP mode)
- **CAN Interface:** MCP2515 CAN controller module - V2+

### Dual-Core Architecture

**Core 0 - Data Collection (Producer)**
- GPS UART reader with DMA-backed ring buffer
- LIS3DH I2C polling at 10Hz target
- BLE/OBD-II connection manager and PID requests
- Microsecond timestamp capture (`time_us_64()`)
- Writes timestamped messages to lock-free ring buffer

**Core 1 - Processing + Storage (Consumer)**
- Reads from inter-core ring buffer
- Maintains live telemetry state for web interface
- Packs data into binary message frames
- Manages SD card writes (batched for efficiency)
- Serves WiFi AP and HTTP/AJAX endpoints
- Handles status LED indicators

### Inter-Core Communication

Single unified ring buffer with typed messages:
- 64KB ring buffer size (~30+ seconds of buffering headroom)
- Tagged message types (GPS, Accelerometer, OBD-II, system events)
- Microsecond timestamps applied at data acquisition (Core 0)
- Lock-free ring buffer implementation for real-time performance

## Data Format

### Binary Message Structure
```c
typedef enum {
    MSG_GPS_NMEA = 0x01,
    MSG_ACCEL_XYZ = 0x02,
    MSG_OBD_PID = 0x03,
    MSG_SESSION_START = 0xF0,
    MSG_SESSION_END = 0xF1
} message_type_t;

typedef struct {
    uint64_t timestamp_us;  // Microsecond timestamp
    uint8_t msg_type;       // Message type from enum
    uint16_t length;        // Payload length
    uint8_t data[];         // Flexible array member (payload)
} __attribute__((packed)) telemetry_msg_t;
```

### Sample Rates
- **GPS:** 10Hz (target for NEO-8M)
- **Accelerometer:** 10Hz
- **OBD-II:** ~1-5Hz (ELM327 limitation)
- **CAN Bus:** 50-100Hz potential (V2+ with direct CAN)

### Data Rate Estimates
- GPS NMEA (10Hz): ~820 bytes/sec
- Accelerometer (10Hz): ~200 bytes/sec
- OBD-II (~2Hz): ~40 bytes/sec
- Message overhead: ~352 bytes/sec
- **Total: ~1.4 KB/sec**

## Storage Management

### Circular Logging
- **High water mark:** 80% of SD card capacity
- **Low water mark:** 50% of SD card capacity
- **File duration:** 5 minutes per file
- **Deletion strategy:** Delete oldest files first when high water reached

### Storage Capacity (32GB card example)
- 25.6GB usable (80% mark)
- ~420KB per 5-minute file
- ~61,000 files or 212+ days of continuous logging

### File Naming Convention
```
YYYYMMDD_HHMMSS.opl
```
Examples: `20250315_143027.opl`

### Write Strategy
- 4KB write buffer
- Flush conditions: 4KB threshold OR 5-second timeout (whichever first)
- Guarantees maximum 5 seconds of data loss on power failure
- Immediate flush on session end

### SD Card Format
- **Format:** FAT32 (cards ≤32GB) or exFAT (cards >32GB)
- **Interface:** SPI
- **Library:** FatFS (included in Pico SDK)

## Web Interface

### Network Configuration
- **Mode:** WiFi Access Point (AP mode)
- **SSID:** OpenPonyTelemetry
- **IP Address:** 192.168.25.1 (standard AP default)
- **Purpose:** Configuration, live monitoring, data download

### Real-Time Dashboard

**Technology Stack:**
- **Framework:** CSS Grid layout
- **Visualization:** https://cdn.rawgit.com/Mikhus/canvas-gauges/gh-pages/download/2.1.7/all/gauge.min.js
- **Communication:** AJAX polling (250-500ms intervals)
- **Embedding:** All assets compiled into firmware (no internet required)

**Dashboard Features:**
- Live automotive-style gauges (speed, RPM, throttle)
- G-force visualization (lateral, longitudinal, vertical)
- Real-time line charts (last 30 seconds history)
- Recording status indicator
- SD card space gauge
- Session duration timer

**API Endpoint:**
```
GET /api/live

Response (JSON):
{
  "gps": {"speed": 67.3, "lat": 42.1234, "lon": -71.5678},
  "accel": {"x": 0.85, "y": -0.32, "z": 1.02},
  "obd": {"rpm": 4250, "throttle": 78.5, "temp": 195},
  "system": {"recording": true, "sd_pct": 23, "session_time": 127}
}
```

### Data Management Interface

**Session List Features:**
- Display all logged sessions with timestamp, size, duration
- Checkbox multi-select for batch operations
- Download as ZIP file (single or multiple sessions)
- "Delete after download" checkbox option
- Storage usage gauge with percentage display

**HTTP Endpoints:**
```
GET  /sessions          - List all session files
GET  /download?files=[] - Download selected sessions as ZIP
POST /delete            - Delete selected sessions
```

## Physical Installation

### Enclosure Design
- **Location:** Center dash/console area (near windshield for GPS)
- **Mounting:** Industrial-strength velcro for easy removal
- **Material:** 3D printed housing

**Enclosure Features:**
- Ventilation holes for heat dissipation
- SD card access slot (no disassembly required)
- Visible LED status indicators
- Cable grommet for Cat5 entry
- Flat back surface for velcro mounting
- GPS antenna flush-mount or integrated on top

### Sensor Placement Rationale
- **Accelerometer:** Center of vehicle (near CG) minimizes rotational effects
- **GPS antenna:** Clear sky view through windshield
- **Consolidated design:** All components in single enclosure

### Wiring

**OBD-II to Enclosure via Cat5:**
- **Pair 1 (Twisted):** CAN-H / CAN-L (for V2+ CAN bus)
- **Pair 2 (Twisted):** +12V / GND (power delivery)
- **Pair 3-4:** Spare (future expansion)

**OBD-II Port Pinout:**
- Pin 4: Chassis Ground
- Pin 6: CAN High (CAN-H) - HS-CAN bus
- Pin 14: CAN Low (CAN-L) - HS-CAN bus
- Pin 16: Battery +12V (switched with ignition)

**Cable Run:**
- From OBD-II port (driver footwell)
- Under dash/trim
- Up to center console/dash area
- Approximately 4-6 feet total length

**Power Regulation:**
- LM2596 buck converter (12V → 5V)
- Handles automotive voltage fluctuations (11-14.5V)
- Powers Pico 2W and all peripherals

### CAN Bus Specifications
- **Distance Limit:** ~40 meters at 500 kbps (High-Speed CAN)
- **Cable:** Cat5/Cat5e twisted pair (one pair for CAN-H/CAN-L)
- **Termination:** 120Ω resistors at each end of bus
- **Shielding:** STP (shielded twisted pair) recommended for automotive EMI environment

## Development Roadmap

### Version 1.0 (Initial Release)

**Core Functionality:**
- ✅ Dual-core data acquisition and logging
- ✅ GPS at 10Hz
- ✅  accelerometer/gyro at 10Hz
- ✅ OBD-II via VGate ELM327 Bluetooth (~1-5Hz)
- ✅ Binary message format with microsecond timestamps
- ✅ SD card circular logging with 5-minute files
- ✅ 80%/50% storage watermark management

**User Interface:**
- ✅ WiFi Access Point mode
- ✅ Live telemetry display (gauges and graphs)
- ✅ Session list with download capability
- ✅ Batch download as ZIP
- ✅ "Delete after download" option
- ✅ Storage usage display

**Firmware Management:**
- ✅ BOOTSEL button USB firmware updates (hold on power-up)

### Version 2.0 (Enhanced Features)

**CAN Bus Integration:**
- Direct CAN bus access via MCP2515 module
- Eliminates ELM327 Bluetooth dependency
- 50-100Hz sample rates (10-20x faster than OBD-II)
- Access to extended PIDs not available via OBD-II:
  - Steering angle
  - Brake pressure
  - Individual wheel speeds
  - Transmission gear position
  - Body control module data

**Configuration Interface:**
- Web-based configuration page
- Selectable OBD-II/CAN PIDs to log
- Adjustable sample rates per sensor
- GPS module configuration
- Storage management settings

**Data Export:**
- CSV export for spreadsheet analysis
- RaceRender format support
- Harry's Lap Timer compatibility
- TrackAddict format support

**Advanced Visualization:**
- GPS track map overlay on dashboard
- Lap time detection and display
- Session comparison tools

### Version 2.5+ (Future Enhancements)

**Firmware:**
- Over-The-Air (OTA) updates via WiFi
- Firmware validation and rollback
- Update without USB cable requirement

**Hardware Expansion:**
- Brake pressure sensor input
- Steering angle sensor (if not via CAN)
- External temperature probes
- Tire pressure monitoring integration

**Software Features:**
- Automatic lap timing
- Predictive lap time based on current session
- Data streaming to phone/tablet app
- Cloud upload capability (optional)

## Technical Implementation Notes

### Ford Mustang Specific Details (2014 GT - S197)

**CAN Bus Architecture:**
- **HS-CAN:** 500 kbps (High-Speed) - Engine, transmission, ABS, steering
- **MS-CAN:** 125 kbps (Medium-Speed) - Body control, HVAC, gauges
- **OBD-II Access:** HS-CAN exposed on pins 6 & 14

**Reverse Engineering Required:**
- Ford uses proprietary CAN message IDs
- Community resources available:
  - FORScan forums
  - OpenXC project (Ford CAN definitions)
  - Mustang tuning communities

**Known Data Available:**
- Standard OBD-II PIDs (via ELM327 or CAN)
- Ford-specific Mode $22 PIDs
- Custom CAN messages (requires sniffing/decoding)

### Development Tools & Libraries

**Pico SDK:**
- Official Raspberry Pi Pico C/C++ SDK
- Hardware abstraction layers
- FatFS filesystem library
- lwIP TCP/IP stack for WiFi
- Multicore support with FIFOs

**CAN Libraries:**
- MCP2515 driver libraries available
- CAN frame parsing utilities
- DBC file support for message definitions

**Web Stack:**
- Embedded web server (lwIP httpd)
- Static file serving from flash
- JSON generation for API responses

### Performance Considerations

**Core 0 Timing:**
- GPS UART: DMA-driven, minimal CPU overhead
- LIS3DH I2C: ~1ms per read at 400kHz
- BLE stack: Asynchronous, event-driven
- Ring buffer writes: <10µs per message

**Core 1 Timing:**
- Ring buffer reads: ~5µs per message
- SD card writes: 10-50ms (batched every 5 seconds)
- HTTP responses: <1ms for JSON serialization
- WiFi stack: Interrupt-driven, minimal polling

**Memory Budget:**
- Code + web assets: ~4MB (in flash)
- Ring buffer: 64KB (in RAM)
- Write buffer: 4KB (in RAM)
- lwIP stack: ~50KB (in RAM)
- Remaining RAM: ~400KB for application use

### Post-Processing Tools (Future)

**Binary to CSV Converter:**
- Read `.opl` binary files
- Parse message types
- Output separate CSV files per data type
- Merge on timestamp for unified dataset

**Data Analysis:**
- Python scripts for common analyses
- G-force heatmaps
- Speed/RPM correlation
- Lap time extraction
- Acceleration/braking metrics

**Visualization:**
- Google Earth KML export
- Track map overlay generator
- Video overlay synchronization tools

## Bill of Materials (BOM)

### Version 1.0 Components

| Component | Part Number/Description | Quantity | Est. Cost | Status |
|-----------|------------------------|----------|-----------|--------|
| Raspberry Pi Pico 2W | RP2350 with WiFi | 1 | $7 | ✅ Have |
| PiCowBell Adalogger | SD Card Reader w/ RTC | 1 | $12 | Have |
| GPS Module | ATGM336H | 1 | $10-15 | On Order |
| Accelerometer | LIS3DH breakout | 1 | $5 | ⏳ Order |
| BLE OBD-II | VGate iCar Pro | 1 | $25 | ✅ Have |
| SD Card Module | MicroSD SPI breakout | 1 | $5 | ⏳ Order |
| MicroSD Card | 16-32GB, FAT32 | 1+ | $10 | ✅ Have (reformatted) |
| Enclosure | 3D printed housing | 1 | Filament | TBD Design |
| Velcro Strips | Industrial strength | 1 set | $5 | TBD |

**V1 Total Estimated Cost:** ~$75-80

### Version 2.0+ Additional Components

| Component | Part Number/Description | Quantity | Est. Cost |
|-----------|------------------------|----------|-----------|
| CAN Controller | MCP2515 CAN module | 1 | $8 |
| GPS Upgrade | NEO-8M module | 1 | $20 |
| Status LEDs | RGB LED or multi-color | 1-3 | $2 |

## Development Environment Setup

### Prerequisites
- Raspberry Pi Pico SDK (C/C++)
- CMake build system
- ARM GCC toolchain
- OpenOCD or picotool for flashing
- Serial terminal (screen, minicom, PuTTY)

### Build Instructions
```bash
# Clone repository
git clone https://github.com/John-MustangGT/OpenPonyLogger.git
cd OpenPonyLogger

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build firmware
make -j4

# Flash to Pico (BOOTSEL mode)
cp OpenPonyLogger.uf2 /media/[user]/RPI-RP2/
```

### Development Phases

**Phase 1: Hardware Bring-Up**
1. SD card mount and basic file I/O
2. Ring buffer implementation and inter-core communication
3. GPS UART reading and NMEA parsing
4. LIS3DH I2C communication
5. Basic data logging to SD card

**Phase 2: Connectivity**
1. WiFi AP mode initialization
2. lwIP HTTP server setup
3. Static web page serving
4. AJAX endpoint implementation
5. Live telemetry structure

**Phase 3: Integration**
1. BLE stack initialization
2. ELM327 connection and PID requests
3. Unified data collection on Core 0
4. Complete logging pipeline
5. Circular file management

**Phase 4: User Interface**
1. Bootstrap + Plotly.js dashboard
2. Real-time gauge implementation
3. Session list page
4. Download/delete functionality
5. Storage management display

**Phase 5: Polish**
1. Error handling and recovery
2. Status LED patterns
3. Performance optimization
4. Documentation and examples
5. Field testing and refinement

## Testing Strategy

### Unit Testing
- Ring buffer stress tests
- SD card write performance
- Message serialization/deserialization
- GPS NMEA parser validation
- LIS3DH calibration

### Integration Testing
- Multi-sensor simultaneous operation
- WiFi + logging concurrent operation
- Storage watermark triggering
- Session file rollover
- Power loss recovery

### Field Testing
- Autocross/track day validation
- Extended runtime stability
- Temperature stress testing
- Vibration resistance
- Real-world data quality assessment

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

**Document Version:** 1.0  
**Last Updated:** 2025-12-02  
**Status:** Planning Phase
