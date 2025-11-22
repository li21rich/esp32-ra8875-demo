# ESP32 RA8875 Steering Wheel Display

Repository for Formula SAE steering wheel display firmware, intended for ESP32-S3-WROOM-1 microcontroller and Adafruit RA8875 display board.

Demo including:
- Display initialization,
- Custom fonts and graphics, 
- & Interrupt and fault handling with two buttons toggling between non-RTD and RTD Debug and Main screens,

using the Adafruit RA8875 display controller running on ESP32 with ESP-IDF with [Roman-Port’s RA8875 driver](https://github.com/Roman-Port/RA8875) for the driver.

If IntelliSense errors occur, try emptying the compilerPath field in .vscode/c_cpp_properties.json and perform a full clean build:
    "compilerPath": ""

Additional notes:
 - See RA8875.h for driver library functions. 
 - [RA8875 Datasheet](https://support.midasdisplays.com/wp-content/uploads/2025/06/RA8875.pdf)
 - [Steering Wheel UI design](https://docs.google.com/spreadsheets/d/1wyTeVe2CrvfaHK9Z1gjt5AWtrlrcMFISqQ3uaPND4KI/edit)
 - Downloaded Comic Sans font adds a few seconds loading time and must be blit out across the screen. By contrast, using internal font is instant.