╔═══════════════════════════════════════════════════════════════╗
║     OpenPonyLogger - Offline Operation Quick Guide           ║
╚═══════════════════════════════════════════════════════════════╝

CRITICAL: For Track Day / Offline Use
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

OpenPonyLogger is designed to work COMPLETELY OFFLINE.
However, you need ONE additional file for full functionality.

┌───────────────────────────────────────────────────────────────┐
│  REQUIRED FOR OFFLINE: gauge.min.js                           │
│  Size: ~490 KB                                                │
│  Download: See instructions below                             │
└───────────────────────────────────────────────────────────────┘

WHY THIS MATTERS
════════════════

❌ WITHOUT gauge.min.js:
   → Gauges won't display offline
   → Requires internet connection
   → WILL FAIL at track without WiFi

✓ WITH gauge.min.js:
   → Works completely offline
   → No internet needed
   → Track-ready! 🏁

QUICK DOWNLOAD
══════════════

METHOD 1: Run Download Script (Easiest)
────────────────────────────────────────
Linux/Mac:
  ./download-gauge-library.sh

Windows:
  download-gauge-library.bat

METHOD 2: Manual Download
─────────────────────────
1. Visit:
   https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js

2. Right-click → Save As → gauge.min.js

3. Place in same directory as index.html

METHOD 3: Command Line
──────────────────────
curl -o gauge.min.js https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js

or

wget -O gauge.min.js https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js

FILE STRUCTURE
══════════════

After download, your directory should look like:

openponylogger/
├── index.html              ✓ Included
├── styles.css              ✓ Included
├── app.js                  ✓ Included
└── gauge.min.js            ← Download this!

Total: ~561 KB (all files)

TESTING OFFLINE
═══════════════

Before Track Day:
1. Download gauge.min.js
2. Enable Airplane Mode on your device
3. Open index.html in browser
4. Verify all 6 gauges display and animate
5. Check all 7 tabs work
6. ✓ Ready for deployment!

HOW IT WORKS
════════════

Smart Fallback System:
  1. Tries local gauge.min.js first
  2. If found → Uses local (works offline!)
  3. If missing → Falls back to CDN (needs internet)

This means:
  • Development: Works with or without local file
  • Production: Include local file for reliability
  • Track Day: MUST have local file!

DEPLOYMENT TO PICO 2W
═════════════════════

File Organization:
/web/
├── index.html
├── styles.css
├── app.js
└── gauge.min.js  ← Include this!

Upload all 4 files to Pico's /web directory

TRACK DAY CHECKLIST
═══════════════════

Before leaving home:
□ Downloaded gauge.min.js (490 KB)
□ Placed in same folder as index.html
□ Tested in airplane mode
□ All gauges displaying
□ Uploaded to Pico 2W
□ Verified on tablet/phone
□ Power bank charged
□ Ready to log! 🏁

TROUBLESHOOTING
═══════════════

Gauges not displaying?
  → Open browser console (F12)
  → Look for "gauge.min.js not found"
  → Download and place in correct directory

Works at home, fails at track?
  → You're using CDN, not local file
  → Download gauge.min.js
  → Test in airplane mode

"RadialGauge is not defined"?
  → Library not loaded
  → Verify gauge.min.js exists
  → Check file size: should be ~490 KB

ALTERNATIVE OPTIONS
═══════════════════

If you can't download gauge.min.js:

1. Let browser cache it:
   → Load once with internet
   → Browser caches for future use
   → Less reliable than local file

2. Use different device:
   → Some devices have better cache
   → Still test offline before track!

3. Wait for PWA version (future):
   → Will bundle everything
   → One-click install
   → Automatic offline support

SUMMARY
═══════

Required Files:
  Core:    index.html, styles.css, app.js (71 KB)
  Offline: gauge.min.js (490 KB)
  Total:   561 KB

Download gauge.min.js from:
  https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js

Place in same directory as index.html

Test in airplane mode before track day!

QUESTIONS?
══════════

Q: Do I NEED gauge.min.js?
A: For offline/track use, YES!
   For online development, optional.

Q: Can I use a different version?
A: v2.1.7 is tested and recommended.
   Other versions may work.

Q: Where does it go on Pico?
A: In /web/ directory with other files.

Q: File size concerns?
A: 490 KB is tiny for modern storage.
   Well worth the offline capability!

Q: Will this work on my tablet?
A: Yes! Any modern browser supports it.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Your track day depends on offline operation!
Download gauge.min.js before you need it!

See OFFLINE_SETUP.md for complete documentation.

Happy logging! 🐎🏁

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
