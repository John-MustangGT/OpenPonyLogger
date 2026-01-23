# Recommended Issues for OpenPonyLogger

Copy-paste these into GitHub Issues to track optimization and monitoring improvements.

---

## Issue 1: Add queue depth monitoring to system stats

**Labels:** `enhancement`, `monitoring`
**Priority:** Medium

### Problem
The storage queue (500 samples, ~50 seconds at 10Hz) can overflow if Core 0 storage writes stall. Currently we track `m_queue_overruns` but don't monitor queue depth in real-time.

### Proposed Solution
Add queue depth to the 5-second system monitoring output:

```
[System] DRAM: 245/320 KB | PSRAM: 85/2048 KB | CPU0: 22% | CPU1: 68% | Queue: 45/500 (9%)
```

### Implementation
- Use `uxQueueMessagesWaiting(m_sample_queue)` to get current depth
- Add to StatusMonitor output alongside memory/CPU stats
- Helps detect impending overflows before they happen
- Add warning log if queue depth >80% (400/500 samples)

### Files to Modify
- `lib/Logger/src/status_monitor.cpp` - Add queue depth query and display
- `lib/Logger/include/status_monitor.h` - Pass storage pointer to access queue

---

## Issue 2: Implement graceful queue overflow handling

**Labels:** `enhancement`, `reliability`
**Priority:** High

### Problem
When the storage queue overflows, samples are dropped and only counted in `m_queue_overruns`. System continues running but data is lost silently (except for logs).

### Current Behavior
```cpp
if (xQueueSend(...) == errQUEUE_FULL) {
    m_queue_overruns++;  // Just count it
}
```

### Proposed Solutions (Pick One or Combine)

**Option 1: Backpressure**
- Block sensor reads when queue is >90% full
- Temporarily reduce sampling rate to 5Hz
- Resume 10Hz when queue drains to <50%

**Option 2: Priority Dropping**
- Keep GPS/OBD samples (critical data)
- Drop gyro samples first (least critical)
- Drop accel samples if still overflowing

**Option 3: Emergency Flush**
- Force immediate storage write when queue hits 95%
- May cause temporary display/WiFi stuttering
- Better than losing 50 seconds of data

### Recommendation
Implement Option 1 (backpressure) + visual indication on display when in reduced-rate mode.

---

## Issue 3: Verify framebuffer PSRAM allocation at runtime

**Labels:** `testing`, `memory`
**Priority:** High (blocking for production)

### Problem
Display framebuffer (64,800 bytes) uses `new GFXcanvas16(240, 135)` which may allocate from DRAM or PSRAM depending on malloc configuration. If it lands in DRAM, we're wasting precious internal memory.

### Current Status
Diagnostics added in commit `20585f7` will show allocation location:
```
[TFT] Free DRAM: 280000 bytes
[TFT] Free PSRAM: 1900000 bytes
[TFT] Canvas allocated: 64800 bytes
[TFT] Free DRAM after: 215000 bytes   <-- If dropped 65KB, used DRAM ❌
[TFT] Free PSRAM after: 1835000 bytes <-- If dropped 65KB, used PSRAM ✅
```

### Action Items
1. **Test:** Flash current code and check serial output during boot
2. **If in DRAM:** Replace with explicit PSRAM allocation:
   ```cpp
   uint16_t* buffer = (uint16_t*)heap_caps_malloc(240*135*sizeof(uint16_t), MALLOC_CAP_SPIRAM);
   // Then create GFXcanvas16 wrapper using this buffer (may need custom implementation)
   ```
3. **If in PSRAM:** Close issue, current implementation is fine

### Files Involved
- `lib/Display/st7789_display.cpp` - Framebuffer allocation
- `platformio.ini` - PSRAM configuration

---

## Issue 4: Add rolling average to CPU load calculation

**Labels:** `enhancement`, `monitoring`
**Priority:** Low

### Problem
Current CPU load calculation shows instantaneous 5-second deltas, which can spike briefly and cause alarm. A rolling average would provide smoother, more meaningful metrics.

### Current Implementation
```cpp
// Raw calculation every 5 seconds
core0_load = (uint8_t)(100 - (delta_idle0 * 200) / delta_total);
```

### Proposed Solution
Track last 3-5 measurements and report moving average:
```
CPU0: 22% (avg) | CPU1: 68% (avg)
```

### Benefits
- Smoother metrics less affected by brief spikes
- More representative of sustained load
- Easier to spot trends vs noise

### Implementation Complexity
Low - just maintain circular buffer of last N measurements.

---

## Issue 5: Investigate flash wear leveling and storage optimization

**Labels:** `research`, `storage`, `long-term`
**Priority:** Low (future planning)

### Background
Internal flash has limited erase cycles (~100k). Current write pattern:
```
86400 seconds/day ÷ 5 = 17,280 writes/day
100,000 cycles ÷ 17,280 = ~5.8 days to wear out a sector
```

ESP32 has wear leveling, but this is still aggressive for long-term deployment.

### Research Questions
1. What is actual flash endurance with ESP32 wear leveling? (probably much better than 5.8 days)
2. Can we reduce write frequency without losing data?
3. Would compression reduce flash writes significantly?
4. Is SD card storage mode (`env:sdcard`) the better long-term solution?

### Potential Optimizations
- **Compression:** LZ4 or similar lightweight compression
- **Adaptive write rate:** Write every 10s when idle, 5s when actively moving
- **Batch writes:** Larger blocks less frequently (10s intervals, 2× block size)
- **PSRAM buffering:** Increase queue to 1000 samples (100s buffer)

### Action Items
1. Test flash endurance in real deployment (weeks/months)
2. Profile actual flash writes with ESP-IDF tools
3. Evaluate compression library overhead (CPU/memory)
4. Consider SD card as primary storage for production

---

## Issue 6: Validate CPU load calculation with real workload

**Labels:** `testing`, `monitoring`
**Priority:** High (blocking for production)

### Problem
CPU load monitoring was added in commit `f04862d` but hasn't been validated under real workload. The calculation assumes:
- FreeRTOS runtime stats are enabled ✅
- Runtime counters increment monotonically
- Idle time delta math is correct

### Potential Issues to Check
- Negative percentages (uint8_t overflow)
- Values >100% (calculation error)
- Both cores showing 0% (stats not enabled)
- Counter wraparound after ~49 days uptime

### Test Plan
1. Flash current code and run for 30+ minutes
2. Observe CPU load output every 5 seconds
3. Verify numbers make sense:
   - Core 0: Should be 10-40% (housekeeping)
   - Core 1: Should be 50-80% (real-time logging)
4. Stress test: Enable WiFi streaming, increase sensor rate
5. Check for invalid values (0%, >100%, negative)

### If Issues Found
- Add bounds checking and clamping (0-100%)
- Add wraparound detection for long uptimes
- Consider alternative method (task watermarks, profiler)

---

## Issue 7: Document system resource allocation and limits

**Labels:** `documentation`
**Priority:** Low

### Problem
System resource allocation is spread across multiple files with no central documentation of limits and usage.

### Proposed Documentation
Create `docs/RESOURCE_ALLOCATION.md` with:

#### Memory Map
```
ESP32-S3 Feather (320KB DRAM + 2MB PSRAM)

DRAM Usage (~245KB used, 75KB free):
├─ Code & Data Sections: ~150KB
├─ FreeRTOS Stacks: ~60KB
│  ├─ RTLogger (Core 1): 16KB
│  ├─ Storage (Core 0): 8KB
│  └─ StatusMonitor (Core 0): 4KB
├─ Display Framebuffer: 65KB (may be in PSRAM)
└─ Other (heap, stacks): ~35KB

PSRAM Usage (~85KB used, 1963KB free):
├─ Storage Queue: 20KB (500 samples × 40 bytes)
├─ Display Framebuffer: 65KB (if allocated correctly)
└─ Future expansion: 1.9MB available
```

#### CPU Allocation
```
Core 0 (Housekeeping): ~20-30% utilization
├─ StatusMonitor: Display updates (1Hz), memory stats (5s)
├─ FlashStorage Writer: Block writes every 5s
├─ WiFi/BLE: WebSocket broadcasts, BLE notifications
└─ Button handling: Interrupt-driven

Core 1 (Real-Time): ~60-80% utilization
├─ RTLogger: Sensor polling at 10Hz
├─ GPS: 1Hz serial read
├─ IMU: 10Hz I2C polling
└─ Queue writes: Non-blocking
```

#### Performance Limits
- Maximum sensor rate: ~50Hz (before Core 1 saturates)
- Storage queue capacity: 50 seconds @ 10Hz
- Display update rate: 1Hz stable, 5Hz may cause conflicts
- Flash write endurance: ~100k cycles per sector (wear leveled)

---

## Notes
- Issues ordered by priority (High → Low)
- Issues 3 and 6 are **blocking** - need runtime validation before production
- Issues 1, 2 are **recommended** - improve robustness
- Issues 4, 5, 7 are **nice-to-have** - future improvements

Copy these into your GitHub Issues tracker to track implementation progress!
