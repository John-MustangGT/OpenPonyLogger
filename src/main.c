/**
 * @file main.c
 * @brief OpenPonyLogger - Main entry point
 * 
 * Initializes hardware, starts both cores, and manages overall system state.
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "telemetry_types.h"
#include "ring_buffer.h"
#include <stdio.h>

// External entry points for each core
extern void core0_entry(void);
extern void core1_entry(void);

// Global ring buffer for inter-core communication
ring_buffer_t g_telemetry_buffer;

// Status LED pin (adjust based on actual hardware)
#define STATUS_LED_PIN 25

/**
 * Initialize status LED
 */
static void init_status_led(void) {
    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_put(STATUS_LED_PIN, 0);
}

/**
 * Blink status LED pattern
 */
static void blink_status(int count, int delay_ms) {
    for (int i = 0; i < count; i++) {
        gpio_put(STATUS_LED_PIN, 1);
        sleep_ms(delay_ms);
        gpio_put(STATUS_LED_PIN, 0);
        sleep_ms(delay_ms);
    }
}

/**
 * Initialize system hardware
 */
static bool init_hardware(void) {
    printf("\n=== OpenPonyLogger v1.0.0 ===\n");
    printf("Initializing hardware...\n");
    
    // Initialize status LED
    init_status_led();
    blink_status(3, 100); // 3 quick blinks on startup
    
    // Initialize ring buffer
    ring_buffer_init(&g_telemetry_buffer);
    printf("Ring buffer initialized: %u bytes\n", g_telemetry_buffer.meta.capacity);
    
    // TODO: Initialize hardware interfaces
    // - GPS UART
    // - Accelerometer I2C  
    // - SD Card SPI
    // - WiFi
    // - BLE stack
    
    printf("Hardware initialization complete\n");
    return true;
}

/**
 * Monitor system health and print periodic status
 */
static void system_monitor_task(void) {
    static uint64_t last_status_print = 0;
    uint64_t current_time = time_us_64();
    
    // Print status every 10 seconds
    if ((current_time - last_status_print) >= 10000000) {
        uint32_t bytes_used, bytes_free, dropped_count;
        bool overflow;
        
        ring_buffer_get_stats(&g_telemetry_buffer, &bytes_used, &bytes_free, 
                             &overflow, &dropped_count);
        
        printf("\n=== System Status ===\n");
        printf("Ring Buffer: %u bytes used, %u bytes free\n", bytes_used, bytes_free);
        printf("Messages dropped: %u\n", dropped_count);
        if (overflow) {
            printf("WARNING: Ring buffer overflow detected!\n");
        }
        
        // Get Core 0 statistics
        extern void core0_get_stats(uint32_t *gps, uint32_t *accel, uint32_t *obd);
        uint32_t gps_count, accel_count, obd_count;
        core0_get_stats(&gps_count, &accel_count, &obd_count);
        printf("Core 0 - GPS: %u, Accel: %u, OBD: %u messages\n", 
               gps_count, accel_count, obd_count);
        
        // Get Core 1 statistics
        extern void core1_get_stats(uint32_t *processed, uint32_t *written, 
                                    uint32_t *buffer_used, bool *sd_ok);
        uint32_t processed, written, buffer_used;
        bool sd_ok;
        core1_get_stats(&processed, &written, &buffer_used, &sd_ok);
        printf("Core 1 - Processed: %u, Written: %u bytes, SD: %s\n",
               processed, written, sd_ok ? "OK" : "ERROR");
        printf("==================\n\n");
        
        last_status_print = current_time;
        
        // Heartbeat LED blink
        gpio_put(STATUS_LED_PIN, 1);
        sleep_ms(50);
        gpio_put(STATUS_LED_PIN, 0);
    }
}

/**
 * Check for BOOTSEL button to enter USB bootloader mode
 */
static void check_bootsel_button(void) {
    // BOOTSEL button check - hold on startup to enter bootloader
    // This would typically be done at boot, but can also be triggered
    // by a specific condition (e.g., web interface command)
    
    // For now, just a placeholder
    // Actual implementation would check GPIO or use bootrom API
}

/**
 * Main function - Core 1 starts here initially, then launches Core 0
 */
int main(void) {
    // Initialize stdio for USB serial debugging
    stdio_init_all();
    sleep_ms(2000); // Give time for USB to enumerate
    
    // Initialize hardware
    if (!init_hardware()) {
        printf("FATAL: Hardware initialization failed!\n");
        while (1) {
            blink_status(5, 200); // Rapid blink indicates error
            sleep_ms(1000);
        }
    }
    
    // Check for bootloader entry request
    check_bootsel_button();
    
    // Start Core 0 for data acquisition
    printf("Launching Core 0 (data acquisition)...\n");
    multicore_launch_core1(core0_entry);
    
    // Core 1 continues here for processing and storage
    printf("Starting Core 1 (processing and storage)...\n");
    core1_entry();
    
    // Should never reach here
    printf("ERROR: Core 1 exited main loop!\n");
    while (1) {
        blink_status(10, 100);
        sleep_ms(1000);
    }
    
    return 0;
}
