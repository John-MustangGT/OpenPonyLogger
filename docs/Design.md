# ESP32-S3 Real-Time Vehicle Logger — Design Document

## Overview
This design describes a dual-core ESP32-S3 real-time vehicle logger that captures GPS, IMU (accelerometer + gyroscope), and CAN-bus data (via BLE ELM327). The design uses core separation, double buffering, and LZ4 compression to support high-speed, reliable data logging with microsecond-resolution timestamps.

Key points:
- Core 1: real-time sensor acquisition and buffer filling
- Core 0: compression (LZ4), flash storage, display, and wireless tasks
- Double buffer: two 16 KB buffers swapped when full
- Timestamping: use esp_timer_get_time() for µs resolution; convert to UTC using GPS lock

---

## Table of Contents
- [System Architecture](#system-architecture)
- [Hardware Interfaces](#hardware-interfaces)
- [FreeRTOS Task Architecture](#freertos-task-architecture)
  - [Core 1 — Real-Time Sensor Acquisition](#core-1---real-time-sensor-acquisition)
  - [Core 0 — Processing, Display, and Storage](#core-0---processing-display-and-storage)
- [Double Buffer Handling](#double-buffer-handling)
- [BLE (ELM327) CAN Bus Integration](#ble-elm327-can-bus-integration)
- [Sensors: GPS, Accelerometer, Gyroscope](#sensors-gps-accelerometer-gyroscope)
- [Timestamping & RTC Strategy](#timestamping--rtc-strategy)
- [Core Pin / Task Assignment (ESP32-S3)](#core-pin--task-assignment-esp32-s3)
- [Notes & Optimizations](#notes--optimizations)
- [Summary](#summary)

---

## System Architecture

- Core 1 (high-priority)
  - Real-time sensor reading (I²C), BLE GATT notifications (ELM327), and buffer filling
- Core 0 (lower priority)
  - LZ4 compression, write to internal SPI flash, Wi‑Fi/BLE management, OLED display
- Communication between cores is via double buffer and FreeRTOS synchronization (semaphores / queues).

Simple block overview:
- Core 1 → fills Active Buffer (16 KB)
- When full → signal Core 0 via semaphore → swap buffers
- Core 0 → compress (LZ4) and write previous buffer to flash

---

## Hardware Interfaces

| Interface | Devices | Notes |
|-----------|---------|-------|
| I²C       | GPS, Accelerometer, Gyroscope | Pull-ups required; consider bus speed and sequencing |
| BLE       | ELM327 (OBD-II) | Receive CAN frames over GATT notifications |
| OLED      | SSD1306 (I²C or SPI) | Managed by Core 0 only |
| Flash     | Internal SPI Flash | Compress with LZ4 before writing to reduce wear and space |

---

## FreeRTOS Task Architecture

### Core 1 — Real-Time Sensor Acquisition
- Task priority: high
- Responsibilities:
  - Poll I²C sensors (accel/gyro)
  - Parse GPS messages
  - Receive BLE CAN notifications (ELM327)
  - Append records to the active 16 KB buffer
  - When buffer full, signal Core 0 and switch buffers
- Design goal: never block for long — keeps acquisition deterministic

Pseudocode (Core 1):
```cpp
// sensorTask pinned to Core 1
void sensorTask(void* param) {
    uint8_t* activeBuffer = bufferA;
    size_t bufferIndex = 0;

    while (true) {
        // Read sensors and append structured sample (with timestamp)
        appendAccelGyroSample(activeBuffer, &bufferIndex);
        appendGPSSampleIfAvailable(activeBuffer, &bufferIndex);
        appendBLECANFramesIfAvailable(activeBuffer, &bufferIndex);

        if (bufferIndex >= BUFFER_SIZE) {
            // notify storage task that a buffer is full
            xSemaphoreGive(bufferFullSemaphore);
            // swap buffers
            activeBuffer = (activeBuffer == bufferA) ? bufferB : bufferA;
            bufferIndex = 0;
        }

        // Wait until the next sample interval (tunable, e.g., 10-20 ms)
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}
```

### Core 0 — Processing, Display, and Storage
- Task priority: lower than sensorTask
- Responsibilities:
  - Wait for buffer-full semaphore
  - Compress the full buffer with LZ4
  - Write compressed blocks to internal flash
  - Update OLED display and handle Wi‑Fi/BLE responsibilities

Pseudocode (Core 0):
```cpp
// storageTask pinned to Core 0
void storageTask(void* param) {
    while (true) {
        // block until a buffer is reported full
        if (xSemaphoreTake(bufferFullSemaphore, portMAX_DELAY)) {
            // Determine the full buffer (the non-active one)
            uint8_t* fullBuffer = (activeBuffer == bufferA) ? bufferB : bufferA;

            // Compress using LZ4
            size_t compressedSize = LZ4_compress_default(
                (const char*)fullBuffer, (char*)compressedBuffer,
                BUFFER_SIZE, MAX_COMPRESSED_SIZE
            );

            // Write compressed data to flash
            writeFlash(compressedBuffer, compressedSize);

            // Optional: flash write batching, wear-leveling, CRC append, index header, etc.
        }
        // Small yield to allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

---

## Double Buffer Handling
- Two buffers: bufferA and bufferB (each 16 KB)
- Core 1 writes to the active buffer; when full, it signals Core 0 and atomically swaps to the other buffer
- Synchronization: use a FreeRTOS binary semaphore or queue to signal buffer-full events
- Important: Core 1 should not block waiting for Core 0 — if flash writes are too slow, consider larger buffers or additional buffering

---

## BLE (ELM327) CAN Bus Integration
- Connect to ELM327 over BLE (GATT)
- Subscribe to notifications that deliver CAN frames
- Each received CAN frame:
  - Timestamp with esp_timer_get_time() (µs since boot)
  - Append to activeBuffer with frame metadata (ID, DLC, data bytes)
- Buffer bursts internally on Core 1 if BLE delivers high-rate bursts

Record example:
- [timestamp_us, can_id, dlc, data[0..7]]

---

## Sensors: GPS, Accelerometer, Gyroscope
- GPS
  - Parse NMEA or binary messages; collect lat/lon, speed, heading
  - Use GPS UTC time to obtain absolute time reference when a lock is achieved
- IMU
  - Read accelerometer and gyroscope at 50–100 Hz (typical)
  - Timestamp each sample with µs resolution
- Sampling considerations
  - Combine IMU samples and GPS/CAN frames into compact binary records
  - Align samples to exact times using esp_timer_get_time()

---

## Timestamping & RTC Strategy
- ESP32-S3: no battery-backed RTC; use esp_timer_get_time() to obtain microsecond-resolution timestamps (int64_t) relative to boot
  - Resolution: 1 µs
- To convert to absolute UTC:
  - When GPS gets a lock, capture:
    - GPS_UTC_time_at_lock (e.g., seconds since epoch)
    - esp_time_at_lock (esp_timer_get_time())
  - For any logged sample:
    - absolute_utc = GPS_UTC_time_at_lock + (sample_esp_time - esp_time_at_lock) / 1_000_000.0

---

## Core Pin / Task Assignment (ESP32-S3)
- Example task creation:
```cpp
// Pin sensorTask to core 1 (real-time)
xTaskCreatePinnedToCore(sensorTask, "SensorTask", 4096, NULL, 10, NULL, 1);

// Pin storageTask to core 0 (IO, compression, display)
xTaskCreatePinnedToCore(storageTask, "StorageTask", 8192, NULL, 5, NULL, 0);
```
- Choose stack sizes and priorities based on profiling; storageTask needs enough stack for LZ4 buffers and flash driver calls.

---

## Notes / Optimizations
- Keep Core 1 fast and non-blocking:
  - Avoid any flash or long-running operations on Core 1
  - Use efficient binary record formats to minimize buffer usage
- BLE bursts:
  - Provide internal queueing or fixed-size CAN record buffering on Core 1
- I²C throughput:
  - Use I²C fast mode or DMA (if supported) for higher sample rates
- Compression:
  - LZ4 reduces storage usage and may increase effective write throughput by reducing flash write time
  - Pick compression block size to match flash page/erase boundaries for efficiency
- Wear-leveling & integrity:
  - Implement CRC or checksums per compressed block and an index or manifest to support robust post-processing and recovery

---

## Summary
- Dual-core ESP32-S3 architecture:
  - Core 1 for real-time sensor logging
  - Core 0 for compression, storage, display, wireless
- Double-buffering with two 16 KB buffers, semaphore-synchronized
- Use esp_timer_get_time() for µs-resolution timestamps and convert to UTC after GPS lock
- LZ4 compression before writing to internal SPI flash for efficient storage

---

If you want, I can:
- Produce a compact binary record layout / schema for logged samples (with field sizes)
- Create an example post-processing script to convert logs to CSV with UTC timestamps
- Generate a checklist for integration and testing on an ESP32-S3 board