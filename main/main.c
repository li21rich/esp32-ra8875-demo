#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "RA8875.h"
#include <string.h>

#define LCD_SPI_HOST      SPI3_HOST
#define LCD_SPI_SPEED     115200   // 2800000 was too unstable. Maybe try inceasing a bit? 
#define LCD_PIN_MOSI      13
#define LCD_PIN_MISO      12
#define LCD_PIN_SCLK      11
#define LCD_PIN_CS        6
#define LCD_PIN_RESET     4

#define RA8875_REG_MODE_CTRL  0x40
#define RA8875_REG_FONT_SEL   0x21

#define RA8875_REG_FG_R 0x63
#define RA8875_REG_FG_G 0x64
#define RA8875_REG_FG_B 0x65

static RA8875_context_t lcd;

void InitAndPrepare() // My helper function for initialization.
{ 
    RA8875_init(&lcd, LCD_SPI_HOST, LCD_SPI_SPEED, LCD_PIN_MOSI, 
                LCD_PIN_MISO, LCD_PIN_SCLK, LCD_PIN_CS, LCD_PIN_RESET);    
    RA8875_configure(&lcd, 26, 32, 96, 0, 32, 23, 2, 800, 480, 0); // Haven't tried to define these.
    RA8875_set_backlight_brightness(&lcd, 0xFF);
    RA8875_clear(&lcd);
}

void SetTextCursor(uint16_t x, uint16_t y) // My helper function for cursor positioning.
{
    RA8875_write_register(&lcd, 0x2A, x & 0xFF); 
    RA8875_write_register(&lcd, 0x2B, x >> 8);  
    RA8875_write_register(&lcd, 0x2C, y & 0xFF); 
    RA8875_write_register(&lcd, 0x2D, y >> 8);  
}

void WriteString(const char* msg) // My helper function for text output.
{
    RA8875_write_command(&lcd, 0x02);
    while (*msg) {
        RA8875_write_data(&lcd, (uint8_t)*msg++);
    }
}

void app_main(void) // See RA8875.h for useful functions.
{
    InitAndPrepare();
    SetTextCursor(0, 0);

    /*
     * Text example. Displays "HELLO WORLD" at top left (20,20).
     */
    RA8875_write_register(&lcd, RA8875_REG_FONT_SEL, 0x00); // Use internal font.

    // Foreground color to white.
    RA8875_write_register(&lcd, RA8875_REG_FG_R, 0x07);
    RA8875_write_register(&lcd, RA8875_REG_FG_G, 0x07);
    RA8875_write_register(&lcd, RA8875_REG_FG_B, 0x03); 

    RA8875_write_register(&lcd, RA8875_REG_MODE_CTRL, 0x80); // Switch to text mode.
    SetTextCursor(20, 20); 
    WriteString("HELLO WORLD 0123456789 QWERTY ASDFGH ZXCVBN");

    RA8875_write_register(&lcd, RA8875_REG_MODE_CTRL, 0x00); // Switch back to graphics mode.

    /*
     * Rectangle example. Draws rainbox box in bottom left corner.
     */
    uint8_t rainbowColor = 0;

    while (1) {
        RA8875_draw_rect(&lcd, 0, 300, 180, 480, rainbowColor, 1);
        rainbowColor++; 
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}