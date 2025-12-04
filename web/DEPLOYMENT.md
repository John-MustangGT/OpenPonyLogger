# OpenPonyLogger - Raspberry Pi Pico 2W Deployment Guide

## Hardware Requirements

### Required Components
- **Raspberry Pi Pico 2W** ($6) - Main controller with WiFi
- **GPS Module** ($12) - NEO-6M or similar UART GPS
- **MPU6050** ($2) - 3-axis accelerometer/gyroscope
- **ELM327 Bluetooth OBD-II** ($8) - Vehicle interface
- **MicroSD Card** ($5) - Data storage (optional)
- **Total**: ~$33

### Optional Components
- USB power bank for portable operation
- 3D printed enclosure
- Mounting hardware
- LED status indicators

## Software Setup

### 1. Install MicroPython on Pico 2W

```bash
# Download MicroPython firmware for Pico W
# Flash using Thonny or picotool
```

1. Hold BOOTSEL button while connecting USB
2. Copy `firmware.uf2` to mounted drive
3. Pico reboots with MicroPython

### 2. Upload Web Files

Copy these files to Pico's filesystem:
```
/
├── main.py           # Main program (create this)
├── web/
│   ├── index.html    # UI (provided)
│   ├── styles.css    # Styles (provided)
│   └── app.js        # Logic (provided)
├── lib/
│   ├── gps.py        # GPS parser
│   ├── mpu6050.py    # Accelerometer driver
│   └── obd.py        # OBD-II interface
```

### 3. Create Main Program

**main.py** - Basic structure:

```python
import network
import socket
import machine
import time
from lib.gps import GPS
from lib.mpu6050 import MPU6050
from lib.obd import OBD2

# Configuration
WIFI_SSID = "OpenPonyLogger"
WIFI_PASSWORD = "pony1234"
WEB_PORT = 80

class OpenPonyLogger:
    def __init__(self):
        self.setup_wifi()
        self.setup_sensors()
        self.setup_webserver()
        
    def setup_wifi(self):
        """Create WiFi Access Point"""
        self.ap = network.WLAN(network.AP_IF)
        self.ap.active(True)
        self.ap.config(essid=WIFI_SSID, password=WIFI_PASSWORD)
        
        while not self.ap.active():
            time.sleep(0.1)
            
        print(f"WiFi AP Started: {WIFI_SSID}")
        print(f"IP Address: {self.ap.ifconfig()[0]}")
        
    def setup_sensors(self):
        """Initialize GPS, Accelerometer, OBD-II"""
        # GPS on UART0
        self.gps = GPS(uart_id=0, tx_pin=0, rx_pin=1)
        
        # MPU6050 on I2C0
        i2c = machine.I2C(0, scl=machine.Pin(9), sda=machine.Pin(8))
        self.mpu = MPU6050(i2c)
        
        # OBD-II via Bluetooth UART
        self.obd = OBD2(uart_id=1, tx_pin=4, rx_pin=5)
        
    def setup_webserver(self):
        """Start HTTP server"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('', WEB_PORT))
        self.sock.listen(5)
        print(f"Web server listening on port {WEB_PORT}")
        
    def handle_request(self, conn):
        """Handle HTTP requests"""
        request = conn.recv(1024).decode()
        
        # Parse request
        if 'GET / ' in request or 'GET /index.html' in request:
            self.serve_file(conn, '/web/index.html', 'text/html')
        elif 'GET /styles.css' in request:
            self.serve_file(conn, '/web/styles.css', 'text/css')
        elif 'GET /app.js' in request:
            self.serve_file(conn, '/web/app.js', 'application/javascript')
        elif 'GET /api/data' in request:
            self.serve_api_data(conn)
        else:
            self.serve_404(conn)
            
    def serve_file(self, conn, filepath, content_type):
        """Serve static files"""
        try:
            with open(filepath, 'r') as f:
                content = f.read()
            
            response = f"""HTTP/1.1 200 OK
Content-Type: {content_type}
Content-Length: {len(content)}
Connection: close

{content}"""
            conn.send(response.encode())
        except:
            self.serve_404(conn)
            
    def serve_api_data(self, conn):
        """Serve sensor data as JSON"""
        # Read sensors
        gps_data = self.gps.get_data()
        accel_data = self.mpu.get_acceleration()
        obd_data = self.obd.get_data()
        
        # Build JSON response
        data = {
            'timestamp': time.time(),
            'gps': gps_data,
            'accelerometer': accel_data,
            'obd': obd_data
        }
        
        import json
        json_data = json.dumps(data)
        
        response = f"""HTTP/1.1 200 OK
Content-Type: application/json
Access-Control-Allow-Origin: *
Content-Length: {len(json_data)}
Connection: close

{json_data}"""
        conn.send(response.encode())
        
    def serve_404(self, conn):
        """Serve 404 error"""
        response = """HTTP/1.1 404 Not Found
Content-Type: text/plain
Content-Length: 13
Connection: close

404 Not Found"""
        conn.send(response.encode())
        
    def run(self):
        """Main loop"""
        print("OpenPonyLogger ready!")
        print(f"Connect to WiFi: {WIFI_SSID}")
        print(f"Open browser to: http://{self.ap.ifconfig()[0]}")
        
        while True:
            try:
                conn, addr = self.sock.accept()
                print(f"Connection from {addr}")
                self.handle_request(conn)
                conn.close()
            except Exception as e:
                print(f"Error: {e}")

# Start the logger
if __name__ == "__main__":
    logger = OpenPonyLogger()
    logger.run()
```

## Sensor Integration

### GPS Module (NEO-6M)

**lib/gps.py**:
```python
import machine
import time

class GPS:
    def __init__(self, uart_id=0, tx_pin=0, rx_pin=1, baudrate=9600):
        self.uart = machine.UART(uart_id, baudrate=baudrate,
                                 tx=machine.Pin(tx_pin),
                                 rx=machine.Pin(rx_pin))
        self.data = {}
        
    def parse_nmea(self, sentence):
        """Parse NMEA sentences"""
        # Implement NMEA parsing (GPGGA, GPRMC, etc.)
        pass
        
    def get_data(self):
        """Return current GPS data"""
        if self.uart.any():
            line = self.uart.readline()
            self.parse_nmea(line)
        return self.data
```

### Accelerometer (MPU6050)

**lib/mpu6050.py**:
```python
import time

class MPU6050:
    def __init__(self, i2c, addr=0x68):
        self.i2c = i2c
        self.addr = addr
        self.init_sensor()
        
    def init_sensor(self):
        """Initialize MPU6050"""
        # Wake up sensor
        self.i2c.writeto_mem(self.addr, 0x6B, b'\x00')
        
    def get_acceleration(self):
        """Read acceleration data"""
        # Read 6 bytes from registers 0x3B-0x40
        data = self.i2c.readfrom_mem(self.addr, 0x3B, 6)
        
        # Convert to g-forces
        ax = self.bytes_to_int(data[0:2]) / 16384.0
        ay = self.bytes_to_int(data[2:4]) / 16384.0
        az = self.bytes_to_int(data[4:6]) / 16384.0
        
        return {'x': ax, 'y': ay, 'z': az}
        
    def bytes_to_int(self, data):
        """Convert bytes to signed int"""
        value = data[0] << 8 | data[1]
        if value >= 0x8000:
            return -((65535 - value) + 1)
        return value
```

### OBD-II Interface

**lib/obd.py**:
```python
import machine
import time

class OBD2:
    def __init__(self, uart_id=1, tx_pin=4, rx_pin=5, baudrate=38400):
        self.uart = machine.UART(uart_id, baudrate=baudrate,
                                 tx=machine.Pin(tx_pin),
                                 rx=machine.Pin(rx_pin))
        self.init_connection()
        
    def init_connection(self):
        """Initialize ELM327"""
        self.send_command('ATZ')  # Reset
        time.sleep(1)
        self.send_command('ATE0') # Echo off
        self.send_command('ATL0') # Linefeeds off
        
    def send_command(self, cmd):
        """Send OBD-II command"""
        self.uart.write(cmd + '\r')
        time.sleep(0.1)
        return self.uart.read()
        
    def get_data(self):
        """Get common OBD-II parameters"""
        data = {}
        
        # Speed (PID 0D)
        speed_raw = self.send_command('010D')
        data['speed'] = self.parse_speed(speed_raw)
        
        # RPM (PID 0C)
        rpm_raw = self.send_command('010C')
        data['rpm'] = self.parse_rpm(rpm_raw)
        
        # Coolant temp (PID 05)
        temp_raw = self.send_command('0105')
        data['coolant_temp'] = self.parse_temp(temp_raw)
        
        return data
        
    def parse_speed(self, raw):
        """Parse speed from response"""
        # Implementation depends on ELM327 response format
        pass
```

## Updating Web UI to Use Real Data

Modify **app.js** to fetch from API instead of generating mock data:

```javascript
// Replace mock data functions with API calls
async fetchSensorData() {
    try {
        const response = await fetch('/api/data');
        const data = await response.json();
        
        // Update gauges with real data
        this.gauges.speed.value = data.obd.speed;
        this.gauges.rpm.value = data.obd.rpm / 1000;
        this.gauges.temp.value = data.obd.coolant_temp;
        
        // Update G-force
        this.gforceData.lat = data.accelerometer.x;
        this.gforceData.long = data.accelerometer.y;
        this.gforceData.vert = data.accelerometer.z;
        
        // Update GPS
        this.gpsData.lat = data.gps.latitude;
        this.gpsData.lon = data.gps.longitude;
        this.gpsData.speed = data.gps.speed;
        
    } catch (error) {
        console.error('Failed to fetch sensor data:', error);
    }
}

// Call this instead of mock data generators
setInterval(() => this.fetchSensorData(), 500);
```

## Wiring Diagram

```
Raspberry Pi Pico 2W
┌─────────────────────┐
│                     │
│  GP0 (UART0 TX) ────┼─→ GPS RX
│  GP1 (UART0 RX) ←───┼── GPS TX
│                     │
│  GP8 (I2C0 SDA) ←───┼─→ MPU6050 SDA
│  GP9 (I2C0 SCL) ────┼─→ MPU6050 SCL
│                     │
│  GP4 (UART1 TX) ────┼─→ ELM327 RX
│  GP5 (UART1 RX) ←───┼── ELM327 TX
│                     │
│  3V3 ───────────────┼─→ Sensor Power
│  GND ───────────────┼─→ Common Ground
│                     │
└─────────────────────┘

Power: USB-C or 5V via VSYS
```

## Installation Steps

1. **Flash MicroPython** to Pico 2W
2. **Copy files** using Thonny or ampy:
   ```bash
   ampy -p /dev/ttyACM0 put main.py
   ampy -p /dev/ttyACM0 put web/ /web
   ampy -p /dev/ttyACM0 put lib/ /lib
   ```
3. **Connect sensors** according to wiring diagram
4. **Power on** Pico 2W
5. **Connect to WiFi**: Network "OpenPonyLogger"
6. **Open browser**: http://192.168.4.1

## Testing

### 1. Test WiFi AP
```python
import network
ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid='TestAP')
print(ap.ifconfig())
```

### 2. Test GPS
```python
from lib.gps import GPS
gps = GPS()
while True:
    print(gps.get_data())
    time.sleep(1)
```

### 3. Test Accelerometer
```python
from lib.mpu6050 import MPU6050
import machine

i2c = machine.I2C(0, scl=machine.Pin(9), sda=machine.Pin(8))
mpu = MPU6050(i2c)
print(mpu.get_acceleration())
```

### 4. Test OBD-II
```python
from lib.obd import OBD2
obd = OBD2()
print(obd.get_data())
```

## Optimization Tips

### Memory Management
```python
# Use gc to free memory
import gc
gc.collect()
```

### Reduce Sampling Rates
```python
# GPS: 1 Hz is sufficient
# Accelerometer: 10-50 Hz for G-forces
# OBD-II: 2-10 Hz depending on parameter
```

### Data Buffering
```python
# Buffer data before writing to SD card
buffer = []
if len(buffer) > 100:
    write_to_sd(buffer)
    buffer.clear()
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| WiFi not starting | Check antenna, increase timeout |
| GPS no fix | Ensure clear sky view, wait 1-2 minutes |
| OBD-II no response | Check baud rate (38400 or 115200) |
| Accelerometer not found | Verify I2C address (0x68 or 0x69) |
| Web page not loading | Check file paths, enable debug output |

## Performance Expectations

- **Boot time**: 3-5 seconds
- **WiFi connection**: 1-2 seconds  
- **Web page load**: <1 second
- **Data update rate**: 2-10 Hz
- **Memory usage**: ~50-100 KB
- **Power consumption**: ~200-300 mA @ 5V

## Next Steps

1. Test each component individually
2. Integrate components step by step
3. Verify data accuracy with known values
4. Calibrate accelerometer on level surface
5. Test in vehicle with engine running
6. Record test session and verify data
7. Fine-tune sampling rates and thresholds

## Resources

- **MicroPython Docs**: https://docs.micropython.org/
- **Pico W Documentation**: https://www.raspberrypi.com/documentation/
- **ELM327 Commands**: https://www.elmelectronics.com/
- **NMEA Sentences**: http://www.gpsinformation.org/dale/nmea.htm

---

**Ready to build your OpenPonyLogger!** 🐎
