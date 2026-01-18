# OpenPony Binary Log Format (.opl)

## Overview

The OpenPony Logger uses a custom binary format (`.opl` extension) designed for efficient storage of high-frequency sensor data from GPS, IMU, and OBD-II sources. The format features:

- **Compression**: Data blocks are compressed using Heatshrink (LZSS)
- **Data Integrity**: CRC32 checksums on all headers and data blocks
- **Session Tracking**: UUID-based session identification
- **Time Synchronization**: GPS UTC time correlation with ESP32 timer
- **Compact Storage**: ~60% smaller than equivalent CSV files

## File Structure

Each `.opl` file contains:

```
[Session Header] [Data Block 1] [Data Block 2] ... [Data Block N]
```

### 1. Session Start Header (68 bytes)

The file begins with a session header that identifies the logging session:

| Offset | Type       | Size | Field Name          | Description                                    |
|--------|------------|------|---------------------|------------------------------------------------|
| 0x00   | uint32_t   | 4    | magic               | Magic number: `0x53545230` ("STR0")           |
| 0x04   | uint8_t    | 1    | version             | Format version (currently `0x01`)              |
| 0x05   | uint8_t[3] | 3    | reserved            | Reserved for future use                        |
| 0x08   | uint8_t[16]| 16   | startup_id          | UUIDv4 session identifier                      |
| 0x18   | int64_t    | 8    | esp_time_at_start   | ESP timer at startup (microseconds)            |
| 0x20   | int64_t    | 8    | gps_utc_at_lock     | GPS UTC time at first lock (seconds since epoch, 0 if unknown) |
| 0x28   | uint8_t[6] | 6    | mac_addr            | Device MAC address (from efuse)                |
| 0x2E   | uint8_t[8] | 8    | fw_sha              | Firmware git commit SHA (8 bytes)              |
| 0x36   | uint32_t   | 4    | startup_counter     | Monotonic startup counter (from NVS)           |
| 0x3A   | uint32_t[2]| 8    | reserved2           | Reserved for future use                        |
| 0x42   | uint32_t   | 4    | crc32               | CRC32 of header (excluding this field)         |

**Total:** 68 bytes (0x44)

### 2. Data Blocks (Variable Size)

Following the session header are one or more compressed data blocks:

#### Data Block Header (48 bytes)

| Offset | Type       | Size | Field Name          | Description                                    |
|--------|------------|------|---------------------|------------------------------------------------|
| 0x00   | uint32_t   | 4    | magic               | Magic number: `0x4C4F4742` ("LOGB")           |
| 0x04   | uint8_t    | 1    | version             | Block version (currently `0x01`)               |
| 0x05   | uint8_t[3] | 3    | reserved            | Reserved/padding                               |
| 0x08   | uint8_t[16]| 16   | startup_id          | UUIDv4 session identifier (matches header)     |
| 0x18   | int64_t    | 8    | timestamp_us        | ESP timer at block creation (microseconds)     |
| 0x20   | uint32_t   | 4    | uncompressed_size   | Size of data before compression                |
| 0x24   | uint32_t   | 4    | compressed_size     | Size of compressed payload                     |
| 0x28   | uint32_t   | 4    | crc32               | CRC32 of compressed payload                    |

**Total:** 48 bytes (0x30)

#### Compressed Payload

Immediately following each block header is the compressed payload (Heatshrink/LZSS format):
- Size: `compressed_size` bytes
- After decompression: `uncompressed_size` bytes
- Contains binary-packed sensor samples

### 3. Sensor Sample Format (Uncompressed Data)

After decompressing a data block, the payload contains a sequence of timestamped sensor samples. Each sample has:

```
[Sample Type (1 byte)] [Timestamp Delta (4 bytes)] [Payload (variable)]
```

#### Sample Types

| Type ID | Sample Name       | Payload Size | Description                          |
|---------|-------------------|--------------|--------------------------------------|
| 0x01    | Accelerometer     | 12 bytes     | 3x float32 (x, y, z) in m/s²        |
| 0x02    | Gyroscope         | 12 bytes     | 3x float32 (x, y, z) in rad/s       |
| 0x03    | Compass/Magnetometer | 12 bytes  | 3x float32 (x, y, z) in μT          |
| 0x04    | GPS Fix           | 24 bytes     | GPS position and velocity data       |
| 0x05    | GPS Satellites    | Variable     | Satellite tracking information       |
| 0x06    | OBD-II Data       | Variable     | OBD-II PID data                      |
| 0x07    | Battery           | 12 bytes     | Battery voltage, current, SoC        |

#### Sample Type 0x01: Accelerometer (17 bytes total)

```
[0x01] [timestamp_delta_us: uint32_t] [x: float32] [y: float32] [z: float32]
```

- `x`, `y`, `z`: Acceleration in m/s²
- Coordinate system: Right-hand rule, device-relative

#### Sample Type 0x02: Gyroscope (17 bytes total)

```
[0x02] [timestamp_delta_us: uint32_t] [x: float32] [y: float32] [z: float32]
```

- `x`, `y`, `z`: Angular velocity in rad/s
- Coordinate system: Right-hand rule, device-relative

#### Sample Type 0x03: Compass/Magnetometer (17 bytes total)

```
[0x03] [timestamp_delta_us: uint32_t] [x: float32] [y: float32] [z: float32]
```

- `x`, `y`, `z`: Magnetic field strength in μT (microteslas)

#### Sample Type 0x04: GPS Fix (29 bytes total)

```
[0x04] [timestamp_delta_us: uint32_t] 
[latitude: float64] [longitude: float64] 
[altitude: float32] [speed: float32] 
[heading: float32] [hdop: float32]
```

- `latitude`: Degrees (WGS84)
- `longitude`: Degrees (WGS84)
- `altitude`: Meters above sea level
- `speed`: Meters per second
- `heading`: Degrees (0-359, true north)
- `hdop`: Horizontal Dilution of Precision

## Decoding Procedure

### Step 1: Read Session Header

```python
import struct

with open('session_12345.opl', 'rb') as f:
    # Read session header (68 bytes)
    header_data = f.read(68)
    
    # Unpack header
    magic, version = struct.unpack('<IB', header_data[0:5])
    assert magic == 0x53545230, "Invalid magic number"
    assert version == 0x01, "Unsupported version"
    
    startup_id = header_data[8:24]
    esp_time = struct.unpack('<q', header_data[24:32])[0]
    gps_utc = struct.unpack('<q', header_data[32:40])[0]
    mac_addr = header_data[40:46]
    fw_sha = header_data[46:54]
    
    # Verify CRC32
    crc_stored = struct.unpack('<I', header_data[64:68])[0]
    crc_calc = zlib.crc32(header_data[0:64])
    assert crc_stored == crc_calc, "Header CRC mismatch"
```

### Step 2: Read Data Blocks

```python
import zlib

while True:
    # Read block header (48 bytes)
    block_header = f.read(48)
    if len(block_header) < 48:
        break  # End of file
    
    # Unpack block header
    magic = struct.unpack('<I', block_header[0:4])[0]
    if magic != 0x4C4F4742:
        break  # End of valid blocks
    
    version = block_header[4]
    timestamp_us = struct.unpack('<q', block_header[24:32])[0]
    uncompressed_size = struct.unpack('<I', block_header[32:36])[0]
    compressed_size = struct.unpack('<I', block_header[36:40])[0]
    crc_stored = struct.unpack('<I', block_header[40:44])[0]
    
    # Read compressed payload
    compressed_data = f.read(compressed_size)
    
    # Verify CRC32
    crc_calc = zlib.crc32(compressed_data)
    assert crc_stored == crc_calc, "Block CRC mismatch"
    
    # Decompress
    uncompressed_data = zlib.decompress(compressed_data)
    assert len(uncompressed_data) == uncompressed_size
    
    # Parse samples (see next step)
    parse_samples(uncompressed_data, timestamp_us)
```

### Step 3: Parse Sensor Samples

```python
def parse_samples(data, block_timestamp_us):
    offset = 0
    samples = []
    
    while offset < len(data):
        sample_type = data[offset]
        offset += 1
        
        # Read timestamp delta
        timestamp_delta = struct.unpack('<I', data[offset:offset+4])[0]
        offset += 4
        
        sample_timestamp = block_timestamp_us + timestamp_delta
        
        if sample_type == 0x01:  # Accelerometer
            x, y, z = struct.unpack('<fff', data[offset:offset+12])
            offset += 12
            samples.append(('accel', sample_timestamp, x, y, z))
            
        elif sample_type == 0x02:  # Gyroscope
            x, y, z = struct.unpack('<fff', data[offset:offset+12])
            offset += 12
            samples.append(('gyro', sample_timestamp, x, y, z))
            
        elif sample_type == 0x04:  # GPS
            lat, lon = struct.unpack('<dd', data[offset:offset+16])
            offset += 16
            alt, speed, heading, hdop = struct.unpack('<ffff', data[offset:offset+16])
            offset += 16
            samples.append(('gps', sample_timestamp, lat, lon, alt, speed, heading, hdop))
    
    return samples
```

## Time Correlation

To convert ESP timer timestamps to UTC time:

```python
# From session header
esp_time_at_start = session_header['esp_time_at_start']  # microseconds
gps_utc_at_lock = session_header['gps_utc_at_lock']      # seconds since epoch

# For any sample timestamp
sample_esp_time = sample['timestamp_us']  # microseconds

# Calculate UTC timestamp
if gps_utc_at_lock > 0:
    # We have GPS time correlation
    esp_delta = sample_esp_time - esp_time_at_start
    utc_timestamp = gps_utc_at_lock + (esp_delta / 1_000_000)
else:
    # No GPS lock - use relative time only
    utc_timestamp = None
```

## Data Integrity Checks

1. **Session Header CRC**: Verify CRC32 of first 64 bytes
2. **Block Header UUID**: Ensure `startup_id` matches session header
3. **Block CRC**: Verify CRC32 of compressed payload
4. **Decompression**: Verify decompressed size matches `uncompressed_size`
5. **Sample Parsing**: Ensure all samples are fully parsed without overflow

## Tools and Libraries

- **Python**: See `tools/openpony_decoder.py` for reference implementation
- **CSV Export**: See `tools/opl_to_csv.py` for conversion utility

## Version History

- **v0.01** (2026-01-18): Initial format specification
  - Session header with UUID tracking
  - Compressed data blocks with CRC32
  - Support for GPS, IMU, and OBD-II data
