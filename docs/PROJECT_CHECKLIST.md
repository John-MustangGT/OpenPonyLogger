# OpenPonyLogger — V2 Prototype Project Checklist

This document is the project checklist for the V2 prototype of OpenPonyLogger (ESP32-S3). It collects the outstanding design decisions, implementation tasks, and tests we should complete before the first hardware/software prototype is declared "ready".

Status legend:
- [ ] Open
- [x] Done
- [~] In progress / TBD

## High-priority (must decide / implement before coding)

- [ ] Partition table & flash layout
  - Decide final partition CSV for ESP-IDF (2 MB logging partition + 2 MB app partition + NVS/config). Consider OTA needs now vs later.
  - Reserve small NVS partition for config/storage.

- [ ] Logging partition format & atomicity guarantees
  - Define exact startup header structure and sizes.
  - Choose header redundancy approach (A/B headers or journaled updates) to allow recovery from partial writes.
  - Decide alignment and erase strategy (use 4 KB flash sector alignment).

- [ ] Block/chunk size, compression, and flash-write strategy
  - Pick compression chunk size (e.g., match buffer size: 16 KB default, consider 64 KB with PSRAM).
  - Define block header layout (magic, uncompressed_size, compressed_size, CRC, timestamp, startup id).
  - Decide whether compressed blocks are aligned to sector boundaries or packed.

- [ ] Binary record / logging schema
  - Define compact, forward-compatible record formats for:
    - IMU sample (fixed-length)
    - GPS record (parsed fields or raw NMEA with header)
    - CAN frame record (ID, DLC, bytes, flags)
    - System events (startup/shutdown, GPS lock)
  - Decide timestamp format (int64 esp_timer microseconds since boot) and any delta encoding rules.

- [ ] Data integrity and recovery strategy
  - CRC granularity (per block recommended) and method for detecting partial writes.
  - Define startup index layout at partition top (number of entries, entry layout).
  - Plan for circular overwrite and safe erase-ahead strategy.

- [ ] PSRAM usage & allocation strategy
  - Decide which buffers live in internal SRAM vs PSRAM (recommend: acquisition buffers in fast SRAM if possible; compression and staging buffers in PSRAM).
  - Confirm allocation strategy using ESP-IDF heap caps and test for DMA constraints.

- [ ] Build system & library integration
  - Confirm ESP-IDF version and PlatformIO configuration.
  - Add minmea as a component (CMakeLists) and LZ4 as a component.
  - Select NimBLE (recommended) for BLE stack and ensure component config in sdkconfig.

## Important (affects reliability & performance)

- [ ] Buffer sizing & sample rates
  - Finalize sample rates and worst-case CAN/BLE burst scenarios.
  - Compute buffer sizes and consider increasing default 16 KB buffers (e.g., 64 KB) using PSRAM.

- [ ] FreeRTOS & concurrency
  - Finalize task priorities and stack sizes (sensorTask, storageTask, BLE handlers).
  - Design safe BLE notification handling (queue from BLE callback to task).

- [ ] Flash erase & wear-leveling
  - Implement erase-ahead and minimize repetitive erases to the same sectors.
  - Consider a simple wear metric or rotation strategy if needed.

- [ ] Power and brownout handling
  - Decide brownout thresholds and safe-shutdown behavior.
  - Add battery monitoring (ADC or fuel gauge) and test charging while logging.

- [ ] GPS time sync & startup behavior
  - Define when to capture GPS_UTC time at lock and how to mark sessions without GPS lock.

- [ ] Boot/startup index design
  - Define how many startup entries to keep and Journal/A/B update strategy for the index.

## Optional / Nice-to-have

- [ ] On-device manifest (firmware version, hardware revision, flash/PSRAM sizes)
- [ ] Host-side recovery tools (script to read raw partition and parse logs)
- [ ] Reserve space for OTA (if desired in future)
- [ ] Live telemetry subset streaming (Wi-Fi/BLE)

## Testing & Validation (must pass)

- [ ] Unit tests
  - Binary encode/decode for records
  - LZ4 compress/decompress roundtrip
  - Header/index journaling with simulated partial writes

- [ ] Bring-up tests
  - I2C: detect GPS & IMU; validate GPIO7 power-enable behavior
  - BLE: connect to VGate iCar Pro, subscribe, and validate CAN frame reception
  - PSRAM: allocate large buffers and confirm no OOM
  - Flash: write/read the logging partition and validate circular overwrite
  - Power cycling: repeated abrupt power loss and recovery tests

- [ ] Performance tests
  - Throughput test at target IMU sample rate + worst-case CAN bursts
  - Measure LZ4 compression time and flash write latency to ensure acquisition keeps up

## Dev workflow / repo tasks

- [ ] Add ESP-IDF partition CSV to repo (2MB log, 2MB app, NVS)
- [ ] Add PlatformIO example config or ESP-IDF CMakeLists snippet to enable PSRAM and include components
- [ ] Add C header file(s) documenting record and block header structs (byte-aligned)
- [ ] Create minimal test harness component that simulates writes and power-loss
- [ ] Add docs/PROJECT_CHECKLIST.md (this file)

## Suggested GitHub Issues (titles & short descriptions)
- Partition table & logging partition: "Partition table: add 2MB logging partition + 2MB app partition + NVS" — add CSV and documentation
- Startup header & journal: "Define startup header and journaled header updates for logging partition" — propose byte layout and ensure recoverability
- Block format & compression: "Define block header and compression strategy (LZ4)" — include alignment and header fields
- Binary record spec: "Define IMU/GPS/CAN record binary formats for log" — include example encoders/decoders
- PSRAM allocation strategy: "Decide PSRAM vs SRAM use for buffers & compression" — add memory map and allocation plan
- BLE integration tests: "BLE ELM327 integration and CAN frame handling" — add test plan
- Flash wear-leveling: "Circular logging erase / wear-leveling strategy" — implement and test

## Next steps (pick one to start)
- [ ] Draft the exact binary header & block formats (I can create byte-level C structs and documentation)
- [ ] Produce the ESP-IDF partition CSV and example sdkconfig settings
- [ ] Create PlatformIO + ESP-IDF component skeleton including minmea & lz4

---
