/**
* Author: Richard Li
* Editors: Richard Li
*/
#include <stdbool.h>
#include "controller.h"
#include "display.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"

#define BTN_GPIO_tmp1_DI            19
#define BTN_GPIO_tmp2_DI            20
#define BTN_GPIO_tmp3_DI            21
#define BTN_GPIO_TOGGLE_MODE_DI     48  // Switch between main/debug - mode
#define BTN_GPIO_TOGGLE_SUBMODE_DI  47  // Switch laps/RTD depending on screen - submode

#define DEBOUNCE_TIME_US (200 * 1000)

volatile bool toggle_mode_pressed = false;
volatile bool toggle_submode_pressed = false;

static volatile int64_t last_mode_press_time_us = 0;
static volatile int64_t last_submode_press_time_us = 0;

static void IRAM_ATTR Controller_ISR_Mode(void* arg)
{
    int64_t current_time_us = esp_timer_get_time();
    if ((current_time_us - last_mode_press_time_us) > DEBOUNCE_TIME_US) {
        toggle_mode_pressed = true;
        last_mode_press_time_us = current_time_us;
    }
}

static void IRAM_ATTR Controller_ISR_Submode(void* arg)
{
    int64_t current_time_us = esp_timer_get_time();
    if ((current_time_us - last_submode_press_time_us) > DEBOUNCE_TIME_US) {
        toggle_submode_pressed = true;
        last_submode_press_time_us = current_time_us;
    }
}

void Controller_Init()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_GPIO_TOGGLE_MODE_DI) | (1ULL << BTN_GPIO_TOGGLE_SUBMODE_DI),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_GPIO_TOGGLE_MODE_DI, Controller_ISR_Mode, NULL);
    gpio_isr_handler_add(BTN_GPIO_TOGGLE_SUBMODE_DI, Controller_ISR_Submode, NULL);
    gpio_intr_enable(BTN_GPIO_TOGGLE_MODE_DI);
    gpio_intr_enable(BTN_GPIO_TOGGLE_SUBMODE_DI);
}