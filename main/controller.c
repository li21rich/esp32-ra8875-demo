/**
* Author: Richard Li
* Editors: Richard Li
*/
#include <stdbool.h>
#include "controller.h"
#include "display.h"
#include "esp_attr.h"
#include "driver/gpio.h"

#define BTN_GPIO_tmp1_DI 19
#define BTN_GPIO_tmp2_DI 20
#define BTN_GPIO_tmp3_DI 21
#define BTN_GPIO_tmp4_DI 47
#define BTN_GPIO_SELECT_DI 48

volatile bool button_pressed = false;

static void IRAM_ATTR Controller_ISR_Handler(void* arg)
{
    button_pressed = true;
}

void Controller_Init()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_GPIO_SELECT_DI),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_GPIO_SELECT_DI, Controller_ISR_Handler, NULL);
    gpio_intr_enable(BTN_GPIO_SELECT_DI);
}