#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/** Pixel struct */
typedef struct {
    uint8_t r, g, b;
} pixel_t;

/** Represents an image */
typedef struct {
    size_t w, h;
    pixel_t* pixels;
} img_t;

/** Which pixel you want to target when adding a colour transformation
    rule */
typedef enum { RC_R = 0, RC_G = 1, RC_B = 2 } RC_RGB_t;
/** Add a colour transformation rule */
extern void recolour_addRule(RC_RGB_t greater, RC_RGB_t lower, float factor);

/** Access a pixel of an image */
#define A(I, x, y) (I.pixels[I.w * x + y])

#endif
