#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "JakWorkers.h"

static inline void _process(pixel_t pin[], pixel_t* p)
{
    size_t i = 0;

#define COMP(A, B) (memcmp((unsigned char*)&A, (unsigned char*)&B, 3 * sizeof(uint8_t)) == 0)
    if(COMP(pin[0], pin[4]) ^ COMP(pin[4], pin[8])
            || COMP(pin[1], pin[4]) ^ COMP(pin[4], pin[7])
            || COMP(pin[2], pin[4]) ^ COMP(pin[4], pin[6])
            || COMP(pin[3], pin[4]) ^ COMP(pin[4], pin[5])
    ) {
        static pixel_t black = { 0, 0, 0 };
        *p = black;
        return;
    }
#undef COMP
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
        pixel_t blank = A(mydata->in, mydata->i, j);
        pixel_t square[9] = {
        /* /     0, 1, 2 */
        /* 0 */  blank, blank, blank,
        /* 1 */  blank, A(mydata->in, mydata->i, j), blank,
        /* 2 */  blank, blank, blank //
        };
        if(mydata->i > 0) {
            if(j > 0) {
                square[0] = A(mydata->in, mydata->i - 1, j - 1);
            }
            square[1] = A(mydata->in, mydata->i - 1, j);
            if(j < mydata->in.w - 1) {
                square[2] = A(mydata->in, mydata->i - 1, j + 1);
            }
        }
        if(j > 0) {
            square[3] = A(mydata->in, mydata->i, j - 1);
        }
        if(j < mydata->in.w - 1) {
            square[5] = A(mydata->in, mydata->i, j + 1);
        }
        if(mydata->i < mydata->in.h - 1) {
            if(j > 0) {
                square[6] = A(mydata->in, mydata->i + 1, j - 1);
            }
            square[7] = A(mydata->in, mydata->i + 1, j);
            if(j < mydata->in.w - 1) {
                square[8] = A(mydata->in, mydata->i + 1, j + 1);
            }
        }
        _process(square, &A(mydata->out, mydata->i, j));
    }

    free(mydata);
}

/** apply a colour transformation based on relationships between colour
  components (RGB)
  rules are added with recolour_addRule */
img_t mobord(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;

    extern img_t mosaic(img_t const);
    img_t tmp = mosaic(img);

    jw_config_t conf = JW_CONFIG_INITIALIZER;

    // process
    jw_init(conf);
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = (tdata_t*)malloc(sizeof(tdata_t));
        data->i = i;
        data->in = tmp;
        data->out = ret;
        jw_add_job(&_tprocess, data);
    }
    jw_main();

    free(tmp.pixels);
    
    return ret;
}
