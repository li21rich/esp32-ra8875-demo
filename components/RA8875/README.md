# RA8875 Driver for ESP32

This is a driver for the [RA8875](https://www.adafruit.com/product/1590) SPI display driver for the ESP32. It's built for the Espressif toolchain.

## Installation

* In the root of your project, create a "components" folder. Then, clone this repo into a new folder in it.
* Next, in the "main" folder at the root of your project, edit the CMakeLists file. In the ``idf_component_register`` line, add a (or append to) ``PRIV_REQUIRES`` argument with the value ``"RA8875"`` (or whatever the name of the folder you cloned this repo into in step 1 was called). Save the file.

## Software Setup

First, bring in the include file with ``#include "RA8875.h"``.

There are two required calls needed to set up the display. You'll need to call ``RA8875_init`` and ``RA8875_configure`` to get the display working, passing in a pointer to a ``RA8875_context_t`` struct you'll allocate. Additionally, you'll probably want to turn on the backlight and clear the screen with ``RA8875_set_backlight_brightness`` and ``RA8875_clear``.

### Initialization

``RA8875_init`` accepts arguments for the pins you'll be using, as well as the SPI host controller. Some of the pins need to be long to the dedicated pins for the SPI host, or else speed suffers. Here's my recommended configuration:

* **Host**: ``HSPI_HOST``
* **Speed**: 2800000 (about the maximum I could get working)
* **MOSI**: Pin 23
* **MISO**: Pin 19
* **SCLK**: Pin 18
* **CS**: Pin 27 (this one can be just about any GPIO)
* **INT**: Pin 14 (this one can be just about any GPIO)

### Configuration

``RA8875_configure`` accepts arguments that are used to configure the RA8875. Frankly, I have no clue what they should be or how they work. Most of the settings I used are borrowed from [Adafruit's driver](https://github.com/adafruit/Adafruit_RA8875/blob/master/Adafruit_RA8875.cpp) for their 800x480 display. View the example below for those settings. They might not work for your display.

### Example

```c
#include "RA8875.h"

RA8875_context_t lcd;

void app_main(void)
{
    //Initialize the display
    RA8875_init(&lcd, HSPI_HOST, 2800000, 23, 19, 18, 27, 14);
    RA8875_configure(&lcd, 26, 32, 96, 0, 32, 23, 2, 800, 480, 0);
    RA8875_set_backlight_brightness(&lcd, 0xFF);
    RA8875_clear(&lcd);
}
```

## Hardware Setup

Setting up the hardware is pretty straightforward. Connect the MOSI, MISO, SCLK, CS, and INT pins of the RA8875 to the GPIOs you specified earlier in the software setup. Then, connect power (3.3v!) and ground. That should be all you need, at least if you're using the Adafruit driver board like I am.

## Usage

Once you've initialized the display, you'll mostly want to use the BTE (block transform engine) commands. Refer to the "RA8875.h" header for more.

* **RA8875_bte_write**: Draws raw data from memory onto the display in a rectangle.
* **RA8875_bte_move**: Copies a block of pixels already on the display to a different location on the display.
* **RA8875_bte_fill**: Fills a rectangle with solid pixels with the color of your choosing.

## Some Tips

### BTE ROP

Some BTE operations have a parameter called "ROP". This allows you to apply boolean operations on your input data. If you just want to copy directly, use ``RA8875_ROP_SRC`` here. Otherwise, refer to the following table of operations that can be applied where S is source data and D is destination data.

| **ROP** | **Operation**      |
|---------|--------------------|
| 0000b   | 0 (Blackness)      |
| 0001b   | ~S * ~D or ~ (S+D) |
| 0010b   | ~S * D             |
| 0011b   | ~S                 |
| 0100b   | S * ~D             |
| 0101b   | ~D                 |
| 0110b   | S^D                |
| 0111b   | ~S+~D or ~ (S * D) |
| 1000b   | S * D              |
| 1001b   | ~ (S^D)            |
| 1010b   | D                  |
| 1011b   | ~S+D               |
| 1100b   | S                  |
| 1101b   | S+~D               |
| 1110b   | S+D                |
| 1111b   | 1 (Whiteness)      |

### Double-Buffering

By default, this display driver doesn't use or support any kind of double-buffering. Double-buffering allows you to update the screen without the user being able to watch it update (ugly!).

However, the display driver does support two layers (although at higher resolutions this sacrifices color depth) that can be displayed independently. You can simulate double buffering by only showing one layer at a time, drawing to the inactive layer, and then swapping the shown layer when you're done.

To do this when calling BCE calls, create a boolean flag ``active`` and, pass it as the target layer. Then, at the end of your rendering loop, swap the active layer by calling ``RA8875_set_layer_transparency(ctx, 0, 0, !active);`` and then swap the active flag with ``active = !active;``. This will mask your updates and make the display look a lot cleaner.

I sacrificed color depth (65535 down to 255) for this feature and I highly recommend you do the same.

### Datasheet

The datasheet I refered to while writing this is available [here](https://cdn-shop.adafruit.com/datasheets/RA8875_DS_V19_Eng.pdf) ([mirror](https://web.archive.org/web/20220613182339/https://cdn-shop.adafruit.com/datasheets/RA8875_DS_V19_Eng.pdf)). Note that it wasn't translated all that well, and there are a number of errors in it I noticed. Yikes.