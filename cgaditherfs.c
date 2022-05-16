#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "common.h"

extern int opt_alt;

typedef struct {
    size_t i;
    img_t img;
    float* luma;
    float* color;
    float* err;
} tddata_t;

#define COLOR_THRESH 4.f
#define LUMA_THRESH 1.f
#define CLAMP(X, MIN, MAX) (X > MAX ? MAX : X < MIN ? MIN : X)
#define MY(WHAT, J) (mydata->WHAT[mydata->i * mydata->img.w + J])
#define MYP1(WHAT, J) (mydata->WHAT[(mydata->i+1) * mydata->img.w + J])
#define DISTRIBUTE_ERR(ERR, J)\
    do {\
        if(J < mydata->img.w - 1)\
            MY(err, J + 1) = err * 7.f/16.f;\
        if(mydata->i < mydata->img.h - 1) {\
            if(J > 0) \
                MYP1(err, J - 1) = err * 3.f/16.f;\
            MYP1(err, J) = err * 5.f/16.f;\
            if(J < mydata->img.w - 1)\
                MYP1(err, J + 1) = err * 1.f/16.f;\
        }\
    } while(0)


static inline float _sqr(uint8_t x) { return (float)x * (float)x; }

static inline float _computeLuma(pixel_t ip)
{
    return ((float)(ip.r + ip.g + ip.b) / 3.f / 255.f) * LUMA_THRESH;
}

static inline float _computeColor(pixel_t ip, float lum)
{
    float r = (float)(ip.r) / 255.f;
    float t = (float)(ip.g + ip.b) / 2.f / 255.f;
    float b = (float)(ip.b) / 255.f;
    float y = (float)(ip.r + ip.g) / 2.f / 255.f;
    float wcmb = !opt_alt;
    float rygb = opt_alt;
    float rb = wcmb * r + rygb * y;
    float gb = wcmb * t + rygb * b;
#if 1
    printf("For pixel #%02x%02x%02x\n\tr=%f\tt=%f\tb=%f\ty=%f\n\trb=%f\tgb=%f\n\tyield=%f\n",
            ip.r, ip.g, ip.b,
            r, t, b, y, rb, gb,
            gb / (rb + gb) * 2.f - 1.f);
#endif
    return (gb / (rb + gb) * 2.f - 1.f) * COLOR_THRESH;
}

static void _proc_getLuma(void* data)
{
    tddata_t* mydata = (tddata_t*)data;
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        MY(luma, j) = _computeLuma(A(mydata->img, mydata->i, j));
    }
}

static void _proc_getColor(void* data)
{
    tddata_t* mydata = (tddata_t*)data;
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        MY(color, j) = _computeColor(A(mydata->img, mydata->i, j), MY(luma, j));
    }
}

inline void _proc_ditherLuma(void* data)
{
    tddata_t* mydata = (tddata_t*)data;
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float Nu = MY(err, j);
        float ol = MY(luma, j);
        float nl = round(CLAMP(ol + Nu, 0, LUMA_THRESH));
        float err = ol - nl;
        DISTRIBUTE_ERR(err, j);
        MY(luma, j) = nl;
#if 0
        pixel_t p = A(mydata->img, mydata->i, j);
        printf("For pixel #%02x%02x%02x x=%zd y=%zd\n\tol=%f\tnl=%f\terr=%f\tNu=%f\n",
                p.r, p.g, p.b,
                mydata->i, j,
                ol,
                nl,
                err,
                Nu);
#endif
    }
}

inline void _proc_ditherColor(void* data)
{
    tddata_t* mydata = (tddata_t*)data;
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float Nu = MY(err, j);
        float oc = MY(color, j);
        float nc = round(CLAMP(oc + Nu, -COLOR_THRESH, COLOR_THRESH));
        float err = oc - nc;
#if 0
        printf(">>> %f %f\n", oc+Nu, nc);
#endif
        DISTRIBUTE_ERR(err, j);
        MY(color, j) = nc;
    }
}

static void _output_layer(void* data)
{
    tddata_t* mydata = (tddata_t*)data;
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        pixel_t p = A(mydata->img, mydata->i, j);

        if(MY(luma, j) < -.5f) {
            p.r = p.g = p.b = 0;
        } else {
            float c = MY(color, j);
            p.r = 255 * (c < 0.5f);
            p.g = 255 * (c > -0.5f);
            p.b = 255 * (!opt_alt);
            if(p.r == 255 && p.g == 255 && MY(luma,j) < 0.5f) {
                p.b = p.r = p.g = 0.f;
            }
        }

        printf("%f\n", MY(luma, j));
        p.r = p.g = p.b = MY(luma, j) * 255;
        printf("%d\n", p.r);

#if 0
        printf("%3zdx%3zd: MGW, RGB = \n\t%f %f %f\n\t%x %x %x\n",
                mydata->i, j,
                MY(luma, j), MY(color, j), MY(err, j),
                p.r, p.g, p.b);
#endif

        A(mydata->img, mydata->i, j) = p;
    }
}

/*
   Multi-step Algorithm

   No. Step                         Colour Space
   0. in RGB                        (r[], g[], b[])
   1. RGB -> HSL                    (h[], s[], l[])
   2. dither_m_c_y                  (M|C|Y, s[], l[])
   3. dither_s                      (M|C|Y, isGray?, l[])
   4. dither_l                      (M|C|Y, isGray?, isWhite?)
   5. dither_w_c_b                  (M|C|Y, isGray?, isWhite?, white|color|black)
   6. _output_layer                 (r[], g[], b[])
        isGray?
            isWhite?
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
 */
img_t cgaditherfs(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;
    tddata_t* ddatas;

    float* color = (float*)malloc(img.w * img.h * sizeof(float));
    float* luma = (float*)malloc(img.w * img.h * sizeof(float));
    float* err = (float*)malloc(img.w * img.h * sizeof(float));

    // process
    // 1. compute luma
    ddatas = (tddata_t*)malloc(img.h * sizeof(tddata_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = img;
        data->luma = luma;
        data->color = color;
        data->err = err;
        _proc_getLuma(data);
        _proc_getColor(data);
    }

    // 2. dither colors to magenta or cyan
    memset(err, 0, img.w * img.h * sizeof(float));
    // can't omp because each pixel depends on previous 4
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_ditherLuma(data);
    }
    memset(err, 0, img.w * img.h * sizeof(float));
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_ditherColor(data);
    }

    // 6. distribute pixels to C, M, black or white
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = ret;
        _output_layer(data);
    }

    free(color);
    free(luma);
    free(err);
    free(ddatas);
    
    return ret;
}
