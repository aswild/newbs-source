#ifndef _UTILS_H_
#define _UTILS_H_

#include <cstdint>

#define TO_RGB(r, g, b) ((((r) & 0xff) << 16) | \
                         (((g) & 0xff) << 8)  | \
                         ((b) & 0xff))

#define RED(_color)   (((color) >> 16) & 0xff)
#define GREEN(_color) (((color) >> 8) & 0xff)
#define BLUE(_color)  ((color) & 0xff)

uint32_t scale_color(float scale, uint32_t color);
uint32_t scale_color(float scale, uint8_t red, uint8_t green, uint8_t blue);
uint32_t hue_to_rgb(float h);

#endif // _UTILS_H_
