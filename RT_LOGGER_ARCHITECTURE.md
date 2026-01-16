# RT Logger Implementation - Architecture Overview

This document describes the real-time logger implementation for the OpenPonyLogger project.

## Architecture

The project uses a Hardware Abstraction Layer (HAL) pattern to decouple the application from specific hardware sensors. This allows sensors to be swapped without changing the core logger logic.

```
┌─────────────────────────────────────────────────┐
│           Main Application (main.cpp)           │
│  - Initialization & Task Scheduling             │
└─────────────────┬───────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────┐
│        RT Logger Thread (rt_logger_thread)      │
│  - Continuous sensor polling & data capture     │
│  - Triggers storage write callbacks             │
└─────────────────┬───────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────┐
│    Sensor Manager & HAL Abstraction Layer       │
│  - Generic interfaces: get_gps(), get_accel()   │
│  - get_gyro(), get_comp() device-agnostic       │
│  - Manages sensor polling                       │
└────┬────────────────┬────────────────┬──────────┘
     │                │                │
     │                │                │
  ┌──▼──┐        ┌────▼─────┐    ┌────▼──────┐
  │ GPS │        │    IMU    │    │  Compass  │
  │     │        │(Accel/Gyro)    │(Magnetom) │
  │Driver       Driver         Sub-driver    │
  └──────┘        └───────────┘    └─────────┘
     │
     └──────────────┬──────────────┐
                    │              │
                ┌───▼──┐      ┌────▼──┐
                │ PA1010D   │   ICM20948
                │ GPS Module│  9-DOF IMU
                └────────┘   └────────┘
```

## Components

### 1. **HAL Layer** (`components/hal/`)
- **sensor_hal.h/cpp**: Core abstraction interfaces
  - `IGPSSensor`: GPS interface
  - `IAccelSensor`: Accelerometer interface
  - `IGyroSensor`: Gyroscope interface
  - `ICompassSensor`: Compass/Magnetometer interface
  - `SensorManager`: Central manager that coordinates all sensors

**Key Feature**: Devices are accessed generically via:
```cpp
gps_data_t gps = sensor_manager.get_gps();
accel_data_t accel = sensor_manager.get_accel();
gyro_data_t gyro = sensor_manager.get_gyro();
compass_data_t comp = sensor_manager.get_comp();
```

This means you can swap PA1010D for any other GPS module by simply implementing the `IGPSSensor` interface.

### 2. **Device Drivers** (`components/drivers/`)

#### PA1010D GPS Driver (`pa1010d_driver.h/cpp`)
- Implements `IGPSSensor` interface
- Handles UART serial communication at 9600 baud
- Parses NMEA sentences (GPRMC, GPGGA)
- Provides latitude, longitude, altitude, speed, and satellite count
- Handles date/time extraction

#### ICM20948 IMU Driver (`icm20948_driver.h/cpp`)
- Implements `IAccelSensor`, `IGyroSensor`, and `ICompassSensor` interfaces
- Communicates via I2C (400kHz clock)
- 3-axis accelerometer: ±16g range
- 3-axis gyroscope: ±2000dps range
- 3-axis magnetometer (compass) data
- Converts raw sensor values to physical units (g, dps, uT)

### 3. **Logger Thread** (`components/logger/rt_logger_thread.h/cpp`)
- FreeRTOS-based real-time task
- Default 100ms update frequency (configurable)
- Continuously polls all sensors via HAL
- Maintains latest sensor snapshots
- Tracks total sample count
- Supports storage write callbacks

**Thread Safety**: Uses FreeRTOS task mechanism for safe concurrent operation.

### 4. **Storage Reporter** (`components/logger/storage_reporter.h/cpp`)
- Formats and reports storage write events to serial port
- Displays latest values for all sensors:
  - GPS: position, altitude, speed, satellite count, timestamp
  - Accel: X, Y, Z acceleration + magnitude
  - Gyro: X, Y, Z angular velocity + magnitude
  - Compass: X, Y, Z magnetic field + magnitude
- Pretty-printed output for easy monitoring

### 5. **Main Application** (`src/main.cpp`)
- Initializes all hardware and drivers
- Creates sensor instances (PA1010D, ICM20948)
- Initializes SensorManager with drivers
- Starts RT logger thread
- Registers storage write callback
- Main loop triggers storage writes every 5 seconds

## Data Flow

1. **Initialization**:
   ```
   main.cpp → init_sensors()
   ├── Create PA1010DDriver (implements IGPSSensor)
   ├── Create ICM20948Driver (implements IAccel/IGyro/ICompassSensor)
   └── SensorManager::init() registers all drivers
   ```

2. **Real-Time Operation**:
   ```
   RTLoggerThread (FreeRTOS Task)
   ├── SensorManager::update_all()
   │   ├── gps_driver->update()       [reads PA1010D via UART]
   │   ├── imu_driver->update()       [reads ICM20948 via I2C]
   │   └── Returns success/failure
   └── Capture latest data snapshots
   ```

3. **Storage Write Event**:
   ```
   RTLoggerThread::trigger_storage_write()
   └── StorageReporter::report_storage_write()
       └── Serial output with formatted sensor data
   ```

## Extending the System

### Adding a New GPS Module
1. Create new class that implements `IGPSSensor`
2. Implement: `init()`, `update()`, `get_data()`, `is_valid()`
3. Update `main.cpp` to instantiate new driver
4. Pass to `SensorManager::init()`

### Adding a New IMU
1. Create new class that implements `IAccelSensor`, `IGyroSensor`, `ICompassSensor`
2. Implement the required virtual functions
3. Update `main.cpp` to instantiate and register

### Modifying Sensor Update Rate
```cpp
// Currently 100ms in main.cpp
RTLoggerThread rt_logger(&sensor_manager, 50);  // 50ms = 20Hz
```

### Modifying Storage Write Frequency
```cpp
// Currently every 5 seconds in loop()
if (now - last_write_time >= 10000) {  // 10 seconds
    rt_logger->trigger_storage_write();
}
```

## Serial Output Example

```
=== STORAGE WRITE EVENT ===
Timestamp: 2026-01-16 14:32:45

--- GPS Data ---
  Latitude:  37.422047
  Longitude: -122.084052
  Altitude:  25.40 m
  Speed:     2.50 kts
  Satellites: 12
  Status: VALID

--- Accelerometer Data ---
  X: 0.1234 g
  Y: -0.0456 g
  Z: 0.9876 g
  Magnitude: 1.0023 g

--- Gyroscope Data ---
  X: -0.54 dps
  Y: 1.23 dps
  Z: 0.12 dps
  Magnitude: 1.35 dps

--- Compass Data ---
  X: 23.45 uT
  Y: -12.34 uT
  Z: 45.67 uT
  Magnitude: 52.14 uT
===========================
```

## Building & Testing

### PlatformIO Build
```bash
platformio run -e esp32dev
```

### Monitor Serial Output
```bash
platformio device monitor
```

### Configuration (platformio.ini)
- Target: ESP32-DevKit
- Framework: Arduino
- Monitor Speed: 115200 baud
- C++ Standard: C++17

## Hardware Requirements
- **MCU**: ESP32 (or compatible)
- **GPS**: PA1010D GNSS module on Serial1 (TX=GPIO17, RX=GPIO16, 9600 baud)
- **IMU**: ICM20948 on I2C (SDA=GPIO21, SCL=GPIO22, Address=0x68)

## Future Enhancements
- SD card storage with circular buffer
- Configurable sensor ranges
- Low-power modes
- Kalman filtering for sensor fusion
- GNSS/INS integration
- Temperature compensation for sensors
