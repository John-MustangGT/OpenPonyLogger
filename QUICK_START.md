# OpenPonyLogger Quick Start Guide

## Hardware Setup
- **Board**: Adafruit ESP32-S3 Feather TFT Reverse
- **Display**: ST7789 1.14" IPS TFT (240×135 pixels) - built-in
- **USB Port**: Connect via USB-C for programming, serial output, and power
- **Sensors** (STEMMA I2C connector):
  - PA1010D GPS (I2C @ 0x10)
  - ICM20948 9-DOF IMU (I2C @ 0x69)
  - MAX17048 Battery Fuel Gauge (I2C @ 0x36)
- **Battery**: LiPo battery connects to JST connector (optional)
- **Status LED**: Built-in WS2812B NeoPixel on GPIO33

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

## Button Controls

- **D0 (GPIO0)**: Pause/resume logging - also wakes from deep sleep
- **D1 (GPIO1)**: Cycle display modes (Main → Info → Dark)
- **D2 (GPIO2)**: Mark event in log data (for lap timing)

See [Button Control](docs/BUTTON_CONTROL.md) for detailed behavior.

## Power Management

The device monitors USB power on GPIO19 (VBUS):
- **USB connected**: Normal operation
- **USB disconnected for 60 seconds**: Automatic shutdown countdown
  - Purple pulsing NeoPixel during countdown
  - TFT displays seconds remaining
  - Enters deep sleep (~50µA current draw)
- **Wake from sleep**: USB reconnection or D0 button press

See [Power Management](docs/POWER_MANAGEMENT.md) for details.

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

### Hardware Settings
Main configuration in `src/main.cpp`:
- I2C bus: GPIO3 (SDA), GPIO4 (SCL) at 400kHz
- GPS: I2C mode (0x10 address)
- Sensor addresses: ICM20948 @ 0x69, MAX17048 @ 0x36
- Logger thread: Configurable 5-100Hz (default 10Hz)
- Status monitor: 10-second interval

### Units Display
Edit `platformio.ini` build flags:
```ini
-DUSE_IMPERIAL=1  # 1 = mph/°F, 0 = km/h/°C
```

### WiFi Access Point
Default SSID: `OpenPonyLogger`  
Default Password: `12345678`  
Access at: `http://192.168.4.1`

See [Display Guide](docs/DISPLAY_GUIDE.md) for units configuration details.

## Architecture

- **Core 0**: Status Monitor + Display + Storage
  - Status reporting every 10 seconds
  - TFT display updates
  - Flash storage management
  - WiFi access point (when enabled)
  
- **Core 1**: Real-Time Logger Thread
  - Continuous sensor polling at configurable rate (5-100Hz)
  - IMU, GPS, battery monitoring
  - Writes buffered data to storage

- **Deep Sleep**: ESP32 low-power mode (~50µA)
  - Triggered by USB power loss after 60-second countdown
  - Wake sources: USB reconnection or D0 button press

This dual-core architecture ensures real-time sensor acquisition is never blocked by display updates or WiFi operations.

See [RT Logger Architecture](docs/RT_LOGGER_ARCHITECTURE.md) for implementation details.
