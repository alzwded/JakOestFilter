#ifndef COMMON_H
#define COMMON_H

typedef struct {
    unsigned char r, g, b;
} pixel_t;

typedef struct {
    size_t w, h;
    pixel_t* pixels;
} img_t;

#define A(I, x, y) (I.pixels[I.w * x + y])

#endif
