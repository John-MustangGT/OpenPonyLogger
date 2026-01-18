# OpenPony Data Tools

Python utilities for decoding and processing OpenPony Logger binary files (.opl).

## Requirements

- Python 3.7+
- numpy (only required for Kalman filtering)

Install dependencies:
```bash
pip install numpy
```

## Files

- **openpony_decoder.py**: Core library for reading .opl binary format
- **opl_to_csv.py**: Command-line tool to convert .opl to CSV

## Quick Start

### 1. Download Log Files

Use the OpenPony Logger web interface (Download tab) to download .opl files from your device.

### 2. Convert to CSV

**Basic conversion (separate CSV per data type):**
```bash
python opl_to_csv.py session_12345.opl
```

This creates:
- `session_12345_gps.csv` - GPS position data
- `session_12345_accel.csv` - Accelerometer data
- `session_12345_gyro.csv` - Gyroscope data
- `session_12345_compass.csv` - Magnetometer data

**Single merged CSV:**
```bash
python opl_to_csv.py session_12345.opl --merged output.csv
```

**With GPS Kalman filtering:**
```bash
python opl_to_csv.py session_12345.opl --kalman
```

Kalman filtering smooths noisy GPS data and estimates velocity.

### 3. Inspect File

**View file information:**
```bash
python openpony_decoder.py session_12345.opl
```

Output:
```
Session Information:
  UUID: 550e8400-e29b-41d4-a716-446655440000
  MAC Address: 24:6f:28:7a:b2:c4
  Firmware SHA: a1b2c3d4e5f6g7h8
  GPS Lock Time: 2026-01-18T15:30:45+00:00

Data Blocks: 42
Total Samples: 12,543

Sample Breakdown:
  accel: 5,021
  gyro: 5,021
  gps: 2,501
```

## Usage Examples

### Export GPS Data with Filtering

```bash
python opl_to_csv.py session_12345.opl --gps-only --kalman
```

Creates `session_12345_gps.csv` with columns:
```
timestamp_utc,timestamp_us,latitude,longitude,altitude,speed,heading,hdop,latitude_filtered,longitude_filtered,velocity_lat,velocity_lon
2026-01-18T15:30:45.123456+00:00,1234567890,37.7749,-122.4194,15.2,12.5,45.0,1.2,37.7749,-122.4193,0.0001,0.0002
```

### Custom Kalman Parameters

Adjust filtering sensitivity:
```bash
python opl_to_csv.py session_12345.opl --kalman \
  --process-noise 0.05 \
  --measurement-noise 15.0
```

- **Process noise**: Lower = trust model more (smoother), higher = trust model less (more responsive)
- **Measurement noise**: Lower = trust GPS more (less filtering), higher = trust GPS less (more filtering)

### Python API

Use the decoder library in your own scripts:

```python
from openpony_decoder import decode_file

# Decode entire file
session, blocks = decode_file('session_12345.opl')

# Print session info
print(f"Session UUID: {session.startup_uuid}")
print(f"GPS locked at: {session.gps_utc_at_lock}")

# Process samples
for block in blocks:
    for sample in block.samples:
        utc_time = sample.to_utc(session)
        
        if sample.type == 'gps':
            print(f"{utc_time}: GPS position ({sample.data['latitude']}, {sample.data['longitude']})")
        
        elif sample.type == 'accel':
            print(f"{utc_time}: Accel ({sample.data['x']}, {sample.data['y']}, {sample.data['z']}) m/s²")
```

### Advanced Filtering Example

```python
from openpony_decoder import OpenPonyDecoder
from opl_to_csv import KalmanFilter

with OpenPonyDecoder('session_12345.opl') as decoder:
    session = decoder.read_session_header()
    kalman = KalmanFilter(process_noise=0.01, measurement_noise=10.0)
    
    for block in decoder.read_blocks():
        for sample in block.samples:
            if sample.type == 'gps':
                utc = sample.to_utc(session)
                
                # Apply Kalman filter
                lat_f, lon_f, v_lat, v_lon = kalman.process(
                    sample.data['latitude'],
                    sample.data['longitude'],
                    utc.timestamp()
                )
                
                print(f"Raw: ({sample.data['latitude']:.6f}, {sample.data['longitude']:.6f})")
                print(f"Filtered: ({lat_f:.6f}, {lon_f:.6f})")
                print(f"Velocity: ({v_lat:.6f}, {v_lon:.6f}) deg/s")
```

## CSV Output Formats

### GPS CSV (with Kalman)

```csv
timestamp_utc,timestamp_us,latitude,longitude,altitude,speed,heading,hdop,latitude_filtered,longitude_filtered,velocity_lat,velocity_lon
2026-01-18T15:30:45.123456+00:00,1234567890,37.7749,-122.4194,15.2,12.5,45.0,1.2,37.7749,-122.4193,0.0001,0.0002
```

### Accelerometer CSV

```csv
timestamp_utc,timestamp_us,x,y,z,unit
2026-01-18T15:30:45.123456+00:00,1234567890,0.12,-0.34,9.81,m/s²
```

### Gyroscope CSV

```csv
timestamp_utc,timestamp_us,x,y,z,unit
2026-01-18T15:30:45.123456+00:00,1234567890,0.01,-0.02,0.05,rad/s
```

### Merged CSV

All sensor types in one file with empty cells for non-applicable columns:

```csv
timestamp_utc,timestamp_us,type,gps_lat,gps_lon,gps_alt,gps_speed,gps_heading,gps_hdop,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,compass_x,compass_y,compass_z
2026-01-18T15:30:45.123456+00:00,1234567890,gps,37.7749,-122.4194,15.2,12.5,45.0,1.2,,,,,,,,
2026-01-18T15:30:45.125456+00:00,1234567892,accel,,,,,,0.12,-0.34,9.81,,,,,
2026-01-18T15:30:45.127456+00:00,1234567894,gyro,,,,,,,,,0.01,-0.02,0.05,,,
```

## Data Analysis Tips

### Importing to Pandas

```python
import pandas as pd

# GPS data
gps_df = pd.read_csv('session_12345_gps.csv')
gps_df['timestamp_utc'] = pd.to_datetime(gps_df['timestamp_utc'])
gps_df.set_index('timestamp_utc', inplace=True)

# Plot GPS track
import matplotlib.pyplot as plt
plt.plot(gps_df['longitude'], gps_df['latitude'])
plt.xlabel('Longitude')
plt.ylabel('Latitude')
plt.show()

# Compare raw vs filtered
if 'latitude_filtered' in gps_df.columns:
    plt.figure(figsize=(12, 6))
    plt.plot(gps_df.index, gps_df['latitude'], label='Raw', alpha=0.5)
    plt.plot(gps_df.index, gps_df['latitude_filtered'], label='Filtered')
    plt.legend()
    plt.ylabel('Latitude (deg)')
    plt.show()
```

### Calculating G-Forces

```python
accel_df = pd.read_csv('session_12345_accel.csv')

# Calculate total acceleration magnitude
accel_df['g_force'] = (accel_df['x']**2 + accel_df['y']**2 + accel_df['z']**2)**0.5 / 9.81

# Find max g-force
max_g = accel_df['g_force'].max()
print(f"Maximum g-force: {max_g:.2f}g")
```

### Synchronizing Data Streams

```python
# Merge GPS and accel data by timestamp
merged = pd.merge_asof(
    accel_df.sort_values('timestamp_us'),
    gps_df.sort_values('timestamp_us'),
    on='timestamp_us',
    direction='nearest',
    tolerance=10000  # 10ms tolerance
)
```

## Binary Format Documentation

For complete binary format specification, see [docs/OPENPONY_BINARY_FORMAT.md](../docs/OPENPONY_BINARY_FORMAT.md).

## Troubleshooting

**"File too small for session header"**
- File is corrupted or incomplete
- Try re-downloading from device

**"Session header CRC mismatch"**
- File corruption during download
- SD card may be failing
- Try re-downloading

**"Block CRC mismatch"**
- Partial corruption in data block
- Script will skip corrupted blocks
- Check SD card health

**"Decompression failed"**
- Compressed data is corrupted
- May indicate SD card write errors during logging

**"numpy is required for Kalman filtering"**
- Install numpy: `pip install numpy`

## License

See [LICENSE](../LICENSE) in repository root.
