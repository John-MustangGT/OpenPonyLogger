# Quick Reference: Units & Display Features

## 🌍 Units System
**Currently Active:** Imperial (mph, °F)

To switch to Metric, edit `platformio.ini`:
```ini
-DUSE_IMPERIAL=0  # Change from 1 to 0
```

### Temperature Display
```
Imperial: 74.3°F (default)
Metric:   23.5°C (when flag set to 0)
```

### Speed Display (when GPS has fix)
```
Imperial: 11.5 mph
Metric:   18.5 km/h
```

## 📊 Display Hardware
**Status:** ✅ Initialized and ready

- **Resolution:** 240×135 pixels
- **Interface:** SPI with Adafruit library
- **Type:** ST7789 1.14" IPS LCD
- **Pin Configuration** (Adafruit ESP32-S3 Feather Reverse TFT):
  - SPI Clock: GPIO36 (hardware HSPI, auto-configured)
  - SPI MOSI: GPIO35 (hardware HSPI, auto-configured)
  - TFT_CS: GPIO42
  - TFT_DC (Data/Command): GPIO40
  - TFT_RST (Reset): GPIO41
  - TFT_BACKLITE: GPIO45
  - TFT_I2C_POWER: GPIO7 (already enabled for STEMMA)

### Current Display Output
Compact 240×135 layout showing:
- Line 1: Uptime and sample count
- Line 2: Acceleration (X/Y/Z) and temperature
- Line 3: Gyroscope (X/Y/Z dps)
- Line 4: Battery percentage and voltage
- Line 5: GPS status (color-coded: green=fix, red=no fix)
- Bar: Battery indicator at bottom

## 📁 Implementation Files
```
lib/
├── Logger/
│   ├── include/
│   │   └── units_helper.h          ← Unit conversion macros
│   └── status_monitor.cpp          ← Updated with conversions
├── Display/
│   ├── include/
│   │   └── st7789_display.h        ← Display interface
│   └── st7789_display.cpp          ← Adafruit ST7789 driver
src/
└── main.cpp                        ← Display initialization added
```

## 🔧 Build Info
- **Status:** ✅ Clean build
- **Flash:** 386KB / 1441KB (26.8%)
- **RAM:** 32KB / 327KB (9.8%)
- **Libraries:** Adafruit_GFX, Adafruit_ST7735_ST7789, Adafruit_BusIO

## 🚀 Next Steps
1. Optional: Implement more sophisticated graphics (charts, gauges)
2. Battery-optimized display updates
3. Touch input handling (if buttons available)

## 📝 Usage
Conversion functions automatically selected at compile time:
```cpp
#include "units_helper.h"

float display_temp = convert_temperature(celsius);  // °F or °C
const char* unit = get_temp_unit();                 // "°F" or "°C"
```

Display updates happen automatically in the status monitor thread (1 second interval).
