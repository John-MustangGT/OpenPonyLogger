#ifndef SESSION_HELPER_H
#define SESSION_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include "session_header.h"

#ifdef __cplusplus
extern "C" {
#endif

bool session_helper_create_session(session_start_header_t* out, const uint8_t fw_sha[8], uint32_t startup_counter);

// Write the session_start_header_t to the named partition at offset (byte offset);
// many systems will write at the current write pointer. Use 0 for partition start.
esp_err_t session_helper_write_session_start_to_partition(const session_start_header_t* hdr, const char* partition_label, size_t offset);

// Commit the session summary to NVS rotating slot (slot_idx 0..7)
esp_err_t session_helper_commit_session_nvs(const session_start_header_t* session, int slot_idx);

// Helper: get current rotating index (0..7)
int session_helper_get_next_slot_index(void);

#ifdef __cplusplus
}
#endif

#endif // SESSION_HELPER_H
