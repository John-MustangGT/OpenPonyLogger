# OpenPonyLogger - Caching Behavior Guide

## Understanding Caching

Caching can cause confusion during development when you update files but don't see changes. This guide explains how caching works and how to control it.

---

## Types of Caching

### 1. Browser Cache
**What it is:** Browser stores copies of files locally to speed up page loads
**Where:** On user's device (phone, tablet, laptop)
**Duration:** Until manually cleared or cache expires

### 2. Server Cache (Python)
**What it is:** Server holds files in memory to serve faster
**Where:** On Raspberry Pi Pico 2W
**Duration:** Usually until server restart

### 3. Service Worker Cache (PWA)
**What it is:** Advanced offline caching for web apps
**Where:** Browser's service worker storage
**Duration:** Until explicitly updated

---

## Default Behavior

### Python HTTP Server (Basic)
```python
# Simple server - NO caching on server side
from http.server import SimpleHTTPServer
python -m http.server 8000
```
- Server doesn't cache (reads files fresh each time)
- BUT browser still caches!

### Browser Behavior
**First Load:**
- Downloads index.html, styles.css, app.js
- Caches all files locally
- Remembers for future visits

**Subsequent Loads:**
- Checks cache first
- Only downloads if changed
- Can cause "stale" content issues

---

## The Problem: Stale Content

### Scenario 1: Development
```
You update:     index.html (add new feature)
Browser shows:  Old version (from cache)
You think:      "My changes don't work!"
Reality:        Browser is using cached version
```

### Scenario 2: Track Day
```
You update:     app.js (fix bug)
Upload to:      Pico 2W
Tablet shows:   Old buggy version
Reality:        Tablet cached the old file
```

---

## Solutions

### For Development

#### Method 1: Hard Refresh (Easiest)
```
Windows/Linux:  Ctrl + F5
Mac:            Cmd + Shift + R
Effect:         Forces fresh download of page
```

#### Method 2: Disable Cache in DevTools
```
1. Open DevTools (F12)
2. Go to Network tab
3. Check "Disable cache"
4. Keep DevTools open while testing
```

#### Method 3: Private/Incognito Mode
```
Chrome:   Ctrl + Shift + N
Firefox:  Ctrl + Shift + P
Safari:   Cmd + Shift + N
Effect:   No cache, fresh each time
```

#### Method 4: Clear Browser Cache
```
Chrome:   Settings → Privacy → Clear browsing data
Firefox:  Options → Privacy → Clear Data
Safari:   Preferences → Privacy → Manage Website Data
```

### For Production (Pico 2W)

#### Method 1: Add Cache-Control Headers (Recommended)
Update your MicroPython server:

```python
def serve_file(self, conn, filepath, content_type):
    """Serve static files with no-cache headers"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        
        # Add cache control headers
        response = f"""HTTP/1.1 200 OK
Content-Type: {content_type}
Content-Length: {len(content)}
Cache-Control: no-cache, no-store, must-revalidate
Pragma: no-cache
Expires: 0
Connection: close

{content}"""
        conn.send(response.encode())
    except:
        self.serve_404(conn)
```

**Effect:** Browser always checks server for latest version

#### Method 2: Version Query Strings
Update index.html to include versions:

```html
<!-- Before -->
<link rel="stylesheet" href="styles.css">
<script src="app.js"></script>

<!-- After -->
<link rel="stylesheet" href="styles.css?v=1.2">
<script src="app.js?v=1.2"></script>
```

**Effect:** Browser treats as new file when version changes

#### Method 3: File Versioning
Include version in filename:

```
index.html
styles-v1.2.css
app-v1.2.js
gauge.min.js
```

Update references in HTML when you release new versions.

---

## Recommended Approach

### Development Environment
```
1. Use browser DevTools with cache disabled
2. Hard refresh (Ctrl+F5) after changes
3. Use incognito mode for clean tests
```

### Testing on Device
```
1. Clear browser cache before testing
2. Use airplane mode test (forces fresh load)
3. Verify version number in about page
```

### Production Deployment
```
1. Add Cache-Control headers to server
2. Use version query strings for files
3. Test on actual device before track day
```

---

## Cache Headers Explained

### No-Cache Configuration
```
Cache-Control: no-cache, no-store, must-revalidate
Pragma: no-cache
Expires: 0
```

**no-cache:** Browser must check with server before using cached version
**no-store:** Don't store in cache at all
**must-revalidate:** Always verify freshness with server
**Pragma: no-cache:** HTTP/1.0 compatibility
**Expires: 0:** Immediately stale

### Long-Cache Configuration (for gauge.min.js)
```
Cache-Control: public, max-age=31536000
```

**Effect:** Cache for 1 year (good for libraries that rarely change)

---

## Smart Caching Strategy

### Files That Rarely Change
```
gauge.min.js → Cache for 1 year
```
**Reason:** External library, stable version

### Files That Change Often
```
index.html → No cache
styles.css → No cache
app.js     → No cache
```
**Reason:** Your code, frequent updates

### Implementation
```python
def serve_file(self, conn, filepath, content_type):
    # Read file
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Determine cache strategy
    if 'gauge.min.js' in filepath:
        cache_header = 'Cache-Control: public, max-age=31536000'
    else:
        cache_header = 'Cache-Control: no-cache, no-store, must-revalidate'
    
    response = f"""HTTP/1.1 200 OK
Content-Type: {content_type}
{cache_header}
Content-Length: {len(content)}
Connection: close

{content}"""
    conn.send(response.encode())
```

---

## localStorage vs File Cache

### File Cache (HTTP)
- Browser caches .html, .css, .js files
- Controlled by HTTP headers
- Cleared with browser cache clear

### localStorage (Configuration)
- Application data (your settings, PIDs, etc.)
- Separate from file cache
- Cleared with "Clear site data" or Factory Reset

**Important:** Clearing browser cache doesn't affect your settings!

---

## Testing Cache Behavior

### Test 1: Verify No-Cache Headers
```
1. Open DevTools (F12)
2. Go to Network tab
3. Load page
4. Click on index.html
5. Check Response Headers
6. Should see: Cache-Control: no-cache
```

### Test 2: Verify Fresh Content
```
1. Load page
2. Note version in About tab
3. Update a file on Pico
4. Refresh page (F5)
5. Should see updated content immediately
```

### Test 3: Hard Refresh Test
```
1. Load page normally
2. Change something obvious (e.g., header text)
3. Regular refresh (F5) → may show old
4. Hard refresh (Ctrl+F5) → should show new
```

---

## Common Issues & Fixes

### Issue: "I updated the file but don't see changes"
**Cause:** Browser cache
**Fix:** 
```
1. Hard refresh (Ctrl+F5)
2. Or clear browser cache
3. Or add no-cache headers to server
```

### Issue: "Works on laptop, broken on tablet"
**Cause:** Different browser, different cache
**Fix:**
```
1. Clear cache on tablet
2. Test in tablet's incognito mode
3. Add cache headers to prevent issue
```

### Issue: "Changes work sometimes, not others"
**Cause:** Inconsistent cache behavior
**Fix:**
```
1. Add proper cache-control headers
2. Use version query strings
3. Clear all caches before testing
```

### Issue: "gauge.min.js not loading"
**Cause:** Not cached AND not available locally
**Fix:**
```
1. Download gauge.min.js locally
2. Place in /web directory
3. Don't rely on CDN caching
```

---

## Track Day Recommendations

### Before Track Day
```
1. Clear all browser cache on tablet/phone
2. Connect to Pico WiFi
3. Load application fresh
4. Verify version in About tab
5. Test all features
6. Disconnect and test offline
```

### At Track
```
1. If page looks wrong:
   - Force refresh (Ctrl+F5 equivalent)
   - Close browser completely
   - Reopen and reconnect
   
2. If still issues:
   - Clear browser cache
   - Reload from Pico
```

### After Updates
```
1. Upload new files to Pico
2. Clear cache on all devices
3. Test on each device
4. Confirm new version displays
```

---

## MicroPython Server Best Practices

### Full Server with Smart Caching
```python
def serve_file(self, conn, filepath, content_type):
    """Serve files with appropriate cache headers"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        
        # Determine caching strategy
        if 'gauge.min.js' in filepath:
            # Cache library for long time
            cache_control = 'public, max-age=31536000'
        elif filepath.endswith('.html'):
            # Never cache HTML
            cache_control = 'no-cache, no-store, must-revalidate'
        else:
            # Short cache for CSS/JS
            cache_control = 'no-cache, must-revalidate, max-age=0'
        
        response = f"""HTTP/1.1 200 OK
Content-Type: {content_type}
Cache-Control: {cache_control}
Pragma: no-cache
Content-Length: {len(content)}
Connection: close

{content}"""
        conn.send(response.encode())
    except Exception as e:
        print(f"Error serving {filepath}: {e}")
        self.serve_404(conn)
```

---

## Quick Reference

| Scenario | Solution |
|----------|----------|
| Development testing | Disable cache in DevTools |
| Updated file not showing | Hard refresh (Ctrl+F5) |
| Clean test needed | Incognito/Private mode |
| Production deployment | Add no-cache headers |
| Library file (gauge.js) | Long cache (1 year) |
| Your files (.html/.js/.css) | No cache |
| Track day updates | Clear cache on all devices |

---

## Summary

**The Problem:**
- Python server doesn't cache files
- But browsers do cache aggressively
- Can cause stale content issues

**The Solution:**
- Add Cache-Control headers to server responses
- Use hard refresh during development
- Clear cache before important tests
- Test offline to verify fresh loads

**For Your Mustang GT:**
- Add no-cache headers to Pico server
- Test thoroughly before track day
- Clear tablet cache before each track session
- Hard refresh if anything looks wrong

**Result:** Always see the latest version, never confused by cache! 🏁

---

## Additional Resources

- Chrome DevTools: https://developer.chrome.com/docs/devtools/
- HTTP Caching: https://developer.mozilla.org/en-US/docs/Web/HTTP/Caching
- Cache-Control: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control

