### ESP32 RA8875 Steering Wheel Display
Firmware for Formula SAE's new live steering wheel display system with the ESP32-S3-WROOM-1 microcontroller and RA8875 display board. Demo excludes CAN data acquisition.
### Includes
- Display initialization (115ms)
- Driver telemetry
- Graphics rendering
- Interrupt and fault handling with two buttons toggling between non-RTD and RTD Debug and Main screens

using the RA8875 display controller running on ESP32 using ESP-IDF with [Roman-Port’s RA8875 driver](https://github.com/Roman-Port/RA8875) for the internal driver.

### Troubleshooting
If IntelliSense errors occur, try clearing the "compilerPath" field in .vscode/c_cpp_properties.json and perform a full clean build:
```json
    "compilerPath": ""
```

## Additional notes:
 - See RA8875.h for driver library functions. 
 - [RA8875 Datasheet](https://support.midasdisplays.com/wp-content/uploads/2025/06/RA8875.pdf)
 - [Steering Wheel UI design](https://docs.google.com/spreadsheets/d/1wyTeVe2CrvfaHK9Z1gjt5AWtrlrcMFISqQ3uaPND4KI/edit)
