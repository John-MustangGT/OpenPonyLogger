# OpenPonyLogger - Build Guide

## Prerequisites

### Required Software

1. **Pico SDK**
   ```bash
   # Clone Pico SDK
   git clone https://github.com/raspberrypi/pico-sdk.git
   cd pico-sdk
   git submodule update --init
   ```

2. **ARM GCC Toolchain**
   ```bash
   # Ubuntu/Debian
   sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
   
   # macOS (via Homebrew)
   brew install cmake
   brew install --cask gcc-arm-embedded
   
   # Windows - Download from ARM website or use scoop
   scoop install gcc-arm-none-eabi
   ```

3. **CMake** (version 3.13 or higher)
   ```bash
   cmake --version  # Should be 3.13+
   ```

4. **Build Tools**
   - Linux/macOS: `make` or `ninja`
   - Windows: MinGW or use WSL

### Environment Setup

```bash
# Set PICO_SDK_PATH (add to ~/.bashrc or ~/.zshrc for persistence)
export PICO_SDK_PATH=/path/to/pico-sdk

# Verify it's set
echo $PICO_SDK_PATH
```

## Initial Clone and Setup

### Fresh Clone

```bash
# Clone the repository with submodules
git clone --recurse-submodules https://github.com/John-MustangGT/OpenPonyLogger.git
cd OpenPonyLogger

# If you forgot --recurse-submodules, initialize them now:
git submodule update --init --recursive
```

### Existing Repository (Adding FatFS Submodule)

If you already have the repository but need to add the FatFS library:

```bash
cd OpenPonyLogger

# Add FatFS as a submodule
git submodule add https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git lib/fatfs

# Initialize and update
git submodule update --init --recursive

# Commit the submodule
git add .gitmodules lib/fatfs
git commit -m "Add FatFS library submodule"
```

## Building the Firmware

### Standard Build

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build (use -j for parallel compilation)
make -j4

# Output will be build/src/OpenPonyLogger.uf2
```

### Build Output

After successful compilation, you'll have:
```
build/src/
├── OpenPonyLogger.uf2      # Flash file for Pico (drag & drop)
├── OpenPonyLogger.elf      # ELF binary (for debugging)
├── OpenPonyLogger.bin      # Raw binary
├── OpenPonyLogger.hex      # Hex format
└── OpenPonyLogger.dis      # Disassembly listing
```

## Flashing to Pico 2W

### Method 1: BOOTSEL Mode (Recommended for Development)

1. **Enter BOOTSEL Mode:**
   - Disconnect Pico from USB
   - Hold BOOTSEL button on Pico
   - Connect USB cable while holding button
   - Release button after 2 seconds
   - Pico will mount as `RPI-RP2` drive

2. **Flash Firmware:**
   ```bash
   # Linux/macOS
   cp build/src/OpenPonyLogger.uf2 /media/$USER/RPI-RP2/
   
   # Or drag and drop the .uf2 file to the RPI-RP2 drive
   ```

3. **Automatic Reboot:**
   - Pico will automatically reboot after copying
   - LED should blink 3 times on startup

### Method 2: Using picotool

```bash
# Install picotool
git clone https://github.com/raspberrypi/picotool.git
cd picotool
mkdir build && cd build
cmake ..
make
sudo make install

# Flash (Pico must be in BOOTSEL mode)
picotool load -x build/src/OpenPonyLogger.uf2

# View info
picotool info
```

### Method 3: Using OpenOCD (Advanced - For Debugging)

```bash
# With SWD debugger connected
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "program build/src/OpenPonyLogger.elf verify reset exit"
```

## Verifying the Build

### Check USB Serial Output

```bash
# Linux
screen /dev/ttyACM0 115200

# macOS
screen /dev/cu.usbmodem* 115200

# Windows (use PuTTY)
# Connect to COMx at 115200 baud
```

Expected startup output:
```
=== OpenPonyLogger v1.0.0 ===
Initializing hardware...
Ring buffer initialized: 65536 bytes
Hardware initialization complete
Launching Core 0 (data acquisition)...
[Core 0] Starting data acquisition core
Starting Core 1 (processing and storage)...
[Core 1] Starting processing and storage core
[Core 1] Initializing SD card...
```

## Troubleshooting

### FatFS Library Missing

**Error:**
```
fatal error: ff.h: No such file or directory
```

**Solution:**
```bash
cd OpenPonyLogger
git submodule update --init --recursive

# Verify submodule is present
ls lib/fatfs/
```

### PICO_SDK_PATH Not Set

**Error:**
```
CMake Error: PICO_SDK_PATH is not set
```

**Solution:**
```bash
export PICO_SDK_PATH=/path/to/pico-sdk

# Make it permanent
echo 'export PICO_SDK_PATH=/path/to/pico-sdk' >> ~/.bashrc
source ~/.bashrc
```

### Wrong Board Type

**Error:**
```
Target "pico2_w" not found in supported boards
```

**Solution:**
Ensure your `src/CMakeLists.txt` has:
```cmake
set(PICO_BOARD pico2_w)  # Must be before pico_sdk_init()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
```

### Printf Format Warnings

**Warning:**
```
warning: format '%lu' expects argument of type 'long unsigned int'
```

**Solution:**
Use `%u` instead of `%lu` for `uint32_t` on RP2350 (32-bit platform). This is already fixed in the current code.

### Hardware DMA Library Missing

**Error:**
```
undefined reference to 'dma_channel_get_default_config'
```

**Solution:**
Add to `CMakeLists.txt`:
```cmake
target_link_libraries(OpenPonyLogger
    hardware_dma    # Add this
)
```

### Binary Info Library Missing

**Error:**
```
undefined reference to 'bi_decl'
```

**Solution:**
Add to `CMakeLists.txt`:
```cmake
target_link_libraries(OpenPonyLogger
    pico_binary_info    # Add this
)
```

### Clean Build (Nuclear Option)

If all else fails:
```bash
cd OpenPonyLogger
rm -rf build
mkdir build && cd build
cmake ..
make -j4
```

## Hardware Configuration

### SD Card Pinout (PiCowbell Adalogger)

Current configuration in `src/hw_config.c`:

| Signal | GPIO | Pin |
|--------|------|-----|
| MISO   | 4    | GP4 |
| MOSI   | 3    | GP3 |
| SCK    | 2    | GP2 |
| CS     | 5    | GP5 |

**To Change Pinout:**
Edit `src/hw_config.c`:
```c
static spi_t spi = {
    .hw_inst = spi0,
    .miso_gpio = 4,        // Change these
    .mosi_gpio = 3,
    .sck_gpio = 2,
    .baud_rate = 12 * 1000 * 1000,
};

static sd_spi_if_t spi_if = {
    .spi = &spi,
    .ss_gpio = 5,          // Chip select pin
};
```

### Status LED

Default status LED is GPIO 25 (built-in LED on Pico).

To change, edit `src/main.c`:
```c
#define STATUS_LED_PIN 25    // Change to your LED pin
```

## Development Workflow

### Iterative Development

```bash
# 1. Make code changes
nano src/core0_producer.c

# 2. Rebuild (only recompiles changed files)
cd build
make -j4

# 3. Flash new firmware
cp src/OpenPonyLogger.uf2 /media/$USER/RPI-RP2/

# 4. Monitor serial output
screen /dev/ttyACM0 115200
```

### Adding New Source Files

1. Create new `.c` file in `src/`
2. Update `src/CMakeLists.txt`:
   ```cmake
   add_executable(OpenPonyLogger
       main.c
       core0_producer.c
       core1_consumer.c
       hw_config.c
       your_new_file.c    # Add here
   )
   ```
3. Rebuild

### Enabling Debug Symbols

In `src/CMakeLists.txt`:
```cmake
target_compile_options(OpenPonyLogger PRIVATE
    -Wall
    -Wextra
    -g3          # Change from -g to -g3 for more debug info
    -O0          # Change from -O2 to disable optimization
)
```

## Build Configurations

### Release Build (Optimized)

```cmake
# In CMakeLists.txt
target_compile_options(OpenPonyLogger PRIVATE
    -O2          # Optimize for speed
    -DNDEBUG     # Disable assertions
)
```

### Debug Build

```cmake
target_compile_options(OpenPonyLogger PRIVATE
    -O0          # No optimization
    -g3          # Maximum debug info
)
```

### Size-Optimized Build

```cmake
target_compile_options(OpenPonyLogger PRIVATE
    -Os          # Optimize for size
)
```

## Memory Analysis

### Check Binary Size

```bash
cd build/src

# Summary
arm-none-eabi-size OpenPonyLogger.elf

# Detailed section info
arm-none-eabi-size -A OpenPonyLogger.elf

# Symbol table
arm-none-eabi-nm --size-sort -S OpenPonyLogger.elf | tail -20
```

Expected output:
```
   text    data     bss     dec     hex filename
  89234    2156   66024  157414   266f6 OpenPonyLogger.elf
```

Where:
- **text**: Code size (~89 KB)
- **data**: Initialized data (~2 KB)
- **bss**: Uninitialized data/buffers (~66 KB, includes 64KB ring buffer)

### Check Flash Usage

```bash
picotool info -a build/src/OpenPonyLogger.uf2
```

## Continuous Integration (Future)

Example GitHub Actions workflow (`.github/workflows/build.yml`):

```yaml
name: Build Firmware

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive
      
      - name: Install ARM Toolchain
        run: |
          sudo apt update
          sudo apt install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
      
      - name: Install Pico SDK
        run: |
          git clone https://github.com/raspberrypi/pico-sdk.git
          cd pico-sdk && git submodule update --init
      
      - name: Build
        run: |
          export PICO_SDK_PATH=$PWD/pico-sdk
          mkdir build && cd build
          cmake ../src
          make -j4
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: firmware
          path: build/src/*.uf2
```

## Next Steps

After successful build:

1. **Test Basic Functionality**: Verify startup messages via USB serial
2. **Test SD Card**: Check if SD card mounts successfully
3. **Add GPS Module**: Integrate UART reading
4. **Add Accelerometer**: Implement I2C driver
5. **Field Testing**: Install in vehicle and validate data logging

## Resources

- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [FatFS Library Docs](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [Getting Started with Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)

## Support

For build issues:
1. Check this guide's troubleshooting section
2. Review GitHub Issues: https://github.com/John-MustangGT/OpenPonyLogger/issues
3. Verify Pico SDK version is up to date
4. Ensure all submodules are initialized

---

**Last Updated:** 2025-01-05  
**Tested With:** Pico SDK 1.5.1, CMake 3.22, GCC 10.3.1
