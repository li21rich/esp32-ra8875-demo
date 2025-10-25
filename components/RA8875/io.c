#include "include/RA8875.h"
#include <string.h>

#define RA8875_DATAWRITE 0x00
#define RA8875_DATAREAD 0x40
#define RA8875_CMDWRITE 0x80
#define RA8875_CMDREAD 0xC0

void RA8875_write_command(RA8875_context_t* ctx, uint8_t reg) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.cmd = RA8875_CMDWRITE;
    t.tx_data[0] = reg;
    t.flags = SPI_TRANS_USE_TXDATA;
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_device, &t);
    assert(ret==ESP_OK);
}

void RA8875_write_data(RA8875_context_t* ctx, uint8_t value) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.cmd = RA8875_DATAWRITE;
    t.tx_data[0] = value;
    t.flags = SPI_TRANS_USE_TXDATA;
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_device, &t);
    assert(ret==ESP_OK);
}

void RA8875_write_data_block(RA8875_context_t* ctx, const uint8_t* buffer, int nbytes) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nbytes * 8;
    t.cmd = RA8875_DATAWRITE;
    t.tx_buffer = buffer;
    t.flags = 0;
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_device, &t);
    assert(ret==ESP_OK);
}

uint8_t RA8875_read_data(RA8875_context_t* ctx) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.cmd = RA8875_DATAREAD;
    t.tx_data[0] = 0;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_device, &t);
    assert(ret==ESP_OK);
    return t.rx_data[0];
}

void RA8875_write_register(RA8875_context_t* ctx, uint8_t reg, uint8_t value) {
    //write_command and write_data combined together for optimization
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 24;
    t.cmd = RA8875_CMDWRITE;
    t.tx_data[0] = reg;
    t.tx_data[1] = RA8875_DATAWRITE;
    t.tx_data[2] = value;
    t.flags = SPI_TRANS_USE_TXDATA;
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_device, &t);
    assert(ret==ESP_OK);
}

uint8_t RA8875_read_register(RA8875_context_t* ctx, uint8_t reg) {
    //write_command and read_data combined together for optimization
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 24;
    t.cmd = RA8875_CMDWRITE;
    t.tx_data[0] = reg;
    t.tx_data[1] = RA8875_DATAREAD;
    t.tx_data[2] = 0;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_device, &t);
    assert(ret==ESP_OK);
    return t.rx_data[2];
}