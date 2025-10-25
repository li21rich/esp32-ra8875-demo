#include "include/RA8875.h"
#include "include/RA8875_registers.h"
#include "driver/gpio.h"

static void set_bte_src(RA8875_context_t* ctx, uint16_t x, uint16_t y, uint8_t layer) {
    RA8875_write_register(ctx, 0x54, x);
    RA8875_write_register(ctx, 0x55, x >> 8);
    RA8875_write_register(ctx, 0x56, y);
    RA8875_write_register(ctx, 0x57, (y >> 8) | (layer << 7));
}

static void set_bte_dst(RA8875_context_t* ctx, uint16_t x, uint16_t y, uint8_t layer) {
    RA8875_write_register(ctx, 0x58, x);
    RA8875_write_register(ctx, 0x59, x >> 8);
    RA8875_write_register(ctx, 0x5A, y);
    RA8875_write_register(ctx, 0x5B, (y >> 8) | (layer << 7));
}

static void set_bte_size(RA8875_context_t* ctx, uint16_t width, uint16_t height) {
    RA8875_write_register(ctx, 0x5C, width & 0xFF);
    RA8875_write_register(ctx, 0x5D, width >> 8);
    RA8875_write_register(ctx, 0x5E, height & 0xFF);
    RA8875_write_register(ctx, 0x5F, height >> 8);
}

static void set_bte_foreground(RA8875_context_t* ctx, uint8_t color) {
    //Set colors (in 256-color mode, we set only the lowest 3:3:2 bits)
    RA8875_write_register(ctx, 0x63, (color >> 0) & 0b111);
    RA8875_write_register(ctx, 0x64, (color >> 3) & 0b111);
    RA8875_write_register(ctx, 0x65, (color >> 6) & 0b11);
}

static void set_bte_opcode(RA8875_context_t* ctx, uint8_t opcode, uint8_t rop) {
    RA8875_write_register(ctx, 0x51, opcode | (rop << 4));
}

static void exec_bte(RA8875_context_t* ctx) {
    RA8875_write_register(ctx, 0x50, 1 << 7); // execute
}

static void wait_for_interrupt(RA8875_context_t* ctx, uint8_t mask) {
    // This won't take long, so just spinlock until it's found
    uint8_t status;
    do {
        while (gpio_get_level(ctx->pin_int)); // active low
        status = RA8875_read_register(ctx, 0xF1); // query
        RA8875_write_register(ctx, 0xF1, status); // clear
    } while (!(status & mask));    
}

#define MAX_BLOCK_SIZE 512
#define INT_BTE_RW 1
#define INT_BTE_COMPLETED (1 << 1)

void RA8875_bte_write(RA8875_context_t* ctx, uint16_t x, uint16_t y, uint8_t layer, uint16_t width, uint16_t height, uint8_t rop, uint8_t* data) {
    //Calculate total length
    int len = (int)width * (int)height;

    //Setup
    set_bte_dst(ctx, x, y, layer);
    set_bte_size(ctx, width, height);
    set_bte_opcode(ctx, 0x00, rop);
    exec_bte(ctx);

    //Wait for interrupt
    wait_for_interrupt(ctx, INT_BTE_RW);

    //Transfer image data in blocks
    int offset = 0;
    int blockLen;
    while (offset < len) {
        //Calculate block len
        blockLen = len - offset;
        blockLen = blockLen > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : blockLen;

        //Transfer
        RA8875_write_command(ctx, 0x02);
        RA8875_write_data_block(ctx, &data[offset], blockLen);

        //Wait for interrupt
        wait_for_interrupt(ctx, INT_BTE_RW);

        //Update state
        offset += blockLen;
    }
}

void RA8875_bte_move(RA8875_context_t* ctx, uint16_t srcX, uint16_t srcY, uint8_t srcLayer, uint16_t dstX, uint16_t dstY, uint8_t dstLayer, uint16_t width, uint16_t height, uint8_t negative, uint8_t rop) {
    set_bte_src(ctx, srcX, srcY, srcLayer);
    set_bte_dst(ctx, dstX, dstY, dstLayer);
    set_bte_size(ctx, width, height);
    set_bte_opcode(ctx, negative ? 0x3 : 0x2, rop);
    exec_bte(ctx);
    wait_for_interrupt(ctx, INT_BTE_COMPLETED);
}

void RA8875_bte_fill(RA8875_context_t* ctx, uint16_t x, uint16_t y, uint8_t layer, uint16_t width, uint16_t height, uint8_t color) {
    set_bte_dst(ctx, x, y, layer);
    set_bte_size(ctx, width, height);
    set_bte_opcode(ctx, 0x0C, 0);
    set_bte_foreground(ctx, color);
    exec_bte(ctx);
    wait_for_interrupt(ctx, INT_BTE_COMPLETED);
}