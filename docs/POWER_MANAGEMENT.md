# Power Management - Deep Sleep & Auto-Shutdown

## Overview

The OpenPonyLogger features automatic power management that detects USB power loss and gracefully enters deep sleep mode to preserve battery life. The system can wake from deep sleep when USB power is restored or when the user presses the D0 button.

---

## Features

### USB Power Detection
- **GPIO19 (VBUS)** continuously monitors USB power presence
- Detects when device switches between USB and battery power
- Triggers shutdown sequence when USB power lost for 60 seconds

### Graceful Shutdown
When USB power is lost:
1. **Countdown begins** - 60-second timer starts
2. **Visual feedback** - Purple pulsing NeoPixel LED
3. **TFT display** - Shows countdown screen with seconds remaining
4. **Session closure** - Current logging session is properly closed
5. **Deep sleep** - Device enters ultra-low-power mode (~50 µA)

### Wake-up Behavior
The device can wake from deep sleep via two sources:

| Wake Source | GPIO | Behavior |
|-------------|------|----------|
| **USB Power Restored** | GPIO19 (VBUS rising edge) | Automatically starts fresh logging session |
| **Button Press** | GPIO0 (D0 button falling edge) | Boots up with logging paused (user must press D0 to start) |

---

## Power Consumption

| State | Current Draw | Duration on 500mAh Battery |
|-------|--------------|----------------------------|
| Normal operation (WiFi + sensors) | ~150 mA | ~3.3 hours |
| Logging paused (display on) | ~80 mA | ~6.25 hours |
| **Deep sleep** | **~50 µA** | **~417 days** |

---

## User Experience

### Shutdown Countdown
When USB power is disconnected:
1. NeoPixel changes to **purple pulsing** (breathing effect)
2. TFT displays shutdown countdown screen:
   ```
   ┌─────────────────────────┐
   │    POWER LOSS           │
   │         !               │
   │ USB power disconnected  │
   │   Shutdown: 45s         │
   │ Connect USB to cancel   │
   └─────────────────────────┘
   ```
3. Countdown updates every second
4. If USB reconnected → countdown canceled, normal operation resumes
5. If countdown reaches 0 → graceful shutdown initiated

### Wake from USB
When USB power is reconnected after deep sleep:
1. Device boots normally
2. Serial output shows: `"Woke from deep sleep: USB power restored (GPIO19)"`
3. **Fresh logging session automatically started**
4. NeoPixel shows normal status (red → yellow → green as GPS locks)

### Wake from Button
When D0 button is pressed during deep sleep:
1. Device boots normally
2. Serial output shows: `"Woke from deep sleep: Button press (GPIO0)"`
3. **Logging remains paused**
4. User must press D0 to start logging
5. Useful for powering on device without immediately logging

---

## NeoPixel Status States

| State | Color | Pattern | Meaning |
|-------|-------|---------|---------|
| BOOTING | 🔴 Red | Solid | System initializing |
| NO_GPS_FIX | 🟡 Yellow | 1Hz Flash | Searching for GPS |
| GPS_3D_FIX | 🟢 Green | Solid | GPS locked |
| PAUSED | 🟡 Yellow | 0.2Hz Flash | Logging paused |
| **SHUTDOWN** | **🟣 Purple** | **Pulsing** | **USB power lost, countdown active** |

---

## Implementation Details

### Power State Monitoring
Location: `lib/Logger/src/status_monitor.cpp`

```cpp
// Check USB power every 100ms in StatusMonitor task loop
bool usb_present = (digitalRead(VBUS_DETECT_PIN) == HIGH);

if (!usb_present && m_usb_powered) {
    // USB lost - start 60-second countdown
    m_usb_loss_time = millis();
    m_shutdown_pending = true;
    NeoPixelStatus::setState(NeoPixelStatus::State::SHUTDOWN);
}

if (usb_present && m_shutdown_pending) {
    // USB restored - cancel shutdown
    m_shutdown_pending = false;
}
```

### Deep Sleep Configuration
```cpp
// Configure wake sources before sleep
esp_sleep_enable_ext0_wakeup(GPIO19, 1);           // USB power (rising edge)
esp_sleep_enable_ext1_wakeup(1ULL << GPIO0, ...);  // D0 button (falling edge)

// Enter deep sleep
esp_deep_sleep_start();
```

### Wake Detection
Location: `src/main.cpp` - `setup()` function

```cpp
esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:  // GPIO19 - USB
        // Start fresh logging session
        break;
    case ESP_SLEEP_WAKEUP_EXT1:  // GPIO0 - Button
        // Boot with logging paused
        rt_logger->pause_storage();
        break;
}
```

---

## Configuration

### Timeout Duration
Default: 60 seconds

To change, edit in `src/main.cpp` and `lib/Logger/src/status_monitor.cpp`:
```cpp
#define USB_TIMEOUT_MS 60000  // milliseconds
```

### VBUS Detection Pin
Defined in `src/main.cpp`:
```cpp
#define VBUS_DETECT_PIN 19  // GPIO19 on ESP32-S3 Feather
```

---

## Use Cases

### Automotive Logging
- Plug device into car's USB port
- Device auto-starts logging when ignition turned on
- When car is turned off (USB power lost):
  - Device waits 60 seconds (in case of quick restart)
  - Then enters deep sleep to preserve battery
- Next time car starts → device wakes and begins fresh session

### Track Day Recording
- Power on via button press (logging paused)
- Start logging when entering track
- Device stays awake during pit stops (USB disconnected briefly)
- After event, device auto-sleeps if USB not reconnected

### Development/Testing
- During development, 60-second window prevents accidental sleep
- Can reconnect USB cable within countdown to cancel shutdown
- Button wake allows testing without auto-starting logging

---

## Serial Debug Messages

```
[Power] USB power lost! Starting 60-second countdown...
[Power] USB restored - shutdown canceled!
[Power] Countdown complete - initiating shutdown...
[Power] Closing logging session...
[Power] Configuring wake sources...
[Power] Entering deep sleep...
[Power] Wake sources: USB power (GPIO19) or D0 button (GPIO0)

=== BOOT START ===
▶ Woke from deep sleep: USB power restored (GPIO19)
▶ Wake from USB: Starting fresh logging session
```

---

## Technical Specifications

### GPIO Pin Assignments
- **GPIO19**: VBUS detection (input, pull-down)
- **GPIO0**: D0 button (input with pull-up, deep sleep wake source)

### Wake Source Configuration
- **EXT0**: Single GPIO with edge detection (GPIO19 rising edge)
- **EXT1**: Multiple GPIOs with level detection (GPIO0 low level)

### RTC Memory
Deep sleep preserves RTC memory, but currently not used. Future enhancement could store:
- Last session metadata
- Total uptime across sleep cycles
- Wake count statistics

---

## Future Enhancements

- [ ] Configurable timeout via web interface
- [ ] Battery level-based shutdown (critical low battery)
- [ ] RTC wake-up for periodic GPS time sync
- [ ] Session resume (continue same session after wake)
- [ ] Wake history logging

---

## References

- [GPIO Pin Mapping](GPIO_PIN_MAPPING.md)
- [ESP32-S3 Deep Sleep API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html)
- Implementation: [status_monitor.cpp](../lib/Logger/src/status_monitor.cpp)
- Wake detection: [main.cpp](../src/main.cpp)

---

**Last Updated**: January 19, 2026  
**Feature Version**: 1.0  
**Deep Sleep Current**: ~50 µA typical
