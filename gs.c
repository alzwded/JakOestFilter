#include <stdlib.h>
#include <string.h>
#include "common.h"

img_t getSquare(img_t const img)
{
    size_t l = (img.w > img.h) ? img.h : img.w;
    img_t ret = { l, l, (pixel_t*)malloc(l * l * sizeof(pixel_t)) };

    size_t hoffset = (img.w > img.h) ? 0 : ((img.h - img.w) / 2);
    size_t woffset = (img.w > img.h) ? ((img.w - img.h) / 2) : 0;

    size_t i = 0;

    for(; i < l; ++i) {
        memcpy(&ret.pixels[i * l], &img.pixels[hoffset * img.w + i * img.w + woffset], l * sizeof(pixel_t));
    }

    return ret;
}
