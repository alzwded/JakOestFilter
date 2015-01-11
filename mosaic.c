#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "common.h"

/** abritrary maximum number of colours */
#define MAXCOLOURS 24

/** the colours */
static pixel_t colours[MAXCOLOURS];
static size_t ncolours = 0;
static unsigned char distrib[256];

/** used to add a new colour */
void mosaic_addColour(
        unsigned char r,
        unsigned char g,
        unsigned char b)
{
    pixel_t colour = { r, g, b };
    if(ncolours < MAXCOLOURS) {
        colours[ncolours] = colour;
        ncolours++;
    } else {
        fprintf(stderr, "Too many colours!\n");
        abort();
    }
}

static inline void _process(pixel_t pin, pixel_t* p)
{
    size_t i = 0;

    int med = (float)(GET(pin, RC_R)
            + GET(pin, RC_G)
            + GET(pin, RC_B)
            ) / 3.f;
    *p = colours[distrib[med]];
}

typedef struct {
    size_t i;
    img_t in, out;
} tdata_t;

static void _tprocess(void* data)
{
    tdata_t* mydata = (tdata_t*)data;
    size_t j;

    for(j = 0; j < mydata->in.w; ++j) {
        _process(A(mydata->in, mydata->i, j), &A(mydata->out, mydata->i, j));
    }

    free(mydata);
}

static void _detdist(img_t img)
{
    int cls[256];
    size_t i, j;
    int thresh, current, count;

    for(i = 0; i < 256; ++i) {
        cls[i] = 0;
    }

    thresh = 0;
    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            pixel_t p = A(img, i, j);
            int med = (float)(
                    GET(p, RC_R)
                    + GET(p, RC_G)
                    + GET(p, RC_B)
                    ) / 3.f;
            thresh += cls[med] == 0;
            cls[med] = 1;
        }
    }

    thresh /= ncolours;
    current = count = 0;

    for(i = 0; i < 256; ++i) {
        count += (cls[i] > 0);
        if(count > thresh) {
            ++current;
            count = 0;
        }

        distrib[i] = current;
    }
}

/** apply a colour transformation based on relationships between colour
  components (RGB)
  rules are added with recolour_addRule */
img_t mosaic(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;

    // determine distribution
    _detdist(img);

    // process
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = (tdata_t*)malloc(sizeof(tdata_t));
        data->i = i;
        data->in = img;
        data->out = ret;
        _tprocess(data);
    }
    
    return ret;
}
