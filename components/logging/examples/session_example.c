/*
 * session_example.c - Example usage of session_helper APIs
 *
 * This example demonstrates how to create a logging session, write it to
 * flash partition, and manage NVS session metadata.
 */

#include <stdio.h>
#include "session_helper.h"
#include "session_header.h"

void app_main(void) {
    session_start_header_t session;
    
    // Example firmware SHA (normally from build system)
    uint8_t fw_sha[8] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    
    // Create a new session
    printf("Creating new logging session...\n");
    bool success = session_helper_create_session(&session, fw_sha, 1);
    
    if (success) {
        printf("Session created successfully!\n");
        printf("  Magic: 0x%08X\n", session.magic);
        printf("  Version: 0x%02X\n", session.version);
        printf("  Startup Counter: %lu\n", (unsigned long)session.startup_counter);
        printf("  ESP Time at Start: %lld us\n", (long long)session.esp_time_at_start);
        printf("  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               session.mac_addr[0], session.mac_addr[1], session.mac_addr[2],
               session.mac_addr[3], session.mac_addr[4], session.mac_addr[5]);
        printf("  FW SHA: ");
        for (int i = 0; i < 8; i++) {
            printf("%02X", session.fw_sha[i]);
        }
        printf("\n");
        printf("  CRC32: 0x%08X\n", session.crc32);
        
        // Get next NVS slot index
        int slot_idx = session_helper_get_next_slot_index();
        printf("\nNext NVS slot index: %d\n", slot_idx);
        
        // Write to NVS (uncomment in real usage)
        // esp_err_t err = session_helper_commit_session_nvs(&session, slot_idx);
        // if (err == ESP_OK) {
        //     printf("Session metadata committed to NVS slot %d\n", slot_idx);
        // }
        
        // Write to flash partition (uncomment in real usage)
        // err = session_helper_write_session_start_to_partition(&session, "storage", 0);
        // if (err == ESP_OK) {
        //     printf("Session header written to flash partition\n");
        // }
    } else {
        printf("Failed to create session!\n");
    }
}
