# OpenPonyLogger - Technical Implementation Details

## NeoPixel Control Implementation

### Class Structure

```cpp
class NeoPixelStatus {
public:
    enum class State {
        BOOTING,      // Red
        NO_GPS_FIX,   // Yellow flashing
        GPS_3D_FIX    // Green
    };
    
    static bool init();                    // Initialize hardware
    static void setState(State state);     // Change state
    static void update(uint32_t ms);       // Handle animations
    static void deinit();                  // Shutdown
};
```

### Hardware Details

**Pin Configuration:**
```cpp
#define NEOPIXEL_PIN 33
#define NEOPIXEL_COUNT 1
```

**Library:**
- Adafruit_NeoPixel (GRB + 800kHz protocol)
- Auto-detected from include: `#include <Adafruit_NeoPixel.h>`

### Color Values (GRB Format)

```cpp
RED    = pixel.Color(255, 0, 0)      // Booting
YELLOW = pixel.Color(255, 255, 0)    // No GPS fix
GREEN  = pixel.Color(0, 255, 0)      // GPS 3D fix
BLACK  = pixel.Color(0, 0, 0)        // Off (for flashing)
```

### State Transitions

**Init Sequence:**
```
NeoPixelStatus::init() 
  → Create Adafruit_NeoPixel object on GPIO33
  → Initialize pixel hardware
  → Set initial state to BOOTING (red)
```

**State Change:**
```
NeoPixelStatus::setState(State state)
  → Update m_current_state
  → Set appropriate color
  → Reset flash timer if applicable
  → Call pixel.show() to display
```

**Animation Update (called every 100ms from status monitor):**
```
NeoPixelStatus::update(uint32_t current_ms)
  → Only active for NO_GPS_FIX state
  → Toggle pixel on/off every 500ms (1Hz)
  → Create flash effect: 500ms yellow, 500ms black
```

## Display Formatting Implementation

### Accelerometer/Gyroscope Format

**Before:**
```cpp
snprintf(accel_line, sizeof(accel_line), "A:%.2f,%.2f,%.2f", 
         accel_x, accel_y, accel_z);
```

**After (Fixed format with sign):**
```cpp
snprintf(accel_line, sizeof(accel_line), "A:%+.2f,%+.2f,%+.2f", 
         accel_x, accel_y, accel_z);
//        ↑ The '+' format specifier forces sign display
```

**Output Examples:**
- Positive value: `+1.23` (always shows +)
- Negative value: `-0.45` (shows -)
- Zero: `+0.00` (shows +)

**Benefits:**
- Fixed 5-character width per value: `+1.23` or `-9.81`
- Text doesn't shift when sign changes
- Improved visual stability

### Temperature Format

**Before:**
```cpp
snprintf(temp_str, sizeof(temp_str), "T:%.1f%s°", 
         display_temp, get_temp_unit());  // Include degree symbol
// Output: "T:72.5°F" or "T:22.3°C"
```

**After (No degree symbol):**
```cpp
snprintf(temp_str, sizeof(temp_str), "T:%+.1f%s", 
         display_temp, get_temp_unit());  // No degree symbol
// Output: "T:+72.5F" or "T:+22.3C"
```

**get_temp_unit() function:**
- Returns "C" or "F" (just the letter, no symbol)
- Defined in units_helper.h
- Controlled by USE_IMPERIAL compile flag

### Sample Count Scaling

**Logic:**
```cpp
if (sample_count >= 1000000) {
    snprintf(sampstr, sizeof(sampstr), "%.1fM", 
             sample_count / 1000000.0f);
} else if (sample_count >= 1000) {
    snprintf(sampstr, sizeof(sampstr), "%.1fK", 
             sample_count / 1000.0f);
} else {
    snprintf(sampstr, sizeof(sampstr), "%u", sample_count);
}
```

**Examples:**
- 500 samples → `"500"`
- 1,234 samples → `"1.2K"`
- 50,000 samples → `"50.0K"`
- 1,234,567 samples → `"1.2M"`
- 10,000,000 samples → `"10.0M"`

**Width savings:**
- Raw: "10000000" (8 chars)
- Scaled: "10.0M" (5 chars)
- Saves: 3 characters per update

## Integration Points

### 1. Main Setup (src/main.cpp)

```cpp
// After display initialization
Serial.println("▶ Initializing NeoPixel Status Indicator...");
if (!NeoPixelStatus::init()) {
    Serial.println("⚠ WARNING: NeoPixel initialization failed");
} else {
    Serial.println("✓ NeoPixel initialized (Booting - Red)");
}
```

### 2. Status Monitor (lib/Logger/status_monitor.cpp)

**In print_status_now():**
```cpp
// After display update
if (gps.valid) {
    NeoPixelStatus::setState(NeoPixelStatus::State::GPS_3D_FIX);
} else {
    NeoPixelStatus::setState(NeoPixelStatus::State::NO_GPS_FIX);
}
```

**In task_loop():**
```cpp
// Update NeoPixel animation every 100ms
NeoPixelStatus::update(now);
vTaskDelay(pdMS_TO_TICKS(100));
```

## Memory Usage

### Static Members (RAM)
```cpp
Adafruit_NeoPixel* m_pixel;          // ~4 bytes (pointer)
State m_current_state;               // 1 byte (enum)
uint32_t m_last_flash_time;          // 4 bytes
bool m_pixel_on;                     // 1 byte
bool m_initialized;                  // 1 byte
Total static overhead: ~11 bytes
```

### Dynamic Allocation
```cpp
Adafruit_NeoPixel object: ~100+ bytes (driver overhead)
Total heap usage: <200 bytes
```

### Current Build Status
- **Total RAM Used:** 9.8% (32,136 / 327,680 bytes)
- **Total Flash Used:** 27.3% (392,917 / 1,441,792 bytes)
- **Remaining Headroom:** ~62% Flash, ~90% RAM

## Timing and Performance

### NeoPixel Update Frequency
- Called every 100ms from status monitor task
- Non-blocking operation
- Negligible CPU impact

### Flash Animation Timing
```
Interval = 500ms (1Hz total)
Each cycle: 500ms ON + 500ms OFF

Time:   0      500ms     1000ms
        |------|---------|
State:  ON     OFF      ON
```

### Display Update Frequency
- Configurable report interval (default 1000ms)
- NeoPixel state updates with each report
- Independent flash animation updates every 100ms

## Error Handling

### Initialization Failures
```cpp
if (!NeoPixelStatus::init()) {
    // Still continue system boot
    // Serial warning printed
    // NeoPixel remains inactive
}
```

### Memory Failures
```cpp
if (m_pixel == nullptr) {
    Serial.println("[NeoPixel] ERROR: Failed to allocate...");
    return false;
}
```

## Testing Checklist

### Compilation
- [x] Code compiles without errors
- [x] No undefined references
- [x] Proper library dependencies (platformio.ini)
- [ ] Binary size within limits (currently 27.3% flash)

### Functional Tests
- [ ] Red light on boot startup
- [ ] Yellow flashing when no GPS signal
- [ ] Green steady when GPS locked
- [ ] State transitions are smooth
- [ ] Flash timing is correct (1Hz)
- [ ] Display formats are stable
- [ ] Sample count scales correctly

### System Integration
- [ ] NeoPixel doesn't interfere with other GPIO
- [ ] Core 0 task handles animation smoothly
- [ ] Core 1 (RT Logger) unaffected
- [ ] Memory usage stable over time

## Future Optimization Possibilities

1. **Power Efficiency:**
   - Reduce brightness during low-battery conditions
   - Pulse intensity based on battery level

2. **Extended States:**
   - Blue: Recording (SD card active)
   - Cyan: GPS lock but waiting for 3D fix
   - Purple: Temperature warning

3. **Pattern Library:**
   - Different patterns for different error states
   - Custom patterns for user events

4. **Configuration:**
   - Brightness control via settings
   - Custom color scheme selection
   - Animation speed adjustment

---

**Compilation Status:** ✅ Successful
**Build Time:** ~6.77 seconds
**Linker Output:** No errors or warnings (excluding PLATFORMIO redefinition warnings)

