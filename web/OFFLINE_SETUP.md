# OpenPonyLogger - Offline/Disconnected Setup Guide

## Overview

OpenPonyLogger is designed to work **completely offline** in disconnected environments (like on-track at a race course). This guide shows you how to set up a fully self-contained installation.

---

## Why Offline Support Matters

**Track Day Reality:**
- No internet connection at most tracks
- Need to work in WiFi-only mode (Pico 2W Access Point)
- Critical that gauges/UI work without external dependencies
- Must be reliable when you need it most

**Solution:**
OpenPonyLogger automatically tries local files first, falls back to CDN only if available.

---

## File Structure for Offline Operation

```
openponylogger/
├── index.html              ← Main application
├── styles.css              ← All styling (no external fonts)
├── app.js                  ← Application logic
└── gauge.min.js            ← Canvas-gauges library (local copy)
```

**Total Size:** ~561 KB (all files)
**Network Required:** None!

---

## Getting Canvas-Gauges Library Locally

The only external dependency is the canvas-gauges library. Here's how to get it:

### Option 1: Download from Official Source (Recommended)

**Direct Download Link:**
https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js

**Steps:**
1. Visit the link above
2. Right-click → Save As → `gauge.min.js`
3. Place file in same directory as `index.html`
4. Done!

### Option 2: Download from CDN

```bash
# Using curl
curl -o gauge.min.js https://cdn.jsdelivr.net/npm/canvas-gauges@2.1.7/gauge.min.js

# Using wget
wget -O gauge.min.js https://cdn.jsdelivr.net/npm/canvas-gauges@2.1.7/gauge.min.js
```

### Option 3: From NPM (if you have Node.js)

```bash
npm install canvas-gauges
cp node_modules/canvas-gauges/gauge.min.js ./
```

### Option 4: Build from Source

```bash
git clone https://github.com/Mikhus/canvas-gauges.git
cd canvas-gauges
npm install
npm run build
cp gauge.min.js /path/to/openponylogger/
```

---

## Verifying Offline Operation

### Test 1: Local File System
```
1. Open index.html from file:// in browser
2. Check browser console (F12)
3. Should see: "Loading local gauge library"
4. No network errors should appear
```

### Test 2: Airplane Mode Test
```
1. Enable Airplane Mode on your device
2. Open index.html
3. All gauges should load and animate
4. All 7 tabs should be functional
5. Configuration should save/load
```

### Test 3: Pico 2W Access Point
```
1. Connect to Pico WiFi AP (no internet)
2. Browse to http://192.168.4.1
3. Application should load completely
4. All features working without external access
```

---

## How the Fallback System Works

The HTML includes intelligent loading:

```javascript
// Try to load local gauge library first
var script = document.createElement('script');
script.src = 'gauge.min.js';
script.onerror = function() {
    // If local fails, load from CDN (only when online)
    console.log('Loading canvas-gauges from CDN...');
    var cdnScript = document.createElement('script');
    cdnScript.src = 'https://cdn.rawgit.com/.../gauge.min.js';
    document.head.appendChild(cdnScript);
};
document.head.appendChild(script);
```

**Loading Order:**
1. Tries `gauge.min.js` in local directory
2. If found → Uses local copy (offline works!)
3. If not found → Falls back to CDN (online only)

This means:
✓ **With local file:** Works completely offline
✓ **Without local file:** Still works if online
✓ **Best of both worlds!**

---

## Complete Offline Package Checklist

### Required Files (Must Have)
- [ ] index.html (22 KB)
- [ ] styles.css (15 KB)
- [ ] app.js (34 KB)
- [ ] gauge.min.js (490 KB) ← Download separately

### Optional Files (Documentation)
- [ ] README.md
- [ ] QUICKSTART.md
- [ ] DEPLOYMENT.md
- [ ] etc.

### Total for Full Offline Operation
**~561 KB** (including gauge library)
**~71 KB** (without gauge library, CDN fallback)

---

## Deployment Scenarios

### Scenario 1: Track Day Tablet
```
Your Setup:
- iPad/Android tablet
- Connected to Pico 2W WiFi AP
- No internet connection
- Gauge library stored locally

Deploy:
1. Copy all 4 files to Pico 2W
2. Pico serves files via HTTP
3. Tablet connects to WiFi AP
4. Browse to http://192.168.4.1
5. Everything works offline!
```

### Scenario 2: Development Laptop
```
Your Setup:
- Laptop with internet
- Testing/development
- Frequent updates

Deploy:
- Option A: Use local gauge.min.js
- Option B: Let it use CDN (easier updates)
```

### Scenario 3: Phone in Car
```
Your Setup:
- Phone with cellular data
- Connected to Pico 2W
- May have internet via cellular

Deploy:
- Include gauge.min.js for reliability
- Will work even if cellular drops
```

---

## Raspberry Pi Pico 2W File Organization

```
Pico 2W Filesystem:
/
├── main.py                 # Main program
├── web/
│   ├── index.html          # UI
│   ├── styles.css          # Styles
│   ├── app.js              # Logic
│   └── gauge.min.js        # Gauges library ← Important!
├── lib/
│   ├── gps.py
│   ├── mpu6050.py
│   └── obd.py
└── data/
    └── sessions/
```

**Critical:** Place `gauge.min.js` in the `/web` directory!

---

## File Size Considerations

### With Offline Support (Recommended)
```
index.html:     22 KB
styles.css:     15 KB
app.js:         34 KB
gauge.min.js:  490 KB
─────────────────────
TOTAL:         561 KB
```

Fits easily on:
- Pico 2W internal flash (264 KB used + 2MB available)
- Any microSD card
- Phone storage
- Tablet storage

### Without gauge.min.js (CDN Required)
```
index.html:     22 KB
styles.css:     15 KB
app.js:         34 KB
─────────────────────
TOTAL:          71 KB

+ Requires internet for gauges to work
+ Will fail at track without connection
+ Not recommended for production use
```

---

## Browser Caching Benefits

Once loaded, browsers cache the gauge library:

**First Load (Online):**
- Downloads gauge.min.js from CDN: ~490 KB
- Caches for future use
- Subsequent loads are instant

**Future Loads (Offline):**
- Uses cached library
- No network required
- Works at track!

**However:** Don't rely on browser cache alone!
- Cache can be cleared
- Different devices need separate downloads
- Local file is more reliable

---

## Troubleshooting Offline Issues

### Issue: Gauges not displaying
**Check:**
```
1. Open browser console (F12)
2. Look for error: "gauge.min.js not found"
3. Verify file is in same directory as index.html
4. Check file size: should be ~490 KB
```

**Fix:**
```
1. Download gauge.min.js from GitHub
2. Place in correct directory
3. Refresh browser (Ctrl+F5)
```

### Issue: "Uncaught ReferenceError: RadialGauge is not defined"
**Cause:** Canvas-gauges library not loaded

**Fix:**
```
1. Verify gauge.min.js exists
2. Check browser console for load errors
3. Try clearing browser cache
4. Reload page
```

### Issue: Works at home, fails at track
**Cause:** Was using CDN, no offline file

**Fix:**
```
1. Add gauge.min.js to deployment
2. Test in airplane mode before track day
3. Verify all gauges display without internet
```

---

## Testing Checklist

Before deploying to track:

### Test 1: Offline Functionality
- [ ] Enable airplane mode
- [ ] Open index.html
- [ ] All 7 tabs load
- [ ] All 6 gauges display
- [ ] Gauges animate smoothly
- [ ] No console errors

### Test 2: Pico 2W Deployment
- [ ] Files uploaded to Pico
- [ ] gauge.min.js in /web directory
- [ ] Can connect to WiFi AP
- [ ] Browse to Pico IP
- [ ] All features working
- [ ] No internet connection needed

### Test 3: Configuration Persistence
- [ ] Add custom PIDs
- [ ] Set startup tab
- [ ] Save configuration
- [ ] Reload page
- [ ] Settings persist (localStorage works)

---

## Alternative: Embedded Gauges

If you can't download gauge.min.js, there's a future option:

**Inline Gauges (Future Enhancement):**
- Create gauges using pure Canvas API
- No external library needed
- Smaller file size
- More control

**Tradeoff:**
- More code to maintain
- Less polished appearance
- Would need to reimplement features

**Current Recommendation:** Use canvas-gauges library
- Mature, well-tested
- Professional appearance
- Worth the 490 KB

---

## Download Instructions Summary

### Quick Setup (3 Steps)

**Step 1:** Download canvas-gauges
```
Visit: https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js
Save as: gauge.min.js
Size: ~490 KB
```

**Step 2:** Organize files
```
openponylogger/
├── index.html
├── styles.css
├── app.js
└── gauge.min.js  ← Place here
```

**Step 3:** Test offline
```
1. Disconnect from internet
2. Open index.html
3. Verify all gauges work
4. ✓ Ready for deployment!
```

---

## Recommended File Hosting

### For Development:
```
Local directory:
~/openponylogger/
├── index.html
├── styles.css
├── app.js
└── gauge.min.js
```

### For Pico 2W Deployment:
```
Pico filesystem:
/web/
├── index.html
├── styles.css
├── app.js
└── gauge.min.js
```

### For Track Day Tablet:
```
Option 1: Direct from Pico WiFi
Option 2: Saved to tablet storage
Option 3: Saved offline web app (PWA)
```

---

## Future Enhancement: Progressive Web App (PWA)

**Planned feature:**
- Install as standalone app
- Full offline support
- No browser needed
- One-click launch

**Benefits:**
- Works like native app
- Automatic offline caching
- Home screen icon
- No URL bar

**Implementation:**
- Add manifest.json
- Add service worker
- Enable install prompt
- Complete offline support

---

## Storage Requirements

### Minimal (CDN-dependent):
- 71 KB
- Requires internet
- Not recommended

### Standard (Offline-capable):
- 561 KB
- Works anywhere
- **Recommended**

### With Documentation:
- 722 KB (includes all .md files)
- Complete package
- Self-contained

### With Sessions Data:
- 561 KB + session data
- ~1-2 MB per hour of logging
- Plan storage accordingly

---

## Deployment Checklist

Before your track day:

- [ ] Download gauge.min.js locally
- [ ] Verify all 4 core files present
- [ ] Test in airplane mode
- [ ] Upload to Pico 2W
- [ ] Verify gauge.min.js in /web directory
- [ ] Connect to Pico WiFi
- [ ] Load application
- [ ] Test all gauges display
- [ ] Test recording functionality
- [ ] Charge power bank
- [ ] Ready to log! 🏁

---

## Summary

**Critical File for Offline:**
```
gauge.min.js (490 KB)
Download from: https://github.com/Mikhus/canvas-gauges/releases
Place in: Same directory as index.html
```

**Fallback Behavior:**
1. ✓ Local file present → Works offline
2. ✗ Local file missing → Needs internet (CDN)

**Recommendation:**
Always include gauge.min.js for reliable offline operation!

**Your track day depends on it!** 🐎

---

## Quick Reference

| Scenario | Needs gauge.min.js? | Works Offline? |
|----------|---------------------|----------------|
| Development | Optional | If cached |
| Track Day | **Required** | ✓ Yes |
| Pico 2W Deploy | **Required** | ✓ Yes |
| Demo/Testing | Optional | If online |

**Bottom Line:** Include gauge.min.js for bulletproof operation!

