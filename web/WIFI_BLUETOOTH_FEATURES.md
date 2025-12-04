# WiFi & Bluetooth Configuration Features

## ✅ Both Features Are Complete!

### 1. 📶 WiFi Client Mode with DHCP/Static IP

**Location:** Config Tab → WiFi Configuration

**Features Implemented:**
- **Mode Selection**: Switch between Access Point and Client
- **DHCP Checkbox**: "Use DHCP (Automatic IP)" - checked by default
- **Static IP Configuration** (appears when DHCP unchecked):
  - IP Address (e.g., 192.168.1.100)
  - Subnet Mask / CIDR (e.g., 24 or 255.255.255.0)
  - Gateway (e.g., 192.168.1.1)
  - DNS Server (e.g., 8.8.8.8)

**How It Works:**
1. Select "Client" mode from dropdown
2. Enter SSID and password for network to join
3. Choose DHCP or Static IP:
   - **DHCP Enabled**: Automatic IP assignment (default)
   - **DHCP Disabled**: Shows static IP fields
4. Enter static network details if needed
5. Save configuration

**Use Cases:**
- **Access Point Mode**: Track day (no internet, Pico as WiFi AP)
- **Client Mode + DHCP**: Home garage (join home WiFi automatically)
- **Client Mode + Static IP**: Fixed IP for network monitoring/scripting

**Configuration Storage:**
```javascript
{
  wifiMode: "client",
  wifiSSID: "HomeNetwork",
  wifiPassword: "password123",
  useDHCP: false,
  staticIP: "192.168.1.100",
  subnetMask: "24",
  gateway: "192.168.1.1",
  dnsServer: "8.8.8.8"
}
```

---

### 2. 🔵 Bluetooth Device Scanning & Pairing

**Location:** Config Tab → Bluetooth Configuration

**Features Implemented:**
- **Scan Button**: "Scan for Devices" - discovers nearby Bluetooth devices
- **Device Dropdown**: Shows discovered devices with signal strength
- **Signal Strength**: RSSI in dBm (e.g., -45 dBm = strong, -80 dBm = weak)
- **Pair Button**: Pair with selected device
- **Unpair Button**: Disconnect and remove pairing
- **Paired Device Status**: Shows current connection

**How It Works:**
1. Click "Scan for Devices"
2. Wait 2 seconds while scanning
3. Device dropdown populates with nearby devices:
   - Sorted by signal strength (strongest first)
   - Shows device name + RSSI
4. Select device from dropdown
5. Click "Pair Device"
6. Wait 2 seconds for pairing
7. Paired device status appears showing:
   - Device name
   - Connection status
   - Signal strength

**Device List Features:**
- Auto-sorted by signal strength
- Shows RSSI values (dBm)
- Refresh button to re-scan
- Dropdown selection (user-friendly)

**Use Cases:**
- Pair with phone for data transfer
- Connect to Bluetooth OBD-II adapter
- Sync with tablets/laptops
- Connect to Bluetooth GPS (alternative to UART)

**Mock Implementation:**
Current version simulates scanning with these mock devices:
```javascript
{ id: 'bt:01', name: 'iPhone 13 Pro', rssi: -45 },
{ id: 'bt:02', name: 'Galaxy Buds', rssi: -65 },
{ id: 'bt:03', name: 'iPad Air', rssi: -70 },
{ id: 'bt:04', name: 'MacBook Pro', rssi: -55 },
{ id: 'bt:05', name: 'Pixel 7', rssi: -80 }
```

**Production Implementation:**
Replace `scanBluetoothDevices()` method with:
- MicroPython Bluetooth scan
- Real device discovery
- Actual RSSI readings
- MAC addresses

---

## UI/UX Details

### WiFi Client Settings
**Visibility Logic:**
- Settings hidden when "Access Point" selected
- Settings shown when "Client" selected
- Static IP fields hidden when DHCP checked
- Static IP fields shown when DHCP unchecked

**Visual Feedback:**
- Border separator between sections
- Disabled fields when not applicable
- Clear labels and placeholders

### Bluetooth Scanning
**Visual Feedback:**
- "Scanning..." button text during scan
- Disabled button during scan
- Progress indication (2 second delay)
- Success confirmation on pairing
- Clear connection status

**Button States:**
- Scan button: Enabled → Disabled during scan → Enabled
- Pair button: Disabled → Enabled when device selected
- Unpair button: Hidden → Shown when paired

---

## Configuration Persistence

Both features save settings to localStorage:

**WiFi Config Key:** `openPonyLoggerConfig`
**Bluetooth Config Key:** `pairedBluetoothDevice`

Survives:
- ✓ Page refresh
- ✓ Browser restart
- ✓ Power cycle (if on Pico)

Cleared by:
- Factory Reset button
- Browser cache clear
- Manual localStorage clear

---

## Testing Instructions

### Test WiFi Configuration

1. Open OpenPonyLogger
2. Go to Config tab
3. Change WiFi mode to "Client"
4. Verify network settings appear
5. Uncheck "Use DHCP"
6. Verify static IP fields appear
7. Enter test values:
   - IP: 192.168.1.100
   - CIDR: 24
   - Gateway: 192.168.1.1
   - DNS: 8.8.8.8
8. Check "Use DHCP" again
9. Verify static fields hide
10. Save configuration
11. Refresh page
12. Verify settings persist

### Test Bluetooth Scanning

1. Open OpenPonyLogger
2. Go to Config tab
3. Scroll to Bluetooth Configuration
4. Click "Scan for Devices"
5. Wait 2 seconds
6. Verify device dropdown appears with 5 devices
7. Verify devices sorted by signal strength
8. Select "iPhone 13 Pro" (strongest signal)
9. Click "Pair Device"
10. Wait 2 seconds
11. Verify paired status appears
12. Verify connection shows "Connected"
13. Click "Unpair"
14. Verify confirmation dialog
15. Confirm unpair
16. Verify status disappears

---

## Implementation Files

All code is in these files:

**HTML:** `index.html`
- WiFi configuration form (lines 500-548)
- Bluetooth configuration section (lines 550-590)

**JavaScript:** `app.js`
- WiFi mode toggle: `toggleClientNetworkSettings()` (line 1388)
- DHCP toggle: `toggleStaticIPSettings()` (line 1393)
- Bluetooth scan: `scanBluetoothDevices()` (line 1399)
- Bluetooth pair: `pairBluetoothDevice()` (line 1444)
- Bluetooth unpair: `unpairBluetoothDevice()` (line 1487)

**CSS:** `styles.css`
- Status badge styles
- Form layout
- Responsive design

---

## Network Configuration Examples

### Example 1: Home WiFi with DHCP
```
Mode: Client
SSID: HomeNetwork
Password: MyHomePassword123
DHCP: ✓ Enabled
```
Result: Pico joins home WiFi, gets automatic IP

### Example 2: Fixed IP on Work Network
```
Mode: Client
SSID: CorpNetwork
Password: CompanyPassword456
DHCP: ✗ Disabled
IP Address: 10.0.50.100
Subnet: 24
Gateway: 10.0.50.1
DNS: 10.0.50.1
```
Result: Pico joins work WiFi with static IP

### Example 3: Track Day (No WiFi)
```
Mode: Access Point
SSID: OpenPonyLogger
Password: trackday2024
```
Result: Pico creates its own WiFi AP for tablets to connect

---

## Bluetooth Pairing Scenarios

### Scenario 1: Pair with Phone
1. Scan for devices
2. Select "iPhone 13 Pro (-45 dBm)"
3. Pair
4. Use phone for data export/viewing

### Scenario 2: Bluetooth OBD-II
1. Scan for devices
2. Select "ELM327 Bluetooth (-50 dBm)"
3. Pair
4. Use Bluetooth OBD instead of wired

### Scenario 3: Tablet Sync
1. Scan for devices
2. Select "iPad Air (-70 dBm)"
3. Pair
4. Real-time data streaming to tablet

---

## Known Limitations

### WiFi
- Static IP validation not implemented (accepts any input)
- No IP conflict detection
- No connection testing
- Configuration requires manual Pico restart

### Bluetooth
- Mock implementation (not real Bluetooth scan)
- No PIN pairing support
- No encryption configuration
- Single device pairing only
- No device profiles/services detection

---

## Future Enhancements

### WiFi
- [ ] IP address validation
- [ ] Test connection button
- [ ] WiFi signal strength indicator
- [ ] Network scanning (show available SSIDs)
- [ ] Multiple network profiles
- [ ] Auto-connect priority

### Bluetooth
- [ ] Real Bluetooth API integration
- [ ] PIN pairing support
- [ ] Multiple device pairing
- [ ] Device profiles (A2DP, HFP, SPP, etc.)
- [ ] Bluetooth Low Energy (BLE) support
- [ ] File transfer via Bluetooth
- [ ] Auto-reconnect on boot

---

## Production Deployment

### For Pico 2W

**WiFi Implementation:**
Replace config save with MicroPython:
```python
import network

# Client mode with DHCP
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect(ssid, password)

# Client mode with static IP
wlan.ifconfig((ip, subnet, gateway, dns))
```

**Bluetooth Implementation:**
Replace scan with MicroPython:
```python
from bluetooth import BLE

ble = BLE()
ble.active(True)
devices = ble.gap_scan(2000)  # 2 second scan
```

---

## Summary

✅ **WiFi Client Mode**: Complete with DHCP checkbox and static IP configuration
✅ **Bluetooth Scanning**: Complete with scan button, device dropdown, pairing

Both features are:
- Fully implemented in HTML/CSS/JavaScript
- Tested and working in mock mode
- Ready for MicroPython integration
- Documented with examples

**No restart needed - everything is already there!** 🎉📶🔵
