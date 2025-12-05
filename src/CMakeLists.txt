# OpenPonyLogger CMake Configuration
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w) 

# Include Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(OpenPonyLogger C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize Pico SDK
pico_sdk_init()

# Main executable
add_executable(OpenPonyLogger
    main.c
    core0_producer.c
    core1_consumer.c
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(OpenPonyLogger)

# Link against Pico SDK libraries
target_link_libraries(OpenPonyLogger
    pico_stdlib          # Standard library (GPIO, timers, etc)
    pico_multicore       # Multicore support
    pico_binary_info     # ADD THIS LINE - Provides pico_set_binary_info
    hardware_timer       # Hardware timer access
    hardware_gpio        # GPIO access
    hardware_uart        # UART for GPS
    hardware_i2c         # I2C for accelerometer
    hardware_spi         # SPI for SD card
    pico_cyw43_arch_none # WiFi support (CYW43 chip on Pico W)
)

# Enable USB serial output for debugging
pico_enable_stdio_usb(OpenPonyLogger 1)
pico_enable_stdio_uart(OpenPonyLogger 0)

# Compiler optimizations and warnings
target_compile_options(OpenPonyLogger PRIVATE
    -Wall
    -Wextra
    -O2              # Optimize for speed
    -g               # Include debug symbols
)

# Define project version
target_compile_definitions(OpenPonyLogger PRIVATE
    PICO_STACK_SIZE=4096
    PICO_HEAP_SIZE=65536
    PROJECT_VERSION_MAJOR=1
    PROJECT_VERSION_MINOR=0
    PROJECT_VERSION_PATCH=0
)

# Memory layout
pico_set_binary_info(OpenPonyLogger
    VERSION_STRING "1.0.0"
    DESCRIPTION "Open-source automotive telemetry system"
    URL "https://github.com/John-MustangGT/OpenPonyLogger"
)

# TODO: Add FatFS library for SD card support
# TODO: Add lwIP library for WiFi/HTTP server
# TODO: Add BLE stack library for OBD-II
