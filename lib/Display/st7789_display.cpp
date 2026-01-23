#include "st7789_display.h"
#include "display_labels.h"
#include "../../../lib/Logger/include/units_helper.h"
#include <SPI.h>
#include <cstdio>
#include <esp_heap_caps.h>

// ST7789 pins for Adafruit ESP32-S3 Feather Reverse TFT
#define TFT_CS    42
#define TFT_DC    40
#define TFT_RST   41
#define TFT_BACKLITE 45
#define TFT_I2C_POWER 7

// ============================================================================
// PSRAMCanvas16 Implementation - PSRAM-only framebuffer
// ============================================================================

PSRAMCanvas16::PSRAMCanvas16(uint16_t w, uint16_t h)
    : Adafruit_GFX(w, h), m_buffer(nullptr), m_buffer_size(0) {
    m_buffer_size = w * h * sizeof(uint16_t);

    // Allocate buffer ONLY in PSRAM using heap_caps_malloc
    m_buffer = (uint16_t*)heap_caps_malloc(m_buffer_size, MALLOC_CAP_SPIRAM);

    if (m_buffer != nullptr) {
        // Clear buffer to black
        memset(m_buffer, 0, m_buffer_size);
    } else {
        Serial.printf("[PSRAMCanvas16] ERROR: Failed to allocate %u bytes in PSRAM!\n", m_buffer_size);
    }
}

PSRAMCanvas16::~PSRAMCanvas16() {
    if (m_buffer != nullptr) {
        heap_caps_free(m_buffer);
        m_buffer = nullptr;
    }
}

void PSRAMCanvas16::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (m_buffer == nullptr || x < 0 || y < 0 || x >= _width || y >= _height) {
        return;
    }
    m_buffer[y * _width + x] = color;
}

void PSRAMCanvas16::fillScreen(uint16_t color) {
    if (m_buffer == nullptr) {
        return;
    }

    // Fast fill using memset for black (0x0000) or white (0xFFFF)
    if (color == 0x0000) {
        memset(m_buffer, 0, m_buffer_size);
    } else if (color == 0xFFFF) {
        memset(m_buffer, 0xFF, m_buffer_size);
    } else {
        // General case: fill with specific color
        for (size_t i = 0; i < (_width * _height); i++) {
            m_buffer[i] = color;
        }
    }
}

// Static member initialization
Adafruit_ST7789* ST7789Display::m_tft = nullptr;
PSRAMCanvas16* ST7789Display::m_canvas = nullptr;
bool ST7789Display::m_initialized = false;
DisplayMode ST7789Display::m_current_mode = DisplayMode::MAIN_SCREEN;

bool ST7789Display::init() {
    if (m_initialized) {
        return true;
    }
    
    Serial.println("[TFT] Starting display initialization...");
    
    // Step 1: Set power pins BEFORE anything else
    Serial.println("[TFT] Configuring power pins (GPIO7=TFT_I2C_POWER)...");
    pinMode(TFT_I2C_POWER, OUTPUT);
    digitalWrite(TFT_I2C_POWER, HIGH);
    delay(10);
    
    // Step 2: Configure backlight (start with it OFF)
    Serial.println("[TFT] Configuring backlight (GPIO45)...");
    pinMode(TFT_BACKLITE, OUTPUT);
    digitalWrite(TFT_BACKLITE, LOW);
    delay(10);
    
    // Step 3: Configure control pins
    Serial.println("[TFT] Configuring control pins (CS=GPIO42, DC=GPIO40, RST=GPIO41)...");
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_CS, HIGH);  // Deselect
    digitalWrite(TFT_DC, HIGH);  // Data mode
    digitalWrite(TFT_RST, HIGH); // Not reset
    delay(50);
    
    // Step 4: Reset display (LOW, wait, HIGH)
    Serial.println("[TFT] Performing hardware reset...");
    digitalWrite(TFT_RST, LOW);
    delay(10);
    digitalWrite(TFT_RST, HIGH);
    delay(120);
    
    // Step 5: Create display object
    Serial.println("[TFT] Creating ST7789 instance with (CS=42, DC=40, RST=41)...");
    m_tft = new Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
    
    if (m_tft == nullptr) {
        Serial.println("[TFT] ERROR: Failed to allocate display object!");
        return false;
    }
    
    // Step 6: Initialize display
    Serial.println("[TFT] Calling init(135, 240) - height=135, width=240...");
    m_tft->init(135, 240);
    delay(100);
    
    Serial.println("[TFT] Setting rotation to 1 (landscape, flipped)...");
    m_tft->setRotation(1);
    delay(50);
    
    Serial.println("[TFT] Filling screen black...");
    m_tft->fillScreen(ST77XX_BLACK);
    delay(50);
    
    // Step 7: Turn on backlight
    Serial.println("[TFT] Enabling backlight (GPIO45)...");
    digitalWrite(TFT_BACKLITE, HIGH);
    delay(100);
    
    // Step 8: Allocate framebuffer EXCLUSIVELY in PSRAM for flicker-free rendering
    Serial.println("[TFT] Allocating framebuffer (240x135x2 = 64,800 bytes) in PSRAM...");
    size_t canvas_size = 240 * 135 * 2;  // 16-bit per pixel

    Serial.printf("[TFT] Free DRAM before: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    Serial.printf("[TFT] Free PSRAM before: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Allocate PSRAMCanvas16 (uses heap_caps_malloc with MALLOC_CAP_SPIRAM - PSRAM only!)
    m_canvas = new PSRAMCanvas16(240, 135);
    if (m_canvas == nullptr || m_canvas->getBuffer() == nullptr) {
        Serial.println("[TFT] ERROR: Failed to allocate PSRAM canvas framebuffer!");
        return false;
    }

    Serial.printf("[TFT] Canvas allocated: %u bytes in PSRAM\n", canvas_size);
    Serial.printf("[TFT] Free DRAM after: %u bytes (should be unchanged)\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    Serial.printf("[TFT] Free PSRAM after: %u bytes (should decrease by ~65KB)\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Step 9: Draw test pattern
    Serial.println("[TFT] Drawing initialization message...");
    m_tft->setTextColor(ST77XX_WHITE);
    m_tft->setTextSize(1);
    m_tft->setCursor(5, 10);
    m_tft->println("ST7789 Ready!");
    m_tft->setCursor(5, 25);
    m_tft->println("Sensors loading...");
    delay(500);

    m_initialized = true;
    Serial.println("[TFT] Display initialization complete!");
    return true;
}

void ST7789Display::off() {
    if (!m_initialized || m_tft == nullptr) return;
    
    // Turn off backlight
    digitalWrite(TFT_BACKLITE, LOW);
}

void ST7789Display::on() {
    if (!m_initialized || m_tft == nullptr) return;
    
    // Turn on backlight
    digitalWrite(TFT_BACKLITE, HIGH);
}

void ST7789Display::update(uint32_t uptime_ms,
                          float temp,
                          float accel_x, float accel_y, float accel_z,
                          float gyro_x, float gyro_y, float gyro_z,
                          float battery_soc, float battery_voltage,
                          bool gps_valid, uint32_t sample_count, float sample_hz,
                          bool is_paused,
                          double gps_latitude,
                          double gps_longitude,
                          double gps_altitude,
                          uint8_t gps_hour,
                          uint8_t gps_minute,
                          uint8_t gps_second,
                          float gps_speed) {
    if (!m_initialized || m_tft == nullptr || m_canvas == nullptr) return;

    // ========== RENDER TO PSRAM CANVAS (NO SPI TRANSACTIONS) ==========
    // Clear canvas once (all rendering happens in RAM)
    m_canvas->fillScreen(ST77XX_BLACK);

    // Calculate uptime
    uint32_t uptime_sec = uptime_ms / 1000;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec / 60) % 60;
    uint32_t seconds = uptime_sec % 60;

    // ========== ROW 1: TIME & SAMPLE COUNT ==========
    char timestr[16];
    snprintf(timestr, sizeof(timestr), "%u:%02u:%02u", hours, minutes, seconds);
    m_canvas->setTextSize(2);
    m_canvas->setTextColor(ST77XX_CYAN);
    m_canvas->setCursor(2, 2);
    m_canvas->print(timestr);

    // Sample count on right side with logging state symbol
    char sampstr[32];
    if (sample_count >= 1000000) {
        snprintf(sampstr, sizeof(sampstr), "%.1fM%s", sample_count / 1000000.0f, is_paused ? "P" : "*");
    } else if (sample_count >= 1000) {
        snprintf(sampstr, sizeof(sampstr), "%.1fK%s", sample_count / 1000.0f, is_paused ? "P" : "*");
    } else {
        snprintf(sampstr, sizeof(sampstr), "%u%s", sample_count, is_paused ? "P" : "*");
    }
    m_canvas->setTextColor(ST77XX_YELLOW);
    m_canvas->setCursor(140, 5);
    m_canvas->print(sampstr);

    // ========== ROW 2: ACCELEROMETER DATA ==========
    char accel_line[40];
    snprintf(accel_line, sizeof(accel_line), "A:%+.2f %+.2f %+.2f", accel_x, accel_y, accel_z);
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setCursor(2, 28);
    m_canvas->print(accel_line);

    // ========== ROW 3: GYROSCOPE DATA ==========
    char gyro_line[40];
    snprintf(gyro_line, sizeof(gyro_line), "G:%+.1f %+.1f %+.1f", gyro_x, gyro_y, gyro_z);
    m_canvas->setCursor(2, 48);
    m_canvas->print(gyro_line);

    // ========== ROW 4: GPS COORDINATES ==========
    if (gps_valid) {
        char gps_line[40];
        snprintf(gps_line, sizeof(gps_line), "%+6.1f %+7.1f %5.0fm",
                 gps_latitude, gps_longitude, gps_altitude);
        m_canvas->setTextColor(ST77XX_GREEN);
        m_canvas->setCursor(2, 68);
        m_canvas->print(gps_line);
    } else {
        m_canvas->setTextColor(ST77XX_RED);
        m_canvas->setCursor(2, 68);
        m_canvas->print("No GPS Fix");
    }

    // ========== ROW 5: GPS SPEED ==========
    if (gps_valid) {
        float display_speed = convert_speed(gps_speed);
        char gps_str[32];
        snprintf(gps_str, sizeof(gps_str), "Spd:%.1f%s", display_speed, get_speed_unit());
        m_canvas->setTextColor(ST77XX_GREEN);
        m_canvas->setCursor(2, 88);
        m_canvas->print(gps_str);
    } else {
        m_canvas->setTextColor(ST77XX_YELLOW);
        m_canvas->setCursor(2, 88);
        m_canvas->print("GPS Waiting");
    }

    // ========== BOTTOM: GPS TIME & BATTERY ==========
    m_canvas->setTextSize(1);

    uint16_t bar_height = 6;
    uint16_t bar_y = 135 - bar_height - 4;  // Hard-coded height
    uint16_t bar_width = 40;

    // Battery bar color
    uint16_t bar_color = ST77XX_GREEN;
    if (battery_soc < 20) {
        bar_color = ST77XX_RED;
    } else if (battery_soc < 50) {
        bar_color = ST77XX_ORANGE;
    }

    // Draw battery bar
    uint16_t filled_width = (uint16_t)(battery_soc / 100.0f * bar_width);
    m_canvas->fillRect(2, bar_y, filled_width, bar_height, bar_color);
    m_canvas->drawRect(2, bar_y, bar_width, bar_height, ST77XX_WHITE);

    // Battery percentage
    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), "%.0f%%", battery_soc);
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setCursor(45, bar_y + 1);
    m_canvas->print(pct_str);

    // GPS time (no String allocations!)
    char time_str[12];
    if (gps_valid) {
        snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", gps_hour, gps_minute, gps_second);
        m_canvas->setTextColor(ST77XX_CYAN);
    } else {
        snprintf(time_str, sizeof(time_str), "--:--:--");
        m_canvas->setTextColor(ST77XX_YELLOW);
    }
    m_canvas->setCursor(75, bar_y + 1);
    m_canvas->print(time_str);

    // Sampling Hz (clamped)
    if (!isfinite(sample_hz) || sample_hz < 0.0f) {
        sample_hz = 0.0f;
    } else if (sample_hz > 999.9f) {
        sample_hz = 999.9f;
    }
    char hz_str[12];
    snprintf(hz_str, sizeof(hz_str), "%.1fHz", sample_hz);
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setCursor(160, bar_y + 1);
    m_canvas->print(hz_str);

    // ========== SINGLE SPI TRANSFER: PUSH CANVAS TO DISPLAY ==========
    // This is the ONLY SPI transaction - all rendering happened in PSRAM
    m_tft->drawRGBBitmap(0, 0, m_canvas->getBuffer(), 240, 135);
}

void ST7789Display::cycle_display_mode() {
    DisplayMode next_mode;
    
    switch (m_current_mode) {
        case DisplayMode::MAIN_SCREEN:
            next_mode = DisplayMode::INFO_SCREEN;
            Serial.println("[Display] Switching to INFO screen");
            break;
        case DisplayMode::INFO_SCREEN:
            next_mode = DisplayMode::DARK;
            Serial.println("[Display] Switching to DARK mode");
            break;
        case DisplayMode::DARK:
            next_mode = DisplayMode::MAIN_SCREEN;
            Serial.println("[Display] Switching to MAIN screen");
            break;
        default:
            next_mode = DisplayMode::MAIN_SCREEN;
            break;
    }
    
    set_display_mode(next_mode);
}

void ST7789Display::set_display_mode(DisplayMode mode) {
    m_current_mode = mode;

    if (mode == DisplayMode::DARK) {
        // Turn off display and backlight
        if (m_canvas != nullptr) {
            m_canvas->fillScreen(ST77XX_BLACK);
            m_tft->drawRGBBitmap(0, 0, m_canvas->getBuffer(), 240, 135);
        }
        digitalWrite(TFT_BACKLITE, LOW);
    } else {
        // Make sure backlight is on
        digitalWrite(TFT_BACKLITE, HIGH);
    }
}

DisplayMode ST7789Display::get_display_mode() {
    return m_current_mode;
}

void ST7789Display::show_info_screen(const char* ip_address, const char* ble_name) {
    if (!m_initialized || m_tft == nullptr || m_canvas == nullptr) return;

    // Render to canvas
    m_canvas->fillScreen(ST77XX_BLACK);

    // Title
    m_canvas->setTextColor(ST77XX_CYAN);
    m_canvas->setTextSize(2);
    m_canvas->setCursor(5, 5);
    m_canvas->println("NETWORK INFO");

    // IP Address
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setTextSize(1);
    m_canvas->setCursor(5, 30);
    m_canvas->println("IP Address:");
    m_canvas->setTextColor(ST77XX_YELLOW);
    m_canvas->setCursor(5, 40);
    if (ip_address != nullptr && ip_address[0] != '\0') {
        m_canvas->println(ip_address);
    } else {
        m_canvas->println("Not available");
    }

    // BLE Name
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setCursor(5, 60);
    m_canvas->println("BLE Device:");
    m_canvas->setTextColor(ST77XX_GREEN);
    m_canvas->setCursor(5, 70);
    if (ble_name != nullptr && ble_name[0] != '\0') {
        m_canvas->println(ble_name);
    } else {
        m_canvas->println("Not configured");
    }

    // Footer
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setTextSize(1);
    m_canvas->setCursor(5, 120);
    m_canvas->println("Press D1 to cycle");

    // Push to display
    m_tft->drawRGBBitmap(0, 0, m_canvas->getBuffer(), 240, 135);
}

void ST7789Display::show_shutdown_screen(uint32_t seconds_remaining) {
    if (!m_initialized || m_tft == nullptr || m_canvas == nullptr) return;

    // Render to canvas
    m_canvas->fillScreen(ST77XX_BLACK);

    // Title
    m_canvas->setTextColor(ST77XX_MAGENTA);
    m_canvas->setTextSize(2);
    m_canvas->setCursor(20, 10);
    m_canvas->println("POWER LOSS");

    // Warning icon (simple exclamation)
    m_canvas->setTextColor(ST77XX_YELLOW);
    m_canvas->setTextSize(4);
    m_canvas->setCursor(100, 35);
    m_canvas->println("!");

    // Shutdown message
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setTextSize(1);
    m_canvas->setCursor(15, 75);
    m_canvas->println("USB power disconnected");

    // Countdown
    m_canvas->setTextColor(ST77XX_CYAN);
    m_canvas->setTextSize(2);
    m_canvas->setCursor(40, 95);
    char countdown[32];
    snprintf(countdown, sizeof(countdown), "Shutdown: %us", seconds_remaining);
    m_canvas->println(countdown);

    // Footer
    m_canvas->setTextColor(ST77XX_GREEN);
    m_canvas->setTextSize(1);
    m_canvas->setCursor(10, 120);
    m_canvas->println("Connect USB to cancel");

    // Push to display
    m_tft->drawRGBBitmap(0, 0, m_canvas->getBuffer(), 240, 135);
}

void ST7789Display::show_splash_screen(const char* version_string, const char* commit_sha,
                                      const char* branch, const char* build_time) {
    if (!m_initialized || m_tft == nullptr || m_canvas == nullptr) return;

    // Render to canvas
    m_canvas->fillScreen(ST77XX_BLACK);

    // Title - Project Name
    m_canvas->setTextColor(ST77XX_CYAN);
    m_canvas->setTextSize(2);
    m_canvas->setCursor(10, 8);
    m_canvas->println("OpenPony");
    m_canvas->setCursor(10, 28);
    m_canvas->println("Logger");

    // Version/Tag
    m_canvas->setTextColor(ST77XX_GREEN);
    m_canvas->setTextSize(1);
    m_canvas->setCursor(5, 55);
    m_canvas->print("Version: ");
    m_canvas->setTextColor(ST77XX_YELLOW);
    if (version_string != nullptr) {
        // Extract just the tag (e.g., "v0.0.0" from full string)
        const char* tag_start = strstr(version_string, "v");
        if (tag_start) {
            char tag[16];
            sscanf(tag_start, "%15s", tag);
            m_canvas->println(tag);
        } else {
            m_canvas->println(version_string);
        }
    }

    // Commit SHA
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setCursor(5, 70);
    m_canvas->print("Commit: ");
    m_canvas->setTextColor(ST77XX_YELLOW);
    if (commit_sha != nullptr) {
        // Show first 7 characters of SHA
        char short_sha[9];
        snprintf(short_sha, sizeof(short_sha), "%.7s", commit_sha);
        m_canvas->println(short_sha);
    }

    // Branch
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setCursor(5, 85);
    m_canvas->print("Branch: ");
    m_canvas->setTextColor(ST77XX_CYAN);
    if (branch != nullptr) {
        // Truncate long branch names
        char short_branch[25];
        snprintf(short_branch, sizeof(short_branch), "%.24s", branch);
        m_canvas->println(short_branch);
    }

    // Build timestamp
    m_canvas->setTextColor(ST77XX_WHITE);
    m_canvas->setTextSize(1);
    m_canvas->setCursor(5, 105);
    m_canvas->print("Built: ");
    m_canvas->setTextColor(ST77XX_GREEN);
    if (build_time != nullptr) {
        // Show truncated timestamp
        char short_time[21];
        snprintf(short_time, sizeof(short_time), "%.20s", build_time);
        m_canvas->println(short_time);
    }

    // Footer - License
    m_canvas->setTextColor(ST77XX_MAGENTA);
    m_canvas->setTextSize(1);
    m_canvas->setCursor(5, 125);
    m_canvas->println("MIT License - Open Source");

    // Push to display
    m_tft->drawRGBBitmap(0, 0, m_canvas->getBuffer(), 240, 135);
}

// ============================================================================
// NeoPixel Status Indicator Implementation
// ============================================================================

#define NEOPIXEL_PIN 33
#define NEOPIXEL_COUNT 1

// Static member initialization
Adafruit_NeoPixel* NeoPixelStatus::m_pixel = nullptr;
NeoPixelStatus::State NeoPixelStatus::m_current_state = NeoPixelStatus::State::BOOTING;
uint32_t NeoPixelStatus::m_last_flash_time = 0;
bool NeoPixelStatus::m_pixel_on = false;
bool NeoPixelStatus::m_initialized = false;
bool NeoPixelStatus::m_enabled = true;  // Start enabled

bool NeoPixelStatus::init() {
    if (m_initialized) {
        return true;
    }
    
    Serial.println("[NeoPixel] Initializing built-in NeoPixel (GPIO33)...");
    
    m_pixel = new Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    
    if (m_pixel == nullptr) {
        Serial.println("[NeoPixel] ERROR: Failed to allocate NeoPixel object!");
        return false;
    }
    
    m_pixel->begin();
    m_pixel->show();
    
    // Start with red (booting state)
    setState(State::BOOTING);
    
    m_initialized = true;
    Serial.println("[NeoPixel] NeoPixel initialization complete!");
    return true;
}

void NeoPixelStatus::setState(State state) {
    if (!m_initialized || m_pixel == nullptr) {
        return;
    }
    
    // Only update if state actually changed (prevents debug spam)
    if (m_current_state == state) {
        return;
    }
    
    m_current_state = state;
    m_last_flash_time = millis();
    m_pixel_on = true;  // Start with pixel on for flashing states
    
    switch (state) {
        case State::BOOTING:
            // Red (solid)
            m_pixel->setPixelColor(0, m_pixel->Color(255, 0, 0));
            m_pixel->show();
            Serial.println("[NeoPixel] State: BOOTING (Red)");
            break;
            
        case State::NO_GPS_FIX:
            // Yellow (1Hz flash)
            m_pixel->setPixelColor(0, m_pixel->Color(255, 255, 0));
            m_pixel->show();
            Serial.println("[NeoPixel] State: NO_GPS_FIX (Yellow 1Hz flash)");
            break;
            
        case State::GPS_3D_FIX:
            // Green (solid)
            m_pixel->setPixelColor(0, m_pixel->Color(0, 255, 0));
            m_pixel->show();
            Serial.println("[NeoPixel] State: GPS_3D_FIX (Green)");
            break;
            
        case State::PAUSED:
            // Yellow (0.2Hz slow flash)
            m_pixel->setPixelColor(0, m_pixel->Color(255, 255, 0));
            m_pixel->show();
            Serial.println("[NeoPixel] State: PAUSED (Yellow 0.2Hz flash)");
            break;
            
        case State::SHUTDOWN:
            // Purple (pulsing)
            m_pixel->setPixelColor(0, m_pixel->Color(128, 0, 128));
            m_pixel->show();
            Serial.println("[NeoPixel] State: SHUTDOWN (Purple pulsing)");
            break;
    }
}

void NeoPixelStatus::update(uint32_t current_ms) {
    if (!m_initialized || m_pixel == nullptr || !m_enabled) {
        return;
    }
    
    // Handle pulsing for SHUTDOWN state (purple breathing effect)
    if (m_current_state == State::SHUTDOWN) {
        // Pulse interval: 1 second (500ms up, 500ms down)
        uint32_t pulse_time = (current_ms - m_last_flash_time) % 1000;
        uint8_t brightness;
        
        if (pulse_time < 500) {
            // Fade up
            brightness = map(pulse_time, 0, 500, 30, 255);
        } else {
            // Fade down
            brightness = map(pulse_time, 500, 1000, 255, 30);
        }
        
        // Apply purple color with breathing brightness
        m_pixel->setPixelColor(0, m_pixel->Color(brightness/2, 0, brightness/2));
        m_pixel->show();
        return;
    }
    
    // Only handle flashing for states that flash
    if (m_current_state != State::NO_GPS_FIX && m_current_state != State::PAUSED) {
        return;
    }
    
    // Determine which flash interval to use based on state
    uint32_t flash_interval = (m_current_state == State::PAUSED) 
                              ? FLASH_INTERVAL_0P2HZ_MS 
                              : FLASH_INTERVAL_1HZ_MS;
    
    // Check if it's time to toggle the flash
    if (current_ms - m_last_flash_time >= flash_interval) {
        m_last_flash_time = current_ms;
        m_pixel_on = !m_pixel_on;
        
        if (m_pixel_on) {
            // Turn on yellow
            m_pixel->setPixelColor(0, m_pixel->Color(255, 255, 0));
        } else {
            // Turn off (black)
            m_pixel->setPixelColor(0, m_pixel->Color(0, 0, 0));
        }
        
        m_pixel->show();
    }
}

void NeoPixelStatus::deinit() {
    if (m_pixel != nullptr) {
        m_pixel->clear();
        m_pixel->show();
        delete m_pixel;
        m_pixel = nullptr;
    }
    m_initialized = false;
    Serial.println("[NeoPixel] NeoPixel deinitialized");
}

void NeoPixelStatus::set_enabled(bool enabled) {
    m_enabled = enabled;
    
    if (!m_initialized || m_pixel == nullptr) {
        return;
    }
    
    if (enabled) {
        Serial.println("[NeoPixel] NeoPixel ENABLED");
        // Restore the current state
        setState(m_current_state);
    } else {
        Serial.println("[NeoPixel] NeoPixel DISABLED");
        // Turn off the pixel
        m_pixel->setPixelColor(0, m_pixel->Color(0, 0, 0));
        m_pixel->show();
    }
}

bool NeoPixelStatus::is_enabled() {
    return m_enabled;
}

