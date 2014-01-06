#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "common.h"

/** radius where the border begins to slowly fade to black */
#define RMIN (.55f)
/** radius where pure blackness happens */
#define RMAX (1.2f)

/** apply a round frame that goes beyond the photo */
img_t frame(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;
    size_t cx = img.w / 2;
    size_t cy = img.h / 2;

    //printf(" cx %d cy %d\n", cx, cy);

    // cache some values
    float R = (img.w > img.h) ? img.h / 2.0 : img.w / 2.0;
    float _c_rmin = RMIN * RMIN * R * R;
    float _c_rmax = RMAX * RMAX * R * R;

    // apply
    for(i = 0; i < ret.h; ++i) {
        for(j = 0; j < ret.w; ++j) {
            float dy = (signed long)cy - (signed long)i;
            float dx = (signed long)cx - (signed long)j;
            float d = abs(dy * dy + dx * dx);
            //printf(" cx %d cy %d dx %f dy %f d %f ref %f\n", cx, cy, dx, dy, d, RMIN * RMIN * R * R);
            // if it's beyond the inner radius, apply the border
            if(d > (_c_rmin)) {
                if(d < (_c_rmax)) {
                    //printf(" d %f ", d);
                    d = (d - _c_rmin) / (_c_rmax - _c_rmin);
                    //printf("after %f\n", d);
                    //A(ret, i, j).r = A(img, i, j).r * (1 - d * d);
                    //A(ret, i, j).g = A(img, i, j).g * (1 - d * d);
                    //A(ret, i, j).b = A(img, i, j).b * (1 - d * d);
                    A(ret, i, j).r = A(img, i, j).r * (1 - d);
                    A(ret, i, j).g = A(img, i, j).g * (1 - d);
                    A(ret, i, j).b = A(img, i, j).b * (1 - d);
                } else {
                    A(ret, i, j).r = A(ret, i, j).g = A(ret, i, j).b = 0;
                }
            // otherwise, just keep the original pixel
            } else {
                A(ret, i, j).r = A(img, i, j).r;
                A(ret, i, j).g = A(img, i, j).g;
                A(ret, i, j).b = A(img, i, j).b;
            }
        }
    }

    return ret;
}
