# OpenPonyLogger Architecture

## Core 0 / Core 1 Resource Management

This document describes the dual-core architecture used to prevent resource starvation on the ESP32-S3.

### Design Philosophy

**Core 1: Real-Time Sensor Acquisition (Never Blocks)**
- Reads sensors at high frequency (1-10Hz)
- Writes ONLY to fast PSRAM buffers via FreeRTOS queues
- Never waits on slow I/O operations
- Maintains deterministic timing for data acquisition

**Core 0: Housekeeping & Slow I/O (Blocking OK)**
- Handles BLE stack (NimBLE requirement)
- Manages WiFi, WebSocket, and display updates
- Drains PSRAM queue and writes to flash storage (blocking operations isolated here)
- Button handling and power management

### Task Allocation

#### Core 1 Tasks
| Task | Priority | Purpose |
|------|----------|---------|
| **RTLoggerThread** | 2 | Primary sensor data collection loop |
| | | - GPS @ configurable Hz (default 1Hz) |
| | | - IMU (accel + gyro + compass) @ configurable Hz |
| | | - Battery monitoring (every loop) |
| | | - WebSocket broadcasts @ 5Hz |
| | | **Writes to PSRAM queue only (non-blocking)** |

#### Core 0 Tasks
| Task | Priority | Purpose |
|------|----------|---------|
| **Arduino loop()** | 1 | Minimal housekeeping, triggers storage writes |
| **StatusMonitor** | 1 | Button debouncing, display updates, USB monitoring |
| | | BLE driver updates @ 10Hz (NimBLE must be on Core 0) |
| | | **OBD sample queuing @ 10Hz with microsecond timestamps** |
| **FlashStorage writer** | 1 | **Drains PSRAM queue → Flash storage** |
| | | Blocking flash erase/write operations isolated here |
| **TimeUpdateTask** | 1 | GPS time synchronization to RTC/NVS |

### Queue-Based Buffering Strategy

The key innovation is using a **large PSRAM queue (500 samples, ~50 seconds)** to decouple the two cores:

```
Core 1 (Fast) → [PSRAM Queue: 500 samples] → Core 0 (Slow)
   ↓                                              ↓
Sensor reads                                  Flash writes
(non-blocking)                                (blocking OK)
```

**Queue Characteristics:**
- **Size**: 500 samples (~50 seconds at 10Hz sample rate)
- **Location**: FreeRTOS queue in PSRAM (fast access, large capacity)
- **Send**: Core 1 uses `xQueueSend(..., timeout=0)` - **never blocks**
- **Receive**: Core 0 blocks up to 100ms waiting for data
- **Monitoring**: Overrun detection warns if Core 0 can't keep up

### OBD-II Timestamping (Accurate Microsecond Timestamps)

**Problem:** OBD data was previously bundled with other sensors and timestamped during the 5-second write cycle, causing up to ±500ms timing errors.

**Solution:** OBD samples are now queued **immediately** when BLE data arrives, with timestamps captured at the moment of reception:

```
1. BLE notification arrives on Core 0 (StatusMonitor @ 10Hz)
   ↓
2. IcarBleDriver parses OBD response
   ↓
3. Timestamp captured: obd_data.timestamp_us = esp_timer_get_time()
   ↓
4. StatusMonitor calls queue_obd_sample(obd_data)
   ↓
5. OBD sample queued to PSRAM with accurate timestamp
   ↓
6. Core 0 FlashStorage writer drains queue as normal
```

**Benefits:**
- ✅ **Microsecond accuracy**: Timestamp captured when data arrives, not 0-5 seconds later
- ✅ **10Hz OBD updates**: Fast-changing PIDs (RPM, throttle, speed) sampled at 100ms intervals
- ✅ **No cross-core access**: OBD is owned by Core 0, no race conditions
- ✅ **Independent from sensor bundle**: OBD timing decoupled from GPS/IMU write cycle

**Timestamp Accuracy:**
- GPS/IMU/Battery: Timestamped when queued (every 5 seconds, accurate to ~1 second)
- OBD: Timestamped when BLE data arrives (accurate to <1ms)

### Flash Storage Write Path

**Core 1 Sensors (GPS, IMU, Battery):**
```
1. Sensor data arrives on Core 1 (RTLoggerThread)
   ↓
2. write_sample() called with sensor struct
   ↓
3. Non-blocking queue send to PSRAM (0ms timeout)
   ↓ (if queue full, sample dropped + warning logged)
4. Core 0 writer task drains queue
   ↓
5. Samples batched into 4KB blocks
   ↓
6. Flash erase (1-10ms BLOCKING) + write
   ↓ (Core 1 unaffected - it's writing to PSRAM)
7. Periodic flush every 5 seconds
```

**Core 0 OBD (Direct Queuing):**
```
1. BLE data arrives on Core 0 (StatusMonitor @ 10Hz)
   ↓
2. Timestamp captured immediately (esp_timer_get_time())
   ↓
3. queue_obd_sample() queues with accurate timestamp
   ↓
4. Core 0 writer drains and writes to flash
```

### Performance Optimizations (Core 0)

To reduce Core 0 load beyond just moving flash writes:

1. **Removed duplicate WebSocket broadcasts**
   - StatusMonitor no longer broadcasts sensor data
   - RTLoggerThread (Core 1) handles all WebSocket broadcasts @ 5Hz
   - Eliminates ~115 lines of JSON serialization from Core 0

2. **Reduced display update frequency**
   - Changed from 2 seconds to 4 seconds
   - Added yield points after display SPI operations

3. **Rate-limited debug output**
   - Sample count prints reduced from 1Hz to 0.1Hz (every 10s)
   - Yield after large serial outputs

4. **Increased BLE/OBD update frequency**
   - Increased from 2Hz to 10Hz (100ms interval) for fast-changing PIDs
   - Captures RPM, throttle, and speed changes at high frequency
   - Yield point after each BLE update

5. **Better task yielding**
   - Reduced main loop delay from 100ms to 50ms
   - Strategic `vTaskDelay(pdMS_TO_TICKS(1))` after blocking operations

### Queue Health Monitoring

The system monitors queue health and warns if Core 0 cannot keep up:

```cpp
// Logged every 30 seconds
[FlashStorage] Queue health: 45/500 used (9.0%), 0 overruns, 12450 queued

// Warning if queue fills
[FlashStorage] WARNING: Queue >80% full! Core 0 struggling to keep up with Core 1
```

**Overrun detection**: If Core 1 tries to queue a sample when the buffer is full, it drops the sample and logs a warning. This should be rare with the 500-sample buffer unless flash writes are extremely slow.

### Resource Usage

**Memory (PSRAM-backed):**
- Queue: 500 × ~80 bytes = ~40KB
- Flash sample buffer: 4KB
- Total: ~44KB in PSRAM

**CPU Load Distribution:**
- Core 0: BLE (2Hz) + Display (0.25Hz) + Flash writes (periodic) + StatusMonitor loop (20Hz)
- Core 1: Sensor reads (1-10Hz) + WebSocket broadcasts (5Hz) + RTLogger loop (50-100Hz)

### Benefits of This Architecture

✅ **Core 1 never blocks** - Deterministic sensor timing
✅ **Core 0 can block** - Flash writes don't affect real-time acquisition
✅ **Large buffer** - 50 seconds of headroom if Core 0 gets busy
✅ **Visible health** - Queue monitoring shows if buffer fills
✅ **Reduced Core 0 load** - WebSocket JSON on Core 1, not Core 0

### Troubleshooting

**Queue overruns (samples dropped):**
- Flash writes are too slow → Reduce sample rate
- Core 0 is starved → Reduce BLE/display frequency further
- Queue too small → Increase QUEUE_SIZE beyond 500

**Core 0 watchdog resets:**
- Check queue usage - if consistently >80%, Core 0 is overloaded
- Verify flash erase operations complete within 10ms
- Ensure BLE updates yield properly

**Core 1 jitter:**
- Verify sensor reads are non-blocking
- Check queue sends never block (timeout=0)
- Monitor loop timing in RTLoggerThread
