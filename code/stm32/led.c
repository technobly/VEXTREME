
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include "xprintf.h"
#include "delay.h"
#include "sys.h"

extern system_options sys_opt;

// APA102-2020 Driver - SPI2 Hardware
// CS   - 33 - PB12  - SPI2_NSS  - NA v0.2 HW (currently V-HALT)
// SCLK - 34 - PB13  - SPI2_SCK  - As of v0.2 HW (use SW SPI)
// SDAT - 35 - PB14  - SPI2_MISO - As of v0.2 HW (use SW SPI)
// DO   - 36 - PB15  - SPI2_MOSI - NA v0.2 HW (currently V-CE)

/*------------------------------------------------------------------------
  Ported to STM32F411 for VEXTREME by Technobly

  ------------------------------------------------------------------------
  -- original header follows ---------------------------------------------
  ------------------------------------------------------------------------

  Arduino library to control Adafruit Dot Star addressable RGB LEDs.

  Written by Limor Fried and Phil Burgess for Adafruit Industries.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  ------------------------------------------------------------------------
  This file is part of the Adafruit Dot Star library.

  Adafruit Dot Star is free software: you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License
  as published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Adafruit Dot Star is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with NeoPixel.  If not, see <http://www.gnu.org/licenses/>.
  ------------------------------------------------------------------------*/

#include "led.h"
#include <string.h>

#define USE_HW_SPI 255 // Assign this to dataPin to indicate 'hard' SPI

uint16_t numLEDs;                // Number of pixels
uint16_t dataPin;                // If soft SPI, data pin #
uint32_t dataPort;               // If soft SPI, data pin port #
uint16_t clockPin;               // If soft SPI, clock pin #
uint32_t clockPort;              // If soft SPI, clock pin port #
uint8_t brightness;              // Global brightness setting
uint8_t *pixels;                 // LED RGB values (3 bytes ea.)
uint8_t rOffset;                 // Index of red in 3-byte pixel
uint8_t gOffset;                 // Index of green byte
uint8_t bOffset;                 // Index of blue byte

void leds_hw_spi_init(void);     // Start hardware SPI
void leds_hw_spi_end(void);      // Stop hardware SPI
void leds_sw_spi_init(void);     // Start bitbang SPI
void leds_sw_spi_out(uint8_t n); // Bitbang SPI write
void leds_sw_spi_end(void);      // Stop bitbang SPI




//--------------- FUN ROUTINES START --------------

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(uint8_t WheelPos) {
    if (WheelPos < 85) {
        return ledsColor(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if (WheelPos < 170) {
        WheelPos -= 85;
        return ledsColor(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
        WheelPos -= 170;
        return ledsColor(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}

// Cycle through the color Wheel one complete revolution on all LEDs
void rainbow(uint8_t wait) {
    uint16_t i, j;
    for (j=0; j<256; j++) {
        for (i=0; i<ledsNumPixels(); i++) {
            ledsSetPixelColor(i, Wheel((i+j) & 255));
        }
        ledsUpdate();
        delay(wait);
    }
}

// Slightly different, this makes the rainbow equally distributed throughout, then wait (ms)
void rainbowCycle(uint8_t wait) {
    uint16_t i, j;
    for (j = 0; j < 256; j++) { // 1 cycle of all colors on wheel
        for (i = 0; i < ledsNumPixels(); i++) {
            ledsSetPixelColor(i, Wheel(((i * 256 / ledsNumPixels()) + j) & 255));
        }
        ledsUpdate();
        delay(wait);
    }
}

#if 1
void rainbowStep(uint8_t step) {
    (void) step;
    static const uint8_t STEP_SIZE = 8;
    static const uint32_t c[6] = {
        0xFF0000, // red
        0xFFFF00, // yellow
        0x00FF00, // green
        0x00FFFF, // cyan
        0x0000FF, // blue
        0xFF00FF, // magenta
    };
    uint8_t *p;
    static uint16_t n = 0;
    static uint8_t w = 0, j = 0;
    if (j < (6 * STEP_SIZE)) {
        j++;
    } else {
        j = 0;
    }
    for (n = 0; n < numLEDs; n++) {
        w = ((j / (STEP_SIZE / 2)) + n) % 6;
        p = &pixels[n * 3];
        if (sys_opt.hw_ver < 0x001e && sys_opt.rgb_type == RGB_TYPE_4) {
            if (n <= 2 || n == 4 || n == 6 || n == 8) {
                p[rOffset] = 0;
                p[gOffset] = 0;
                p[bOffset] = 0;
                continue;
            }
        }
        p[rOffset] = (uint8_t)(c[w] >> 16);
        p[gOffset] = (uint8_t)(c[w] >>  8);
        p[bOffset] = (uint8_t)c[w];
    }
    ledsUpdate();
}
#else
// Designed to step through a distributed rainbow equally distributed across the LEDs
void rainbowStep(uint8_t step) {
    static uint16_t n = 0;
    static uint8_t w = 0, j = 0;
    uint8_t *p;
    j += step;
    for (n = 0; n < numLEDs; n++) {
        w = ((n * 256 / numLEDs) + j) & 255;
        p = &pixels[n * 3];
        if (sys_opt.hw_ver < 0x001e && sys_opt.rgb_type == RGB_TYPE_4) {
            if (n <= 2 || n == 4 || n == 6 || n == 8) {
                p[rOffset] = 0;
                p[gOffset] = 0;
                p[bOffset] = 0;
                continue;
            }
        }
        if (w < 85) {
            p[rOffset] = w * 3;
            p[gOffset] = 255 - w;
            p[bOffset] = 0;
        } else if (w < 170) {
            w -= 85;
            p[rOffset] = 255 - w * 3;
            p[gOffset] = 0;
            p[bOffset] = w * 3;
        } else {
            w -= 170;
            p[rOffset] = 0;
            p[gOffset] = w * 3;
            p[bOffset] = 255 - w * 3;
        }
    }
    ledsUpdate();
}
#endif
// Wipe a color across all LEDs with a particular direction and speed/delay
void colorWipe(bool dir, uint32_t c, uint8_t wait) {
    int i;
    if (dir) {
        for (i = 0; i < ledsNumPixels(); i++) {
            ledsSetPixelColor(i, c);
            ledsUpdate();
            delay(wait);
        }
    } else {
        for (i = ledsNumPixels() - 1; i >= 0; i--) {
            ledsSetPixelColor(i, c);
            ledsUpdate();
            delay(wait);
        }
    }
}

uint32_t dimColor(uint32_t color, uint8_t width) {
   return (((color&0xFF0000)/width)&0xFF0000) + (((color&0x00FF00)/width)&0x00FF00) + (((color&0x0000FF)/width)&0x0000FF);
}

// Using a counter and for() loop, input a value 0 to 251 to get a color value.
// The colors transition like: red - org - ylw - grn - cyn - blue - vio - mag - back to red.
// Entering 255 will give you white, if you need it.
uint32_t colorWheel(uint8_t WheelPos) {
  uint8_t state = WheelPos / 21;
  switch(state) {
    case 0: return ledsColor(255, 0, 255 - ((((WheelPos % 21) + 1) * 6) + 127)); break;
    case 1: return ledsColor(255, ((WheelPos % 21) + 1) * 6, 0); break;
    case 2: return ledsColor(255, (((WheelPos % 21) + 1) * 6) + 127, 0); break;
    case 3: return ledsColor(255 - (((WheelPos % 21) + 1) * 6), 255, 0); break;
    case 4: return ledsColor(255 - (((WheelPos % 21) + 1) * 6) + 127, 255, 0); break;
    case 5: return ledsColor(0, 255, ((WheelPos % 21) + 1) * 6); break;
    case 6: return ledsColor(0, 255, (((WheelPos % 21) + 1) * 6) + 127); break;
    case 7: return ledsColor(0, 255 - (((WheelPos % 21) + 1) * 6), 255); break;
    case 8: return ledsColor(0, 255 - ((((WheelPos % 21) + 1) * 6) + 127), 255); break;
    case 9: return ledsColor(((WheelPos % 21) + 1) * 6, 0, 255); break;
    case 10: return ledsColor((((WheelPos % 21) + 1) * 6) + 127, 0, 255); break;
    case 11: return ledsColor(255, 0, 255 - (((WheelPos % 21) + 1) * 6)); break;
    default: return ledsColor(0, 0, 0); break;
  }
}

// Cycles - one cycle is scanning through all pixels left then right (or right then left)
// Speed - how fast one cycle is (32 with 16 pixels is default KnightRider speed)
// Width - how wide the trail effect is on the fading out LEDs.  The original display used
//         light bulbs, so they have a persistance when turning off.  This creates a trail.
//         Effective range is 2 - 8, 4 is default for 16 pixels.  Play with this.
// Color - 32-bit packed RGB color value.  All pixels will be this color.
// knightRider(cycles, speed, width, color);
void __attribute__((optimize("O0"))) knightRider(uint16_t cycles, uint16_t speed, uint8_t width, uint16_t first, uint16_t last, uint32_t color) {
  uint32_t old_val[last+1]; // up to 256 lights!
  // Larson time baby!
  for (int i = 0; i < cycles; i++) {
    for (int count = first+1; count<last+1; count++) {
      ledsSetPixelColor(count, color);
      old_val[count] = color;
      for (int x = count; x>0; x--) {
        old_val[x-1] = dimColor(old_val[x-1], width);
        ledsSetPixelColor(x-1, old_val[x-1]);
      }
      ledsUpdate();
      delay(speed);
    }
    for (int count = last+1-1; count>=first; count--) {
      ledsSetPixelColor(count, color);
      old_val[count] = color;
      for (int x = count; x<=last+1 ;x++) {
        old_val[x-1] = dimColor(old_val[x-1], width);
        ledsSetPixelColor(x+1, old_val[x+1]);
      }
      ledsUpdate();
      delay(speed);
    }
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for (int a=0; a<30; a++) {  // Repeat 30 times...
    for (int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      ledsClear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for (int c=b; c<ledsNumPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (ledsNumPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / ledsNumPixels();
        uint32_t color = ledsGamma32(ledsColorHSV2(hue, 255, 255)); // hue -> RGB
        ledsSetPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      ledsUpdate();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}

//--------------- FUN ROUTINES END --------------





// Constructor for hardware SPI -- must connect to MOSI, SCK pins
void ledsInitHW(uint16_t n, uint8_t o) {
    dataPin = USE_HW_SPI;
    brightness = 0;
    pixels = NULL;
    rOffset = o & 3;
    gOffset = (o >> 2) & 3;
    bOffset = (o >> 4) & 3;

    updateLength(n);
}

// Constructor for 'soft' (bitbang) SPI -- any two pins can be used
void ledsInitSW(uint16_t n, uint32_t dp, uint16_t d,
    uint32_t cp, uint16_t c, uint8_t o)
{
    dataPort = dp;
    dataPin = d;
    clockPort = cp;
    clockPin = c;
    brightness = 0;
    pixels = NULL;
    rOffset = o & 3;
    gOffset = (o >> 2) & 3;
    bOffset = (o >> 4) & 3;

    if (dataPin == USE_HW_SPI) {
        leds_hw_spi_init();
    } else {
        leds_sw_spi_init();
    }

    updateLength(n);
}

void updateLength(uint16_t n) {
    if (pixels) {
        free(pixels);
    }
    uint16_t bytes = n * 3;
    if ((pixels = (uint8_t *)malloc(bytes))) {
        numLEDs = n;
        ledsClear();
    } else {
        numLEDs = 0;
    }
}

void ledsEnd(void) { // Destructor
    if (pixels) {
        free(pixels);
    }
    if (dataPin == USE_HW_SPI) {
        leds_hw_spi_init();
    } else {
        leds_sw_spi_init();
    }
}

// SPI STUFF ---------------------------------------------------------------

void leds_hw_spi_init() { // Initialize hardware SPI

  // 72MHz / 4 = 18MHz (sweet spot)
  // Any slower than 18MHz and you are barely faster than Software SPI.
  // Any faster than 18MHz and the code overhead dominates.

/* v0.2 hardware not ready for HW SPI2
    // TODO: update GPIO code to SPI2
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE,
            GPIO3 | GPIO4 | GPIO5);
    gpio_set_af(GPIOB, GPIO_AF5, GPIO3 | GPIO4 | GPIO5);

    rcc_periph_clock_enable(RCC_SPI2);
    SPI1_I2SCFGR = 0;
    spi_init_master(SPI2, SPI_CR1_BAUDRATE_FPCLK_DIV_2, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1, 
        SPI_CR1_DFF_8BIT,SPI_CR1_MSBFIRST);
    spi_enable_software_slave_management(SPI2);
    spi_set_nss_high(SPI2);
    spi_enable(SPI2);
*/
}

void leds_hw_spi_end() { // Stop hardware SPI
    spi_disable(SPI2);
}

void leds_sw_spi_init() { // Init 'soft' (bitbang) SPI
    // re-init pins if leds_sw_spi_end() is implemented
    gpio_clear(dataPort, dataPin);
    gpio_clear(clockPort, clockPin);
}

void leds_sw_spi_end() { // Stop 'soft' SPI
    // de-init pins if desired
}

void leds_sw_spi_out(uint8_t n) { // Bitbang SPI write
    for (uint8_t i=8; i--; n <<= 1) {
        if (n & 0x80) {
            gpio_set(dataPort, dataPin);
        } else {
            gpio_clear(dataPort, dataPin);
        }
        gpio_set(clockPort, clockPin);
        gpio_clear(clockPort, clockPin);
    }
}

/* ISSUE DATA TO LED STRIP -------------------------------------------------

  Although the LED driver has an additional per-pixel 5-bit brightness
  setting, it is NOT used or supported here because it's a brain-dead
  misfeature that's counter to the whole point of Dot Stars, which is to
  have a much faster PWM rate than NeoPixels.  It gates the high-speed
  PWM output through a second, much slower PWM (about 400 Hz), rendering
  it useless for POV.  This brings NOTHING to the table that can't be
  already handled better in one's sketch code.  If you really can't live
  without this abomination, you can fork the library and add it for your
  own use, but any pull requests for this will NOT be merged, nuh uh!
*/

void ledsUpdate() {
    if (!pixels) {
        return;
    }

    uint8_t *ptr = pixels, i;            // -> LED data
    uint16_t n   = numLEDs;              // Counter

    //__disable_irq(); // If 100% focus on SPI clocking required

    if (dataPin == USE_HW_SPI) {

        for (i=0; i<4; i++) {
            spi_xfer(SPI2, 0x00);             // 4 byte start-frame marker
        }
        do {                                  // For each pixel...
            spi_xfer(SPI2, (0xE0 + brightness)); // Factor in brightness value
            for (i=0; i<3; i++) {
                spi_xfer(SPI2, *ptr++);       // Write R,G,B
            }
        } while(--n);

        // Four end-frame bytes are seemingly indistinguishable from a white
        // pixel, and empirical testing suggests it can be left out...but it's
        // always a good idea to follow the datasheet, in case future hardware
        // revisions are more strict (e.g. might mandate use of end-frame
        // before start-frame marker).  i.e. let's not remove this.
        for (i=0; i<4; i++) {
            spi_xfer(SPI2, 0xFF);
        }
    } else {                                 // Soft (bitbang) SPI
        // xprintf("Update SW SPI\n");
        for (i=0; i<4; i++) {
            leds_sw_spi_out(0);              // Start-frame marker
        }
        do {                                 // For each pixel...
            leds_sw_spi_out(0xE0 + brightness); // Factor in brightness value
            for (i=0; i<3; i++) {
                leds_sw_spi_out(*ptr++);     // R,G,B
            }
        } while(--n);
        for (i=0; i<4; i++) {
            leds_sw_spi_out(0xFF);           // End-frame marker (see note above)
        }
    }

    //__enable_irq();
}

void ledsClear() { // Write 0s (off) to full pixel buffer
    memset(pixels, 0, numLEDs * 3);
}

// Set pixel color, separate R,G,B values (0-255 ea.)
void ledsSetPixelColorRGB(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
    if (sys_opt.hw_ver < 0x1e && sys_opt.rgb_type == RGB_TYPE_4) {
        if (n <= 2 || n == 4 || n == 6 || n == 8) {
            uint8_t *p = &pixels[n * 3];
            p[rOffset] = 0;
            p[gOffset] = 0;
            p[bOffset] = 0;
            return;
        }
    }
    if (n < numLEDs) {
        uint8_t *p = &pixels[n * 3];
        p[rOffset] = r;
        p[gOffset] = g;
        p[bOffset] = b;
    }
}

// Set pixel color, 'packed' RGB value (0x000000 - 0xFFFFFF)
void ledsSetPixelColor(uint16_t n, uint32_t c) {
    if (sys_opt.hw_ver < 0x1e && sys_opt.rgb_type == RGB_TYPE_4) {
        if (n <= 2 || n == 4 || n == 6 || n == 8) {
            uint8_t *p = &pixels[n * 3];
            p[rOffset] = 0;
            p[gOffset] = 0;
            p[bOffset] = 0;
            return;
        }
    }
    if (n < numLEDs) {
        uint8_t *p = &pixels[n * 3];
        p[rOffset] = (uint8_t)(c >> 16);
        p[gOffset] = (uint8_t)(c >>  8);
        p[bOffset] = (uint8_t)c;
    }
}

// Convert separate R,G,B to packed value
uint32_t ledsColor(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Read color from previously-set pixel, returns packed RGB value.
uint32_t ledsGetPixelColor(uint16_t n) {
    if (n >= numLEDs) {
        return 0;
    }
    uint8_t *p = &pixels[n * 3];
    return ((uint32_t)p[rOffset] << 16) |
           ((uint32_t)p[gOffset] <<  8) |
            (uint32_t)p[bOffset];
}

uint16_t ledsNumPixels() { // Ret. strip length
    return numLEDs;
}

// Set global strip brightness.  This does not have an immediate effect;
// must be followed by a call to show(). Good news is that brightness setting
// in this library is 'non destructive' -- it's applied as global data
// being issued to the upper byte of each pixel, not during setPixel(),
// and also means that getPixelColor() returns the exact value originally stored.
void ledsSetBrightness(uint8_t b) {
    // global brightness value is 5 bits in the MSB of each 32-bit pixel data
    // 111xxxxx RRRRRRRR GGGGGGGG BBBBBBBB, where xxxxx is 0 - 31 brightness
    brightness = b >> 3;
}

uint8_t ledsGetBrightness() {
    return brightness << 3; // Reverse above operation
}

// Return pointer to the library's pixel data buffer.  Use carefully,
// much opportunity for mayhem.  It's mostly for code that needs fast
// transfers, e.g. SD card to LEDs.  Color data is in BGR order.
uint8_t *ledsGetPixels() {
    return pixels;
}

/*!
  @brief   Convert hue, saturation and value into a packed 32-bit RGB color
           that can be passed to setPixelColor() or other RGB-compatible
           functions.
  @param   hue  An unsigned 16-bit value, 0 to 65535, representing one full
                loop of the color wheel, which allows 16-bit hues to "roll
                over" while still doing the expected thing (and allowing
                more precision than the wheel() function that was common to
                prior NeoPixel examples).
  @param   sat  Saturation, 8-bit value, 0 (min or pure grayscale) to 255
                (max or pure hue). Default of 255 if unspecified.
  @param   val  Value (brightness), 8-bit value, 0 (min / black / off) to
                255 (max or full brightness). Default of 255 if unspecified.
  @return  Packed 32-bit RGB with the most significant byte set to 0 -- the
           white element of WRGB pixels is NOT utilized. Result is linearly
           but not perceptually correct, so you may want to pass the result
           through the gamma32() function (or your own gamma-correction
           operation) else colors may appear washed out. This is not done
           automatically by this function because coders may desire a more
           refined gamma-correction function than the simplified
           one-size-fits-all operation of gamma32(). Diffusing the LEDs also
           really seems to help when using low-saturation colors.
*/
uint32_t ledsColorHSV2(uint16_t hue, uint8_t sat, uint8_t val) {

    uint8_t r, g, b;

    // Remap 0-65535 to 0-1529. Pure red is CENTERED on the 64K rollover;
    // 0 is not the start of pure red, but the midpoint...a few values above
    // zero and a few below 65536 all yield pure red (similarly, 32768 is the
    // midpoint, not start, of pure cyan). The 8-bit RGB hexcone (256 values
    // each for red, green, blue) really only allows for 1530 distinct hues
    // (not 1536, more on that below), but the full unsigned 16-bit type was
    // chosen for hue so that one's code can easily handle a contiguous color
    // wheel by allowing hue to roll over in either direction.
    hue = (hue * 1530L + 32768) / 65536;
    // Because red is centered on the rollover point (the +32768 above,
    // essentially a fixed-point +0.5), the above actually yields 0 to 1530,
    // where 0 and 1530 would yield the same thing. Rather than apply a
    // costly modulo operator, 1530 is handled as a special case below.

    // So you'd think that the color "hexcone" (the thing that ramps from
    // pure red, to pure yellow, to pure green and so forth back to red,
    // yielding six slices), and with each color component having 256
    // possible values (0-255), might have 1536 possible items (6*256),
    // but in reality there's 1530. This is because the last element in
    // each 256-element slice is equal to the first element of the next
    // slice, and keeping those in there this would create small
    // discontinuities in the color wheel. So the last element of each
    // slice is dropped...we regard only elements 0-254, with item 255
    // being picked up as element 0 of the next slice. Like this:
    // Red to not-quite-pure-yellow is:        255,   0, 0 to 255, 254,   0
    // Pure yellow to not-quite-pure-green is: 255, 255, 0 to   1, 255,   0
    // Pure green to not-quite-pure-cyan is:     0, 255, 0 to   0, 255, 254
    // and so forth. Hence, 1530 distinct hues (0 to 1529), and hence why
    // the constants below are not the multiples of 256 you might expect.

    // Convert hue to R,G,B (nested ifs faster than divide+mod+switch):
    if (hue < 510) {         // Red to Green-1
        b = 0;
        if (hue < 255) {     //   Red to Yellow-1
            r = 255;
            g = hue;         //     g = 0 to 254
        } else {             //   Yellow to Green-1
            r = 510 - hue;   //     r = 255 to 1
            g = 255;
        }
    } else if (hue < 1020) { // Green to Blue-1
        r = 0;
        if (hue <  765) {    //   Green to Cyan-1
            g = 255;
            b = hue - 510;   //     b = 0 to 254
        } else {             //   Cyan to Blue-1
            g = 1020 - hue;  //     g = 255 to 1
            b = 255;
        }
    } else if (hue < 1530) { // Blue to Red-1
        g = 0;
        if (hue < 1275) {    //   Blue to Magenta-1
            r = hue - 1020;  //     r = 0 to 254
            b = 255;
        } else {             //   Magenta to Red-1
            r = 255;
            b = 1530 - hue;  //     b = 255 to 1
        }
    } else {                 // Last 0.5 Red (quicker than % operator)
        r = 255;
        g = b = 0;
    }

    // Apply saturation and value to R,G,B, pack into 32-bit result:
    uint32_t v1 =   1 + val; // 1 to 256; allows >>8 instead of /255
    uint16_t s1 =   1 + sat; // 1 to 256; same reason
    uint8_t  s2 = 255 - sat; // 255 to 0
    return ((((((r * s1) >> 8) + s2) * v1) & 0xff00) << 8) |
            (((((g * s1) >> 8) + s2) * v1) & 0xff00)       |
           ( ((((b * s1) >> 8) + s2) * v1)           >> 8);
}

// A 32-bit variant of gamma8() that applies the same function
// to all components of a packed RGB or WRGB value.
uint32_t ledsGamma32(uint32_t x) {
    uint8_t *y = (uint8_t *)&x;
    // All four bytes of a 32-bit value are filtered even if RGB (not WRGB),
    // to avoid a bunch of shifting and masking that would be necessary for
    // properly handling different endianisms (and each byte is a fairly
    // trivial operation, so it might not even be wasting cycles vs a check
    // and branch for the RGB case). In theory this might cause trouble *if*
    // someone's storing information in the unused most significant byte
    // of an RGB value, but this seems exceedingly rare and if it's
    // encountered in reality they can mask values going in or coming out.
    for (uint8_t i=0; i<4; i++) {
        y[i] = _LedsGammaTable[y[i]]; // 0-255 in, 0-255 out
    }
    return x; // Packed 32-bit return
}
