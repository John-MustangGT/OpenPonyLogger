# OpenPonyLogger Architecture

## Multi-Core Threading Architecture

This document describes the dual-core architecture, thread priorities, synchronization mechanisms, and data structure interactions used in the OpenPonyLogger.

---

## Design Philosophy

**Core 1: Real-Time Sensor Acquisition (Never Blocks)**
- Reads sensors at high frequency (1-10Hz)
- Writes ONLY to fast PSRAM queues via FreeRTOS
- Never waits on slow I/O operations
- Maintains deterministic timing for data acquisition

**Core 0: Housekeeping & Slow I/O (Blocking OK)**
- Handles BLE stack (NimBLE Core 0 requirement)
- Manages WiFi, WebSocket, and display updates
- Drains PSRAM queue and writes to storage (blocking operations isolated here)
- Button handling and power management

---

## Thread/Task Allocation Table

### Core 1 Tasks (Real-Time)

| Task Name | Core | Priority | Stack | Purpose | Data Structures Accessed | Synchronization |
|-----------|------|----------|-------|---------|-------------------------|-----------------|
| **RTLoggerThread** | 1 | 2 (High) | 4096 | Primary sensor collection loop | • m_last_gps/accel/gyro/compass/battery (read/write)<br>• PSRAM queue (write-only)<br>• SensorManager (read-only) | • **Volatile flags**: m_running, m_storage_paused, m_mark_event<br>• **Lock-free**: Queue write with 0 timeout |
| **Arduino loop()** | 1 | 1 | 8192 | Triggers storage write every 5s | • RTLoggerThread (read-only) | None (calls trigger_storage_write()) |

### Core 0 Tasks (Housekeeping)

| Task Name | Core | Priority | Stack | Purpose | Data Structures Accessed | Synchronization |
|-----------|------|----------|-------|---------|-------------------------|-----------------|
| **StatusMonitor** | 0 | 1 (Low) | 6144 | Button handling, power management, BLE updates | • GPIO pins (buttons, VBUS)<br>• RTLoggerThread (read-only)<br>• FlashStorage/SDStorage (OBD queuing)<br>• IcarBleDriver (BLE updates)<br>• ST7789Display (SPI writes) | • **Volatile flags**: m_running, m_shutdown_pending<br>• **Yields**: vTaskDelay(1ms) after display/BLE ops |
| **FlashStorage/SDStorage Writer** | 0 | 1 (Low) | 8192 | Drains PSRAM queue to storage | • PSRAM queue (read via xQueueReceive)<br>• Flash partition or SD file (blocking writes)<br>• m_sample_buffer (4KB block)<br>• NVS (offset tracking) | • **Blocking OK**: Flash erase (1-10ms)<br>• **Yields**: Every 100ms queue timeout |
| **TimeUpdateTask** | 0 | 1 (Low) | 2048 | GPS time sync to RTC/NVS | • GPS time queue<br>• RTCManager<br>• NVS (time storage) | • Small queue (10 items)<br>• Periodic wake (1Hz) |
| **WiFi/WebSocket** | 0 | Variable | N/A | AsyncWebServer handles WiFi | • WiFiManager static state<br>• ConfigManager (NVS reads) | • **Internal**: ESPAsyncWebServer threading<br>• **Static guards**: m_initialized flag |
| **BLE Stack (NimBLE)** | 0 | Variable | N/A | OBD-II Bluetooth LE | • IcarBleDriver static state<br>• BLE scan/connection state | • **Requirement**: NimBLE MUST run on Core 0<br>• **Callbacks**: Run on Core 0 |

---

## Data Structures & Synchronization

### 1. PSRAM Queue (Core 1 → Core 0)

**Purpose:** Decouple sensor acquisition (Core 1) from storage writes (Core 0)

**Specification:**
```cpp
QueueHandle_t m_sample_queue;
static constexpr size_t QUEUE_SIZE = 500;  // ~50 seconds at 10Hz
```

**Allocation:** PSRAM (external SPIRAM)
```cpp
m_queue_buffer = (StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_SPIRAM);
m_queue_storage = (uint8_t*)heap_caps_malloc(QUEUE_SIZE * sizeof(SampleData), MALLOC_CAP_SPIRAM);
m_sample_queue = xQueueCreateStatic(QUEUE_SIZE, sizeof(SampleData), m_queue_storage, m_queue_buffer);
```

**Memory:** ~24 KB (500 samples × 48 bytes)

**Thread Access:**
- **Core 1 (Write)**: `xQueueSend(queue, &sample, 0)` - **0 timeout, never blocks**
- **Core 0 (Read)**: `xQueueReceive(queue, &sample, pdMS_TO_TICKS(100))` - 100ms timeout

**Synchronization:**
- Lock-free for writes (0 timeout)
- Core 0 blocks waiting for data (acceptable)
- Overrun detection: Drops sample if queue full, logs warning

---

### 2. Volatile Cross-Thread Flags

**Purpose:** Signal state changes between cores without heavy synchronization

**RTLoggerThread Flags (Core 1 reads, Core 0/others write):**
```cpp
volatile bool m_running;           // Thread lifecycle control
volatile bool m_storage_paused;    // Pause/resume logging
volatile bool m_mark_event;        // Event marker flag
```

**StatusMonitor Flags (Core 0 reads/writes):**
```cpp
volatile bool m_running;
volatile bool m_shutdown_pending;
volatile bool m_shutdown_initiated;
volatile bool m_usb_powered;
```

**Why Volatile:**
- Prevents compiler from caching in registers
- Ensures visibility across CPU cores
- Guarantees atomic read/write on ESP32-S3 (aligned single-byte/word)

**No Mutexes:** Flags are single-direction signals, not shared resources requiring mutual exclusion

---

### 3. Sensor Data Structures (Read-Only Sharing)

**RTLoggerThread Latest Samples:**
```cpp
gps_data_t m_last_gps;
accel_data_t m_last_accel;
gyro_data_t m_last_gyro;
compass_data_t m_last_compass;
battery_data_t m_last_battery;
```

**Access Pattern:**
- **Core 1**: Writes periodically (1-10Hz)
- **Core 0**: Reads for display/status (4 second intervals)

**Synchronization:** None needed - reads are informational only, stale data acceptable

---

### 4. NVS (Non-Volatile Storage)

**Purpose:** Persist configuration, offsets, time

**Thread-Safe:** Yes (Preferences library handles locking internally)

**Accessed By:**
- **ConfigManager**: Load/save settings (any thread, typically Core 0)
- **FlashStorage**: Save write offset (Core 0)
- **SDStorage**: Session tracking (Core 0)
- **RTCManager**: Time persistence (Core 0)

**Key Pattern:**
```cpp
Preferences prefs;
prefs.begin(namespace, readOnly);
// ... read/write operations ...
prefs.end();  // Auto-closes, releases locks
```

---

### 5. Storage Backends (Core 0 Only)

**Flash Storage (Internal 2MB):**
```cpp
const esp_partition_t* m_partition;
size_t m_write_offset;
uint8_t m_sample_buffer[4096];
```

**SD Storage (Adalogger FeatherWing):**
```cpp
File m_current_file;
String m_current_file_path;
uint8_t m_sample_buffer[4096];
```

**Synchronization:** None needed - only Core 0 writer task accesses

**Shared SPI Bus:**
- TFT Display: CS=GPIO42
- SD Card: CS=GPIO10
- SPI.begin() called once by ST7789Display::init()
- Both devices share MOSI/MISO/SCK (GPIO35/37/36)

---

## Detailed Thread Interactions

### RTLoggerThread (Core 1)

**Interacts With:**
- **SensorManager** (read sensors via I2C/UART)
- **PSRAM Queue** (write sensor samples)
- **WiFiManager** (WebSocket broadcasts @ 5Hz)
- **StatusMonitor** (reads via get_last_* methods)

**Flow:**
```
1. Wake every 100ms (10Hz main loop)
2. Read sensors (GPS, IMU, Battery)
3. Package into SampleData structures
4. xQueueSend() to PSRAM queue (0 timeout)
   ├─ If queue full: Drop sample, increment overrun counter
   └─ If success: Increment samples_queued
5. Broadcast WebSocket JSON @ 5Hz
6. Check volatile flags (m_storage_paused, m_running)
7. Repeat
```

**Never Blocks On:**
- Flash/SD writes (handled by Core 0)
- BLE operations (owned by Core 0)
- Display updates (owned by Core 0)

---

### StatusMonitor (Core 0)

**Interacts With:**
- **GPIO Pins** (buttons, VBUS detection)
- **RTLoggerThread** (read sensor data for display)
- **FlashStorage/SDStorage** (queue OBD samples directly)
- **IcarBleDriver** (BLE updates @ 10Hz)
- **ST7789Display** (SPI updates every 4s)
- **WiFiManager** (check client count)

**Flow:**
```
1. Wake every 50ms (20Hz loop)
2. Check buttons (GPIO0/1/2)
   └─ Debounce, trigger actions
3. Check VBUS (GPIO19) - USB power monitoring
   ├─ If lost: Start 60s countdown
   ├─ If timeout: Enter 2-stage sleep
   └─ If restored: Cancel shutdown
4. Update BLE @ 10Hz (100ms interval)
   ├─ IcarBleDriver::loop()
   ├─ If OBD data received:
   │  ├─ Capture timestamp: esp_timer_get_time()
   │  └─ queue_obd_sample() → PSRAM queue
   └─ vTaskDelay(1ms) - yield
5. Update display every 4s
   └─ vTaskDelay(1ms) after SPI ops
6. Check volatile flags (m_running)
7. Repeat
```

**Auto-Start/Stop Logic:**
```
If dynamics_auto_enabled:
  speed > threshold → resume_storage()
  speed == 0 for timeout → pause_storage()

If data_auto_enabled:
  RPM > 0 → resume_storage()
  RPM == 0 for timeout → pause_storage()
```

---

### Storage Writer Task (Core 0)

**Interacts With:**
- **PSRAM Queue** (drain via xQueueReceive)
- **Flash Partition or SD File** (blocking writes)
- **NVS** (save write offset)

**Flow:**
```
1. Wait for data: xQueueReceive(queue, &sample, 100ms)
   └─ Blocks up to 100ms (yields CPU)
2. If data received:
   ├─ Append to m_sample_buffer[4096]
   ├─ If buffer full: flush_block()
   │  ├─ Create log_block_t header
   │  ├─ Calculate CRC32
   │  ├─ Write to flash/SD (BLOCKING 1-10ms)
   │  └─ Update offset
   └─ Reset buffer
3. Every 5 seconds: flush_block() (periodic)
4. Check m_paused flag
5. Repeat
```

**Blocking Operations (Isolated to Core 0):**
- `esp_partition_erase_range()` - 1-10ms
- `esp_partition_write()` - 1-5ms
- `SD.write()` + `SD.flush()` - 1-20ms (card dependent)

---

## Power Management & Sleep

### 2-Stage Sleep System

**Stage 1: Light Sleep (5 minutes)**
```cpp
esp_sleep_enable_ext0_wakeup(GPIO19, 1);      // USB power restore
esp_sleep_enable_timer_wakeup(5 * 60 * 1e6);  // 5 minute timeout
esp_light_sleep_start();
```

**Power:** ~0.8 mA (WiFi/BLE powered, RAM retained)

**Wake Sources:**
- GPIO19 (VBUS) goes HIGH → USB restored → Resume
- Timer expires → Proceed to Stage 2

**Stage 2: Deep Sleep (Until USB)**
```cpp
esp_sleep_enable_ext0_wakeup(GPIO19, 1);  // USB power restore ONLY
esp_deep_sleep_start();
```

**Power:** ~10 µA (device reboots on wake)

**Wake Source:**
- GPIO19 (VBUS) goes HIGH → USB restored → Full reboot

**Critical Note:** ESP32-S3 can use **EITHER** ext0 **OR** ext1, not both!

---

## Memory Allocation Strategy

### Internal SRAM (512 KB)

**Reserved For:**
- Stack for all tasks (~28 KB total: RTLogger 4KB + loop 8KB + StatusMonitor 6KB + Storage 8KB + TimeUpdate 2KB)
- Static variables and globals (~20 KB)
- Heap for small allocations (~464 KB free)

**NOT Used For:**
- Large queues (moved to PSRAM)
- Sample buffers (in PSRAM or static)

### External PSRAM (2 MB)

**Allocated For:**
- PSRAM Queue: 24 KB (500 × 48 bytes)
- Future large buffers

**Allocation:**
```cpp
heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
```

### Flash (4 MB Total)

**Flash Build:**
- App: 1.5 MB (factory partition)
- Storage: 2 MB (circular buffer)

**SD Card Build:**
- Factory: 1.5 MB
- OTA: 1.5 MB (dual partition for safe updates)
- Storage: SD card (unlimited)

---

## Thread Safety Summary

| Mechanism | Use Case | Thread Access |
|-----------|----------|---------------|
| **Volatile Flags** | Simple state signaling | Core 0 ↔ Core 1 |
| **PSRAM Queue** | Sensor data buffering | Core 1 (write) → Core 0 (read) |
| **NVS (Preferences)** | Configuration persistence | Any thread (internally locked) |
| **Read-Only Sharing** | Display latest sensor values | Core 1 (write) ← Core 0 (read) |
| **Core Isolation** | Storage, BLE, WiFi | Core 0 only |
| **No Mutexes** | Lock-free design | Volatile + queues sufficient |

---

## Performance Characteristics

### Queue Health Monitoring

```
[FlashStorage] Queue health: 45/500 used (9.0%), 0 overruns, 12450 queued
```

**Warnings:**
- Queue >80% full → Core 0 struggling
- Overruns detected → Dropped samples

**Typical Usage:**
- Normal operation: <10% full
- Flash write bursts: 20-30% full
- WiFi heavy activity: 30-50% full

### CPU Load Distribution

**Core 0:**
- BLE updates: 10Hz (5% CPU)
- Display: 0.25Hz (2% CPU)
- Button scanning: 20Hz (1% CPU)
- Storage writes: Periodic (10-20% CPU)
- **Idle:** ~70%

**Core 1:**
- Sensor reads: 10Hz (15% CPU)
- WebSocket: 5Hz (10% CPU)
- Queue writes: Negligible (<1% CPU)
- **Idle:** ~75%

---

## Troubleshooting Guide

### Queue Overruns (Samples Dropped)

**Symptoms:**
```
[FlashStorage] WARNING: Queue overrun! Dropped sample (queue full)
[FlashStorage] Queue health: 500/500 used (100.0%), 15 overruns
```

**Causes:**
- Flash writes too slow (check erase times)
- Core 0 watchdog starvation
- WiFi/BLE eating too much Core 0 time

**Solutions:**
- Reduce sample rate (10Hz → 5Hz)
- Increase QUEUE_SIZE beyond 500
- Reduce WiFi broadcast rate
- Add more vTaskDelay() yields

---

### Core 0 Watchdog Resets

**Symptoms:**
```
Task watchdog got triggered. The following tasks did not reset the watchdog in time:
 - IDLE0 (CPU 0)
```

**Causes:**
- Display SPI taking too long without yielding
- BLE operations blocking
- Flash erase >10ms

**Solutions:**
- Add vTaskDelay(1) after display updates
- Reduce display update frequency
- Check flash erase times with profiling

---

### Race Conditions / Missed Events

**Symptoms:**
- Pause/resume doesn't work
- Event marks disappear
- Shutdown doesn't trigger

**Cause:** Missing `volatile` keyword on cross-thread flags

**Solution:** Already fixed - all flags marked `volatile`

---

## Build-Time Configuration

### Storage Backend Selection

**Flash Build (Default):**
```bash
pio run -e flash
```
- FlashStorage backend
- 2MB internal storage
- WiFi download only

**SD Card Build:**
```bash
pio run -e sdcard
```
- SDStorage backend
- Adalogger FeatherWing
- Dual OTA partitions

**Conditional Compilation:**
```cpp
#if defined(USE_SD_CARD)
    #include "sd_storage.h"
    #define StorageBackend SDStorage
#elif defined(USE_FLASH_STORAGE)
    #include "flash_storage.h"
    #define StorageBackend FlashStorage
#endif
```

---

## References

- ESP32-S3 Technical Reference: Dual-core architecture
- FreeRTOS Documentation: Queue, task, synchronization primitives
- NimBLE Stack: Core 0 requirement
- [GPIO Pin Mapping](docs/GPIO_PIN_MAPPING.md)
- [Power Management](docs/POWER_MANAGEMENT.md)
- [Log Format](docs/LOG_FORMAT.md)

---

**Last Updated:** January 22, 2026
**Firmware Version:** v1.0
**Hardware:** ESP32-S3 Feather TFT + Adalogger FeatherWing
