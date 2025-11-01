/**
* Author: Richard Li
* Editors: Richard Li
* Additional Notes:
* - See RA8875.h for driver library functions. 
* - Datasheet: https://support.midasdisplays.com/wp-content/uploads/2025/06/RA8875.pdf
*/

#include <time.h> // Only needed for render time feedback
#include "display.h"
#include "RA8875.h"
#include "comicsans_font.h"

// LCD SPI configuration and pin assignments  
#define LCD_SPI_HOST              SPI3_HOST
#define LCD_SPI_SPEED             115200   // 200000 = good, 115200 = safe, 2800000 = too unstable. Maybe test increasing a bit.
#define LCD_PIN_MOSI              13
#define LCD_PIN_MISO              12
#define LCD_PIN_SCLK              11
#define LCD_PIN_CS                6
#define LCD_PIN_RESET             4

// Horizontal and vertical sync + display resolution
#define LCD_HSYNC_NONDISP         26
#define LCD_HSYNC_START           32
#define LCD_HSYNC_PW              96
#define LCD_HSYNC_FINETUNE        0
#define LCD_VSYNC_NONDISP         32
#define LCD_VSYNC_START           23
#define LCD_VSYNC_PW              2
#define LCD_WIDTH                 800
#define LCD_HEIGHT                480
#define LCD_VOFFSET               0
#define LCD_BRIGHTNESS_80_PCT     0xCC // 80 percent because 0xFF hurts my eyes

// RA8875 register addresses  
#define RA8875_REG_FONT_SEL       0x21  // Font mode
#define RA8875_REG_FONT_SIZE      0x22  // Font size
#define RA8875_REG_MODE_CTRL      0x40  // Text vs. Graphic mode
#define RA8875_REG_FG_R           0x63  // Foreground Red
#define RA8875_REG_FG_G           0x64  // Foreground Green
#define RA8875_REG_FG_B           0x65  // Foreground Blue
#define RA8875_REG_CURSOR_X_LOW   0x2A  // Font write cursor X low byte
#define RA8875_REG_CURSOR_X_HIGH  0x2B  // Font write cursor X high byte
#define RA8875_REG_CURSOR_Y_LOW   0x2C  // Font write cursor Y low byte
#define RA8875_REG_CURSOR_Y_HIGH  0x2D  // Font write cursor Y high byte
#define RA8875_REG_FONT_SRC       0x41  // Font source selector

// RA8875 register values
#define RA8875_VAL_MODE_GRAPHIC   0x00  // Graphic mode
#define RA8875_VAL_MODE_TEXT      0x80  // Text mode

// Colors
#define COLOR_WHITE              255   
#define COLOR_GREEN              32
#define COLOR_RED                5

// Misc
#define LABELS_Y_OFFSET          11
#define VALUES_Y_OFFSET          55
#define GLYPH_SCALE              2  
#define FONT_SIZE_TRIPLE         (0x0A | 0x40)  // 3x3 | transparent background 
 
static RA8875_context_t lcd;
static DisplayFont_t currentFont = DISPLAY_FONT_INTERNAL;
static GlyphBuffer glyph_cache[256];
static bool inGraphicMode = false;
static const uint8_t glyphAdvanceComicSans[256] = { // Allows for custom spacing for wider or narrower letters
    [0 ... 255] = 16,  // Defaults to 16
    [' '] = 12, ['l'] = 10, ['i'] = 13, ['o'] = 15, ['r'] = 15, ['g'] = 15, ['a'] = 17, ['M'] = 18, ['G'] = 18,
};

Screen_t CURRENT_SCREEN = SCREEN_MAIN;

static void Display_ForegroundWhite(void) 
{
    RA8875_write_register(&lcd, RA8875_REG_FG_R, 0x07);
    RA8875_write_register(&lcd, RA8875_REG_FG_G, 0x07);
    RA8875_write_register(&lcd, RA8875_REG_FG_B, 0x03); 
}

static void Display_PrecomputeGlyphs(void)
{
    for (int i = 0; i < 256; i++) {
        const Glyph8x16* g = glyphs[i];
        if (!g) continue;

        for (int row = 0; row < 16; row++) {
            uint8_t bits = g->bitmap[row];
            for (int col = 0; col < 8; col++) {
                glyph_cache[i].pixels[row][col] = (bits & (1 << (7 - col))) ? 1 : 0;
            }
        }
    }
}

static void Display_WriteString(const char* msg) 
{
    RA8875_write_command(&lcd, 0x02);

    while (*msg) {
        RA8875_write_data(&lcd, (uint8_t)*msg++);
    }
}

static void Display_SetTextCursor(uint16_t x, uint16_t y) 
{
    RA8875_write_register(&lcd, RA8875_REG_CURSOR_X_LOW, x & 0xFF);
    RA8875_write_register(&lcd, RA8875_REG_CURSOR_X_HIGH, x >> 8);
    RA8875_write_register(&lcd, RA8875_REG_CURSOR_Y_LOW, y & 0xFF);
    RA8875_write_register(&lcd, RA8875_REG_CURSOR_Y_HIGH, y >> 8);
}

static void Display_ResetState(void)
{
    RA8875_clear(&lcd);
    Display_SetTextCursor(0, 0);
    Display_ForegroundWhite();;
}

// Span batching + Caching for faster special font load
static void Display_BlitGlyph(uint16_t x, uint16_t y, const GlyphBuffer* buf)
{
    const uint8_t scale = GLYPH_SCALE;
    Display_ForegroundWhite();

    for (int row = 0; row < 16; row++) {
        uint16_t py1 = y + row * scale;
        uint16_t py2 = py1 + scale - 1;

        int col = 0;
        while (col < 8) {
            // Skip off pixels
            if (!buf->pixels[row][col]) {
                col++;
                continue;
            }

            // Start of span
            int start = col;
            while (col < 8 && buf->pixels[row][col]) col++;

            // End of span
            uint16_t px1 = x + start * scale;
            uint16_t px2 = x + (col - 1) * scale + scale - 1;

            RA8875_draw_rect_fast(&lcd, px1, py1, px2, py2);
        }
    }
}

static void Display_DrawBordersMain(void)
{
    // Horizontal lines
    RA8875_draw_rect(&lcd, 0, 120, 800, 120 + 1, COLOR_WHITE, 1);  
    RA8875_draw_rect(&lcd, 0, 240, 800, 240 + 1, COLOR_WHITE, 1);  
    RA8875_draw_rect(&lcd, 0, 360, 800, 360 + 1, COLOR_WHITE, 1);
    // Vertical lines
    RA8875_draw_rect(&lcd, 200, 0, 200 + 1, 480, COLOR_WHITE, 1);  
    RA8875_draw_rect(&lcd, 400, 120, 400 + 1, 240, COLOR_WHITE, 1);
    RA8875_draw_rect(&lcd, 600, 0, 600 + 1, 480, COLOR_WHITE, 1);
    RA8875_draw_rect(&lcd, 700, 360, 700 + 1, 480, COLOR_WHITE, 1);
}

static void Display_DrawBordersDebug(void)
{
    // Horizontal lines
    RA8875_draw_rect(&lcd, 0, 120, 800, 120 + 1, COLOR_WHITE, 1);  
    RA8875_draw_rect(&lcd, 0, 240, 800, 240 + 1, COLOR_WHITE, 1);  
    RA8875_draw_rect(&lcd, 0, 360, 800, 360 + 1, COLOR_WHITE, 1);  
    // Vertical lines
    RA8875_draw_rect(&lcd, 200, 120, 200 + 1, 480, COLOR_WHITE, 1);  
    RA8875_draw_rect(&lcd, 400, 0, 400 + 1, 480, COLOR_WHITE, 1);
    RA8875_draw_rect(&lcd, 600, 120, 600 + 1, 480, COLOR_WHITE, 1);
}

void Display_Init(void) 
{
    RA8875_init(&lcd, LCD_SPI_HOST, LCD_SPI_SPEED, LCD_PIN_MOSI, 
                LCD_PIN_MISO, LCD_PIN_SCLK, LCD_PIN_CS, LCD_PIN_RESET);    
    RA8875_configure(&lcd, 
                    LCD_HSYNC_NONDISP, LCD_HSYNC_START, LCD_HSYNC_PW, LCD_HSYNC_FINETUNE,
                    LCD_VSYNC_NONDISP, LCD_VSYNC_START, LCD_VSYNC_PW,
                    LCD_WIDTH, LCD_HEIGHT, LCD_VOFFSET);
    RA8875_clear(&lcd);
    RA8875_set_backlight_brightness(&lcd, LCD_BRIGHTNESS_80_PCT); 
    Display_PrecomputeGlyphs();
    Display_SetTextCursor(0, 0);
}

void Display_EnableDrawMode(void) 
{
    if (!inGraphicMode) {
        RA8875_write_register(&lcd, RA8875_REG_MODE_CTRL, RA8875_VAL_MODE_GRAPHIC); // Enable graphic mode if not already
        inGraphicMode = true;
    }
}

void Display_EnableTextModeAndFont(DisplayFont_t fontType) 
{
    currentFont = fontType;
    if (fontType == DISPLAY_FONT_INTERNAL) {
        if(inGraphicMode) {
            RA8875_write_register(&lcd, RA8875_REG_MODE_CTRL, RA8875_VAL_MODE_TEXT);    // Switch to text mode
        }

        Display_ForegroundWhite();
        Display_SetInternalFontSize(FONT_SIZE_TRIPLE); 
    } else if (fontType == DISPLAY_FONT_COMIC_SANS) {
        Display_EnableDrawMode(); // We write comic sans as graphical drawings
    }
}

void Display_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, bool filled) 
{
    RA8875_draw_rect(&lcd, x1, y1, x2, y2, color, filled);
}

void Display_SetInternalFontSize(uint8_t fontSize)
{
    RA8875_write_register(&lcd, RA8875_REG_FONT_SIZE, fontSize);
    RA8875_write_register(&lcd, RA8875_REG_FONT_SRC, 0x00);
    RA8875_write_register(&lcd, RA8875_REG_FONT_SEL, 0x00);
}

void Display_WriteTextAt(uint16_t x, uint16_t y, const char* msg) 
{
    if (currentFont == DISPLAY_FONT_INTERNAL)  {
        Display_SetTextCursor(x, y);
        Display_WriteString(msg);
    } else if (currentFont == DISPLAY_FONT_COMIC_SANS) {
        uint16_t cursorX = x;

        while (*msg) {
            uint8_t ch = (uint8_t)*msg++;
            const Glyph8x16* glyph = glyphs[ch];

            if (glyph) {
                Display_BlitGlyph(cursorX, y, &glyph_cache[ch]);
                cursorX += glyphAdvanceComicSans[ch];
            }
        }
    }
}

void Display_WriteNumberAt(uint16_t x, uint16_t y, bool isWholeNumber, float value) 
{
    char buffer[7]; // Enough for "99.99" + '\0'
    const char* format = isWholeNumber ? "%d" : "%.2f";
    snprintf(buffer, sizeof(buffer), format, value);
    Display_WriteTextAt(x, y, buffer);
}

void Display_RenderMainScreen()
{
    Display_ResetState();
    clock_t start = clock();

    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();

    // Background rectangles 
    Display_DrawRect(600, 45, 800, 120, COLOR_RED, 1);
    Display_DrawRect(200, 165, 400, 240, COLOR_GREEN, 1);
    Display_DrawRect(200, 285, 600, 360, COLOR_RED, 1);

    // White Borders in Main Screen
    Display_DrawBordersMain();


    // =======================
    // ======== TEXT =========
    // ======================= 
    
    // Text Labels 
    Display_EnableTextModeAndFont(DISPLAY_FONT_COMIC_SANS); 
    Display_WriteTextAt(20, 0 + LABELS_Y_OFFSET, "LV Voltage");
    Display_WriteTextAt(340, 0 + LABELS_Y_OFFSET, "Last Lap");
    Display_WriteTextAt(610, 0 + LABELS_Y_OFFSET, "Pack Voltage");
    Display_WriteTextAt(10, 120 + LABELS_Y_OFFSET, "Max Motor T");
    Display_WriteTextAt(270, 120 + LABELS_Y_OFFSET, "Diff");
    Display_WriteTextAt(470, 120 + LABELS_Y_OFFSET, "Pred");
    Display_WriteTextAt(650, 120 + LABELS_Y_OFFSET, "Rotor T");
    Display_WriteTextAt(20, 240 + LABELS_Y_OFFSET, "Max IGBT T");
    Display_WriteTextAt(350, 240 + LABELS_Y_OFFSET, "Pack %");
    Display_WriteTextAt(630, 240 + LABELS_Y_OFFSET, "Max Cell T");
    Display_WriteTextAt(30, 360 + LABELS_Y_OFFSET, "Inv Fault");
    Display_WriteTextAt(310, 360 + LABELS_Y_OFFSET, "Endurance %");
    Display_WriteTextAt(640, 360 + LABELS_Y_OFFSET, "TV");
    Display_WriteTextAt(740, 360 + LABELS_Y_OFFSET, "TC");

    clock_t end = clock();
    double elapsed_ms = ((double)(end - start)) * 1000.0 / CLOCKS_PER_SEC;
    printf("Rendered MAIN SCREEN in %.2f ms\n", elapsed_ms);

    // Text Values
    float defaultFloat = 0.0;
    int defaultInt = 0;
    
    Display_EnableTextModeAndFont(DISPLAY_FONT_INTERNAL);  
    Display_WriteNumberAt(50, 0 + VALUES_Y_OFFSET, false, defaultFloat);
    Display_WriteNumberAt(350, 0 + VALUES_Y_OFFSET, false, defaultFloat);
    Display_WriteNumberAt(690, 0 + VALUES_Y_OFFSET, true, defaultInt);
    Display_WriteNumberAt(80, 120 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(250, 120 + VALUES_Y_OFFSET, false, defaultFloat);   
    Display_WriteNumberAt(490, 120 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(690, 120 + VALUES_Y_OFFSET, true, defaultInt);  
    Display_WriteNumberAt(80, 240 + VALUES_Y_OFFSET, true, defaultInt);    
    Display_WriteNumberAt(390, 240 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(690, 240 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteTextAt(45, 360 + VALUES_Y_OFFSET, "None"); 
    Display_WriteNumberAt(390, 360 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(640, 360 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(740, 360 + VALUES_Y_OFFSET, true, defaultInt);   
}

void Display_RenderDebugScreen()
{
    Display_ResetState();
    clock_t start = clock();

    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();

    // Background rectangles 
    Display_DrawRect(0, 45, 800, 120, COLOR_RED, 1);
    Display_DrawRect(0, 165, 200, 240, COLOR_RED, 1);
    Display_DrawRect(600, 165, 800, 240, COLOR_RED, 1);
    Display_DrawRect(0, 405, 200, 480, COLOR_RED, 1);

    // White Borders in Main Screen
    Display_DrawBordersDebug();


    // =======================
    // ======== TEXT =========
    // ======================= 
    
    // Text Labels 
    Display_EnableTextModeAndFont(DISPLAY_FONT_COMIC_SANS); 
    Display_WriteTextAt(130, 0 + LABELS_Y_OFFSET, "GPS Lat");
    Display_WriteTextAt(550, 0 + LABELS_Y_OFFSET, "GPS Long");
    Display_WriteTextAt(0, 120 + LABELS_Y_OFFSET, "F Brake Press");
    Display_WriteTextAt(220, 120 + LABELS_Y_OFFSET, "Max IGBT T");
    Display_WriteTextAt(410, 120 + LABELS_Y_OFFSET, "F Brake Bias");
    Display_WriteTextAt(660, 120 + LABELS_Y_OFFSET, "Rotor T");
    Display_WriteTextAt(30, 240 + LABELS_Y_OFFSET, "APPS Arb");
    Display_WriteTextAt(220, 240 + LABELS_Y_OFFSET, "Steer Angle");
    Display_WriteTextAt(415, 240 + LABELS_Y_OFFSET, "Max Motor T");
    Display_WriteTextAt(660, 240 + LABELS_Y_OFFSET, "Speed");
    Display_WriteTextAt(30, 360 + LABELS_Y_OFFSET, "Min Cell V");
    Display_WriteTextAt(220, 360 + LABELS_Y_OFFSET, "Peak Cell T");
    Display_WriteTextAt(440, 360 + LABELS_Y_OFFSET, "Inv Fault");
    Display_WriteTextAt(630, 360 + LABELS_Y_OFFSET, "LV Voltage");

    clock_t end = clock();
    double elapsed_ms = ((double)(end - start)) * 1000.0 / CLOCKS_PER_SEC;
    printf("Rendered DEBUG SCREEN in %.2f ms\n", elapsed_ms);

    // Text Values
    float defaultFloat = 0.0;
    int defaultInt = 0;
    
    Display_EnableTextModeAndFont(DISPLAY_FONT_INTERNAL);  
    Display_WriteNumberAt(180, 0 + VALUES_Y_OFFSET, true, defaultInt);
    Display_WriteNumberAt(580, 0 + VALUES_Y_OFFSET, true, defaultInt);
    Display_WriteNumberAt(90, 120 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(290, 120 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(490, 120 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(690, 120 + VALUES_Y_OFFSET, true, defaultInt);  
    Display_WriteNumberAt(90, 240 + VALUES_Y_OFFSET, true, defaultInt);    
    Display_WriteNumberAt(290, 240 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(490, 240 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(690, 240 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(50, 360 + VALUES_Y_OFFSET, false, defaultFloat);   
    Display_WriteNumberAt(290, 360 + VALUES_Y_OFFSET, true, defaultInt); 
    Display_WriteTextAt(440, 360 + VALUES_Y_OFFSET, "None");   
    Display_WriteNumberAt(650, 360 + VALUES_Y_OFFSET, false, defaultFloat);   
}

void Display_SwitchScreen(void)
{
    if (CURRENT_SCREEN == SCREEN_DEBUG) {
        Display_RenderMainScreen();
        CURRENT_SCREEN = SCREEN_MAIN;
    } else {
        Display_RenderDebugScreen();
        CURRENT_SCREEN = SCREEN_DEBUG;
    }
}