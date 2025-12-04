# OpenPonyLogger - Logo Setup Instructions

## 📁 Logo Files Needed

You have two logo images that need to be added to your project:

1. **logo-full.png** - Full logo with text (Image 1)
2. **logo-icon.png** - Icon version for favicon (Image 2)

## 🎯 Where to Place the Files

### File Structure
```
openponylogger/
├── index.html              ✓ Already updated
├── styles.css              ✓ Already updated with logo styles
├── app.js                  ✓ Ready to go
├── gauge.min.js            (download separately)
├── logo-full.png           ← Add this file (Full logo)
└── logo-icon.png           ← Add this file (Icon logo)
```

**Important:** Both logo files must be in the **same directory** as index.html

## 📝 Steps to Add Logos

### 1. Save the Logo Images

From the images you uploaded:

**Full Logo (Image 1):**
- The circular badge with "OPEN PONY LOGGER" text
- Mustang GT in the center with gear background
- Save as: `logo-full.png`
- Recommended size: 800x800px (it will scale automatically)

**Icon Logo (Image 2):**
- Simplified version without text
- Just the Mustang GT with gear
- Save as: `logo-icon.png`
- Recommended size: 512x512px (works for all icon sizes)

### 2. Add Files to Project Directory

**On your computer:**
```bash
# Place both files in the same directory as index.html
cp logo-full.png /path/to/openponylogger/
cp logo-icon.png /path/to/openponylogger/
```

**On Pico 2W:**
```bash
# Upload to /web directory
ampy --port /dev/ttyACM0 put logo-full.png /web/logo-full.png
ampy --port /dev/ttyACM0 put logo-icon.png /web/logo-icon.png
```

### 3. Verify Installation

Open `index.html` in your browser. You should see:

✓ **About tab:** Full logo displayed at the top
✓ **Browser tab:** Icon logo as favicon
✓ **Bookmarks:** Icon logo in bookmark bar
✓ **Mobile home screen:** Icon logo if saved as PWA

## 🎨 Where the Logos Appear

### Full Logo (logo-full.png)
**Location:** About tab (default startup page)
**Purpose:** 
- Main branding
- Project identity
- Professional appearance

**Display:**
- Desktop: Max 300px wide
- Mobile: Max 200px wide
- Automatically scales
- Centered on page

### Icon Logo (logo-icon.png)
**Location:** Multiple places
**Purpose:**
- Browser favicon (tab icon)
- Bookmark icon
- Mobile app icon (if saved to home screen)
- PWA manifest icon

**Formats:**
- 16x16px - Browser tab
- 32x32px - Browser toolbar
- 180x180px - Apple touch icon
- 512x512px - PWA splash screen

**Note:** Browser automatically scales the icon to all needed sizes!

## 🔧 HTML Changes Made

The following was already added to `index.html`:

### 1. Favicon Links (in `<head>`)
```html
<link rel="icon" type="image/png" href="logo-icon.png">
<link rel="apple-touch-icon" href="logo-icon.png">
```

### 2. Logo Image (in About tab)
```html
<div class="logo-container">
    <img src="logo-full.png" alt="OpenPonyLogger Logo" class="logo-full">
</div>
```

### 3. CSS Styles (in styles.css)
```css
.logo-container {
    text-align: center;
    margin-bottom: 1rem;
    padding: 1rem 0;
}

.logo-full {
    max-width: 300px;
    width: 100%;
    height: auto;
    display: block;
    margin: 0 auto;
}
```

## 🧪 Testing

After adding the logo files:

### Test 1: Full Logo on About Page
1. Open `index.html` in browser
2. Should show logo at top of About tab
3. Logo should be centered and sized appropriately
4. Should scale on mobile devices

### Test 2: Favicon
1. Check browser tab
2. Should show car/gear icon
3. Bookmark the page - icon should appear

### Test 3: Mobile
1. Open on phone/tablet
2. Tap "Add to Home Screen" (iOS) or "Install App" (Android)
3. Icon should appear on home screen

## 📱 PWA Manifest (Optional Enhancement)

To make OpenPonyLogger installable as an app, create `manifest.json`:

```json
{
  "name": "OpenPonyLogger",
  "short_name": "OPLogger",
  "description": "Open-Source Automotive Telemetry System",
  "start_url": ".",
  "display": "standalone",
  "background_color": "#1a1a2e",
  "theme_color": "#2196f3",
  "icons": [
    {
      "src": "logo-icon.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "any maskable"
    }
  ]
}
```

Then add to `<head>` in index.html:
```html
<link rel="manifest" href="manifest.json">
```

## 🎯 File Checklist

Before deploying to Pico 2W:

- [ ] logo-full.png saved (800x800px recommended)
- [ ] logo-icon.png saved (512x512px recommended)
- [ ] Both files in same directory as index.html
- [ ] Files uploaded to Pico 2W /web directory
- [ ] index.html updated (already done)
- [ ] styles.css updated (already done)
- [ ] Tested in browser - logo appears
- [ ] Tested favicon - icon appears in tab

## 🚀 Deployment

### Desktop Testing
```bash
# Navigate to project directory
cd /path/to/openponylogger

# Make sure you have all files
ls -la
# Should show: index.html, styles.css, app.js, logo-full.png, logo-icon.png, gauge.min.js

# Start simple HTTP server
python -m http.server 8000

# Open browser to: http://localhost:8000
```

### Pico 2W Deployment
```bash
# Upload all files including logos
ampy --port /dev/ttyACM0 put index.html /web/index.html
ampy --port /dev/ttyACM0 put styles.css /web/styles.css
ampy --port /dev/ttyACM0 put app.js /web/app.js
ampy --port /dev/ttyACM0 put gauge.min.js /web/gauge.min.js
ampy --port /dev/ttyACM0 put logo-full.png /web/logo-full.png
ampy --port /dev/ttyACM0 put logo-icon.png /web/logo-icon.png

# Connect to Pico WiFi and browse to its IP
```

## 💡 Tips

**Image Optimization:**
- Use PNG format (supports transparency)
- Compress images to reduce file size
- Keep under 100KB each for fast loading
- Logo-full.png can be larger (better quality)
- Logo-icon.png should be smaller (faster loading)

**Tools for Compression:**
- Online: tinypng.com
- Command line: `pngcrush logo-full.png logo-full-compressed.png`
- GUI: ImageOptim (Mac), FileOptimizer (Windows)

**Design Variations:**
You could also create:
- `logo-full-light.png` - Light background version for docs
- `logo-banner.png` - Wide banner for GitHub README
- `logo-square.png` - Square version for social media

## 🎨 Current Implementation

**What's Already Done:**
✅ HTML updated with logo references
✅ CSS styles added for logo display
✅ Favicon links added
✅ Responsive sizing implemented
✅ Version number updated to 1.3.0

**What You Need to Do:**
1. Save the two logo images from your Gemini generation
2. Name them exactly: `logo-full.png` and `logo-icon.png`
3. Place in same directory as index.html
4. Open index.html - logos should appear!

## 📞 Troubleshooting

**Logo doesn't appear:**
- Check file names match exactly: `logo-full.png` (not Logo-Full.png)
- Verify files are in same directory as index.html
- Check browser console (F12) for errors
- Hard refresh: Ctrl+F5 (Windows) or Cmd+Shift+R (Mac)

**Favicon doesn't show:**
- Clear browser cache
- Close and reopen browser
- Wait a few minutes (browsers cache favicons)
- Try incognito mode

**Wrong size on mobile:**
- CSS automatically scales
- Should work on all screen sizes
- If not, check styles.css loaded properly

## 🏁 Summary

The code is **ready** - just add your two logo PNG files:
1. `logo-full.png` (the circular badge with text)
2. `logo-icon.png` (the simplified icon)

Both files go in the same directory as index.html, and you're done! 🎉

Your professional OpenPonyLogger branding will be complete! 🐎🔧
