#include "include/RA8875.h"
#include "include/RA8875_registers.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>

int RA8875_init(RA8875_context_t* ctx, int host, int speed, int pinMosi, int pinMiso, int pinSclk, int pinCs, int pinInt) {
    //Clear context
    memset(ctx, 0, sizeof(RA8875_context_t));
    ctx->pin_int = pinInt;

    //Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = pinMosi,
        .miso_io_num = pinMiso,
        .sclk_io_num = pinSclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8000,
    };
    if (spi_bus_initialize(host, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK)
        return 0;

    //Initialize SPI device
    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = speed,
        .duty_cycle_pos = 128,
        .mode = 0,
        .spics_io_num = pinCs,
        .queue_size = 3
    };
    if (spi_bus_add_device(host, &devcfg, &ctx->spi_device) != ESP_OK)
        return 0;
    
    //Configure interrupt GPIO pin
    gpio_config_t pinConf = {
        .pin_bit_mask = 1U << pinInt,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&pinConf);

    //Test SPI connection; sometimes the screen just takes a bit to boot?
    while (RA8875_read_register(ctx, 0) != 0x75) {
        printf("ERROR: Got invalid value reading register 0x75. Check the SPI connection. Retrying...\n");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    //Enable BTE interrupts
    RA8875_write_register(ctx, 0xF0, 0b11); //enable
    RA8875_write_register(ctx, 0xF1, 0xFF); //clear

    return 1;
}

void RA8875_configure(RA8875_context_t* ctx, uint8_t hsync_nondisp, uint8_t hsync_start, uint8_t hsync_pw, uint8_t hsync_finetune, uint16_t vsync_nondisp, uint16_t vsync_start, uint8_t vsync_pw, uint16_t width, uint16_t height, uint16_t voffset) {
    //Turn display on
    RA8875_write_register(ctx, RA8875_PWRR, RA8875_PWRR_NORMAL | RA8875_PWRR_DISPON);

    //Enable TFT
    RA8875_write_register(ctx, RA8875_GPIOX, 1);

    //Configure backlight
    RA8875_write_register(ctx, RA8875_P1CR, RA8875_P1CR_ENABLE | (RA8875_PWM_CLK_DIV1024 & 0xF));

    //PLL Init
    RA8875_write_register(ctx, RA8875_PLLC1, RA8875_PLLC1_PLLDIV1 + 23); //+11
    RA8875_write_register(ctx, RA8875_PLLC2, RA8875_PLLC2_DIV1); //DIV4
    
    //Initialize LCD
    RA8875_write_register(ctx, RA8875_SYSR, RA8875_SYSR_8BPP);
    RA8875_write_register(ctx, RA8875_PCSR, RA8875_PCSR_PDATL | RA8875_PCSR_2CLK);

    //Enable two layers
    RA8875_write_register(ctx, 0x20, 1 << 7);

    //Horizontal settings registers
    RA8875_write_register(ctx, RA8875_HDWR, (width / 8) - 1); // H width: (HDWR + 1) * 8 = 480
    RA8875_write_register(ctx, RA8875_HNDFTR, RA8875_HNDFTR_DE_HIGH + hsync_finetune);
    RA8875_write_register(ctx, RA8875_HNDR, (hsync_nondisp - hsync_finetune - 2) / 8); // H non-display: HNDR * 8 + HNDFTR + 2 = 10
    RA8875_write_register(ctx, RA8875_HSTR, hsync_start / 8 - 1); // Hsync start: (HSTR + 1)*8
    RA8875_write_register(ctx, RA8875_HPWR, RA8875_HPWR_LOW + (hsync_pw / 8 - 1)); // HSync pulse width = (HPWR+1) * 8

    //Vertical settings registers
    RA8875_write_register(ctx, RA8875_VDHR0, (uint16_t)(height - 1 + voffset) & 0xFF);
    RA8875_write_register(ctx, RA8875_VDHR1, (uint16_t)(height - 1 + voffset) >> 8);
    RA8875_write_register(ctx, RA8875_VNDR0, vsync_nondisp - 1); // V non-display period = VNDR + 1
    RA8875_write_register(ctx, RA8875_VNDR1, vsync_nondisp >> 8);
    RA8875_write_register(ctx, RA8875_VSTR0, vsync_start - 1); // Vsync start position = VSTR + 1
    RA8875_write_register(ctx, RA8875_VSTR1, vsync_start >> 8);
    RA8875_write_register(ctx, RA8875_VPWR, RA8875_VPWR_LOW + vsync_pw - 1); // Vsync pulse width = VPWR + 1

    /* Set active window X */
    RA8875_write_register(ctx, RA8875_HSAW0, 0); // horizontal start point
    RA8875_write_register(ctx, RA8875_HSAW1, 0);
    RA8875_write_register(ctx, RA8875_HEAW0, (uint16_t)(width - 1) & 0xFF); // horizontal end point
    RA8875_write_register(ctx, RA8875_HEAW1, (uint16_t)(width - 1) >> 8);

    /* Set active window Y */
    RA8875_write_register(ctx, RA8875_VSAW0, 0 + voffset); // vertical start point
    RA8875_write_register(ctx, RA8875_VSAW1, 0 + voffset);
    RA8875_write_register(ctx, RA8875_VEAW0, (uint16_t)(height - 1 + voffset) & 0xFF); // vertical end point
    RA8875_write_register(ctx, RA8875_VEAW1, (uint16_t)(height - 1 + voffset) >> 8);
}

void RA8875_clear(RA8875_context_t* ctx) {
    RA8875_write_register(ctx, RA8875_MCLR, RA8875_MCLR_START | RA8875_MCLR_FULL);
}

void RA8875_set_backlight_brightness(RA8875_context_t* ctx, uint8_t brightness) {
    RA8875_write_register(ctx, RA8875_P1DCR, brightness);
}