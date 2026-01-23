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

// Display dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 135

// Static member initialization
Adafruit_ST7789* ST7789Display::m_tft = nullptr;
uint16_t* ST7789Display::m_framebuffer = nullptr;
bool ST7789Display::m_initialized = false;
DisplayMode ST7789Display::m_current_mode = DisplayMode::MAIN_SCREEN;

// ============================================================================
// Framebuffer Helper Functions - Direct PSRAM buffer manipulation
// ============================================================================

void ST7789Display::fb_clear(uint16_t color) {
    if (m_framebuffer == nullptr) return;

    if (color == 0x0000) {
        memset(m_framebuffer, 0, TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t));
    } else {
        for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
            m_framebuffer[i] = color;
        }
    }
}

void ST7789Display::fb_setPixel(int16_t x, int16_t y, uint16_t color) {
    if (m_framebuffer == nullptr || x < 0 || y < 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    m_framebuffer[y * TFT_WIDTH + x] = color;
}

void ST7789Display::fb_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (m_framebuffer == nullptr) return;

    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            int16_t px = x + i;
            int16_t py = y + j;
            if (px >= 0 && px < TFT_WIDTH && py >= 0 && py < TFT_HEIGHT) {
                m_framebuffer[py * TFT_WIDTH + px] = color;
            }
        }
    }
}

void ST7789Display::fb_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (m_framebuffer == nullptr) return;

    // Top and bottom
    for (int16_t i = 0; i < w; i++) {
        fb_setPixel(x + i, y, color);
        fb_setPixel(x + i, y + h - 1, color);
    }
    // Left and right
    for (int16_t j = 0; j < h; j++) {
        fb_setPixel(x, y + j, color);
        fb_setPixel(x + w - 1, y + j, color);
    }
}

void ST7789Display::fb_drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint8_t size) {
    // Use TFT's built-in font drawing directly to framebuffer
    // This is a simplified version - for now, we'll use TFT draw directly instead of framebuffer
}

void ST7789Display::fb_print(int16_t x, int16_t y, const char* str, uint16_t color, uint8_t size) {
    // For text, we'll render directly to TFT after framebuffer transfer
    // This is a limitation of not having GFX canvas
}

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
    Serial.println("[TFT] Allocating framebuffer (240x135x2 = 64,800 bytes) ONLY in PSRAM...");
    size_t framebuffer_size = TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t);  // 64,800 bytes

    Serial.printf("[TFT] Free DRAM before: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    Serial.printf("[TFT] Free PSRAM before: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Allocate raw framebuffer in PSRAM ONLY (no heap allocator, no object wrapper)
    m_framebuffer = (uint16_t*)heap_caps_malloc(framebuffer_size, MALLOC_CAP_SPIRAM);
    if (m_framebuffer == nullptr) {
        Serial.println("[TFT] ERROR: Failed to allocate framebuffer in PSRAM!");
        return false;
    }

    // Clear framebuffer
    memset(m_framebuffer, 0, framebuffer_size);

    Serial.printf("[TFT] Framebuffer allocated: %u bytes in PSRAM\n", framebuffer_size);
    Serial.printf("[TFT] Free DRAM after: %u bytes (MUST be unchanged!)\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
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
    // TEMPORARILY DISABLED - need to reimplement rendering without GFXcanvas
    // TODO: Rewrite rendering to use framebuffer directly or TFT functions
    (void)uptime_ms; (void)temp; (void)accel_x; (void)accel_y; (void)accel_z;
    (void)gyro_x; (void)gyro_y; (void)gyro_z; (void)battery_soc; (void)battery_voltage;
    (void)gps_valid; (void)sample_count; (void)sample_hz; (void)is_paused;
    (void)gps_latitude; (void)gps_longitude; (void)gps_altitude;
    (void)gps_hour; (void)gps_minute; (void)gps_second; (void)gps_speed;
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
        if (m_tft != nullptr) {
            m_tft->fillScreen(ST77XX_BLACK);
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
    if (!m_initialized || m_tft == nullptr || m_framebuffer == nullptr) return;
    // TEMPORARILY DISABLED
    (void)ip_address; (void)ble_name;
}

void ST7789Display::show_shutdown_screen(uint32_t seconds_remaining) {
    if (!m_initialized || m_tft == nullptr || m_framebuffer == nullptr) return;
    return; // TEMPORARILY DISABLED

}

void ST7789Display::show_splash_screen(const char* version_string, const char* commit_sha,
                                      const char* branch, const char* build_time) {
    if (!m_initialized || m_tft == nullptr || m_framebuffer == nullptr) return;
    return; // TEMPORARILY DISABLED

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

