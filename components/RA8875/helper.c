#include "include/RA8875.h"
#include "include/RA8875_registers.h"

void RA8875_set_write_cursor_position(RA8875_context_t* ctx, uint16_t x, uint16_t y) {
    RA8875_write_register(ctx, RA8875_CURH0, x);
    RA8875_write_register(ctx, RA8875_CURH1, x >> 8);
    RA8875_write_register(ctx, RA8875_CURV0, y);
    RA8875_write_register(ctx, RA8875_CURV1, y >> 8);
}

void RA8875_set_read_cursor_position(RA8875_context_t* ctx, uint16_t x, uint16_t y) {
    RA8875_write_register(ctx, RA8875_RCURH0, x);
    RA8875_write_register(ctx, RA8875_RCURH1, x >> 8);
    RA8875_write_register(ctx, RA8875_RCURV0, y);
    RA8875_write_register(ctx, RA8875_RCURV1, y >> 8);
}

void RA8875_set_layer_transparency(RA8875_context_t* ctx, uint8_t scrollMode, uint8_t floatingWindowsEnable, uint8_t displayMode) {
    RA8875_write_register(ctx, 0x52, displayMode | (floatingWindowsEnable << 5) | (scrollMode << 6));
}

void RA8875_set_writing_layer(RA8875_context_t* ctx, uint8_t layer) {
    RA8875_write_register(ctx, 0x41, layer & 1);
}