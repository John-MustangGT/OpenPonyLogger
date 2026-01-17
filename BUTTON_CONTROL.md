# Button Control Implementation

## Button Configuration

### Hardware Setup
Three buttons connected to GPIO pins on the Adafruit ESP32-S3 Feather:

| Button | GPIO | Pin Name | Function |
|--------|------|----------|----------|
| D0 | GPIO5 | Bottom | Pause/Resume Storage |
| D1 | GPIO6 | Middle | Reserved |
| D2 | GPIO8 | Top | Mark Event |

**Connection:** All buttons use INPUT_PULLUP (pull-up resistor enabled)
- Unpressed: GPIO reads HIGH
- Pressed: GPIO reads LOW

---

## D0 Button - Pause/Resume Storage

### Functionality
- **First Press:** Pauses storage writes (data collection continues)
- **Second Press:** Resumes storage writes
- **Repeating:** Toggles between paused and running states

### NeoPixel Feedback During Pause
When paused, the NeoPixel changes animation:
- **Normal (1Hz Flash):** 500ms on, 500ms off (fast flash)
- **Paused (0.2Hz Flash):** 2500ms on, 2500ms off (very slow flash)

### Use Cases
- Check readings on TFT without writing to storage
- Prepare for a data run (pause, review settings, then resume)
- Pause before the actual event starts
- Conserve battery/storage space during idle time

### Implementation Details
```cpp
if (m_rt_logger->is_storage_paused()) {
    m_rt_logger->resume_storage();
} else {
    m_rt_logger->pause_storage();
}
```

---

## D2 Button - Mark Event

### Functionality
- **Press:** Marks the next collected frame with an event flag
- **Only works while logging:** Has no effect when storage is paused
- **Returns to normal:** Automatically clears flag after next write

### Use Cases
- Mark lap start/finish
- Mark race start/end
- Mark acceleration run start
- Mark any noteworthy moment in the data
- Create bookmarks in the data file for later analysis

### Implementation Details
```cpp
if (m_rt_logger != nullptr && !m_rt_logger->is_storage_paused()) {
    m_rt_logger->mark_event();
}
```

---

## Button Debouncing

### Debounce Settings
```cpp
#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_LONG_PRESS_MS 1000  // Reserved for future use
```

### Debounce Logic
1. Detect state change (HIGH → LOW)
2. Start debounce timer
3. Wait 20ms (mechanical switch settling time)
4. If still LOW, process the button press
5. Ignore any additional presses until button releases

### Benefits
- Eliminates false triggers from switch bounce
- Single press results in exactly one action
- No double-trigger issues

---

## Serial Feedback

When buttons are pressed, the system logs:

```
[Button] D0: Storage PAUSED
[Button] D0: Storage RESUMED
[Button] D2: Event marked!
```

These messages appear in the serial monitor for debugging and confirmation.

---

## Status Monitor Integration

### Task Location
All button handling runs in the **StatusMonitor** task (Core 0):
- 100ms loop cycle
- Non-blocking digital reads
- Debouncing handled in task loop

### Real-Time Feedback
```
Button Press → Debounce (20ms) → Action → NeoPixel Update → Status Report
```

---

## NeoPixel Status During Operations

### Complete State Machine

| Situation | LED State | Animation |
|-----------|-----------|-----------|
| Booting | 🔴 Red | Solid |
| Searching GPS (Running) | 🟡 Yellow | 1Hz Flash |
| GPS Locked (Running) | 🟢 Green | Solid |
| Paused (Any GPS state) | 🟡 Yellow | 0.2Hz Slow Flash |

### Visual Difference
**0.2Hz (Paused) vs 1Hz (Searching):**
- Paused: 2.5 seconds on, 2.5 seconds off (very leisurely)
- Searching: 0.5 seconds on, 0.5 seconds off (noticeable pulse)

The difference is obvious at a glance - paused is a slow, deliberate flash.

---

## Code Changes Summary

### Files Modified
1. **src/main.cpp**
   - Added button GPIO definitions (D0=GPIO5, D2=GPIO8)
   - Added button initialization in setup()

2. **lib/Logger/rt_logger_thread.h**
   - Added `pause_storage()` method
   - Added `resume_storage()` method
   - Added `is_storage_paused()` method
   - Added `mark_event()` method

3. **lib/Logger/rt_logger_thread.cpp**
   - Implemented pause/resume logic
   - Added storage_paused and mark_event member variables
   - Initialize new members in constructor

4. **lib/Display/st7789_display.h**
   - Added PAUSED state to NeoPixelStatus enum
   - Added FLASH_INTERVAL_0P2HZ_MS constant

5. **lib/Display/st7789_display.cpp**
   - Updated setState() to handle PAUSED state
   - Updated update() to support two flash rates (1Hz and 0.2Hz)

6. **lib/Logger/status_monitor.cpp**
   - Added button GPIO definitions
   - Added button state tracking in task_loop()
   - Added D0 (pause/resume) button handling with debouncing
   - Added D2 (mark event) button handling with debouncing
   - Updated NeoPixel state logic to check pause status

---

## Compilation Status

✅ **SUCCESS - 3.45 seconds**
- RAM: 9.8% (32,136 / 327,680 bytes)
- Flash: 27.3% (393,865 / 1,441,792 bytes)
- No errors, no critical warnings

---

## Testing Checklist

### Button Hardware
- [ ] Verify D0 (GPIO5) button connection
- [ ] Verify D2 (GPIO8) button connection
- [ ] Test button mechanical operation (clicks cleanly)
- [ ] Verify GPIO pull-up is working

### D0 Pause/Resume Functionality
- [ ] Press D0 → NeoPixel changes to slow flash (0.2Hz)
- [ ] Serial shows "Storage PAUSED"
- [ ] Press D0 again → NeoPixel returns to previous state
- [ ] Serial shows "Storage RESUMED"
- [ ] Data collection continues while paused (TFT still updates)
- [ ] Storage doesn't write while paused

### D2 Event Marking
- [ ] Press D2 while running → Serial shows "Event marked!"
- [ ] Press D2 while paused → No response (expected)
- [ ] Event flag gets written to next frame
- [ ] Multiple presses in sequence work correctly
- [ ] No false triggers from bounce

### NeoPixel Animation
- [ ] Slow flash (0.2Hz) is clearly different from fast flash (1Hz)
- [ ] Animation smooth without stuttering
- [ ] State transitions are clean (no flicker)

### Integration
- [ ] Buttons don't interfere with I2C sensors
- [ ] Buttons don't interfere with SPI display
- [ ] Buttons don't interfere with GPS
- [ ] CPU usage remains low
- [ ] Long duration test (>1 hour) stable

---

## Future Enhancements

Possible additions with the middle button (D1):
- Cycle through different display modes
- Adjust brightness of display/LED
- Trigger emergency stop
- Toggle debug output verbosity
- Mark different event types (lap start vs finish)

---

## User Guide

### Quick Reference
1. **Blue/Green/Top button (D2)**: Mark important moments while logging
2. **Hold (reserved for future use)**
3. **Bottom button (D0)**: Toggle pause when you need to check status

### Example Flight Sequence
1. Power on → Red LED (booting)
2. After 5 sec → Yellow blinking fast (searching GPS)
3. After ~30 sec → Green solid (locked, ready to go)
4. Press D2 to mark "Flight Start"
5. Press D0 to pause and check telemetry
6. Press D0 again to resume
7. Press D2 to mark "Flight Stop"
8. Look for [event] markers in logged data for analysis

---

## Technical Notes

### Button Behavior
- Buttons are **edge-triggered**, not level-triggered
- Only responds when state *changes* from HIGH to LOW
- Ignores noise and bouncing within 20ms window
- Button must release and press again for next action

### Pause vs Stop
- **Pause:** Sensor data is still collected and displayed, just not written to storage
- **Stop:** Would halt collection entirely (not implemented)

### Event Marking
- Mark flag is consumed on next storage write
- Works in real-time (no buffering delay)
- Event is written to current frame or next frame depending on timing

