#pragma once

#include <stdint.h>
#include "driver/spi_master.h"

typedef struct {

    spi_device_handle_t spi_device;
    int pin_int;

} RA8875_context_t;

/* CORE COMMANDS */

/// <summary>
/// Sets up the SPI host to communicate with the screen. This will first clear all data in ctx, so feel free to use uninitialized memory.
/// In my testing, the maximum stable speed seems to be ~2800000.
/// </summary>
int RA8875_init(RA8875_context_t* ctx, int host, int speed, int pinMosi, int pinMiso, int pinSclk, int pinCs, int pinInt);

/// <summary>
/// Sets up the display. This is a required command during initialization.
/// </summary>
void RA8875_configure(RA8875_context_t* ctx, uint8_t hsync_nondisp, uint8_t hsync_start, uint8_t hsync_pw, uint8_t hsync_finetune, uint16_t vsync_nondisp, uint16_t vsync_start, uint8_t vsync_pw, uint16_t width, uint16_t height, uint16_t voffset);

/// <summary>
/// Clears the current active layer with black.
/// </summary>
void RA8875_clear(RA8875_context_t* ctx);

/// <summary>
/// Sets the backlight brightness on a scale of 0x00-0xFF, where 0xFF is the brightest.
/// </summary>
void RA8875_set_backlight_brightness(RA8875_context_t* ctx, uint8_t brightness);

/* REGISTERS */

/// <summary>
/// Writes a command to the device.
/// </summary>
void RA8875_write_command(RA8875_context_t* ctx, uint8_t reg);

/// <summary>
/// Writes a single byte of data to the device.
/// </summary>
void RA8875_write_data(RA8875_context_t* ctx, uint8_t value);

/// <summary>
/// Writes a large block of data to the device. There is an upper limit to size. Maximum is ~512 bytes.
/// </summary>
void RA8875_write_data_block(RA8875_context_t* ctx, const uint8_t* buffer, int nbytes);

/// <summary>
/// Requests a read from the device. Usually prefixed by a command.
/// </summary>
uint8_t RA8875_read_data(RA8875_context_t* ctx);

/// <summary>
/// Writes a register, combining a write command and write data into one transaction for speed.
/// </summary>
void RA8875_write_register(RA8875_context_t* ctx, uint8_t reg, uint8_t value);

/// <summary>
/// Reads a register, combining a write command and read data into one transaction for speed.
/// </summary>
uint8_t RA8875_read_register(RA8875_context_t* ctx, uint8_t reg);

/* DRAWING */

/// <summary>
/// Draws a rectangle on the screen, optionally filling it in.
/// </summary>
void RA8875_draw_rect(RA8875_context_t* ctx, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, uint8_t filled);

/// <summary>
/// ADDED BY WISCONSIN RACING. Draws rectangle without color set
/// </summary>
void RA8875_draw_rect_fast(RA8875_context_t* ctx, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);


/// <summary>
/// Draws a data directly to the screen in a linear fashion, not in a rectangle. RA8875_bte_write is typically more useful.
/// </summary>
void RA8875_draw_data(RA8875_context_t* ctx, uint16_t x, uint16_t y, const uint8_t* buffer, int len);

/* HELPERS */

/// <summary>
/// Sets the write cursor location for drawing. Not hugely useful on it's own.
/// </summary>
void RA8875_set_write_cursor_position(RA8875_context_t* ctx, uint16_t x, uint16_t y);

/// <summary>
/// Sets the read cursor location for drawing. Not hugely useful on it's own.
/// </summary>
void RA8875_set_read_cursor_position(RA8875_context_t* ctx, uint16_t x, uint16_t y);

/// <summary>
/// Sets the layer transparency register (pg 31)
/// </summary>
void RA8875_set_layer_transparency(RA8875_context_t* ctx, uint8_t scrollMode, uint8_t floatingWindowsEnable, uint8_t displayMode);

/// <summary>
/// Sets the memory write control register layer. This has the sideeffect of disabling the graphic cursor.
/// </summary>
void RA8875_set_writing_layer(RA8875_context_t* ctx, uint8_t layer);

/* BTE */

/*

    ROP defines binary operations applied between operations, where D is destination data and S is source data:

    0000b 0 ( Blackness )
    0001b ~S・~D or ~ ( S+D )
    0010b ~S・D
    0011b ~S
    0100b S・~D
    0101b ~D
    0110b S^D
    0111b ~S+~D or ~ ( S・D )
    1000b S・D
    1001b ~ ( S^D )
    1010b D
    1011b ~S+D
    1100b S
    1101b S+~D
    1110b S+D
    1111b 1 ( Whiteness ) 

*/

#define RA8875_ROP_SRC 0b1100

/// <summary>
/// Draws a block of pixels from memory onto the display.
/// </summary>
void RA8875_bte_write(RA8875_context_t* ctx, uint16_t x, uint16_t y, uint8_t layer, uint16_t width, uint16_t height, uint8_t rop, uint8_t* data);

/// <summary>
/// Copies data already on the screen around from one place to another.
/// </summary>
void RA8875_bte_move(RA8875_context_t* ctx, uint16_t srcX, uint16_t srcY, uint8_t srcLayer, uint16_t dstX, uint16_t dstY, uint8_t dstLayer, uint16_t width, uint16_t height, uint8_t negative, uint8_t rop);

/// <summary>
/// Fills a rectangle on the display.
/// </summary>
void RA8875_bte_fill(RA8875_context_t* ctx, uint16_t x, uint16_t y, uint8_t layer, uint16_t width, uint16_t height, uint8_t color);

