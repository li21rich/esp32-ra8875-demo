#pragma once

#include <stdint.h>
#include <stdbool.h>


typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_DEBUG = 1
} Screen_t;

extern Screen_t CURRENT_SCREEN;

typedef enum {
    DISPLAY_FONT_INTERNAL,
    DISPLAY_FONT_COMIC_SANS
} DisplayFont_t;

// Initialization
void Display_Init(void);

// Display screens
void Display_RenderMainScreen(void);
void Display_RenderDebugScreen(void);
void Display_SwitchScreen(void); 

// Mode Switching
void Display_EnableDrawMode(void);
void Display_EnableTextModeAndFont(DisplayFont_t fontType);

// Drawing
void Display_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, bool filled);

// Text
void Display_SetInternalFontSize(uint8_t fontSize);
void Display_WriteTextAt(uint16_t x, uint16_t y, const char* msg);
void Display_WriteNumberAt(uint16_t x, uint16_t y, bool isWholeNumber, float value);