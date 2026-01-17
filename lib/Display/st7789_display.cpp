#include "st7789_display.h"
#include "display_labels.h"
#include "../../../lib/Logger/include/units_helper.h"
#include <SPI.h>
#include <cstdio>

// ST7789 pins for Adafruit ESP32-S3 Feather Reverse TFT
#define TFT_CS    42
#define TFT_DC    40
#define TFT_RST   41
#define TFT_BACKLITE 45
#define TFT_I2C_POWER 7

// Static member initialization
Adafruit_ST7789* ST7789Display::m_tft = nullptr;
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
    
    // Step 8: Draw test pattern
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
                          bool gps_valid, uint32_t sample_count,
                          bool is_paused,
                          double gps_latitude,
                          double gps_longitude,
                          double gps_altitude,
                          uint8_t gps_hour,
                          uint8_t gps_minute,
                          uint8_t gps_second,
                          float gps_speed) {
    if (!m_initialized || m_tft == nullptr) return;
    
    // Clear screen to dark background
    m_tft->fillScreen(ST77XX_BLACK);
    delayMicroseconds(100);  // Small delay to ensure display processes the clear
    
    // Pre-clear all text rows to prevent ghosting
    m_tft->fillRect(0, 0, 240, 22, ST77XX_BLACK);     // Row 1 (time/samples)
    m_tft->fillRect(0, 25, 240, 20, ST77XX_BLACK);    // Row 2 (accel)
    m_tft->fillRect(0, 45, 240, 20, ST77XX_BLACK);    // Row 3 (gyro)
    m_tft->fillRect(0, 65, 240, 22, ST77XX_BLACK);    // Row 4 (GPS coords)
    m_tft->fillRect(0, 85, 240, 20, ST77XX_BLACK);    // Row 5 (speed)
    
    // Calculate uptime
    uint32_t uptime_sec = uptime_ms / 1000;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec / 60) % 60;
    uint32_t seconds = uptime_sec % 60;
    
    // ========== ROW 1: TIME & SAMPLE COUNT ==========
    char timestr[16];
    snprintf(timestr, sizeof(timestr), "%u:%02u:%02u", hours, minutes, seconds);
    DisplayLabel::draw(m_tft, timestr, 2, 2, ST77XX_CYAN, ST77XX_BLACK, 2);
    
    // Sample count on right side with logging state symbol
    char sampstr[32];
    if (sample_count >= 1000000) {
        snprintf(sampstr, sizeof(sampstr), "%.1fM%s", sample_count / 1000000.0f, is_paused ? "⏸" : "●");
    } else if (sample_count >= 1000) {
        snprintf(sampstr, sizeof(sampstr), "%.1fK%s", sample_count / 1000.0f, is_paused ? "⏸" : "●");
    } else {
        snprintf(sampstr, sizeof(sampstr), "%u%s", sample_count, is_paused ? "⏸" : "●");
    }
    DisplayLabel::draw(m_tft, sampstr, 140, 5, ST77XX_YELLOW, ST77XX_BLACK, 2);
    
    // ========== ROW 2: ACCELEROMETER DATA (LARGER) ==========
    char accel_line[40];
    snprintf(accel_line, sizeof(accel_line), "A:%+.2f %+.2f %+.2f", accel_x, accel_y, accel_z);
    DisplayLabel::draw(m_tft, accel_line, 2, 28, ST77XX_WHITE, ST77XX_BLACK, 2);
    
    // ========== ROW 3: GYROSCOPE DATA (LARGER) ==========
    char gyro_line[40];
    snprintf(gyro_line, sizeof(gyro_line), "G:%+.1f %+.1f %+.1f", gyro_x, gyro_y, gyro_z);
    DisplayLabel::draw(m_tft, gyro_line, 2, 48, ST77XX_WHITE, ST77XX_BLACK, 2);
    
    // ========== ROW 4: GPS COORDINATES (LATITUDE LONGITUDE ALTITUDE) ==========
    if (gps_valid) {
        char gps_line[40];
        snprintf(gps_line, sizeof(gps_line), "%+6.1f %+7.1f %5.0fm", 
                 gps_latitude, gps_longitude, gps_altitude);
        DisplayLabel::draw(m_tft, gps_line, 2, 68, ST77XX_GREEN, ST77XX_BLACK, 2);
    } else {
        DisplayLabel::draw(m_tft, "No GPS Fix", 2, 68, ST77XX_RED, ST77XX_BLACK, 2);
    }
    
    // ========== ROW 5: GPS SPEED STATUS ==========
    if (gps_valid) {
        float display_speed = convert_speed(gps_speed);
        char gps_str[32];
        snprintf(gps_str, sizeof(gps_str), "Spd:%.1f%s", display_speed, get_speed_unit());
        DisplayLabel::draw(m_tft, gps_str, 2, 88, ST77XX_GREEN, ST77XX_BLACK, 2);
    } else {
        DisplayLabel::draw(m_tft, "GPS Waiting", 2, 88, ST77XX_YELLOW, ST77XX_BLACK, 2);
    }
    
    // ========== BOTTOM: GPS TIME & BATTERY INDICATOR ==========
    m_tft->setTextSize(1);
    m_tft->setTextColor(ST77XX_WHITE);
    
    uint16_t bar_height = 6;
    uint16_t bar_y = m_tft->height() - bar_height - 4;
    uint16_t bar_width = 40;  // Fixed small width (5 chars wide-ish)
    
    // Battery bar color
    uint16_t bar_color = ST77XX_GREEN;
    if (battery_soc < 20) {
        bar_color = ST77XX_RED;
    } else if (battery_soc < 50) {
        bar_color = ST77XX_ORANGE;
    }
    
    // Calculate filled portion
    uint16_t filled_width = (uint16_t)(battery_soc / 100.0f * bar_width);
    
    // Draw filled portion
    m_tft->fillRect(2, bar_y, filled_width, bar_height, bar_color);
    
    // Draw border
    m_tft->drawRect(2, bar_y, bar_width, bar_height, ST77XX_WHITE);
    
    // Display percentage next to bar
    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), "%.0f%%", battery_soc);
    DisplayLabel::draw(m_tft, pct_str, 45, bar_y + 1, ST77XX_WHITE, ST77XX_BLACK, 1);
    
    // Display GPS time on the right side (if valid)
    if (gps_valid) {
        // Build time string
        String time_str = String(gps_hour < 10 ? "0" : "") + String(gps_hour) + ":" +
                         String(gps_minute < 10 ? "0" : "") + String(gps_minute) + ":" +
                         String(gps_second < 10 ? "0" : "") + String(gps_second);
        DisplayLabel::draw(m_tft, time_str.c_str(), 75, bar_y + 1, ST77XX_CYAN, ST77XX_BLACK, 1);
    } else {
        DisplayLabel::draw(m_tft, "--:--:--", 75, bar_y + 1, ST77XX_YELLOW, ST77XX_BLACK, 1);
    }
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
        m_tft->fillScreen(ST77XX_BLACK);
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
    if (!m_initialized || m_tft == nullptr) return;
    
    // Clear screen
    m_tft->fillScreen(ST77XX_BLACK);
    
    // Title
    m_tft->setTextColor(ST77XX_CYAN);
    m_tft->setTextSize(2);
    m_tft->setCursor(5, 5);
    m_tft->println("NETWORK INFO");
    
    // IP Address
    m_tft->setTextColor(ST77XX_WHITE);
    m_tft->setTextSize(1);
    m_tft->setCursor(5, 30);
    m_tft->println("IP Address:");
    m_tft->setTextColor(ST77XX_YELLOW);
    m_tft->setCursor(5, 40);
    if (ip_address != nullptr && ip_address[0] != '\0') {
        m_tft->println(ip_address);
    } else {
        m_tft->println("Not available");
    }
    
    // BLE Name
    m_tft->setTextColor(ST77XX_WHITE);
    m_tft->setCursor(5, 60);
    m_tft->println("BLE Device:");
    m_tft->setTextColor(ST77XX_GREEN);
    m_tft->setCursor(5, 70);
    if (ble_name != nullptr && ble_name[0] != '\0') {
        m_tft->println(ble_name);
    } else {
        m_tft->println("Not configured");
    }
    
    // Footer
    m_tft->setTextColor(ST77XX_WHITE);
    m_tft->setTextSize(1);
    m_tft->setCursor(5, 120);
    m_tft->println("Press D1 to cycle");
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
    }
}

void NeoPixelStatus::update(uint32_t current_ms) {
    if (!m_initialized || m_pixel == nullptr || !m_enabled) {
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

