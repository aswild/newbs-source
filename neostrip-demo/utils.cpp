#include <cmath>

#include "utils.h"

uint32_t scale_color(float scale, uint32_t color)
{
    if (scale <= 0.0)
        return 0;
    if (scale >= 1.0)
        return color;

    return scale_color(scale, RED(color), GREEN(color), BLUE(color));
}

uint32_t scale_color(float scale, uint8_t red, uint8_t green, uint8_t blue)
{
    if (scale <= 0.0)
        return 0;
    if (scale >= 1.0)
        return TO_RGB(red, green, blue);

    return TO_RGB((uint8_t)round(red*scale), (uint8_t)round(green*scale), (uint8_t)round(blue*scale));
}

// Converts HSV to RGB with the given hue, assuming
// maximum saturation and value
uint32_t hue_to_rgb(float h)
{
    // lots of floating point magic from the internet and scratching my head
    float r, g, b;
    if (h > 360)
        h -= 360;
    if (h < 0)
        h += 360;
    int i = (int)(h / 60.0);
    float f = (h / 60.0) - i;
    float q = 1 - f;

    switch (i % 6)
    {
        case 0: r = 1; g = f; b = 0; break;
        case 1: r = q; g = 1; b = 0; break;
        case 2: r = 0; g = 1; b = f; break;
        case 3: r = 0; g = q; b = 1; break;
        case 4: r = f; g = 0; b = 1; break;
        case 5: r = 1; g = 0; b = q; break;
        default: r = 0; g = 0; b = 0; break;
    }

    // scale to integers and return the packed value
    uint8_t R = (uint8_t)(r * 255);
    uint8_t G = (uint8_t)(g * 255);
    uint8_t B = (uint8_t)(b * 255);

    return (R << 16) | (G << 8) | B;
}

