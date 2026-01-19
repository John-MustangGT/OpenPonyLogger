# Display & Units Configuration Guide

## Hardware Overview

**Display:** ST7789 TFT 1.14" IPS LCD  
**Resolution:** 240×135 pixels  
**Interface:** SPI (Hardware HSPI)  
**Controller:** Adafruit ESP32-S3 Feather TFT Reverse  

### Pin Configuration
- **SPI MOSI**: GPIO35 (auto-configured)
- **SPI CLK**: GPIO36 (auto-configured)
- **TFT_CS**: GPIO42
- **TFT_DC**: GPIO40 (Data/Command)
- **TFT_RST**: GPIO41 (Reset)
- **TFT_BACKLITE**: GPIO45
- **TFT_I2C_POWER**: GPIO7 (shared with STEMMA I2C)

---

## Units System Configuration

### Current Setting
**Active:** Imperial (mph, °F)

### Switching Units
Edit `platformio.ini` build flags:

```ini
build_flags =
    -DUSE_IMPERIAL=1  # 1 = Imperial, 0 = Metric
```

**Imperial (DUSE_IMPERIAL=1):**
- Temperature: °F (Fahrenheit)
- Speed: mph (miles per hour)

**Metric (DUSE_IMPERIAL=0):**
- Temperature: °C (Celsius)
- Speed: km/h (kilometers per hour)

### Temperature Conversion
- **Formula:** °F = (°C × 9/5) + 32
- **Implementation:** `lib/Logger/include/units_helper.h`

### Speed Conversion
- GPS provides speed in knots
- **mph:** knots × 1.15078
- **km/h:** knots × 1.852

---

## Display Layout

### Main Screen (240×135)

```
┌─────────────────────────────────────┐
│ TIME: H:MM:SS    SAMPLES: 1.5K●    │  ← Row 1: Uptime & Sample count
│ A:+X.XX,+Y.XX,+Z.XX                 │  ← Row 2: Accelerometer (g)
│ G:+X.X,+Y.X,+Z.X                    │  ← Row 3: Gyroscope (dps)
│ LAT LON ALT                         │  ← Row 4: GPS coordinates
│ SPEED mph/kph                       │  ← Row 5: GPS speed
│ T:+XX.XF  B:XX%                     │  ← Row 6: Temp & Battery
│ [████████░░] 85%                    │  ← Battery bar
└─────────────────────────────────────┘
```

### Display Modes
Cycle through modes by pressing **D1 button**:

1. **MAIN_SCREEN**: Sensor data (default)
2. **INFO_SCREEN**: Network info (IP, BLE status)
3. **DARK**: Display off (backlight disabled)

---

## Display Features

### Fixed-Width Number Format
All numbers use sign-prefixed format to prevent text shifting:

**Accelerometer:**
- Format: `A:+X.XX,+Y.XX,+Z.XX`
- Always shows `+` or `-` sign
- Example: `A:+1.23,-0.45,+9.81`

**Gyroscope:**
- Format: `G:+X.X,+Y.X,+Z.X`
- Example: `G:+0.5,-1.2,+0.8`

### Auto-Scaling Sample Count
Large numbers are automatically scaled:

| Count | Display | Format |
|-------|---------|--------|
| 500 | `500` | Raw number |
| 1,500 | `1.5K` | Thousands |
| 50,000 | `50.0K` | Thousands |
| 1,500,000 | `1.5M` | Millions |

### Temperature Color Coding

| Range | Color | Status |
|-------|-------|--------|
| ≤75°F (24°C) | 🔵 Cyan | Normal |
| 75-85°F (24-29°C) | 🟠 Orange | Elevated |
| >85°F (29°C) | 🔴 Red | High |

### GPS Status Indication
- **Green text**: Valid GPS fix (3D lock)
- **Red text**: No GPS fix

### Battery Display
- **Percentage**: 0-100%
- **Bar graph**: Visual charge level
- **Arrow**: ↑ Charging, ↓ Discharging

---

## NeoPixel Status LED

**Location:** GPIO33 (built-in WS2812B RGB LED)

### Status States

| Color | Pattern | Meaning |
|-------|---------|---------|
| 🔴 Red | Solid | System booting |
| 🟡 Yellow | 1Hz Flash (500ms on/off) | Searching for GPS |
| 🟢 Green | Solid | GPS 3D fix acquired |
| 🟡 Yellow | 0.2Hz Flash (2500ms on/off) | Logging paused |
| 🟣 Purple | Pulsing (breathing) | USB power lost, shutdown countdown |

---

## Display Control

### Brightness
- **Backlight Pin:** GPIO45
- **Control:** Digital HIGH/LOW (no PWM currently)
- **On:** `digitalWrite(TFT_BACKLITE, HIGH)`
- **Off:** `digitalWrite(TFT_BACKLITE, LOW)`

### Power Management
- Display shares power with I2C bus via GPIO7
- GPIO7 must be HIGH for display operation
- In DARK mode: backlight off, content frozen
- In deep sleep: display powered off

---

## Button Controls

### D1 Button (GPIO1) - Cycle Display Mode
Press to cycle through display modes:
- **MAIN_SCREEN** → **INFO_SCREEN** → **DARK** → *(repeat)*

**Behavior:**
- Pull configuration: INPUT (LOW by default)
- Active state: HIGH when pressed
- Debounce: 20ms
- In DARK mode: NeoPixel is disabled

### D0 Button (GPIO0) - Pause/Resume
- **Pull configuration:** INPUT_PULLUP (HIGH by default)
- **Active state:** LOW when pressed
- **Function:** Pause/resume data logging
- **NeoPixel feedback:** Changes to slow 0.2Hz flash when paused

### D2 Button (GPIO2) - Mark Event
- **Pull configuration:** INPUT (LOW by default)
- **Active state:** HIGH when pressed
- **Function:** Insert event marker in log

---

## Implementation Files

### Core Display Functions
**File:** `lib/Display/st7789_display.cpp`

```cpp
ST7789Display::init()           // Initialize hardware
ST7789Display::update()         // Update sensor data display
ST7789Display::show_info_screen() // Show network info
ST7789Display::show_shutdown_screen() // Shutdown countdown
ST7789Display::on()             // Enable backlight
ST7789Display::off()            // Disable backlight
ST7789Display::cycle_display_mode() // Switch display modes
```

### Units Helper
**File:** `lib/Logger/include/units_helper.h`

```cpp
convert_temperature(float celsius)  // Returns °F or °C based on flag
convert_speed(float knots)          // Returns mph or km/h
get_temp_unit()                     // Returns "F" or "C"
get_speed_unit()                    // Returns "mph" or "kph"
```

### NeoPixel Control
**File:** `lib/Display/st7789_display.cpp`

```cpp
NeoPixelStatus::init()          // Initialize GPIO33 LED
NeoPixelStatus::setState(State) // Change LED state
NeoPixelStatus::update(ms)      // Update animations (call every 100ms)
NeoPixelStatus::set_enabled()   // Enable/disable LED
```

---

## Troubleshooting

### Display Not Initializing
1. Check GPIO7 (TFT_I2C_POWER) is HIGH
2. Verify SPI bus configuration in platformio.ini
3. Check physical display connection
4. Review serial output for initialization errors

### Text Appears Garbled
- Check character encoding (ASCII only recommended)
- Avoid special Unicode symbols
- Use `get_temp_unit()` instead of hardcoded degree symbols

### Display Flickering
- Reduce update frequency if needed
- Check power supply voltage (3.3V stable)
- Verify backlight GPIO45 connection

### NeoPixel Not Working
- Verify GPIO33 is not reassigned
- Check NeoPixel initialization in setup()
- Ensure `NeoPixelStatus::update()` is called regularly

---

## Performance Notes

### Update Frequency
- **StatusMonitor:** 10 seconds (default)
- **NeoPixel Animation:** 100ms update loop
- **Display Refresh:** On-demand (not continuous)

### Memory Usage
- Display buffer: ~32KB (managed by Adafruit library)
- NeoPixel: Minimal (single LED)
- Text rendering: Stack-based temporary buffers

---

## Future Enhancements

- [ ] PWM backlight brightness control
- [ ] Touch screen support (if hardware added)
- [ ] Custom fonts for better readability
- [ ] Graph/plot mode for real-time data visualization
- [ ] User-configurable display layouts

---

**See Also:**
- [GPIO Pin Mapping](GPIO_PIN_MAPPING.md)
- [Power Management](POWER_MANAGEMENT.md)
- [Button Control](BUTTON_CONTROL.md)

---

**Last Updated:** January 19, 2026  
**Hardware:** Adafruit ESP32-S3 Feather TFT Reverse  
**Display:** ST7789 1.14" IPS 240×135
