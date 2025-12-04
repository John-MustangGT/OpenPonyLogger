## Bill of Materials (BOM)

### Version 1.0 Components

| Component | Part Number/Description | Quantity | Est. Cost | Status |
|-----------|------------------------|----------|-----------|--------|
| Raspberry Pi Pico 2W | Adafruit #6315 (with headers) | 2-3 | $8 ea | ⏳ Order |
| GPS Module | NEO-6M (dev) / NEO-8M (prod) | 1 | $10-15 | ✅ Have (NEO-6M) |
| Accelerometer | MPU6050 breakout | 1 | $5 | ⏳ Order |
| BLE OBD-II | VGate iCar Pro | 1 | $25 | ✅ Have |
| SD Card Module | MicroSD SPI breakout | 1 | $5 | ⏳ Order |
| MicroSD Card | 16-32GB, FAT32 | 1+ | $10 | ✅ Have (reformatted) |
| Buck Converter | LM2596 12V→5V module | 1 | $2 | ⏳ Order |
| Cat5 Cable | Ethernet cable (4-6 feet) | 1 | $3 | ⏳ Order |
| Enclosure | 3D printed housing | 1 | Filament | TBD Design |
| Velcro Strips | Industrial strength | 1 set | $5 | ⏳ Order |

**V1 Total Estimated Cost:** ~$85-95 (with 2-3 Pico boards for dev/spares)

### Version 2.0+ Additional Components

| Component | Part Number/Description | Quantity | Est. Cost |
|-----------|------------------------|----------|-----------|
| CAN Controller | Adafruit PiCowbell CAN Bus (MCP2515) | 1 | $15 |
| GPS Upgrade | NEO-8M module | 1 | $20 |
| Status LEDs | RGB LED or multi-color | 1-3 | $2 |

### Development Hardware (Already Have)

| Component | Description | Notes |
|-----------|-------------|-------|
| RP2040 Feather | Adafruit #4884 | Use for prototyping/testing Core 0/1 logic |
| MTK3339 GPS | (if found) | Alternative GPS for initial dev work |

### Notes on Component Selection

**Raspberry Pi Pico 2W (Adafruit #6315):**
- RP2350-A2 revision (has E9 erratum for GPIO pull-downs)
- Pre-soldered headers save time
- Ordering 2-3 units recommended: 1 for development, 1 for vehicle install, 1 spare
- 4MB flash sufficient for firmware + Bootstrap + Plotly.js (~4MB total)
- 520KB RAM provides excellent headroom for ring buffers and lwIP stack

**CAN Bus Future Option:**
- Adafruit PiCowbell CAN Bus module is designed specifically for Pico
- Stacks directly on Pico headers (no wiring needed for prototyping)
- MCP2515 controller with proper termination
- Perfect for V2+ direct CAN bus implementation
