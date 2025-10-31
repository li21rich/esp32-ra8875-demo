/**
* Author: Richard Li
* Editors: Richard Li
* Additional Notes:
* - This code is for the Main Screen. Debug screen not implemented yet.
* - Downloaded fonts i.e. Comic Sans add 3s render time. Internal fonts are instant.
* - Comic Sans W, G, and % character values in comicsans_font.h need to be fixed
*/
#include <stdio.h>
#include <string.h>
#include "display.h"
#include <time.h>

#define COLOR_GREEN       32
#define COLOR_RED         5

#define LABELS_Y_OFFSET 11
#define VALUES_Y_OFFSET 55

void app_main(void) 
{
    clock_t start = clock();

    Display_Init();
    
    // =======================
    // ====== DRAWINGS =======
    // ======================= 

    Display_EnableDrawMode();

    // Background rectangles 
    Display_DrawRect(200, 165, 400, 240, COLOR_GREEN, 1);
    Display_DrawRect(600, 45, 800, 120, COLOR_RED, 1);
    Display_DrawRect(200, 285, 600, 360, COLOR_RED, 1);

    // White Borders in Main Screen
    Display_DrawBordersMain();


    // =======================
    // ======== TEXT =========
    // ======================= 
    
    // Text Labels 
    Display_EnableTextModeAndFont(DISPLAY_FONT_COMIC_SANS); 
    Display_WriteTextAt(20, 0 + LABELS_Y_OFFSET, "LV Voltage");
    Display_WriteTextAt(340, 0 + LABELS_Y_OFFSET, "Last Lap");
    Display_WriteTextAt(610, 0 + LABELS_Y_OFFSET, "Pack Voltage");
    Display_WriteTextAt(10, 120 + LABELS_Y_OFFSET, "Max Motor T");
    Display_WriteTextAt(270, 120 + LABELS_Y_OFFSET, "Diff");
    Display_WriteTextAt(470, 120 + LABELS_Y_OFFSET, "Pred");
    Display_WriteTextAt(650, 120 + LABELS_Y_OFFSET, "Rotor T");
    Display_WriteTextAt(20, 240 + LABELS_Y_OFFSET, "Max IGBT T");
    Display_WriteTextAt(350, 240 + LABELS_Y_OFFSET, "Pack %");
    Display_WriteTextAt(620, 240 + LABELS_Y_OFFSET, "Max Cell T");
    Display_WriteTextAt(30, 360 + LABELS_Y_OFFSET, "Inv Fault");
    Display_WriteTextAt(310, 360 + LABELS_Y_OFFSET, "Endurance %");
    Display_WriteTextAt(640, 360 + LABELS_Y_OFFSET, "TV");
    Display_WriteTextAt(740, 360 + LABELS_Y_OFFSET, "TC");

    clock_t end = clock();
    double elapsed_ms = ((double)(end - start)) * 1000.0 / CLOCKS_PER_SEC;
    printf("Loading time: %.2f ms\n", elapsed_ms);

    // Text Values
    float defaultFloat = 0.0;
    int defaultInt = 0;
    
    Display_EnableTextModeAndFont(DISPLAY_FONT_INTERNAL);  
    Display_WriteNumberAt(50, 0 + VALUES_Y_OFFSET, false, defaultFloat);
    Display_WriteNumberAt(350, 0 + VALUES_Y_OFFSET, false, defaultFloat);
    Display_WriteNumberAt(690, 0 + VALUES_Y_OFFSET, true, defaultInt);
    Display_WriteNumberAt(80, 120 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(250, 120 + VALUES_Y_OFFSET, false, defaultFloat);   
    Display_WriteNumberAt(490, 120 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(690, 120 + VALUES_Y_OFFSET, true, defaultInt);  
    Display_WriteNumberAt(80, 240 + VALUES_Y_OFFSET, true, defaultInt);    
    Display_WriteNumberAt(390, 240 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(690, 240 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteTextAt(45, 360 + VALUES_Y_OFFSET, "None"); 
    Display_WriteNumberAt(390, 360 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(640, 360 + VALUES_Y_OFFSET, true, defaultInt);   
    Display_WriteNumberAt(740, 360 + VALUES_Y_OFFSET, true, defaultInt);   

   
}