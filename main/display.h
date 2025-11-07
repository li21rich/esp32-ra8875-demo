#pragma once

#include <stdint.h>
#include <stdbool.h>


typedef enum {
    SCREEN_MAIN_NO_LAPS = 0,
    SCREEN_MAIN_LAPS = 1,
    SCREEN_DEBUG_RTD = 2,
    SCREEN_DEBUG_NO_RTD = 3,
    SCREEN_WARN = 4
    
} Screen_t;

extern Screen_t CURRENT_SCREEN;

typedef enum {
    DISPLAY_FONT_INTERNAL,
    DISPLAY_FONT_COMIC_SANS
} DisplayFont_t;

typedef struct {
    uint16_t x1, y1, x2, y2;
} LineSpec;

// Initialization
void Display_Init(void);

// Display screens
void Display_SwitchScreen(Screen_t nextScreen); 

// Write Mode Switching
void Display_EnableDrawMode(void);
void Display_EnableTextModeAndFont(DisplayFont_t fontType);

// Drawing and Text
void Display_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, bool filled);
void Display_WriteTextAt(uint16_t x, uint16_t y, const char* msg);
void Display_WriteNumberAt(uint16_t x, uint16_t y, bool isWholeNumber, float value, bool hasManyDigits);