/**
* Author: Richard Li
* Editors: Richard Li
*/

#include "display.h"
#include "RA8875.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LCD SPI configuration and pin assignments  
#define LCD_SPI_HOST              SPI3_HOST
#define LCD_SPI_SPEED             180000 // 115.2k stable, 170k effective, 190k to 2.8m unstable ** maybe increase?
#define LCD_PIN_SCLK              11     // ** 37
#define LCD_PIN_MOSI              13     // ** 38 
#define LCD_PIN_MISO              12     // ** 39
#define LCD_PIN_CS                6      // ** 40
#define LCD_PIN_RESET             5      // Pin unused. ** 42
#define LCD_PIN_INT               4      // ** ??

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

// RA8875 register mode values
#define RA8875_VAL_MODE_GRAPHIC   0x00  // Graphic mode
#define RA8875_VAL_MODE_TEXT      0x80  // Text mode

// Colors - 8-bit val interpreted as 3:3:2 RGB in 256-color mode. 7–6 → blue (2 bits) 5–3 → green (3 bits) 2–0 → red (3 bits)
#define COLOR_WHITE              255   
#define COLOR_GREEN              32
#define COLOR_RED                5
#define COLOR_YELLOW             63

// Font sizez (0x40 for transparent bg)
#define FONT_SIZE_2X         (0x05 | 0x40)  // 2x2
#define FONT_SIZE_3X         (0x0A | 0x40)  // 3x3
#define FONT_SIZE_4X      (0x0F | 0x40)  // 4x4

// Y Offsets for Label/Value rows
#define LABELS_Y_OFFSET          11
#define VALUES_Y_OFFSET          55

// RTOS Delay for mode switching
#define DEFAULT_DELAY            20  // Allows rectangles to fully render before switching to text mode

// Borders
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

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

// RA8875 driver instance
static RA8875_context_t lcd;

// Text vs. Graphic mode tracking
static bool inGraphicMode = false;

// Screen tracking (SCREEN_MAIN_NO_LAPS, SCREEN_MAIN_LAPS, SCREEN_DEBUG_RTD, SCREEN_DEBUG_NO_RTD, SCREEN_WARN)
Screen_t CURRENT_SCREEN;

static void Display_ForegroundWhite(void) 
{
    RA8875_write_register(&lcd, RA8875_REG_FG_R, 0x07);
    RA8875_write_register(&lcd, RA8875_REG_FG_G, 0x07);
    RA8875_write_register(&lcd, RA8875_REG_FG_B, 0x03); 
}

static void Display_InternalFontSize(uint8_t size) 
{
    RA8875_write_register(&lcd, RA8875_REG_FONT_SIZE, size);
    RA8875_write_register(&lcd, RA8875_REG_FONT_SRC, 0x00);
    RA8875_write_register(&lcd, RA8875_REG_FONT_SEL, 0x00);
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

static void Display_DrawBorders(const LineSpec* lines, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        Display_DrawRect(lines[i].x1, lines[i].y1, lines[i].x2, lines[i].y2, COLOR_WHITE, true);
    }
}

static void Display_RenderMainScreen(bool isLaps) // Handles both laps and no-laps main screen
{
    Display_ResetState();

    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();

    // Rectangles 
    if (isLaps) {
        Display_DrawRect(0, 170, 800, 240, COLOR_RED, true);
    } else {
        Display_DrawRect(0, 90, 800, 180, COLOR_RED, true);
    }
    
    // White Borders in Main Screen
    if (isLaps) {
        Display_DrawBorders(mainBordersLaps, ARRAY_LEN(mainBordersLaps));
    } else {
        Display_DrawBorders(mainBordersNoLaps, ARRAY_LEN(mainBordersNoLaps));
    }

    vTaskDelay(pdMS_TO_TICKS(DEFAULT_DELAY));

    // =======================
    // ======== TEXT =========
    // ======================= 
    
    // Text Labels 
    Display_EnableTextModeAndFont(FONT_SIZE_2X); 

    if (isLaps) {
        Display_WriteTextAt(30,    0 + LABELS_Y_OFFSET, "Lap Diff");
        Display_WriteTextAt(305,   0 + LABELS_Y_OFFSET, "Last Lap Time");
        Display_WriteTextAt(640,   0 + LABELS_Y_OFFSET, "Predicted");
        Display_WriteTextAt(350, 120 + LABELS_Y_OFFSET, "Pack %");
        Display_WriteTextAt(370, 240 + LABELS_Y_OFFSET, "Lap");
        Display_WriteTextAt(40,  360 + LABELS_Y_OFFSET, "Torque Limit"); // No coloring/warning
        Display_WriteTextAt(315, 360 + LABELS_Y_OFFSET, "TC Lat Mode");
        Display_WriteTextAt(590, 360 + LABELS_Y_OFFSET, "TV Balance");
    } else {
        Display_WriteTextAt(350,   0 + LABELS_Y_OFFSET + 30, "Pack %");
        Display_WriteTextAt(270, 185 + LABELS_Y_OFFSET + 30, "Distance Traveled");
        Display_WriteTextAt(40,  360 + LABELS_Y_OFFSET, "Torque Limit"); // No coloring/warning
        Display_WriteTextAt(320, 360 + LABELS_Y_OFFSET, "TC Lat Mode");
        Display_WriteTextAt(590, 360 + LABELS_Y_OFFSET, "TV Balance");
    }

    // Text Values
    float defaultFloat = 0.00;
    int defaultInt = 0;
    
    Display_InternalFontSize(FONT_SIZE_3X);

    if (isLaps) {
        Display_WriteNumberAt(40,  0   + VALUES_Y_OFFSET, false, defaultFloat, false); // Lap Diff
        Display_WriteNumberAt(350, 0   + VALUES_Y_OFFSET, false, defaultFloat, false); // Last Lap Time
        Display_WriteNumberAt(660, 0   + VALUES_Y_OFFSET, false, defaultFloat, false); // Predicted
        Display_WriteNumberAt(350, 120 + VALUES_Y_OFFSET, false, defaultFloat, false); // Pack %
        Display_WriteNumberAt(380, 240 + VALUES_Y_OFFSET, true, defaultInt, false); // Lap
        Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // Torque Limit
        Display_WriteNumberAt(380, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // TC Lat Mode
        Display_WriteNumberAt(660, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // TV Balance

    } else {
        Display_WriteNumberAt(350, 0   + VALUES_Y_OFFSET + 50, false, defaultFloat, false); // Pack %
        Display_WriteNumberAt(350, 180 + VALUES_Y_OFFSET + 50, false, defaultFloat, false); // Distance Traveled
        Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // Torque Limit
        Display_WriteNumberAt(390, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // TC Lat Mode
        Display_WriteNumberAt(660, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // TV Balance
    }
}

static void Display_RenderStaticDebugScreen()
{
    Display_ResetState();

    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();
    Display_DrawBorders(debugBordersNoRTD, ARRAY_LEN(debugBordersNoRTD)); // White Borders in Debug Screen
    vTaskDelay(pdMS_TO_TICKS(DEFAULT_DELAY));

    // =======================
    // ======== TEXT =========
    // ======================= 
    
    // Text Labels 
    Display_EnableTextModeAndFont(FONT_SIZE_2X); 

    Display_WriteTextAt(20,  0   + LABELS_Y_OFFSET, "LV Voltage");
    Display_WriteTextAt(240, 0   + LABELS_Y_OFFSET, "GPS Long");
    Display_WriteTextAt(450, 0   + LABELS_Y_OFFSET, "GPS Lat");
    Display_WriteTextAt(605, 0   + LABELS_Y_OFFSET, "Pack Voltage");
    Display_WriteTextAt(10,   120 + LABELS_Y_OFFSET, "Motor T Max"); 
    Display_WriteTextAt(240, 120 + LABELS_Y_OFFSET, "APP Arb");

    Display_WriteTextAt(400, 120 + LABELS_Y_OFFSET, "Torque");  // Split into three lines for manual spacing
    Display_WriteTextAt(510, 120 + LABELS_Y_OFFSET, "Rq");      // Split into three lines for manual spacing
    Display_WriteTextAt(550, 120 + LABELS_Y_OFFSET, "Avg");      // Split into three lines for manual spacing

    Display_WriteTextAt(650, 120 + LABELS_Y_OFFSET, "Rotor T");
    Display_WriteTextAt(30,  240 + LABELS_Y_OFFSET, "Inv T Max"); 
    Display_WriteTextAt(210, 240 + LABELS_Y_OFFSET, "Steer Angle");
    Display_WriteTextAt(405, 240 + LABELS_Y_OFFSET, "F Brake Bias");
    Display_WriteTextAt(650, 240 + LABELS_Y_OFFSET, "Logging");
    Display_WriteTextAt(20,  360 + LABELS_Y_OFFSET, "Min Cell V"); 
    Display_WriteTextAt(210, 360 + LABELS_Y_OFFSET, "Peak Cell T");

    Display_WriteTextAt(405, 360 + LABELS_Y_OFFSET, "F");           // Split into two lines for manual spacing
    Display_WriteTextAt(425, 360 + LABELS_Y_OFFSET, "Brake Press"); // Split into two lines for manual spacing

    Display_WriteTextAt(620, 360 + LABELS_Y_OFFSET, "Power Limit");
    
    // Text Values
    float defaultFloat = 0;
    int defaultInt = 0;
    char* defaultString = "RR";
    
    Display_InternalFontSize(FONT_SIZE_3X);

    Display_WriteNumberAt(50,  0   + VALUES_Y_OFFSET, false, defaultFloat, false); // LV Voltage   
    Display_WriteNumberAt(215, 0   + VALUES_Y_OFFSET, false, defaultFloat, true); // GPS Long (many digits)
    Display_WriteNumberAt(415, 0   + VALUES_Y_OFFSET, false, defaultFloat, true); // GPS Lat (many digits)
    Display_WriteNumberAt(650, 0   + VALUES_Y_OFFSET, false, defaultFloat, false); // Pack Voltage 
    Display_WriteNumberAt(40,  120 + VALUES_Y_OFFSET, true, defaultInt, false); // Motor Temp Max + corner
    Display_WriteTextAt(  80,  120 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(250, 120 + VALUES_Y_OFFSET, false, defaultFloat, false); // APP Arb
    Display_WriteNumberAt(450, 120 + VALUES_Y_OFFSET, false, defaultFloat, false); // Torque Req Avg
    Display_WriteNumberAt(690, 120 + VALUES_Y_OFFSET, true, defaultInt, false); // Rotor Temp
    Display_WriteNumberAt(40,  240 + VALUES_Y_OFFSET, true, defaultInt, false); // Inverter temp max + corner
    Display_WriteTextAt(  80,  240 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(250, 240 + VALUES_Y_OFFSET, false, defaultFloat, false); // Steer Angle
    Display_WriteNumberAt(450, 240 + VALUES_Y_OFFSET, false, defaultFloat, false); // Front Brake Bias
    Display_WriteNumberAt(690, 240 + VALUES_Y_OFFSET, true, defaultInt, false); // Logging
    Display_WriteNumberAt(30,  360 + VALUES_Y_OFFSET, true, defaultInt, false); // Min Cell V + index
    Display_WriteTextAt(  50,  360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt, false); 
    Display_WriteNumberAt(230, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // Peak Cell T + index
    Display_WriteTextAt(  250, 360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(320, 360 + VALUES_Y_OFFSET, true, defaultInt, false); 
    Display_WriteNumberAt(450, 360 + VALUES_Y_OFFSET, false, defaultFloat, false); // Front Brake Pressure
    Display_WriteNumberAt(690, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // Power Limit
}

static void Display_RenderRTDDebugScreen(void)
{
    Display_ResetState();

    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();   
    Display_DrawRect(200, 170, 600, 240, COLOR_RED, true);
    Display_DrawBorders(debugBordersRTD, ARRAY_LEN(debugBordersRTD));
    vTaskDelay(pdMS_TO_TICKS(DEFAULT_DELAY));

    // =======================
    // ======== TEXT =========
    // ======================= 

    Display_EnableTextModeAndFont(FONT_SIZE_2X); 

    Display_WriteTextAt(20,  0   + LABELS_Y_OFFSET, "LV Voltage");
    Display_WriteTextAt(305, 0   + LABELS_Y_OFFSET, "Last Lap Time");
    Display_WriteTextAt(605, 0   + LABELS_Y_OFFSET, "Pack Voltage");
    Display_WriteTextAt(10,  120 + LABELS_Y_OFFSET, "Motor T Max"); 
    Display_WriteTextAt(355, 120 + LABELS_Y_OFFSET, "Pack %");
    Display_WriteTextAt(650, 120 + LABELS_Y_OFFSET, "Rotor T");
    Display_WriteTextAt(30,  240 + LABELS_Y_OFFSET, "Inv T Max"); 
    Display_WriteTextAt(380, 240 + LABELS_Y_OFFSET, "Lap");
    Display_WriteTextAt(605, 240 + LABELS_Y_OFFSET, "Torque Limit"); // No coloring/warning
    Display_WriteTextAt(20,  360 + LABELS_Y_OFFSET, "Min Cell V"); 
    Display_WriteTextAt(210, 360 + LABELS_Y_OFFSET, "Peak Cell T");
    Display_WriteTextAt(405, 360 + LABELS_Y_OFFSET, "F");           // Split into two lines for manual spacing
    Display_WriteTextAt(425, 360 + LABELS_Y_OFFSET, "Brake Press"); // Split into two lines for manual spacing
    Display_WriteTextAt(640, 360 + LABELS_Y_OFFSET, "TC");
    Display_WriteTextAt(740, 360 + LABELS_Y_OFFSET, "TV");

    Display_InternalFontSize(FONT_SIZE_3X);

    float defaultFloat = 0.0;
    int defaultInt = 0;
    char* defaultString = "RR";
    Display_WriteNumberAt(50,  0   + VALUES_Y_OFFSET, false, defaultFloat, false); // LV Voltage   
    Display_WriteNumberAt(355, 0   + VALUES_Y_OFFSET, false, defaultFloat, false); // Last Lap Time  
    Display_WriteNumberAt(655, 0   + VALUES_Y_OFFSET, false, defaultFloat, false); // Pack Voltage 
    Display_WriteNumberAt(50,  120 + VALUES_Y_OFFSET, true, defaultInt, false); // Motor Temp Max + corner
    Display_WriteTextAt(  90,  120 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(355, 120 + VALUES_Y_OFFSET, false, defaultFloat, false); // Pack % 
    Display_WriteNumberAt(690, 120 + VALUES_Y_OFFSET, true, defaultInt, false); // Rotor Temp
    Display_WriteNumberAt(50,  240 + VALUES_Y_OFFSET, true, defaultInt, false); // Inverter temp max + corner
    Display_WriteTextAt(  90,  240 + VALUES_Y_OFFSET, defaultString);
    Display_WriteNumberAt(390, 240 + VALUES_Y_OFFSET, true, defaultInt, false); // Lap Num
    Display_WriteNumberAt(690, 240 + VALUES_Y_OFFSET, true, defaultInt, false); // Torque Limit
    Display_WriteNumberAt(20,  360 + VALUES_Y_OFFSET, true, defaultInt, false); // Min Cell V + index
    Display_WriteTextAt(  40,  360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(120, 360 + VALUES_Y_OFFSET, true, defaultInt, false); 
    Display_WriteNumberAt(220, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // Peak Cell T + index
    Display_WriteTextAt(  240, 360 + VALUES_Y_OFFSET, ",i="); 
    Display_WriteNumberAt(320, 360 + VALUES_Y_OFFSET, true, defaultInt, false); 
    Display_WriteNumberAt(455, 360 + VALUES_Y_OFFSET, false, defaultFloat, false); // Front Brake Pressure
    Display_WriteNumberAt(640, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // TC Mode
    Display_WriteNumberAt(740, 360 + VALUES_Y_OFFSET, true, defaultInt, false); // TV Balance
}

static void Display_Warn() 
{
    Display_ResetState();
    Display_EnableDrawMode();
    Display_DrawRect(0, 0, 800, 480, COLOR_RED, true);
    vTaskDelay(pdMS_TO_TICKS(DEFAULT_DELAY));
    Display_EnableTextModeAndFont(FONT_SIZE_4X);
    Display_ForegroundWhite();
    Display_WriteTextAt(290, 200, "WARNING");
}

void Display_Init(void) // Defaults to static debug screen
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
    Display_SwitchScreen(SCREEN_DEBUG_NO_RTD);
}

void Display_EnableDrawMode(void) 
{
    if (!inGraphicMode) {
        RA8875_write_register(&lcd, RA8875_REG_MODE_CTRL, RA8875_VAL_MODE_GRAPHIC); // Enable graphic mode if not already
        inGraphicMode = true;
    }
}

// Internal font size is set here but you can also use Display_InternalFontSize() to change internal font size
void Display_EnableTextModeAndFont(uint8_t size) 
{
    if(inGraphicMode) {
        RA8875_write_register(&lcd, RA8875_REG_MODE_CTRL, RA8875_VAL_MODE_TEXT); // Switch to text mode
        inGraphicMode = false;
    }

    Display_ForegroundWhite();
    Display_InternalFontSize(size);
}

void Display_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, bool filled) 
{
    RA8875_draw_rect(&lcd, x1, y1, x2, y2, color, filled);
}

void Display_WriteTextAt(uint16_t x, uint16_t y, const char* msg) 
{
    Display_SetTextCursor(x, y);
    RA8875_write_command(&lcd, 0x02);

    while (*msg) {
        RA8875_write_data(&lcd, (uint8_t)*msg++);
    }
}

void Display_WriteNumberAt(uint16_t x, uint16_t y, bool isWholeNumber, float value, bool hasManyDigits) 
{ 
    if (hasManyDigits) {
        char buffer[11]; // Enough for "999.99999" + '\0' 
        const char* format = "%.5f"; 
        snprintf(buffer, sizeof(buffer), format, value); 
        Display_WriteTextAt(x, y, buffer); 
    } else {
        char buffer[8]; // Enough for "999.99" + '\0' 
        const char* format = isWholeNumber ? "%d" : "%.2f"; 
        snprintf(buffer, sizeof(buffer), format, value); 
        Display_WriteTextAt(x, y, buffer); 
    }
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
            Display_RenderRTDDebugScreen();
            break;
        case SCREEN_WARN:
            Display_Warn();
            break;
        default:
            return;
    }

    CURRENT_SCREEN = nextScreen;
}