#include "display_labels.h"
#include <cstring>

void DisplayLabel::draw(Adafruit_ST7789* tft,
                       const char* text,
                       int16_t x, int16_t y,
                       uint16_t text_color,
                       uint16_t bg_color,
                       uint8_t text_size) {
    if (!tft || !text) return;

    // Calculate approximate text dimensions to clear the right area
    int16_t char_width = 6 * text_size;
    int16_t char_height = 8 * text_size;
    int16_t text_width = strlen(text) * char_width;
    
    // Clear the area first with a bit of padding
    tft->fillRect(x, y, text_width + 10, char_height + 2, bg_color);
    
    // Now draw the text with background color
    tft->setTextSize(text_size);
    tft->setTextColor(text_color, bg_color);
    tft->setCursor(x, y);
    tft->print(text);
}

int16_t DisplayLabel::draw_and_measure(Adafruit_ST7789* tft,
                                       const char* text,
                                       int16_t x, int16_t y,
                                       uint16_t text_color,
                                       uint16_t bg_color,
                                       uint8_t text_size) {
    if (!tft || !text) return 0;

    // Calculate approximate text dimensions
    int16_t char_width = 6 * text_size;
    int16_t char_height = 8 * text_size;
    int16_t text_width = strlen(text) * char_width;
    
    // Clear the area first with padding
    tft->fillRect(x, y, text_width + 10, char_height + 2, bg_color);
    
    // Draw the text
    tft->setTextSize(text_size);
    tft->setTextColor(text_color, bg_color);
    tft->setCursor(x, y);
    tft->print(text);

    return text_width;
}

void DisplayLabel::clear_area(Adafruit_ST7789* tft,
                             int16_t x, int16_t y,
                             int16_t w, int16_t h) {
    if (tft) {
        tft->fillRect(x, y, w, h, ST77XX_BLACK);
    }
}
