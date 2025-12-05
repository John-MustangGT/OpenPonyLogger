# OpenPonyLogger - Core Framework

## Overview

This is the foundational framework for the OpenPonyLogger dual-core telemetry system. It implements the basic producer-consumer architecture with lock-free inter-core communication.

## Architecture

### Dual-Core Design

**Core 0 (Producer - Data Acquisition)**
- Polls sensors at configured rates (GPS: 10Hz, Accelerometer: 10Hz, OBD-II: variable)
- Timestamps all data with microsecond precision
- Writes to lock-free ring buffer for Core 1

**Core 1 (Consumer - Processing & Storage)**
- Reads from ring buffer
- Maintains live telemetry state for web interface
- Buffers data and writes to SD card
- Manages log file rotation (15-minute files)
- Serves WiFi AP and HTTP endpoints (TODO)

### Inter-Core Communication

Single unified 64KB ring buffer with typed messages:
- Thread-safe lock-free implementation
- Atomic read/write operations
- Message types: GPS, Accelerometer, OBD-II, System events
- Microsecond timestamps applied at data acquisition

## File Structure

```
.
├── telemetry_types.h      # Core data structures and message types
├── ring_buffer.h          # Lock-free ring buffer implementation
├── core0_producer.c       # Core 0 - Data acquisition
├── core1_consumer.c       # Core 1 - Processing and storage
├── main.c                 # Main entry point and system initialization
└── CMakeLists.txt         # Build configuration
```

## Key Features Implemented

### Ring Buffer
- ✅ 64KB capacity (~30+ seconds of buffering)
- ✅ Lock-free single producer, single consumer
- ✅ Atomic operations for thread safety
- ✅ Overflow detection and dropped message counting
- ✅ Runtime statistics

### Core 0 (Data Acquisition)
- ✅ Scheduling loop with configurable sensor rates
- ✅ Per-PID polling intervals for OBD-II
- ✅ Microsecond timestamp capture
- ✅ Support for adding/removing custom PIDs
- ✅ Message statistics tracking

### Core 1 (Storage & Processing)
- ✅ 4KB write buffer for SD card
- ✅ Time-based flush (1 minute) or size-based flush (4KB)
- ✅ 15-minute log file rotation
- ✅ Configuration header in each log file
- ✅ Live telemetry state for web interface
- ✅ Session start/end markers

### Message Types Supported
- ✅ GPS NMEA sentences
- ✅ GPS fix data (parsed)
- ✅ Accelerometer XYZ + Gyroscope data
- ✅ OBD-II PID responses
- ✅ Session start/end events
- ✅ System error events

## Building the Project

### Prerequisites

1. **Pico SDK** - Install from https://github.com/raspberrypi/pico-sdk
2. **ARM GCC Toolchain** - `arm-none-eabi-gcc`
3. **CMake** - Version 3.13 or higher
4. **Build Tools** - Make or Ninja

### Setup

```bash
# Clone the repository
git clone https://github.com/John-MustangGT/OpenPonyLogger.git
cd OpenPonyLogger

# Set PICO_SDK_PATH environment variable
export PICO_SDK_PATH=/path/to/pico-sdk

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make -j4
```

### Flashing

1. Hold BOOTSEL button on Pico while connecting USB
2. Copy `OpenPonyLogger.uf2` to the mounted `RPI-RP2` drive
3. Pico will automatically reboot and start running

## Current Status

### Implemented ✅
- Dual-core architecture
- Lock-free ring buffer
- Message type system
- Basic scheduling loops
- SD card logging framework
- Live telemetry state management
- Statistics tracking

### TODO - Hardware Interfaces 🚧
- [ ] GPS UART initialization and NMEA parsing
- [ ] LIS3DH I2C driver for accelerometer
- [ ] BLE stack for OBD-II communication
- [ ] SD card SPI initialization (FatFS integration)
- [ ] WiFi AP mode setup
- [ ] HTTP server for web interface

### TODO - Features 🎯
- [ ] Web interface (live dashboard)
- [ ] AJAX endpoints for telemetry data
- [ ] Session management (download, delete)
- [ ] Configuration web page
- [ ] Circular logging with storage watermarks
- [ ] OTA firmware updates

## Configuration

### Adjustable Parameters

**Core 0 Polling Rates** (in `core0_producer.c`):
```c
#define GPS_POLL_INTERVAL_US      100000  // 100ms = 10Hz
#define ACCEL_POLL_INTERVAL_US    100000  // 100ms = 10Hz
#define OBD_POLL_INTERVAL_US      500000  // 500ms = 2Hz
```

**Per-PID OBD-II Rates**:
```c
// High-priority: 10Hz
{0x01, 0x0C, 100000, 0, true},  // RPM
{0x01, 0x0D, 100000, 0, true},  // Speed
{0x01, 0x11, 100000, 0, true},  // Throttle

// Medium-priority: 2Hz
{0x01, 0x04, 500000, 0, true},  // Engine load

// Low-priority: 0.2Hz
{0x01, 0x2F, 5000000, 0, true}, // Fuel level
```

**Core 1 Logging** (in `core1_consumer.c`):
```c
#define LOG_BUFFER_SIZE 4096           // 4KB write buffer
#define LOG_FLUSH_INTERVAL_US 60000000 // 1 minute
#define LOG_FILE_DURATION_SEC 900      // 15 minutes
```

## Memory Usage

**Flash (RP2350)**
- Firmware: ~100KB (estimated with current code)
- Web assets: TODO (will be embedded)
- Total: <1MB (plenty of room in 4MB flash)

**RAM (520KB total)**
- Ring buffer: 64KB
- Log write buffer: 4KB
- Stack/heap: ~50KB
- lwIP TCP/IP stack: ~50KB (TODO)
- BLE stack: ~100KB (TODO)
- Remaining: ~250KB for application use

## Debugging

### USB Serial Output

The firmware is configured to output debug information via USB serial:

```bash
# Linux/Mac
screen /dev/ttyACM0 115200

# Windows (use PuTTY or similar)
# COM port will vary
```

Expected output:
```
=== OpenPonyLogger v1.0.0 ===
Initializing hardware...
Ring buffer initialized: 65536 bytes
Hardware initialization complete
Launching Core 0 (data acquisition)...
[Core 0] Starting data acquisition core
Starting Core 1 (processing and storage)...
[Core 1] Starting processing and storage core
[Core 1] Initializing SD card...
[Core 1] SD card mounted successfully
[Core 1] Opened new log file: log_0001.opl

=== System Status ===
Ring Buffer: 1234 bytes used, 64302 bytes free
Messages dropped: 0
Core 0 - GPS: 150, Accel: 150, OBD: 30 messages
Core 1 - Processed: 330, Written: 16384 bytes, SD: OK
==================
```

### Statistics

Both cores provide statistics functions:
- `core0_get_stats()` - Messages sent by type
- `core1_get_stats()` - Messages processed, bytes written, SD status
- `ring_buffer_get_stats()` - Buffer usage, overflow flags, dropped count

## Data Rate Estimates

Based on 10Hz GPS, 10Hz accelerometer, ~2Hz OBD-II average:

- GPS NMEA: ~82 bytes × 10Hz = 820 bytes/sec
- GPS fix: ~32 bytes × 10Hz = 320 bytes/sec
- Accelerometer: ~24 bytes × 10Hz = 240 bytes/sec
- OBD-II: ~20 bytes × 2Hz = 40 bytes/sec
- Message overhead: ~15 bytes × 32Hz = 480 bytes/sec

**Total: ~1.9 KB/sec**

With 4KB flush buffer, writes occur every ~2 seconds (or 60 seconds, whichever comes first).

## Log File Format

### Structure

```
[Configuration Header]
[Data Message 1]
[Data Message 2]
...
[Data Message N]
[Session End Marker]
```

### Configuration Header
```c
struct {
    uint8_t version[3];      // Firmware version [major, minor, patch]
    uint32_t config_size;    // Size of this config block
    uint16_t gps_rate_hz;    // GPS sample rate
    uint16_t accel_rate_hz;  // Accelerometer sample rate
    uint16_t obd_rate_hz;    // OBD-II average sample rate
    char device_name[32];    // Device identifier
    uint8_t flags;           // Feature flags
}
```

### Data Messages
Each message contains:
- 64-bit microsecond timestamp
- Time source flag (uptime/GPS/RTC)
- Sensor identifier
- Data type
- Payload length
- Variable-length payload

### Post-Processing

Binary log files can be converted to CSV or other formats using post-processing tools (TODO).

## Next Steps

1. **Hardware Driver Integration**
   - GPS UART with DMA-backed ring buffer
   - LIS3DH I2C driver
   - BLE stack initialization
   - SD card FatFS integration

2. **Web Interface**
   - WiFi AP mode setup
   - HTTP server with lwIP
   - Live telemetry AJAX endpoint
   - Session management interface

3. **Testing & Validation**
   - Unit tests for ring buffer
   - SD card write performance testing
   - Multi-sensor concurrent operation
   - Field testing in vehicle

## Contributing

This is an open-source project. Contributions welcome for:
- Hardware driver implementations
- Web interface development
- Post-processing tools
- Documentation improvements
- Bug fixes and optimizations

## License

MIT License - See LICENSE file for details

## Credits

**Project Lead:** John Orthoefer  
**Target Vehicle:** 2014 Ford Mustang GT "Ciara"  
**Hardware:** Raspberry Pi Pico 2W (RP2350)
