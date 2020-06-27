/*------------------------------------------------------------------------
  Ported to STM32F411 for VEXTREME by Technobly

  ------------------------------------------------------------------------
  -- original header follows ---------------------------------------------
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

#ifndef _ADAFRUIT_DOT_STAR_H_
#define _ADAFRUIT_DOT_STAR_H_

// Color-order flag for LED pixels (optional extra parameter to constructor):
// Bits 0,1 = R index (0-2), bits 2,3 = G index, bits 4,5 = B index
#define RGB_RGB (0 | (1 << 2) | (2 << 4))
#define RGB_RBG (0 | (2 << 2) | (1 << 4))
#define RGB_GRB (1 | (0 << 2) | (2 << 4))
#define RGB_GBR (2 | (0 << 2) | (1 << 4))
#define RGB_BRG (1 | (2 << 2) | (0 << 4))
#define RGB_BGR (2 | (1 << 2) | (0 << 4))

// Addressable LED Preset Colors
static const uint32_t colors[10] = {
    0,        // off
    0xFF0000, // red
    0xFF9900, // orange
    0xFFFF00, // yellow
    0x00FF00, // green
    0x00FFFF, // cyan
    0x0000FF, // blue
    0x7700FF, // pink
    0xFF00FF, // magenta
    0xFFFFFF  // white
};

/* 8-bit gamma-correction table.
   Copy & paste this snippet into a Python REPL to regenerate:
import math
gamma=2.6
for x in range(256):
    print("{:3},".format(int(math.pow((x)/255.0,gamma)*255.0+0.5))),
    if x&15 == 15: print
*/
static const uint8_t _LedsGammaTable[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,
    3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  7,
    7,  7,  8,  8,  8,  9,  9,  9, 10, 10, 10, 11, 11, 11, 12, 12,
   13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,
   20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,
   30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,
   42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
   58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,
   76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,
   97, 99,100,102,103,105,106,108,109,111,112,114,115,117,119,120,
  122,124,125,127,129,130,132,134,136,137,139,141,143,145,146,148,
  150,152,154,156,158,160,162,164,166,168,170,172,174,176,178,180,
  182,184,186,188,191,193,195,197,199,202,204,206,209,211,213,215,
  218,220,223,225,227,230,232,235,237,240,242,245,247,250,252,255};

// Fun routines
uint32_t Wheel(uint8_t WheelPos);
void rainbow(uint8_t wait);
void rainbowCycle(uint8_t wait);
void rainbowStep(uint8_t step);
void colorWipe(bool dir, uint32_t c, uint8_t wait);
uint32_t dimColor(uint32_t color, uint8_t width);
uint32_t colorWheel(uint8_t WheelPos);
void knightRider(uint16_t cycles, uint16_t speed, uint8_t width, uint16_t first, uint16_t last, uint32_t color);
void theaterChaseRainbow(int wait);

void ledsInitHW(uint16_t n, uint8_t o);
void ledsInitSW(uint16_t n, uint32_t dp, uint16_t d, uint32_t cp, uint16_t c, uint8_t o);
void updateLength(uint16_t n);
void ledsEnd(void);                                  // Destructor
void ledsBegin(void);                                // Prime pins/SPI for output
void ledsClear(void);                                // Set all pixel data to zero
void ledsSetBrightness(uint8_t);                     // Set global brightness 0-255
void ledsSetPixelColor(uint16_t n, uint32_t c);
void ledsSetPixelColorRGB(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
void ledsUpdate(void);                               // Issue color data to strip

uint32_t ledsColor(uint8_t r, uint8_t g, uint8_t b); // R,G,B to 32-bit color
uint32_t ledsGetPixelColor(uint16_t n);              // Return 32-bit pixel color

uint16_t ledsNumPixels(void);                        // Return number of pixels

uint8_t ledsGetBrightness(void);                     // Return global brightness
uint8_t *ledsGetPixels(void);                        // Return pixel data pointer

/**
 * A gamma-correction function for 32-bit packed RGB or WRGB
 * colors. Makes color transitions appear more perceptially correct.
 */
uint32_t ledsColorHSV2(uint16_t hue, uint8_t sat, uint8_t val);
uint32_t ledsGamma32(uint32_t x);

#endif // _ADAFRUIT_DOT_STAR_H_
