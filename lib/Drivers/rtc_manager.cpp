#include "rtc_manager.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <sys/time.h>
#include <Arduino.h>

// PCF8523 Register Addresses
static const uint8_t PCF8523_REG_CTRL1 = 0x00;
static const uint8_t PCF8523_REG_SECONDS = 0x01;
static const uint8_t PCF8523_REG_MINUTES = 0x02;
static const uint8_t PCF8523_REG_HOURS = 0x03;
static const uint8_t PCF8523_REG_DAYS = 0x04;
static const uint8_t PCF8523_REG_WEEKDAYS = 0x05;
static const uint8_t PCF8523_REG_MONTHS = 0x06;
static const uint8_t PCF8523_REG_YEARS = 0x07;

RTCManager::RTCManager() : m_wire(nullptr), m_available(false) {}

bool RTCManager::init(TwoWire& wire) {
    m_wire = &wire;
    
    // Try to read from PCF8523
    m_wire->beginTransmission(I2C_ADDRESS);
    m_wire->write(PCF8523_REG_CTRL1);
    if (m_wire->endTransmission() != 0) {
        Serial.println("[RTC] PCF8523 not detected at 0x68");
        m_available = false;
        return false;
    }
    
    // Read register to confirm communication
    m_wire->requestFrom(I2C_ADDRESS, 1);
    if (m_wire->available() < 1) {
        Serial.println("[RTC] PCF8523 communication failed");
        m_available = false;
        return false;
    }
    
    uint8_t ctrl1 = m_wire->read();
    Serial.printf("[RTC] PCF8523 detected at 0x68, CTRL1=0x%02X\n", ctrl1);
    
    m_available = true;
    return true;
}

uint8_t RTCManager::bcd_to_decimal(uint8_t bcd) {
    return ((bcd & 0xF0) >> 4) * 10 + (bcd & 0x0F);
}

uint8_t RTCManager::decimal_to_bcd(uint8_t decimal) {
    return ((decimal / 10) << 4) | (decimal % 10);
}

time_t RTCManager::read_time() {
    if (!m_available || !m_wire) {
        return 0;
    }
    
    // Read 7 bytes: seconds through years
    m_wire->beginTransmission(I2C_ADDRESS);
    m_wire->write(PCF8523_REG_SECONDS);
    if (m_wire->endTransmission() != 0) {
        return 0;
    }
    
    m_wire->requestFrom(I2C_ADDRESS, 7);
    if (m_wire->available() < 7) {
        return 0;
    }
    
    uint8_t seconds = bcd_to_decimal(m_wire->read() & 0x7F);
    uint8_t minutes = bcd_to_decimal(m_wire->read() & 0x7F);
    uint8_t hours = bcd_to_decimal(m_wire->read() & 0x3F);
    uint8_t days = bcd_to_decimal(m_wire->read() & 0x3F);
    m_wire->read();  // weekday, skip
    uint8_t months = bcd_to_decimal(m_wire->read() & 0x1F);
    uint8_t years = bcd_to_decimal(m_wire->read());
    
    // Convert to Unix timestamp
    tm time_struct;
    time_struct.tm_sec = seconds;
    time_struct.tm_min = minutes;
    time_struct.tm_hour = hours;
    time_struct.tm_mday = days;
    time_struct.tm_mon = months - 1;  // tm_mon is 0-based
    time_struct.tm_year = years + 100;  // tm_year is years since 1900
    time_struct.tm_isdst = -1;
    
    time_t result = mktime(&time_struct);
    Serial.printf("[RTC] Read time: %04d-%02d-%02d %02d:%02d:%02d (Unix: %ld)\n",
                  years + 2000, months, days, hours, minutes, seconds, result);
    
    return result;
}

bool RTCManager::write_time(long time_val) {
    if (!m_available || !m_wire) {
        return false;
    }
    
    // Convert Unix timestamp to struct tm
    time_t t = static_cast<time_t>(time_val);
    struct tm* time_struct = localtime(&t);
    if (!time_struct) {
        return false;
    }
    
    uint8_t seconds = decimal_to_bcd(time_struct->tm_sec);
    uint8_t minutes = decimal_to_bcd(time_struct->tm_min);
    uint8_t hours = decimal_to_bcd(time_struct->tm_hour);
    uint8_t days = decimal_to_bcd(time_struct->tm_mday);
    uint8_t months = decimal_to_bcd(time_struct->tm_mon + 1);  // tm_mon is 0-based
    uint8_t years = decimal_to_bcd(time_struct->tm_year - 100);  // tm_year is years since 1900
    
    // Write to PCF8523
    m_wire->beginTransmission(I2C_ADDRESS);
    m_wire->write(PCF8523_REG_SECONDS);
    m_wire->write(seconds);
    m_wire->write(minutes);
    m_wire->write(hours);
    m_wire->write(days);
    m_wire->write(0);  // weekday - not critical
    m_wire->write(months);
    m_wire->write(years);
    
    uint8_t result = m_wire->endTransmission();
    
    if (result == 0) {
        Serial.printf("[RTC] Wrote time: %04d-%02d-%02d %02d:%02d:%02d (Unix: %ld)\n",
                      time_struct->tm_year + 1900, time_struct->tm_mon + 1,
                      time_struct->tm_mday, time_struct->tm_hour, time_struct->tm_min,
                      time_struct->tm_sec, time_val);
        return true;
    } else {
        Serial.printf("[RTC] Failed to write time (error: %d)\n", result);
        return false;
    }
}

void RTCManager::restore_system_time_on_boot() {
    time_t boot_time = 0;
    
    // Try 1: PCF8523
    if (m_available) {
        boot_time = read_time();
        if (boot_time > 0) {
            Serial.println("[RTC] Boot time restored from PCF8523");
            timeval tv = {boot_time, 0};
            settimeofday(&tv, nullptr);
            return;
        }
    }
    
    // Try 2: NVS "gps_time"
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("system", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint32_t gps_time = 0;
        err = nvs_get_u32(nvs_handle, "gps_time", &gps_time);
        nvs_close(nvs_handle);
        
        if (err == ESP_OK && gps_time > 0) {
            boot_time = (time_t)gps_time;
            Serial.printf("[RTC] Boot time restored from NVS: %ld\n", boot_time);
            timeval tv = {boot_time, 0};
            settimeofday(&tv, nullptr);
            return;
        }
    }
    
    // Fallback: Use compile-time as default
    Serial.println("[RTC] No RTC or NVS time available, using compile-time default");
    // ESP32-S3 internal RTC will have some default value
}
