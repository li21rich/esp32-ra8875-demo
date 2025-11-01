/**
* Author: Richard Li
* Editors: Richard Li
* Additional Notes:
* - Downloaded font Comic Sans adds 4s render time. Internal fonts are instant.
* - Comic Sans W, G, and % character values in comicsans_font.h need to be fixed
*/
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h" // see line 29

void app_main(void) 
{
    // Init
    Display_Init();
    Controller_Init(); 

    /* 
    * Disable watchdog for IDLE tasks since we do long renders.
    * Eventually this line has to be removed so watchdog is addressed properly.
    * Probably by implementing background render task?
    */
    esp_task_wdt_deinit();

    // Display main screen
    Display_RenderMainScreen();   

    while(1) {
        if (button_pressed) {
            button_pressed = false;
            Display_SwitchScreen();
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}