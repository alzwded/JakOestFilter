#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "common.h"

extern int opt_alt;
extern int opt_balt;
extern int opt_calt;

typedef struct {
    size_t i;
    img_t img;
    float* h,* s,* v;
    float* err;
} tddata_t;

#define COLOR_THRESH 2.f
#define CLAMP(X, MIN, MAX) (X > MAX ? MAX : X < MIN ? MIN : X)
#define MAX(A, B, C) ((A >= B && A >= C) ? A : (B >= A && B >= C) ? B : C)
#define MIN(A, B, C) ((A <= B && A <= C) ? A : (B <= A && B <= C) ? B : C)
#define MY(WHAT, J) (mydata->WHAT[mydata->i * mydata->img.w + J])
#define MYP1(WHAT, J) (mydata->WHAT[(mydata->i+1) * mydata->img.w + J])
#define DISTRIBUTE_ERR(ERR, J)\
    do {\
        if(J < mydata->img.w - 1)\
            MY(err, J + 1) += err * 7.f/16.f;\
        if(mydata->i < mydata->img.h - 1) {\
            if(J > 0) \
                MYP1(err, J - 1) += err * 3.f/16.f;\
            MYP1(err, J) += err * 5.f/16.f;\
            if(J < mydata->img.w - 1)\
                MYP1(err, J + 1) += err * 1.f/16.f;\
        }\
    } while(0)


static void _proc_toHSL(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        pixel_t ip = A(mydata->img, mydata->i, j);

        float M = MAX(ip.r, ip.g, ip.b)/255.f;
        float m = MIN(ip.r, ip.g, ip.b)/255.f;
        float c = M - m;
        float v = M;
        float s = (v < 0.0001 ? 0.f : c/v);

        float rm, gc;
        if(!opt_calt) {
            rm = (!opt_alt ? (ip.r+ip.b)/2.f : ip.r);
            gc = (!opt_alt ? ip.g : (ip.g+ip.b)/2.f);
        } else {
            rm = (!opt_alt ? (ip.r+ip.g)/2.f : (ip.r + ip.b)/2.f);
            gc = (!opt_alt ? ip.b : ip.g);
        }

        float h = (gc - rm)/(gc + rm);
        h = (float)(opt_balt) * (h * COLOR_THRESH) + (float)(!opt_balt) * (h * .5f + .5f);

        MY(h, j) = h;
        MY(s, j) = s;
        MY(v, j) = v;

#if 0
        printf("#%02x%02x%02x = (%f,%f,%f)\n",
                ip.r, ip.g, ip.b,
                h, s, v);
#endif
    }
}

inline void _proc_ditherValue(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float Nu = MY(err, j);
        float old = MY(v, j);
        float new = round(CLAMP(old + Nu, 0.f, 1.f));
        float err = old - new;
        DISTRIBUTE_ERR(err, j);
        MY(v, j) = new;
    }
}

inline void _proc_ditherSaturation(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float Nu = MY(err, j);
        float old = MY(s, j);
        float new = round(CLAMP(old + Nu, 0.f, 1.f));
        float err = old - new;
        DISTRIBUTE_ERR(err, j);
        MY(s, j) = new;
    }
}

inline void _proc_ditherHue(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float Nu = MY(err, j);
        float old = MY(h, j);
        float new = (!opt_balt)
            ? round(CLAMP(old + Nu, 0.f, 1.f))
            : round(CLAMP(old + Nu, -COLOR_THRESH, COLOR_THRESH));
        float err = old - new;
        DISTRIBUTE_ERR(err, j);
        MY(h, j) = new;
    }
}

static void _output_layer(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        pixel_t p = A(mydata->img, mydata->i, j);

        if(MY(s, j) < .5f) {
            //printf("desaturated ");
            p.r = p.g = p.b = (MY(v, j) >= .5f) * 255;
        } else if(MY(v, j) < .5f) {
            //printf("devalued ");
            p.r = p.g = p.b = 0;
        } else {
            //printf("use hue ");
            p.r = (MY(h, j) < .5f) * 255;
            p.g = (MY(h, j) > .5f-opt_balt) * 255;
            p.b = (!opt_alt) * 255;
        }

#if 0
        printf("%3zdx%3zd: MGW, RGB = \n\t%f %f %f\n\t%x %x %x\n",
                mydata->i, j,
                MY(h, j), MY(s, j), MY(v, j),
                p.r, p.g, p.b);
#endif

        A(mydata->img, mydata->i, j) = p;
    }
}

/*
   Multi-step Algorithm

   No. Step                         Colour Space
   0. in RGB                        (r[], g[], b[])
   1. RGB -> HSV                    (h[], s[], v[])
   2. dither_h                      (M|C|Y, s[], l[])
   3. dither_s                      (M|C|Y, isGray?, v[])
   4. dither_v                      (M|C|Y, isGray?, isColor?)
   6. _output_layer                 (r[], g[], b[])
        isGray?
            isColor?
                out = FFFFFF
            else
                out = 000000
        else white|color|black
            is white
                out = FFFFFF
            is black 
                out = 000000
            is color
                is M
                    out = FF00FF
                is Y
                    out = FFFFFF
                is C
                    out = 00FFFF
    opt_alt implies the blue channel is 0
    opt_balt allows "yellow", i.e. Y or W are also used as a color when s & v are 1
 */
img_t cgaditherfs(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;
    tddata_t* ddatas;

    float* h = (float*)malloc(img.w * img.h * sizeof(float));
    float* s = (float*)malloc(img.w * img.h * sizeof(float));
    float* v = (float*)malloc(img.w * img.h * sizeof(float));
    float* err = (float*)malloc(img.w * img.h * sizeof(float));

    // process
    // 1. compute HSV
    ddatas = (tddata_t*)malloc(img.h * sizeof(tddata_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = img;
        data->h = h;
        data->s = s;
        data->v = v;
        data->err = err;
        _proc_toHSL(data);
    }

    // 2. dither colors to magenta or cyan
    memset(err, 0, img.w * img.h * sizeof(float));
    // can't omp because each pixel depends on previous 4
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_ditherHue(data);
    }
    memset(err, 0, img.w * img.h * sizeof(float));
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_ditherSaturation(data);
    }
    memset(err, 0, img.w * img.h * sizeof(float));
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_ditherValue(data);
    }

    // 6. distribute pixels to C, M, black or white
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = ret;
        _output_layer(data);
    }

    free(h);
    free(s);
    free(v);
    free(err);
    free(ddatas);
    
    return ret;
}
