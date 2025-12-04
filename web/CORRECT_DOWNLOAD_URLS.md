# Canvas-Gauges Library - Correct Download URLs

## Working Download URLs

Since GitHub releases URL doesn't work, here are the correct CDN URLs:

### Option 1: UNPKG (Recommended)
```bash
curl -o gauge.min.js https://unpkg.com/canvas-gauges@2.1.7/gauge.min.js
```

### Option 2: jsDelivr
```bash
curl -o gauge.min.js https://cdn.jsdelivr.net/npm/canvas-gauges@2.1.7/gauge.min.js
```

### Option 3: cdnjs (Cloudflare)
```bash
curl -o gauge.min.js https://cdnjs.cloudflare.com/ajax/libs/canvas-gauges/2.1.7/gauge.min.js
```

## Direct Download in Browser

Visit any of these URLs in your browser:

1. https://unpkg.com/canvas-gauges@2.1.7/gauge.min.js
2. https://cdn.jsdelivr.net/npm/canvas-gauges@2.1.7/gauge.min.js
3. https://cdnjs.cloudflare.com/ajax/libs/canvas-gauges/2.1.7/gauge.min.js

Then:
- Right-click on page
- Save As → `gauge.min.js`
- Place in same directory as index.html

## NPM Method (If You Have Node.js)

```bash
# Install package
npm install canvas-gauges

# Copy to your project
cp node_modules/canvas-gauges/gauge.min.js ./

# Clean up
rm -rf node_modules package-lock.json
```

## Verification

After downloading, verify the file:

```bash
# Check size (should be ~490 KB)
ls -lh gauge.min.js

# Check content (should show minified JS)
head -c 100 gauge.min.js

# Should see something like:
# !function(t,i){"object"==typeof exports...
```

## For Your Pico 2W

```bash
# On your computer, download:
curl -o gauge.min.js https://unpkg.com/canvas-gauges@2.1.7/gauge.min.js

# Then upload to Pico 2W:
# (using your preferred method - ampy, Thonny, etc.)
```

## Updated Download Scripts

The download helper scripts have been updated with the correct URL.

