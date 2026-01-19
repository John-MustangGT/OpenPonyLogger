# GPIO Pin Mapping - OpenPonyLogger

## Overview
This document provides a comprehensive mapping of all GPIO pins used in the OpenPonyLogger project for the **Adafruit ESP32-S3 Feather TFT Reverse** board.

---

## Pin Allocation Summary

| GPIO | Function | Direction | Interface | Notes |
|------|----------|-----------|-----------|-------|
| **GPIO0** | Button D0 (Bottom) | Input (Pull-up) | Digital | Pause/Resume Storage (HIGH→LOW when pressed) / Deep sleep wake |
| **GPIO1** | Button D1 (Middle) | Input | Digital | Cycle Display Mode (LOW→HIGH when pressed) |
| **GPIO2** | Button D2 (Top) | Input | Digital | Mark Event (LOW→HIGH when pressed) |
| **GPIO3** | I2C SDA | Bidirectional | I2C | STEMMA QT connector (400kHz) |
| **GPIO4** | I2C SCL | Output | I2C | STEMMA QT connector (400kHz) |
| **GPIO7** | I2C/TFT Power Enable | Output | Digital | Shared power for I2C bus and TFT (hold HIGH) |
| **GPIO13** | Red LED | Output | Digital | Status indicator LED (active HIGH) |
| **GPIO16** | GPS RX | Input | UART | GPS receiver (UART mode) - Serial1 |
| **GPIO17** | GPS TX | Output | UART | GPS transmitter (UART mode) - Serial1 |
| **GPIO19** | USB Power Detect (VBUS) | Input | Digital | USB power detection (HIGH = USB powered) / Deep sleep wake |
| **GPIO33** | NeoPixel | Output | WS2812B | Built-in status LED (GRB + 800kHz) |
| **GPIO35** | SPI MOSI | Output | SPI (HSPI) | Hardware SPI for TFT display |
| **GPIO36** | SPI CLK | Output | SPI (HSPI) | Hardware SPI clock for TFT display |
| **GPIO37** | SPI MISO | Input | SPI (HSPI) | Hardware SPI (currently unused, reserved) |
| **GPIO40** | TFT DC | Output | SPI | TFT Data/Command select |
| **GPIO41** | TFT RST | Output | SPI | TFT Hardware Reset |
| **GPIO42** | TFT CS | Output | SPI | TFT Chip Select |
| **GPIO45** | TFT Backlight | Output | Digital PWM | TFT backlight control |

---

## Detailed Pin Descriptions

### User Interface Pins

#### Buttons (Digital Input)
- **GPIO0** - Button D0 (Bottom button)
  - Function: Pause/Resume data logging to storage
  - Configuration: `INPUT_PULLUP` (HIGH by default, goes LOW when pressed)
  - Defined in: [src/main.cpp](../src/main.cpp#L29)

- **GPIO1** - Button D1 (Middle button)
  - Function: Cycle display mode (Main → Info → Dark → Main)
  - Configuration: `INPUT` (LOW by default, goes HIGH when pressed)
  - Defined in: [src/main.cpp](../src/main.cpp#L30)

- **GPIO2** - Button D2 (Top button)
  - Function: Mark event in data log
  - Configuration: `INPUT` (LOW by default, goes HIGH when pressed)
  - Defined in: [src/main.cpp](../src/main.cpp#L31)

#### Status Indicators
- **GPIO13** - Red LED
  - Function: Status indicator LED
  - Configuration: Digital output (active HIGH)
  - Note: Built-in red LED on ESP32-S3 Feather board

- **GPIO33** - NeoPixel (Built-in WS2812B RGB LED)
  - Function: Visual status indicator
  - Protocol: WS2812B (GRB color order, 800kHz)
  - States:
    - 🔴 Red (solid): System booting
    - 🟡 Yellow (1Hz flash): Searching for GPS fix
    - 🟢 Green (solid): GPS 3D fix acquired
    - 🟡 Yellow (0.2Hz flash): Data logging paused
    - 🟣 Purple (pulsing): USB power lost, shutdown countdown active
  - Defined in: [lib/Display/st7789_display.cpp](../lib/Display/st7789_display.cpp#L321)

---

### I2C Bus (STEMMA QT Connector)

- **GPIO3** - I2C SDA (Data)
  - Function: I2C data line
  - Peripherals connected:
    - PA1010D GPS Module (0x10) - when using I2C mode
    - ICM-20948 9-DOF IMU (0x69)
    - MAX17048 Battery Monitor (0x36)
  - Speed: 400kHz
  - Defined in: [src/main.cpp](../src/main.cpp#L21)

- **GPIO4** - I2C SCL (Clock)
  - Function: I2C clock line
  - Speed: 400kHz
  - Defined in: [src/main.cpp](../src/main.cpp#L22)

- **GPIO7** - I2C/TFT Power Enable
  - Function: Power enable for I2C bus and TFT display
  - Configuration: Output, held HIGH to enable power
  - Shared between STEMMA QT and TFT display
  - Defined in: [src/main.cpp](../src/main.cpp#L23)

---

### Power Management

- **GPIO19** - VBUS Detection (USB Power)
  - Function: Detects USB power presence
  - Configuration: Input (HIGH = USB powered, LOW = battery only)
  - Features:
    - Monitors USB connection status
    - Triggers 60-second shutdown countdown when USB power lost
    - Wake source for deep sleep (rising edge = USB restored)
  - Power states:
    - USB present: Normal operation
    - USB lost >60s: Graceful shutdown → deep sleep
    - Deep sleep current: ~50 µA (weeks of battery life)
  - Defined in: [src/main.cpp](../src/main.cpp#L35)

**Deep Sleep Wake Sources:**
1. GPIO19 (VBUS) rising edge → USB power restored → Fresh logging session
2. GPIO0 (D0 button) falling edge → User button → Boot with logging paused

---

### UART (GPS Module - Alternative to I2C)

- **GPIO16** - GPS RX
  - Function: UART receive from PA1010D GPS
  - Interface: Serial1
  - Baud rate: 9600
  - Note: Only used when `GPS_USE_I2C = false`
  - Defined in: [src/main.cpp](../src/main.cpp#L20)

- **GPIO17** - GPS TX
  - Function: UART transmit to PA1010D GPS
  - Interface: Serial1
  - Baud rate: 9600
  - Note: Only used when `GPS_USE_I2C = false`
  - Defined in: [src/main.cpp](../src/main.cpp#L19)

---

### SPI Bus (Hardware HSPI - TFT Display)

The ESP32-S3 uses dedicated hardware SPI pins for the TFT display:

- **GPIO35** - SPI MOSI (Master Out, Slave In)
  - Function: SPI data output to TFT
  - Interface: Hardware HSPI (automatically configured by Adafruit_ST7789 library)
  - Documented in: [lib/Display/include/st7789_display.h](../lib/Display/include/st7789_display.h#L24)

- **GPIO36** - SPI CLK (Clock)
  - Function: SPI clock signal for TFT
  - Interface: Hardware HSPI (automatically configured by Adafruit_ST7789 library)
  - Documented in: [lib/Display/include/st7789_display.h](../lib/Display/include/st7789_display.h#L23)

- **GPIO37** - SPI MISO (Master In, Slave Out)
  - Function: SPI data input (currently unused by TFT)
  - Status: Reserved for future use (SD card or other SPI peripherals)
  - Interface: Hardware HSPI

---

### TFT Display Control Pins

- **GPIO40** - TFT DC (Data/Command)
  - Function: Selects between data and command mode for TFT
  - HIGH = Data, LOW = Command
  - Defined in: [lib/Display/st7789_display.cpp](../lib/Display/st7789_display.cpp#L9)

- **GPIO41** - TFT RST (Reset)
  - Function: Hardware reset for ST7789 display controller
  - Active LOW reset
  - Defined in: [lib/Display/st7789_display.cpp](../lib/Display/st7789_display.cpp#L10)

- **GPIO42** - TFT CS (Chip Select)
  - Function: SPI chip select for TFT display
  - Active LOW
  - Defined in: [lib/Display/st7789_display.cpp](../lib/Display/st7789_display.cpp#L8)

- **GPIO45** - TFT Backlight
  - Function: PWM control for TFT backlight brightness
  - HIGH = On, LOW = Off
  - Can be PWM modulated for brightness control
  - Defined in: [lib/Display/st7789_display.cpp](../lib/Display/st7789_display.cpp#L11)

---

## Reserved Pins for Future Use

### Recommended for External GPS (UART)
If you want to add an external serial GPS module:
- **TX2/RX2** (Serial2) - Can be assigned to any available GPIO pins
- Recommended: Use GPIO8 and GPIO9 (currently available on the Feather breakout)

### SD Card (If Adding External Storage)
The SPI bus is already configured for the TFT. To add an SD card:
- **SPI MOSI**: GPIO35 (shared with TFT)
- **SPI MISO**: GPIO37 (shared, currently unused)
- **SPI CLK**: GPIO36 (shared with TFT)
- **SD CS**: Recommend **GPIO10** (separate chip select)

---

## I2C Device Addresses

| Device | I2C Address | Interface | Notes |
|--------|-------------|-----------|-------|
| PA1010D GPS | 0x10 | I2C | Only when GPS_USE_I2C = true |
| ICM-20948 IMU | 0x69 | I2C | 9-DOF (Accel, Gyro, Compass) |
| MAX17048 Battery Monitor | 0x36 | I2C | Fuel gauge |

---

## Wireless Interfaces (No GPIO pins)

### WiFi
- **Interface**: Internal ESP32-S3 WiFi radio
- **Mode**: Access Point (AP mode)
- **SSID**: "OpenPonyLogger"
- **Password**: "Mustang1234"
- **IP**: 192.168.4.1
- **Purpose**: Web interface for file download and configuration

### Bluetooth Low Energy (BLE)
- **Interface**: Internal ESP32-S3 BLE radio (NimBLE stack)
- **Mode**: Client (connects to OBD-II dongle)
- **Target Device**: vgate iCar 2 Pro
- **Purpose**: Receive OBD-II data from vehicle

---

## Pin Configuration Table by Function

### Power Management
| Pin | Function | State |
|-----|----------|-------|
| GPIO7 | I2C/TFT Power | HIGH (always enabled) |

### Digital Inputs
| Pin | Function | Default State | Active State |
|-----|----------|---------------|--------------|  
| GPIO0 | Button D0 (Pull-up) | HIGH | LOW (pressed) |
| GPIO1 | Button D1 | LOW | HIGH (pressed) |
| GPIO2 | Button D2 | LOW | HIGH (pressed) |

### Digital Outputs
| Pin | Function | Active State |
|-----|----------|--------------|
| GPIO7 | Power Enable | HIGH |
| GPIO13 | Red LED | HIGH (on) |
| GPIO45 | TFT Backlight | HIGH (on) |

### Communication Buses
| Bus | Pins | Speed/Config |
|-----|------|--------------|
| I2C | GPIO3 (SDA), GPIO4 (SCL) | 400kHz |
| UART1 | GPIO17 (TX), GPIO16 (RX) | 9600 baud (GPS) |
| SPI | GPIO35 (MOSI), GPIO36 (CLK), GPIO37 (MISO) | Hardware HSPI |

---

## Hardware Block Diagram

```
ESP32-S3 Feather TFT
┌─────────────────────────────────────────────────────────────┐
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ User Interface                                        │   │
│  │  - GPIO0: Button D0 (Pull-up, HIGH→LOW)              │   │
│  │  - GPIO1, 2: Buttons D1, D2 (LOW→HIGH)               │   │
│  │  - GPIO13: Red LED (Status)                          │   │
│  │  - GPIO33: NeoPixel Status LED (WS2812B)             │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ I2C Bus (GPIO3/SDA, GPIO4/SCL @ 400kHz)              │   │
│  │  - 0x10: PA1010D GPS (optional, I2C mode)            │   │
│  │  - 0x69: ICM-20948 IMU (Accel, Gyro, Compass)        │   │
│  │  - 0x36: MAX17048 Battery Monitor                    │   │
│  │  - GPIO7: Power Enable (HIGH)                        │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ UART (Serial1 @ 9600 baud)                           │   │
│  │  - GPIO17/TX, GPIO16/RX: PA1010D GPS (UART mode)     │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ SPI Bus (Hardware HSPI)                              │   │
│  │  - GPIO35: MOSI (Data Out)                           │   │
│  │  - GPIO36: CLK (Clock)                               │   │
│  │  - GPIO37: MISO (Data In) - Reserved                 │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ TFT Display (ST7789, 240x135)                        │   │
│  │  - GPIO42: CS (Chip Select)                          │   │
│  │  - GPIO40: DC (Data/Command)                         │   │
│  │  - GPIO41: RST (Reset)                               │   │
│  │  - GPIO45: Backlight                                 │   │
│  │  - GPIO7: Power Enable (shared with I2C)             │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Wireless (Internal - No GPIO)                        │   │
│  │  - WiFi: AP mode (192.168.4.1)                       │   │
│  │  - BLE: Client (connects to vgate iCar Pro)          │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## Configuration Notes

### GPS Communication Mode Selection
The PA1010D GPS can operate in either I2C or UART mode (selected in [src/main.cpp](../src/main.cpp#L35)):

```cpp
#define GPS_USE_I2C  true   // true = I2C mode, false = UART mode
```

- **I2C Mode** (default): Uses GPIO3/4 with address 0x10
- **UART Mode**: Uses GPIO16/17 with Serial1 @ 9600 baud

### Future Expansion

#### External Serial GPS
For adding a second GPS or external GPS module via UART:
- **Serial2** can be configured on any available GPIO pins
- Recommended pins: GPIO8 (TX2), GPIO9 (RX2)
- Add to main.cpp:
  ```cpp
  #define GPS2_TX_PIN  8
  #define GPS2_RX_PIN  9
  Serial2.begin(9600, SERIAL_8N1, GPS2_RX_PIN, GPS2_TX_PIN);
  ```

#### SD Card Storage
To add an SD card reader (shares SPI bus with TFT):
- **MOSI**: GPIO35 (shared with TFT)
- **MISO**: GPIO37 (currently unused)
- **CLK**: GPIO36 (shared with TFT)
- **CS**: GPIO10 (dedicated chip select for SD card)

---

## References

- [Adafruit ESP32-S3 Feather TFT Documentation](https://learn.adafruit.com/adafruit-esp32-s3-feather-tft)
- [ST7789 Display Driver](../lib/Display/include/st7789_display.h)
- [Main Application Code](../src/main.cpp)
- [Hardware Configuration](../platformio.ini)

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2026-01-19 | 1.0 | Initial GPIO pin mapping documentation |

---

**Last Updated**: January 19, 2026  
**Board**: Adafruit ESP32-S3 Feather TFT Reverse  
**Firmware**: OpenPonyLogger v1.0
