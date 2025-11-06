/**
* Author: Richard Li
* Editors: Richard Li
* Additional Notes:
* - See RA8875.h for driver library functions. 
* - RA8875 Datasheet: https://support.midasdisplays.com/wp-content/uploads/2025/06/RA8875.pdf
*/

#include "display.h"
#include "RA8875.h"
#include "comicsans_font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LCD SPI configuration and pin assignments  
#define LCD_SPI_HOST              SPI3_HOST
#define LCD_SPI_SPEED             140000   // 115200 = safe, 2800000 = highly unstable. Increase when done.
#define LCD_PIN_MOSI              13
#define LCD_PIN_MISO              12
#define LCD_PIN_SCLK              11
#define LCD_PIN_CS                6
#define LCD_PIN_RESET             5 // was incorrectly 4, I think? Pin unused.
#define LCD_PIN_INT               4

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
#define LCD_BRIGHTNESS_100_PCT    0xFF

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

// Layers
#define LAYER_DISPLAY   0
#define LAYER_OFFSCREEN 1

// Misc
#define LABELS_Y_OFFSET          11
#define VALUES_Y_OFFSET          55
#define GLYPH_SCALE              2  
#define FONT_SIZE_TRIPLE         (0x0A | 0x40)  // 3x3 | transparent background
#define FONT_SIZE_QUADRUPLE      (0x0F | 0x40)  // 4x4 | transparent background
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

static RA8875_context_t lcd;
static DisplayFont_t currentFont = DISPLAY_FONT_INTERNAL;
static GlyphBuffer glyph_cache[256];
static bool inGraphicMode = false;
static const LineSpec mainBordersNoLaps[] = {
    {0, 180, 800, 181}, {0, 360, 800, 361},
    {266, 360, 267, 480}, {533, 360, 534, 480}
};
static const LineSpec mainBordersLaps[] = {
    {0, 120, 800, 121}, {0, 240, 800, 241}, {0, 360, 800, 361},
    {184, 0, 185, 120}, {266, 360, 267, 480}, {615, 0, 616, 120}, {533, 360, 534, 480}
};
static const LineSpec debugBordersRTD[] = {
    {0, 120, 800, 121}, {0, 240, 800, 241}, {0, 360, 800, 361},
    {200, 0, 201, 480}, {400, 360, 401, 480}, {600, 0, 601, 480}, {700, 360, 701, 480}
};
static const LineSpec debugBordersNoRTD[] = {
    {0, 120, 800, 121}, {0, 240, 800, 241}, {0, 360, 800, 361},
    {200, 0, 201, 480}, {400, 0, 401, 480}, {600, 0, 601, 480}
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init" // Suppress overrides warnings
static const uint8_t glyphAdvanceComicSans[256] = { // Allows for custom spacing for wider or narrower letters
    [0 ... 255] = 16,  // Defaults to 16
    [' '] = 12, ['l'] = 13, ['i'] = 13, ['o'] = 15, ['r'] = 15, ['g'] = 15, ['a'] = 17, ['m'] = 17, ['N'] = 17, ['M'] = 18, ['G'] = 18,
};
#pragma GCC diagnostic pop

Screen_t CURRENT_SCREEN;

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
    Display_ForegroundWhite();
}

// Span batching + Caching for faster special font load
static void Display_BlitGlyph(uint16_t x, uint16_t y, const GlyphBuffer* buf)
{
    const uint8_t scale = GLYPH_SCALE;
    Display_ForegroundWhite();

    for (int col = 0; col < 8; col++) {
        uint16_t px1 = x + col * scale;
        uint16_t px2 = px1 + scale - 1;

        int row = 0;
        while (row < 16) {
            if (!buf->pixels[row][col]) {
                row++;
                continue;
            }

            int start_row = row;
            
            while (row < 16 && buf->pixels[row][col]) {
                row++;
            }

            uint16_t py1 = y + start_row * scale;
            uint16_t py2 = y + (row - 1) * scale + scale - 1;
            RA8875_draw_rect_fast(&lcd, px1, py1, px2, py2);
        }
    }
}

static void Display_DrawBorders(const LineSpec* lines, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        Display_DrawRect(lines[i].x1, lines[i].y1, lines[i].x2, lines[i].y2, COLOR_WHITE, true);
    }
}

static void Display_RenderMainScreen(bool isLaps)
{
    Display_ResetState();

    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();

    // Rectangles 
    //Display_DrawRect(0, 0, 200, 184, COLOR_GREEN, 1);
    if (isLaps) {
        Display_DrawRect(260, 140 + VALUES_Y_OFFSET, 450, 215, COLOR_WHITE, false);
    } else {
        Display_DrawRect(260, 20 + VALUES_Y_OFFSET, 450, 95, COLOR_WHITE, false);
    }
    
    // White Borders in Main Screen
    if (isLaps) {
        Display_DrawBorders(mainBordersLaps, ARRAY_LEN(mainBordersLaps));
    } else {
        Display_DrawBorders(mainBordersNoLaps, ARRAY_LEN(mainBordersNoLaps));
    }


    // =======================
    // ======== TEXT =========
    // ======================= 
    
    // Text Labels 
    Display_EnableTextModeAndFont(DISPLAY_FONT_COMIC_SANS); 

    if (isLaps) {
        Display_WriteTextAt(30,    0 + LABELS_Y_OFFSET, "Lap Diff");
        Display_WriteTextAt(310,   0 + LABELS_Y_OFFSET, "Last Lap Time");
        Display_WriteTextAt(640,   0 + LABELS_Y_OFFSET, "Predicted");
        Display_WriteTextAt(350, 120 + LABELS_Y_OFFSET, "Pack %"); // needs loading bar AND value
        Display_WriteTextAt(380, 240 + LABELS_Y_OFFSET, "Lap");
        Display_WriteTextAt(40,  360 + LABELS_Y_OFFSET, "Torque Limit"); // No coloring/warning
        Display_WriteTextAt(315, 360 + LABELS_Y_OFFSET, "TC Lat Mode");
        Display_WriteTextAt(590, 360 + LABELS_Y_OFFSET, "TV Balance");
    } else {
        Display_WriteTextAt(350,   0 + LABELS_Y_OFFSET, "Pack %"); // needs loading bar AND value
        Display_WriteTextAt(270, 185 + LABELS_Y_OFFSET, "Distance Traveled");
        Display_WriteTextAt(40,  360 + LABELS_Y_OFFSET, "Torque Limit"); // No coloring/warning
        Display_WriteTextAt(320, 360 + LABELS_Y_OFFSET, "TC Lat Mode");
        Display_WriteTextAt(590, 360 + LABELS_Y_OFFSET, "TV Balance");
    }

    // Text Values
    float defaultFloat = 0.00;
    int defaultInt = 0;
    
    Display_EnableTextModeAndFont(DISPLAY_FONT_INTERNAL);  

    if (isLaps) {
        Display_WriteNumberAt(40,  0   + VALUES_Y_OFFSET, false, defaultFloat); // Lap Diff
        Display_WriteNumberAt(360, 0   + VALUES_Y_OFFSET, false, defaultFloat); // Last Lap Time
        Display_WriteNumberAt(660, 0   + VALUES_Y_OFFSET, false, defaultFloat); // Predicted
        Display_WriteNumberAt(460, 120 + VALUES_Y_OFFSET, false, defaultFloat); // Pack %
        Display_WriteNumberAt(390, 240 + VALUES_Y_OFFSET, true, defaultInt); // Lap
        Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt); // Torque Limit
        Display_WriteNumberAt(380, 360 + VALUES_Y_OFFSET, true, defaultInt); // TC Lat Mode
        Display_WriteNumberAt(660, 360 + VALUES_Y_OFFSET, true, defaultInt); // TV Balance

    } else {
        Display_WriteNumberAt(460, 0   + VALUES_Y_OFFSET, false, defaultFloat); // Pack %
        Display_WriteNumberAt(350, 180 + VALUES_Y_OFFSET, false, defaultFloat); // Distance Traveled
        Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt); // Torque Limit
        Display_WriteNumberAt(390, 360 + VALUES_Y_OFFSET, true, defaultInt); // TC Lat Mode
        Display_WriteNumberAt(660, 360 + VALUES_Y_OFFSET, true, defaultInt); // TV Balance
    }
}

static void Display_RenderStaticDebugScreen()
{
    Display_ResetState();

    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();

    // Rectangles 
    Display_DrawRect(405, 380 + VALUES_Y_OFFSET, 495, 450, COLOR_WHITE, false);

    // White Borders in Debug Screen
    Display_DrawBorders(debugBordersNoRTD, ARRAY_LEN(debugBordersNoRTD));


    // =======================
    // ======== TEXT =========
    // ======================= 
    
    // Text Labels 
    Display_EnableTextModeAndFont(DISPLAY_FONT_COMIC_SANS); 

    Display_WriteTextAt(20,  0   + LABELS_Y_OFFSET, "LV Voltage");
    Display_WriteTextAt(240, 0   + LABELS_Y_OFFSET, "GPS Long");
    Display_WriteTextAt(450, 0   + LABELS_Y_OFFSET, "GPS Lat");
    Display_WriteTextAt(615, 0   + LABELS_Y_OFFSET, "Pack Voltage");
    Display_WriteTextAt(20,   120 + LABELS_Y_OFFSET, "Motor T Max"); 
    Display_WriteTextAt(240, 120 + LABELS_Y_OFFSET, "APP Arb");
    Display_WriteTextAt(400, 120 + LABELS_Y_OFFSET, "Torque Rq Avg");
    Display_WriteTextAt(650, 120 + LABELS_Y_OFFSET, "Rotor T");
    Display_WriteTextAt(30,  240 + LABELS_Y_OFFSET, "Inv T Max"); 
    Display_WriteTextAt(220, 240 + LABELS_Y_OFFSET, "Steer Angle");
    Display_WriteTextAt(410, 240 + LABELS_Y_OFFSET, "F Brake Bias");
    Display_WriteTextAt(650, 240 + LABELS_Y_OFFSET, "Logging");
    Display_WriteTextAt(20,  360 + LABELS_Y_OFFSET, "Min Cell V"); 
    Display_WriteTextAt(220, 360 + LABELS_Y_OFFSET, "Peak Cell T");
    Display_WriteTextAt(405, 360 + LABELS_Y_OFFSET, "F Brake Press");
    Display_WriteTextAt(620, 360 + LABELS_Y_OFFSET, "Power Limit");
    
    // Text Values
    float defaultFloat = 0.0;
    int defaultInt = 0;
    char* defaultString = "RR";
    
    Display_EnableTextModeAndFont(DISPLAY_FONT_INTERNAL);  

    Display_WriteNumberAt(50,  0   + VALUES_Y_OFFSET, false, defaultFloat); // LV Voltage   
    Display_WriteNumberAt(250, 0   + VALUES_Y_OFFSET, false, defaultFloat); // GPS Long
    Display_WriteNumberAt(450, 0   + VALUES_Y_OFFSET, false, defaultFloat); // GPS Lat
    Display_WriteNumberAt(640, 0   + VALUES_Y_OFFSET, false, defaultFloat); // Pack Voltage 
    Display_WriteNumberAt(40,  120 + VALUES_Y_OFFSET, true, defaultInt); // Motor Temp Max + corner
    Display_WriteTextAt(  80,  120 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(250, 120 + VALUES_Y_OFFSET, false, defaultFloat); // APP Arb
    Display_WriteNumberAt(450, 120 + VALUES_Y_OFFSET, false, defaultFloat); // Torque Req Avg
    Display_WriteNumberAt(690, 120 + VALUES_Y_OFFSET, true, defaultInt); // Rotor Temp
    Display_WriteNumberAt(40,  240 + VALUES_Y_OFFSET, true, defaultInt); // Inverter temp max + corner
    Display_WriteTextAt(  80,  240 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(290, 240 + VALUES_Y_OFFSET, true, defaultInt); // Steer Angle
    Display_WriteNumberAt(490, 240 + VALUES_Y_OFFSET, true, defaultInt); // Front Brake Bias
    Display_WriteNumberAt(690, 240 + VALUES_Y_OFFSET, true, defaultInt); // Logging
    Display_WriteNumberAt(20,  360 + VALUES_Y_OFFSET, true, defaultInt); // Min Cell V + index
    Display_WriteTextAt(  40,  360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt); 
    Display_WriteNumberAt(220, 360 + VALUES_Y_OFFSET, true, defaultInt); // Peak Cell T + index
    Display_WriteTextAt(  240, 360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(320, 360 + VALUES_Y_OFFSET, true, defaultInt); 
    Display_WriteNumberAt(500, 360 + VALUES_Y_OFFSET, false, defaultFloat); // Front Brake Pressure
    Display_WriteNumberAt(690, 360 + VALUES_Y_OFFSET, true, defaultInt); // Power Limit
}

static void Display_UsePrerenderedDebugRTD()
{
    Display_ResetState();
    RA8875_bte_move(&lcd, 0, 0, LAYER_OFFSCREEN, 0, 0, LAYER_DISPLAY, 800, 480, 0, 0xC);
    float defaultFloat = 0.0;
    int defaultInt = 0;
    char* defaultString = "RR";
    Display_EnableTextModeAndFont(DISPLAY_FONT_INTERNAL);  
    Display_WriteNumberAt(50,  0   + VALUES_Y_OFFSET, false, defaultFloat); // LV Voltage   
    Display_WriteNumberAt(355, 0   + VALUES_Y_OFFSET, false, defaultFloat); // Last Lap Time  
    Display_WriteNumberAt(650, 0   + VALUES_Y_OFFSET, false, defaultFloat); // Pack Voltage 
    Display_WriteNumberAt(50,  120 + VALUES_Y_OFFSET, true, defaultInt); // Motor Temp Max + corner
    Display_WriteTextAt(  90,  120 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(355, 120 + VALUES_Y_OFFSET, false, defaultFloat); // Pack % 
    Display_WriteNumberAt(690, 120 + VALUES_Y_OFFSET, true, defaultInt); // Rotor Temp
    Display_WriteNumberAt(50,  240 + VALUES_Y_OFFSET, true, defaultInt); // Inverter temp max + corner
    Display_WriteTextAt(  90,  240 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(390, 240 + VALUES_Y_OFFSET, true, defaultInt); // Lap Num
    Display_WriteNumberAt(690, 240 + VALUES_Y_OFFSET, true, defaultInt); // Torque Limit
    Display_WriteNumberAt(20,  360 + VALUES_Y_OFFSET, true, defaultInt); // Min Cell V + index
    Display_WriteTextAt(  40,  360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt); 
    Display_WriteNumberAt(220, 360 + VALUES_Y_OFFSET, true, defaultInt); // Peak Cell T + index
    Display_WriteTextAt(  240, 360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(320, 360 + VALUES_Y_OFFSET, true, defaultInt); 
    Display_WriteNumberAt(500, 360 + VALUES_Y_OFFSET, false, defaultFloat); // Front Brake Pressure
    Display_WriteNumberAt(640, 360 + VALUES_Y_OFFSET, true, defaultInt); // TC Mode
    Display_WriteNumberAt(740, 360 + VALUES_Y_OFFSET, true, defaultInt); // TV Balance
}

static void Display_PrerenderDebugRTDLabels(void) 
{
    Display_ResetState();
    Display_EnableDrawMode();
    Display_DrawBorders(debugBordersRTD, ARRAY_LEN(debugBordersRTD));
    Display_DrawRect(410, 380 + VALUES_Y_OFFSET, 490, 450, COLOR_WHITE, false);
    Display_EnableTextModeAndFont(DISPLAY_FONT_COMIC_SANS); 
    Display_WriteTextAt(20,  0   + LABELS_Y_OFFSET, "LV Voltage");
    Display_WriteTextAt(305, 0   + LABELS_Y_OFFSET, "Last Lap Time");
    Display_WriteTextAt(610, 0   + LABELS_Y_OFFSET, "Pack Voltage");
    Display_WriteTextAt(10,  120 + LABELS_Y_OFFSET, "Motor T Max"); 
    Display_WriteTextAt(350, 120 + LABELS_Y_OFFSET, "Pack %");
    Display_WriteTextAt(650, 120 + LABELS_Y_OFFSET, "Rotor T");
    Display_WriteTextAt(30,  240 + LABELS_Y_OFFSET, "Inv T Max"); 
    Display_WriteTextAt(380, 240 + LABELS_Y_OFFSET, "Lap");
    Display_WriteTextAt(610, 240 + LABELS_Y_OFFSET, "Torque Limit"); // No coloring/warning
    Display_WriteTextAt(20,  360 + LABELS_Y_OFFSET, "Min Cell V"); 
    Display_WriteTextAt(220, 360 + LABELS_Y_OFFSET, "Peak Cell T");
    Display_WriteTextAt(405, 360 + LABELS_Y_OFFSET, "F Brake Press");
    Display_WriteTextAt(640, 360 + LABELS_Y_OFFSET, "TC");
    Display_WriteTextAt(740, 360 + LABELS_Y_OFFSET, "TV");
    RA8875_bte_move(&lcd, 0, 0, LAYER_DISPLAY, 0, 0, LAYER_OFFSCREEN, 800, 480, 0, 0xC);  // Save to off-screen
}

static void Display_InternalFontSize(uint8_t size) 
{
    RA8875_write_register(&lcd, RA8875_REG_FONT_SIZE, size);
    RA8875_write_register(&lcd, RA8875_REG_FONT_SRC, 0x00);
    RA8875_write_register(&lcd, RA8875_REG_FONT_SEL, 0x00);
}

static void Display_Warn() 
{
    Display_ResetState();
    Display_EnableDrawMode();
    Display_DrawRect(0, 0, 800, 480, COLOR_RED, true);
    vTaskDelay(pdMS_TO_TICKS(50));
    Display_EnableTextModeAndFont(DISPLAY_FONT_INTERNAL);
    Display_InternalFontSize(FONT_SIZE_QUADRUPLE);
    Display_ForegroundWhite();
    Display_WriteTextAt(290, 200, "WARNING");
}

void Display_Init(void) 
{
    RA8875_init(&lcd, LCD_SPI_HOST, LCD_SPI_SPEED, LCD_PIN_MOSI, LCD_PIN_MISO,
                LCD_PIN_SCLK, LCD_PIN_CS, LCD_PIN_INT);    
    RA8875_configure(&lcd, 
                    LCD_HSYNC_NONDISP, LCD_HSYNC_START, LCD_HSYNC_PW, LCD_HSYNC_FINETUNE,
                    LCD_VSYNC_NONDISP, LCD_VSYNC_START, LCD_VSYNC_PW,
                    LCD_WIDTH, LCD_HEIGHT, LCD_VOFFSET);
    RA8875_clear(&lcd);
    RA8875_set_backlight_brightness(&lcd, LCD_BRIGHTNESS_100_PCT); 
    Display_SetTextCursor(0, 0);
    Display_PrecomputeGlyphs();
    Display_PrerenderDebugRTDLabels();
    Display_SwitchScreen(SCREEN_DEBUG_NO_RTD);
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
        Display_InternalFontSize(FONT_SIZE_TRIPLE);
    } else if (fontType == DISPLAY_FONT_COMIC_SANS) {
        Display_EnableDrawMode(); // We write comic sans as graphical drawings
    }
}

void Display_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, bool filled) 
{
    RA8875_draw_rect(&lcd, x1, y1, x2, y2, color, filled);
}

void Display_WriteTextAt(uint16_t x, uint16_t y, const char* msg) 
{
    if (currentFont == DISPLAY_FONT_INTERNAL)  {
        Display_SetTextCursor(x, y);
        RA8875_write_command(&lcd, 0x02);

        while (*msg) {
            RA8875_write_data(&lcd, (uint8_t)*msg++);
        }
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

void Display_SwitchScreen(Screen_t nextScreen) 
{
    if (CURRENT_SCREEN == nextScreen) return;
    
    switch (nextScreen) {
        case SCREEN_MAIN_NO_LAPS:
            Display_RenderMainScreen(false);
            break;
        case SCREEN_MAIN_LAPS:
            Display_RenderMainScreen(true);
            break;
        case SCREEN_DEBUG_NO_RTD:
            Display_RenderStaticDebugScreen();
            break;
        case SCREEN_DEBUG_RTD:
            Display_UsePrerenderedDebugRTD();
            break;
        case SCREEN_WARN:
            Display_Warn();
            break;
        default:
            return;
    }

    CURRENT_SCREEN = nextScreen;
}