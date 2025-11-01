# ESP32 RA8875 Display Screen

Repository for Formula SAE steering wheel display firmware.

This demo showcases:
- Display initialization
- Custom fonts and graphics
- Interrupt handling for switching between Debug and Main screens

using the Adafruit RA8875 display controller running on ESP32 with ESP-IDF with [Roman-Port’s RA8875 driver](https://github.com/Roman-Port/RA8875) for the driver.

If facing issues during build, you may need to empty the compilerPath field in .vscode/c_cpp_properties.json and full clean:
    "compilerPath": "",
