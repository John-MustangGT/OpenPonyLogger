#ifndef VERSION_INFO_H
#define VERSION_INFO_H

// Build version information
// GIT_COMMIT_SHA is defined at compile time via build flags
#ifndef GIT_COMMIT_SHA
#define GIT_COMMIT_SHA "unknown"
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

#define PROJECT_NAME "OpenPonyLogger"
#define PROJECT_VERSION "1.0.0"
#define PROJECT_LICENSE "MIT"

// Convenience function to get full version string
inline const char* get_version_string() {
    static char version_buffer[128];
    snprintf(version_buffer, sizeof(version_buffer), 
             "%s v%s (commit: %.7s, built: %s)",
             PROJECT_NAME, PROJECT_VERSION, GIT_COMMIT_SHA, BUILD_TIMESTAMP);
    return version_buffer;
}

#endif // VERSION_INFO_H
