#include "st7789_display.h"
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
    
    Serial.println("[TFT] Setting rotation to 3 (landscape)...");
    m_tft->setRotation(3);
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
                          float gps_speed) {
    if (!m_initialized || m_tft == nullptr) return;
    
    // Clear screen to dark background
    m_tft->fillScreen(ST77XX_BLACK);
    
    // Calculate uptime
    uint32_t uptime_sec = uptime_ms / 1000;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec / 60) % 60;
    uint32_t seconds = uptime_sec % 60;
    
    // ========== ROW 1: TIME & SAMPLE COUNT ==========
    m_tft->setTextColor(ST77XX_CYAN);
    m_tft->setTextSize(2);
    m_tft->setCursor(2, 2);
    
    char timestr[16];
    snprintf(timestr, sizeof(timestr), "%u:%02u:%02u", hours, minutes, seconds);
    m_tft->println(timestr);
    
    // Sample count on right side
    m_tft->setTextColor(ST77XX_YELLOW);
    m_tft->setCursor(140, 5);
    
    // Scale samples with K (thousands) or M (millions)
    char sampstr[12];
    if (sample_count >= 1000000) {
        snprintf(sampstr, sizeof(sampstr), "%.1fM", sample_count / 1000000.0f);
    } else if (sample_count >= 1000) {
        snprintf(sampstr, sizeof(sampstr), "%.1fK", sample_count / 1000.0f);
    } else {
        snprintf(sampstr, sizeof(sampstr), "%u", sample_count);
    }
    m_tft->println(sampstr);
    
    // ========== ROW 2: ACCELEROMETER DATA (LARGER) ==========
    m_tft->setTextColor(ST77XX_WHITE);
    m_tft->setTextSize(2);
    m_tft->setCursor(2, 28);
    
    char accel_line[40];
    snprintf(accel_line, sizeof(accel_line), "A:%+.2f %+.2f %+.2f", accel_x, accel_y, accel_z);
    m_tft->println(accel_line);
    
    // ========== ROW 3: GYROSCOPE DATA (LARGER) ==========
    m_tft->setCursor(2, 48);
    
    char gyro_line[40];
    snprintf(gyro_line, sizeof(gyro_line), "G:%+.1f,%+.1f,%+.1f", gyro_x, gyro_y, gyro_z);
    m_tft->println(gyro_line);
    
    // ========== ROW 4: TEMPERATURE & BATTERY % (LARGER) ==========
    float display_temp = convert_temperature(temp);
    
    m_tft->setCursor(2, 68);
    m_tft->setTextColor(ST77XX_WHITE);
    
    // Color temperature based on value
    uint16_t temp_color = ST77XX_CYAN;
    if (display_temp > 85) {
        temp_color = ST77XX_RED;
    } else if (display_temp > 75) {
        temp_color = ST77XX_ORANGE;
    }
    
    char temp_str[20];
    snprintf(temp_str, sizeof(temp_str), "T:%+.1f%s", display_temp, get_temp_unit());
    m_tft->setTextColor(temp_color);
    m_tft->print(temp_str);
    
    // Battery percentage (larger)
    uint16_t batt_color = ST77XX_GREEN;
    if (battery_soc < 20) {
        batt_color = ST77XX_RED;
    } else if (battery_soc < 50) {
        batt_color = ST77XX_ORANGE;
    }
    m_tft->setTextColor(batt_color);
    m_tft->print("  B:");
    
    char batt_str[10];
    snprintf(batt_str, sizeof(batt_str), "%.0f%%", battery_soc);
    m_tft->println(batt_str);
    
    // ========== ROW 5: GPS STATUS (Large) ==========
    m_tft->setTextSize(2);
    m_tft->setCursor(2, 88);
    
    if (gps_valid) {
        m_tft->setTextColor(ST77XX_GREEN);
        float display_speed = convert_speed(gps_speed);
        char gps_str[20];
        snprintf(gps_str, sizeof(gps_str), "GPS:%.1f%s", display_speed, get_speed_unit());
        m_tft->println(gps_str);
    } else {
        m_tft->setTextColor(ST77XX_RED);
        m_tft->println("GPS:NO FIX");
    }
    
    // ========== BOTTOM: BATTERY INDICATOR & CHARGE STATUS ==========
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
    m_tft->setCursor(45, bar_y + 1);
    m_tft->setTextColor(ST77XX_WHITE);
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%.0f%%", battery_soc);
    m_tft->print(pct_str);
    
    // Charging indicator (lightning bolt or +/- symbol)
    // Assuming discharging by default; could use battery current if available
    m_tft->setCursor(75, bar_y + 1);
    if (battery_soc < 100) {
        m_tft->setTextColor(ST77XX_YELLOW);
        m_tft->print("↓");  // Discharging
    } else {
        m_tft->setTextColor(ST77XX_CYAN);
        m_tft->print("~");  // Full/Idle
    }
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
            // Yellow (will flash)
            m_pixel->setPixelColor(0, m_pixel->Color(255, 255, 0));
            m_pixel->show();
            Serial.println("[NeoPixel] State: NO_GPS_FIX (Yellow flashing)");
            break;
            
        case State::GPS_3D_FIX:
            // Green (solid)
            m_pixel->setPixelColor(0, m_pixel->Color(0, 255, 0));
            m_pixel->show();
            Serial.println("[NeoPixel] State: GPS_3D_FIX (Green)");
            break;
    }
}

void NeoPixelStatus::update(uint32_t current_ms) {
    if (!m_initialized || m_pixel == nullptr) {
        return;
    }
    
    // Only handle flashing for NO_GPS_FIX state
    if (m_current_state != State::NO_GPS_FIX) {
        return;
    }
    
    // Check if it's time to toggle the flash
    if (current_ms - m_last_flash_time >= FLASH_INTERVAL_MS) {
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

