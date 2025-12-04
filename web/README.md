# OpenPonyLogger Web UI

A modern, responsive web interface for the OpenPonyLogger automotive telemetry system, designed specifically for the 2014 Ford Mustang GT "Ciara".

## Overview

OpenPonyLogger is an open-source alternative to expensive commercial data loggers ($400-800), built around a Raspberry Pi Pico 2W with a target cost of just $33. This web UI provides real-time monitoring and data visualization for:

- OBD-II engine data
- GPS tracking and satellite visualization
- 3-axis accelerometer G-force measurements
- Session recording and playback
- System configuration

## Features

### 7 Main Tabs

1. **About** - Project information and hardware specs
2. **Status** - System health and connection monitoring
3. **Gauges** - Real-time automotive instruments (Speed, RPM, Temp, Oil Pressure, Boost, Throttle)
4. **G-Force** - Live 3-axis accelerometer visualization with peak tracking
5. **GPS** - Satellite sky plot, position, and speed data
6. **Sessions** - Recording management and session history
7. **Config** - System configuration and settings

### Design Features

- **Dark Theme** - Optimized for sunlight visibility in automotive environments
- **Responsive Layout** - Works on desktop, tablet, and mobile devices
- **Pure HTML/CSS/JavaScript** - No build process required
- **Canvas-Gauges** - Professional-looking analog instrument displays
- **Minimal Dependencies** - Runs efficiently on Raspberry Pi Pico 2W
- **CSS Grid Layout** - Modern, flexible responsive design

## Technology Stack

- **HTML5** - Semantic markup
- **CSS3** - CSS Grid, custom properties, animations
- **JavaScript** - ES6+ modern JavaScript
- **Canvas-Gauges.js** - High-quality gauge rendering
- **Canvas API** - Custom visualizations for G-force and GPS displays

## File Structure

```
openponylogger-web/
├── index.html          # Main HTML structure with all 7 tabs
├── styles.css          # Dark theme CSS with responsive design
├── app.js              # Application logic and data simulation
└── README.md           # This file
```

## Installation

### For Development/Testing

1. Clone or download the files to a directory
2. Open `index.html` in a modern web browser
3. All features work with mock data for prototyping

### For Raspberry Pi Pico 2W Deployment

1. Set up a simple HTTP server on the Pico 2W:
   ```python
   import socket
   import network
   
   # Serve the HTML/CSS/JS files
   # Connect to WiFi and serve on port 80
   ```

2. Copy `index.html`, `styles.css`, and `app.js` to the Pico's filesystem

3. Access via `http://[pico-ip-address]` from any device on the network

## Usage

### Navigation

- Click any tab button to switch between views
- All tabs feature smooth animations and transitions
- Tab state is preserved during navigation

### Gauges Tab

- Displays 6 analog gauges with realistic automotive styling
- Real-time updates every 500ms
- Color-coded zones (green/yellow/red) for at-a-glance status
- Gauges include:
  - Speed (0-180 MPH)
  - RPM (0-8000)
  - Coolant Temperature (100-250°F)
  - Oil Pressure (0-100 PSI)
  - Boost/Vacuum (-5 to +20 PSI)
  - Throttle Position (0-100%)

### G-Force Tab

- Real-time 2D visualization of lateral and longitudinal forces
- Center crosshair with 0.25g increment rings
- Live position indicator with trail effect
- Peak G-force tracking for:
  - Maximum acceleration
  - Maximum braking
  - Maximum cornering

### GPS Tab

- Satellite sky plot showing constellation positions
- Color-coded satellites by signal strength (SNR)
  - Green: >35 dB (excellent)
  - Yellow: 25-35 dB (good)
  - Red: <25 dB (marginal)
- Heading indicator
- Position, altitude, and speed display
- Satellite list with PRN and signal strength

### Sessions Tab

- Start/stop recording with visual feedback
- Session history with key statistics:
  - Date and time
  - Duration
  - Distance traveled
  - Maximum speed
  - Maximum G-force
  - Average speed
- Export individual sessions or all data
- Delete unwanted sessions

### Config Tab

- **General Settings**: Device name, timezone, units
- **Data Logging**: Sample rate, auto-record options
- **WiFi Configuration**: AP or Client mode setup
- **OBD-II Settings**: Protocol selection, timeout
- **Display Settings**: Dark mode, brightness control
- Save, reset, or factory reset options

## Mock Data

The current implementation includes realistic mock data generators for prototyping:

- **Gauges**: Simulated engine parameters with realistic ranges
- **G-Force**: Random but realistic lateral/longitudinal forces
- **GPS**: Fixed position (Framingham, MA) with moving satellites
- **Sessions**: Pre-populated sample sessions

To integrate real hardware data, modify the update functions in `app.js` to fetch from actual sensors and OBD-II interface.

## Color Scheme

The dark theme uses high-contrast colors optimized for daylight visibility:

- **Primary Background**: #0a0a0a (near black)
- **Secondary Background**: #1a1a1a (dark gray)
- **Card Background**: #252525 (medium gray)
- **Primary Accent**: #ff6b35 (orange-red)
- **Secondary Accent**: #f7931e (amber)
- **Success**: #4caf50 (green)
- **Warning**: #ffc107 (yellow)
- **Danger**: #f44336 (red)
- **Info**: #2196f3 (blue)

## Browser Compatibility

Tested and working on:
- Chrome/Edge (v90+)
- Firefox (v88+)
- Safari (v14+)
- Mobile browsers (iOS Safari, Chrome Mobile)

## Performance Considerations

- Gauges update at 2 Hz (500ms interval)
- G-force data updates at 10 Hz (100ms interval)
- GPS data updates at 1 Hz (1000ms interval)
- Canvas rendering uses requestAnimationFrame for smooth animations
- Minimal CPU usage suitable for embedded systems

## Future Enhancements

- [ ] WebSocket support for real-time data streaming
- [ ] Session playback with time slider
- [ ] Track map overlay on GPS display
- [ ] Lap timing functionality
- [ ] Data export to multiple formats (CSV, GPX, JSON)
- [ ] Historical data graphing
- [ ] Integration with actual hardware sensors
- [ ] OBD-II protocol auto-detection
- [ ] Custom dashboard layouts
- [ ] Voice announcements for key metrics

## Hardware Integration

To connect to actual hardware:

1. **OBD-II Interface**: Replace mock data in `updateGauges()` with ELM327 commands
2. **GPS Module**: Parse NMEA sentences and update `gpsData` object
3. **Accelerometer**: Read MPU6050 via I2C and update `gforceData` object
4. **System Status**: Query Pico's CPU temp, memory, etc.

## License

Open Source - Free to use, modify, and distribute

## Credits

- **Developer**: John Orthoefer
- **Vehicle**: 2014 Ford Mustang GT "Ciara"
- **Gauge Library**: Canvas-Gauges by Mikhus
- **Inspired by**: Need for affordable automotive telemetry

## Support

For questions, suggestions, or contributions, please refer to the project repository.

---

**OpenPonyLogger** - Professional telemetry on a budget 🐎
