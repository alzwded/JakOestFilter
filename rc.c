#include <stdlib.h>
#include <string.h>
#include "common.h"

img_t recolour(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };
    memcpy(ret.pixels, img.pixels, img.w * img.h * sizeof(pixel_t));
    return ret;
}
