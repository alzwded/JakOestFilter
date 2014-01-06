#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} pixel_t;

typedef struct {
    size_t w, h;
    pixel_t* pixels;
} img_t;

typedef enum { RC_R = 0, RC_G = 1, RC_B = 2 } RC_RGB_t;

extern void recolour_addRule(RC_RGB_t greater, RC_RGB_t lower, float factor);

#define A(I, x, y) (I.pixels[I.w * x + y])

#endif
