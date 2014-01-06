#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "common.h"

#define PI_2 (3.14159 / 2.0)
#define SKEW(X) ((PI_2 - atan(X)) / PI_2)

static inline void _modif(pixel_t* p, img_t const img, float vh, float vw, size_t i, size_t j)
{
    float r = .0f, g = .0f, b = .0f;
    float s = .0f;

    long minh = (long)(((long)i - 1) * (vh));
    long minw = (long)(((long)j - 1) * (vw));
    long maxh = (long)(((long)i + 1) * (vh));
    long maxw = (long)(((long)j + 1) * (vw));

    //printf("\t%f %f\n", vw, vh);
    //printf("\t%ld %ld %ld %ld\n", minh, maxh, minw, maxw);

    long ii, jj;

    for(ii = minh; ii <= maxh; ++ii) {
        if(ii < 0 || ii >= img.h) continue;
        for(jj = minw; jj <= maxw; ++jj) {
            if(jj < 0 || jj >= img.w) continue;

            float y = (vh * i) - (ii);
            float x = (vw * j) - (jj);
            float vv = (vh + vw) / 2.0;
            float dist = abs( y * y + x * x ) / (vv * vv);
            float skew = 0.0;

            dist = dist * dist * dist * dist;

            skew = SKEW(dist);

            r += skew * A(img, ii, jj).r;
            g += skew * A(img, ii, jj).g;
            b += skew * A(img, ii, jj).b;
            s += skew;
        }
    }

    (*p).r = (int)(r / s);
    (*p).g = (int)(g / s);
    (*p).b = (int)(b / s);
}

img_t downSample800(img_t const img)
{
    img_t ret = { 800, 800, (pixel_t*)malloc(800 * 800 * sizeof(pixel_t)) };

    size_t i, j;

    float vw = (float)img.w / ret.w;
    float vh = (float)img.h / ret.h;

    assert(img.w >= 800 && img.h >= 800);

    for(i = 0; i < ret.h; ++i) {
        if(i % 50 == 0) {
            printf(" %.0f%%", (float)i * 100.0f / ret.h);
            fflush(stdout);
        }
        for(j = 0; j < ret.w; ++j) {
            _modif(&A(ret, i, j), img, vh, vw, i, j);

        }
    }
    printf(" 100%%\n");

    return ret;
}
