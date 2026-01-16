# OpenPonyLogger Quick Start Guide

## Hardware Setup
- **Board**: Adafruit ESP32-S3 Feather TFT Reverse
- **USB Port**: Connect via USB-C for both programming and serial output
- **Sensors**:
  - PA1010D GPS (I2C @ 0x10, or UART on Serial1)
  - ICM20948 9-DOF IMU (I2C @ 0x69)
  - MAX17048 Battery Fuel Gauge (I2C @ 0x36)

## Building & Uploading

### Build
```bash
platformio run --environment esp32s3dev
```

### Upload Firmware
```bash
platformio run --target upload --environment esp32s3dev
```

**IMPORTANT**: After upload completes, press the **RESET** button on the Feather board to exit download mode and start the application.

### Monitor Serial Output
```bash
platformio device monitor --environment esp32s3dev --baud 115200
```

**Note**: The serial monitor will show several reconnection attempts as the USB JTAG stabilizes. This is normal.

## What to Expect

### Boot Messages
After reset, you'll see:
1. Connection dots (`.......................`) as USB stabilizes
2. OpenPonyLogger header
3. Initialization steps with checkmarks (`✓`) or errors (`✗`)
4. System Ready message

### Status Reports
Every 10 seconds, a formatted status box displays:
- **Uptime** (HH:MM:SS)
- **Write count** (number of sensor data writes)
- **GPS Status** (validity, latitude, longitude, altitude, satellite count)
- **IMU Data** (accelerometer, gyroscope, compass readings)
- **Battery Status** (SOC %, voltage, current, temperature)
- **Sample rate** (samples/second)

### Storage Write Events
Every 5 seconds, detailed sensor data is logged with timestamp.

## Troubleshooting

### No Serial Output
1. Ensure you pressed **RESET** button after upload
2. Check USB cable is properly connected
3. Verify baud rate is 115200
4. Try unplugging and replugging the USB cable

### "Device not configured" Error
- This is expected during initial connection
- The board will automatically reconnect

### Board Stuck in Download Mode
- Press the **RESET** button
- Or disconnect USB and reconnect

### Sensor Not Found
- Check I2C addresses match hardware configuration
- Verify SDA (GPIO21) and SCL (GPIO22) wiring
- For GPS UART mode: uncomment `#define GPS_USE_I2C false` in src/main.cpp

## Configuration

All hardware settings are in `src/main.cpp`:
- I2C pin mapping (SDA=21, SCL=22)
- GPS communication mode (I2C default, set to false for UART)
- Sensor I2C addresses
- Logger thread update rate (100ms default)
- Status report interval (10 seconds default)

## Architecture

- **Core 0**: Status Monitor Thread - Reports system health every 10 seconds
- **Core 1**: Real-Time Logger Thread - Continuously polls sensors at 100ms intervals
- **Main Loop**: Triggers storage write events every 5 seconds

This gives you real-time visibility into what the logger is doing without blocking sensor operations.
