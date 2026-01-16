# NeoPixel Status Indicator - Quick Reference

## Visual Status Guide

```
╔════════════════════════════════════════════╗
║           NEOPIXEL STATUS STATES            ║
╠════════════════════════════════════════════╣
║                                             ║
║  🔴 RED (Solid)           → BOOTING        ║
║     System initializing                     ║
║     Appears during startup                  ║
║                                             ║
║  🟡 YELLOW (Flashing)     → NO GPS FIX     ║
║     Searching for GPS satellite lock        ║
║     1Hz flash: 500ms on, 500ms off         ║
║     Indicates GPS is searching              ║
║                                             ║
║  🟢 GREEN (Solid)         → GPS 3D FIX     ║
║     Valid GPS position acquired             ║
║     Ready for accurate altitude/position    ║
║                                             ║
╚════════════════════════════════════════════╝
```

## Display Format Examples

### Accelerometer & Gyroscope (Fixed Format with Signs)
```
A:+1.23,-0.45,+9.81    ← Always shows +/- sign
G:+0.5,-1.2,+0.8      ← Prevents text jumping
```

### Temperature (No Degree Symbol)
```
T:+72.5F   ← Clean format, no ° symbol
T:+22.3C   ← Avoids Unicode issues
```

### Sample Count (Auto-Scaled)
```
Samples:      500     ← Raw count
Samples:      1.5K    ← Thousands (K)
Samples:      2.3M    ← Millions (M)
```

## Color Coding

### Temperature Indicators
- 🔵 Cyan: Normal (≤75°F/24°C)
- 🟠 Orange: Elevated (75-85°F/24-29°C)
- 🔴 Red: High (>85°F/29°C)

### Battery Indicators
- 🟢 Green: Good (>50%)
- 🟠 Orange: Low (20-50%)
- 🔴 Red: Critical (<20%)

## Hardware Pin Configuration

| Component | Pin | Protocol | Note |
|-----------|-----|----------|------|
| NeoPixel | GPIO33 | WS2812B | Built-in on Adafruit ESP32-S3 Feather |
| TFT Display | Various | SPI | See st7789_display.h |
| I2C Sensors | GPIO3/4 | I2C | 400kHz clock |

## State Machine

```
         System Boot
              ↓
         🔴 RED (Boot)
              ↓
    ┌─────────────────┐
    │   Sensors Init  │
    └────────┬────────┘
             ↓
    ┌─────────────────┐
    │  GPS Connected? │
    └────────┬────────┘
             ↓
       ┌─────┴─────┐
       │           │
      NO          YES
       │           │
       ↓           ↓
   🟡 YELLOW   🟢 GREEN
   (Flashing)  (Solid)
```

## Integration Timeline

1. **Setup Phase:** NeoPixel shows red
2. **Initialization:** Transitions to yellow when GPS begins searching
3. **GPS Acquisition:** Turns green once 3D fix is obtained
4. **Runtime:** Follows GPS status (green while locked, yellow if lock is lost)

---

**File Locations:**
- Implementation: `lib/Display/st7789_display.cpp`
- Header: `lib/Display/include/st7789_display.h`
- Integration: `lib/Logger/status_monitor.cpp`
- Configuration: `platformio.ini`

