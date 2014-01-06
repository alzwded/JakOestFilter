#ifndef COMMON_H
#define COMMON_H

typedef struct {
    unsigned char r, g, b;
} pixel_t;

#define A800(p, x, y) (p[800 * x + y])

#endif
