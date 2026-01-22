#ifndef VERSION_INFO_H
#define VERSION_INFO_H

// Auto-generated at build time by scripts/generate_version.py
// DO NOT EDIT MANUALLY - changes will be overwritten

#define PROJECT_NAME "OpenPonyLogger"
#define PROJECT_LICENSE "MIT"

// Git information (captured at build time)
#define GIT_COMMIT_SHA "9ed099b"
#define GIT_BRANCH "claude/optimize-core-resources-kHcMm"
#define GIT_TAG "v0.0.0"

// Build timestamp
#define BUILD_TIMESTAMP "2026-01-22 07:30:00"

// Version string formats
inline const char* get_version_string() {
    static char version_buffer[128];
    snprintf(version_buffer, sizeof(version_buffer),
             "%s %s (%.7s on %s)",
             PROJECT_NAME, GIT_TAG, GIT_COMMIT_SHA, GIT_BRANCH);
    return version_buffer;
}

inline const char* get_full_version_string() {
    static char version_buffer[256];
    snprintf(version_buffer, sizeof(version_buffer),
             "%s %s\nCommit: %s\nBranch: %s\nBuilt: %s",
             PROJECT_NAME, GIT_TAG, GIT_COMMIT_SHA, GIT_BRANCH, BUILD_TIMESTAMP);
    return version_buffer;
}

#endif // VERSION_INFO_H
