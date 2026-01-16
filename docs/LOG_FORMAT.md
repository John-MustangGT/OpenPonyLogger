# Log Format — OpenPonyLogger (V2)

## Overview
- Logging partition: raw binary rolling storage (no filesystem), 2 MB.
- Data is written in compressed blocks (LZ4) produced from in-memory acquisition buffers (default 16 KB).
- Each compressed block is preceded by a fixed block header containing metadata and a CRC of the compressed payload.
- Records inside uncompressed data are raw device bytes prefixed with a record-type and a timestamp offset from the Session Start (µs since session start).

## Record header
Each record inside an uncompressed buffer begins with:
- uint8_t msg_type;               // message type
- uint64_t timestamp_offset_us;   // microsecond offset from Session Start (little-endian)

All numeric fields are little-endian.

## Message / Record Types
- 0x01 — IMU (accelerometer + gyro) — fixed-length record
- 0x02 — GPS (parsed NMEA fields or parsed minmea output) — variable-length
- 0x03 — CAN Frame — variable-length (ID, DLC, data bytes)
- 0x04 — COMPASS — bearing (float32, IEEE-754) degrees (0.0 - 360.0)

Notes:
- Each record = [ msg_type (1) | timestamp_offset_us (8) | payload... ]
- Payload encoding per type is defined in RECORD_SCHEMA.md (or in-code headers) — IMU should be fixed-size for efficiency.

## Block Header (log_block_header_t)
Each compressed block written to flash is preceded by this packed header. Fields are little-endian.

- uint32_t magic;           // 'LOGB' (0x4C4F4742)
- uint8_t  version;         // format version (e.g., 0x01)
- uint8_t  reserved[3];     // padding / future flags
- uint8_t  startup_id[16];  // UUIDv4 session ID
- int64_t  timestamp_us;    // esp_timer_get_time() when the buffer was closed (µs since boot)
- uint32_t uncompressed_size; // size of original uncompressed buffer (bytes)
- uint32_t compressed_size;   // size of compressed payload following header (bytes)
- uint32_t crc32;             // CRC32 of compressed payload

Immediately following the header: compressed payload (compressed_size bytes).

## Session Start Header
A Session Start marker is written at the beginning of each new logging session and a copy of summary metadata is stored in NVS rotating slots.

Suggested packed Session Start header fields:
- uint32_t magic;             // e.g., 'STR0'
- uint8_t  version;           // format version
- uint8_t  reserved[3];
- uint8_t  startup_id[16];    // UUIDv4 for this session
- int64_t  esp_time_at_start; // esp_timer_get_time() at startup (µs)
- int64_t  gps_utc_at_lock;   // seconds since epoch (0 if unknown at startup)
- uint8_t  mac_addr[6];       // device MAC from efuse
- uint8_t  fw_sha[8];         // short git SHA (8 bytes)
- uint32_t startup_counter;   // monotonic counter persisted in NVS (optional)
- uint32_t reserved2[2];      // future use
- uint32_t crc32;             // CRC32 of the header (excluding this field)

## NVS Session Index (persist last 8 sessions)
- Namespace: "logging"
- Keys:
  - "session_idx" (uint8_t) — next rotation index 0..7
  - "session_meta_0" .. "session_meta_7" — each a fixed-size blob containing:
    - startup_id (16 bytes)
    - first_block_offset (uint32_t)
    - last_block_offset (uint32_t)
    - session_start_time_esp_us (int64_t)
    - session_stop_time_esp_us (int64_t)
    - mac_addr (6 bytes)
    - fw_sha (8 bytes)

Write order recommendation when finalizing a session:
1. Write session_meta_X blob via nvs_set_blob(...)
2. nvs_commit()
3. Update session_idx to new index and nvs_commit()

On boot, read session_idx then read available session_meta blobs to reconstruct recent sessions. This preserves session boundaries even if the logging partition wraps.

## Write / Recovery Semantics
- Flow:
  1. Core 1 fills acquisition buffer in internal SRAM.
  2. Core 0 compresses the buffer into PSRAM.
  3. Writer task erases ahead as needed and writes the log_block_header_t and the compressed payload.
  4. CRC32 in header validates payload on recovery.

- Recovery:
  - On boot or host extraction, scan the storage partition for valid LOGB headers and verify CRC; ignore invalid/partial blocks.
  - Use NVS session index to find session boundaries; fall back to scanning if necessary.

## Checksums
- Use CRC32 (IEEE) for compressed payload. Consider also CRC over header+payload if higher integrity desired.

## Versioning
- Increment block/header version when layout changes. Parsers should skip unknown block versions.

## Appendix: example record encodings
- IMU: recommended fixed-size struct (accelerometer + gyro), e.g., 6 floats (6*4=24 bytes) or use int16 fixed-point to save space.
- Compass (0x04): single float32 (4 bytes) bearing in degrees.


Please ensure RECORD_SCHEMA.md is created with precise per-message payload layouts for IMU/GPS/CAN to allow host parsers to decode logs.