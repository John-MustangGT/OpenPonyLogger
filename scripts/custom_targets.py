#!/usr/bin/env python3
"""
Add custom PlatformIO targets for ESP32 device management.
This script adds erase targets for quick and full flash erase.
"""
Import("env")

def erase_callback(*args, **kwargs):
    """Quick erase - erase app partition only (keeps NVS)"""
    print("=" * 60)
    print("QUICK ERASE: Erasing application partition only")
    print("This will keep NVS config, WiFi settings, and logs intact")
    print("=" * 60)
    env.Execute("esptool.py --chip esp32s3 --port $UPLOAD_PORT erase_region 0x10000 0x3F0000")

def erase_flash_callback(*args, **kwargs):
    """Full chip erase - erase entire flash including NVS"""
    print("=" * 60)
    print("FULL CHIP ERASE: Erasing entire flash memory")
    print("WARNING: This will erase:")
    print("  - Application firmware")
    print("  - NVS configuration (settings, WiFi, etc.)")
    print("  - All log files")
    print("  - Build info and hardware config")
    print("Device will boot with factory defaults after this!")
    print("=" * 60)
    env.Execute("esptool.py --chip esp32s3 --port $UPLOAD_PORT erase_flash")

# Add custom targets (only if not already added)
if "erase" not in env.get("__PIO_TARGETS", {}):
    env.AddCustomTarget(
        name="erase",
        dependencies=None,
        actions=erase_callback,
        title="Quick Erase",
        description="Erase application partition only (keeps NVS config)"
    )

    env.AddCustomTarget(
        name="erase_flash",
        dependencies=None,
        actions=erase_flash_callback,
        title="Full Chip Erase",
        description="Erase entire flash including NVS (factory reset)"
    )

    print("Custom targets added:")
    print("  - 'erase': Quick erase (app only)")
    print("  - 'erase_flash': Full chip erase (including NVS)")
