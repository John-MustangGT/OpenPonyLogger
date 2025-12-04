# OpenPonyLogger - Quick Start Guide

## Getting Started in 30 Seconds

### Option 1: Instant Preview (Easiest)

1. Download all files (`index.html`, `styles.css`, `app.js`)
2. Double-click `index.html`
3. Your browser opens with a fully functional demo!

### Option 2: Local Web Server (Recommended for Development)

```bash
# Python 3
python -m http.server 8000

# Python 2
python -m SimpleHTTPServer 8000

# Node.js (with http-server)
npx http-server -p 8000
```

Then open: `http://localhost:8000`

## First Look

When you open the interface, you'll see:

1. **Header**: Connection status indicator (simulated)
2. **Navigation**: 7 tabs across the top
3. **Content**: Active tab content (starts on "About")

## Tour of Features

### 1. About Tab ✓
**What you see**: Project overview, features, hardware specs
**Try this**: Read about the $33 solution vs $400-800 commercial units

### 2. Status Tab
**What you see**: System health, OBD-II, GPS, and sensor status
**Try this**: Watch the storage bar and connection badges

### 3. Gauges Tab ⭐
**What you see**: 6 analog automotive gauges
**Try this**: 
- Watch gauges animate with realistic data
- Notice color zones (green = safe, yellow = caution, red = danger)
- Observe the smooth needle movement

### 4. G-Force Tab
**What you see**: Real-time G-force visualization
**Try this**:
- Watch the orange indicator move around the circular plot
- See peak G-forces update on the right
- Imagine this during hard braking or cornering!

### 5. GPS Tab
**What you see**: Satellite sky plot with heading indicator
**Try this**:
- Count the satellites (colored by signal strength)
- Watch the heading needle rotate
- Check the position data (set to Framingham, MA)

### 6. Sessions Tab
**What you see**: Recording controls and session history
**Try this**:
- Click "Start Recording" → button turns red and pulses
- Click "Stop Recording" → new session appears in list
- Click any action button (View/Export/Delete)

### 7. Config Tab
**What you see**: All system settings
**Try this**:
- Change the brightness slider → see value update
- Toggle checkboxes for logging options
- Click "Save Configuration" to test

## Key Features to Notice

### Dark Theme for Automotive Use
- High contrast for sunlight visibility
- No bright white that causes glare
- Orange/amber accents for easy reading

### Responsive Design
- Resize your browser window
- Works on desktop, tablet, and phone
- Navigation adapts automatically

### Professional Gauges
- Powered by canvas-gauges library
- Realistic automotive styling
- Smooth animations
- Color-coded warning zones

## What's Currently Simulated

All data is currently **mock/simulated** for prototyping:

| Feature | Simulation |
|---------|------------|
| Speed | Random 45-75 MPH |
| RPM | Random 2500-4500 |
| Temperature | Random 185-195°F |
| G-Forces | Random ±0.6g |
| GPS | Fixed position with moving satellites |
| Sessions | Pre-populated sample data |

## Next Steps for Real Hardware

To connect actual sensors:

1. **OBD-II**: Integrate ELM327 commands in `updateGauges()`
2. **GPS**: Parse NMEA sentences in `updateGPSData()`
3. **Accelerometer**: Read MPU6050 values in `updateGForceData()`
4. **WiFi**: Set up Pico 2W as access point or client

## Customization Ideas

### Easy Changes

**Change colors** (in `styles.css`):
```css
--accent-primary: #ff6b35;  /* Change to any color */
```

**Adjust gauge ranges** (in `app.js`):
```javascript
maxValue: 180,  /* Change speed limit */
```

**Add custom sessions** (in `app.js`):
```javascript
this.sessions.push({
    name: 'Your Session Name',
    // ... other fields
});
```

## Troubleshooting

### Gauges not showing?
- Check browser console for errors
- Verify canvas-gauges.js is loading from CDN
- Try refreshing the page

### Animations choppy?
- Close other browser tabs
- Check CPU usage
- Animations adapt to device performance

### Dark mode too dark/bright?
- Use the brightness slider in Config tab
- Or edit CSS: `--bg-primary: #0a0a0a;`

## Performance Tips

Current update rates:
- **Gauges**: 2 Hz (every 500ms)
- **G-Force**: 10 Hz (every 100ms)
- **GPS**: 1 Hz (every 1000ms)

On Raspberry Pi Pico 2W:
- These rates are suitable for the hardware
- Canvas rendering is optimized
- No heavy frameworks = fast load times

## Deployment on Pico 2W

```python
# Simple MicroPython web server
import socket
import network

# Create WiFi access point
ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid='OpenPonyLogger', password='pony1234')

# Serve files on port 80
# (Add your HTTP server code here)
```

Then access via: `http://192.168.4.1` (or your AP IP)

## File Sizes

- `index.html`: ~20 KB
- `styles.css`: ~14 KB  
- `app.js`: ~29 KB
- **Total**: ~63 KB (plus canvas-gauges from CDN)

Perfect for embedded systems!

## Tips for Car Installation

1. **Mount tablet/phone** running the UI
2. **Connect to Pico's WiFi** AP
3. **Position for minimal glare** (dark theme helps)
4. **Keep charged** during sessions
5. **Test in daylight** before track day!

## What Makes This Special

✅ **No login required** - instant access
✅ **No cloud dependency** - works offline  
✅ **No subscription fees** - free forever
✅ **Open source** - modify as you like
✅ **Low cost** - $33 hardware budget
✅ **Professional quality** - looks like $1000 software

## Questions?

**"Can I use this now?"**
Yes! Download and open `index.html`

**"Will it work on my phone?"**
Yes! Fully responsive design

**"Can I customize it?"**
Yes! All code is open and commented

**"When will hardware integration be ready?"**
Add sensor reading code to `app.js` functions

**"Can I contribute?"**
Yes! Fork, modify, and share improvements

## Resources

- **Canvas-Gauges Docs**: https://canvas-gauges.com/
- **ELM327 Commands**: Standard OBD-II PID reference
- **NMEA Sentences**: GPS data format documentation
- **MPU6050 Guide**: I2C accelerometer datasheet

---

**Now go ahead and open `index.html` to see it in action!** 🐎

Remember: This is currently running with simulated data. When you're ready to connect real hardware, you'll modify the update functions to read from actual sensors instead of generating random values.

**Happy logging!**
