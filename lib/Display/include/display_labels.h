#ifndef DISPLAY_LABELS_H
#define DISPLAY_LABELS_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

/**
 * @brief Simple label manager for Adafruit displays
 * Handles text rendering with automatic background clearing
 */
class DisplayLabel {
public:
    /**
     * @brief Draw a label with text color and automatic background
     * @param tft Pointer to Adafruit_ST7789 display
     * @param text The text to display
     * @param x X coordinate
     * @param y Y coordinate
     * @param text_color Color of the text
     * @param bg_color Background color (ST77XX_BLACK for transparent)
     * @param text_size Font size (1-5)
     */
    static void draw(Adafruit_ST7789* tft,
                    const char* text,
                    int16_t x, int16_t y,
                    uint16_t text_color,
                    uint16_t bg_color = ST77XX_BLACK,
                    uint8_t text_size = 1);

    /**
     * @brief Draw a label and return its width for alignment
     * @return Width of rendered text in pixels
     */
    static int16_t draw_and_measure(Adafruit_ST7789* tft,
                                    const char* text,
                                    int16_t x, int16_t y,
                                    uint16_t text_color,
                                    uint16_t bg_color = ST77XX_BLACK,
                                    uint8_t text_size = 1);

    /**
     * @brief Clear a rectangular area
     */
    static void clear_area(Adafruit_ST7789* tft,
                          int16_t x, int16_t y,
                          int16_t w, int16_t h);
};

#endif // DISPLAY_LABELS_H
