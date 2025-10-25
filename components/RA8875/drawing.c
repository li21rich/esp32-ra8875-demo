#include "include/RA8875.h"
#include "include/RA8875_registers.h"

static void set_double_register(RA8875_context_t* ctx, uint8_t reg, uint16_t value) {
    RA8875_write_register(ctx, reg, value);
    RA8875_write_register(ctx, reg + 1, value >> 8);
}

void RA8875_draw_rect(RA8875_context_t* ctx, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, uint8_t filled) {
    //Set up
    set_double_register(ctx, 0x91, x1);
    set_double_register(ctx, 0x93, y1);
    set_double_register(ctx, 0x95, x2);
    set_double_register(ctx, 0x97, y2);

    //Set colors (in 256-color mode, we set only the lowest 3:3:2 bits)
    RA8875_write_register(ctx, 0x63, (color >> 0) & 0b111);
    RA8875_write_register(ctx, 0x64, (color >> 3) & 0b111);
    RA8875_write_register(ctx, 0x65, (color >> 6) & 0b11);

    //Execute
    RA8875_write_register(ctx, RA8875_DCR, filled ? 0xB0 : 0x90);
}

void RA8875_draw_data(RA8875_context_t* ctx, uint16_t x, uint16_t y, const uint8_t* buffer, int len) {
    //Set up
    set_double_register(ctx, RA8875_CURH0, x);
    set_double_register(ctx, RA8875_CURV0, y);

    //Send payload
    RA8875_write_command(ctx, RA8875_MRWC);
    RA8875_write_data_block(ctx, buffer, len);
}