# OpenPonyLogger Web UI - Feature Updates

## New Features Added

### 1. Additional PIDs Configuration

Monitor custom OBD-II PIDs beyond the standard parameters displayed on the gauges.

**Location**: Config Tab → Additional PIDs section

**Features**:
- Add unlimited custom PIDs in hexadecimal format (e.g., `010C` for RPM)
- Configure individual sample rates for each PID:
  - 10 Hz - Very fast (every 100ms)
  - 5 Hz - Fast (every 200ms)
  - 1 Hz - Standard (every second)
  - 0.5 Hz - Medium (every 2 seconds)
  - 0.25 Hz - Slow (every 4 seconds)
  - 0.2 Hz - Slower (every 5 seconds)
  - 0.1 Hz - Very slow (every 10 seconds)
- Remove PIDs individually
- Saved with configuration

**Use Cases**:
- Monitor aftermarket sensors (boost, oil pressure, AFR)
- Log manufacturer-specific PIDs
- Track parameters not in standard gauges
- Optimize data collection rates per parameter

**Example PIDs** (Ford Mustang GT):
- `010C` - Engine RPM
- `010D` - Vehicle Speed
- `0105` - Coolant Temperature
- `010F` - Intake Air Temperature
- `0111` - Throttle Position
- `0133` - Barometric Pressure

---

### 2. Default Startup Tab

Choose which tab displays when opening OpenPonyLogger.

**Location**: Config Tab → Display Settings → Default Startup Tab

**Options**:
- About (default)
- Status
- Gauges
- G-Force
- GPS
- Sessions
- Config

**Use Cases**:
- **Gauges**: Skip intro, go straight to instruments
- **Status**: Check system health immediately
- **Sessions**: Quick access to start recording
- **G-Force**: Track day mode - monitor G-forces immediately

**How to Use**:
1. Navigate to Config tab
2. Find "Default Startup Tab" dropdown
3. Select your preferred tab
4. Click "Save Configuration"
5. Next time you open the app, it starts on your chosen tab

---

## Best Practices

### Sample Rate Selection

**High Frequency (10 Hz)**: Engine RPM, Speed, Throttle, Boost
**Medium Frequency (1 Hz)**: Temperatures, Pressures
**Low Frequency (0.1-0.25 Hz)**: Fuel level, Status codes

### Startup Tab Selection

**Track Day**: Gauges or G-Force
**Diagnostics**: Status or Config
**Daily Driving**: Sessions
**First Time Users**: About (default)

