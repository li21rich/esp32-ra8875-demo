/**
* Author: Richard Li
* Editors: Richard Li
* Additional Notes:
* - Downloaded font Comic Sans adds a few seconds render time. Internal fonts are instant.
*/
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "controller.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void ToggleWhetherDrive(void) {
    if (CURRENT_SCREEN == SCREEN_DEBUG_NO_RTD) {
        Display_SwitchScreen(SCREEN_MAIN_NO_LAPS);
    } else { 
        Display_SwitchScreen(SCREEN_DEBUG_NO_RTD);
    }
}

static void ToggleDuringDrive(void) {
    switch (CURRENT_SCREEN) {
        case SCREEN_MAIN_NO_LAPS:
            Display_SwitchScreen(SCREEN_MAIN_LAPS);
            break;
        case SCREEN_MAIN_LAPS:
            Display_SwitchScreen(SCREEN_WARN);
            vTaskDelay(pdMS_TO_TICKS(550));
            Display_SwitchScreen(SCREEN_DEBUG_RTD);
            break;
        case SCREEN_DEBUG_RTD:
            Display_SwitchScreen(SCREEN_MAIN_NO_LAPS);
            break;
        default:
            break;
    }
}

void app_main(void) 
{
    // TEMP
    //esp_task_wdt_deinit();

    // Init
    Display_Init(); // Defaults to static debug screen
    Controller_Init();

    while (1) {
        if (toggle_mode_pressed) {
            toggle_mode_pressed = false;
            ToggleWhetherDrive();
            vTaskDelay(pdMS_TO_TICKS(1500));
        } else if (toggle_submode_pressed){
            toggle_submode_pressed = false;

            if (CURRENT_SCREEN != SCREEN_DEBUG_NO_RTD) {
                ToggleDuringDrive();
                vTaskDelay(pdMS_TO_TICKS(1500));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}