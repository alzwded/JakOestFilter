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
    float* c,* l;
    float* err,* err2,* err3;
} tddata_t;

#define CLAMP(X, MIN, MAX) (X > MAX ? MAX : X < MIN ? MIN : X)
#define MAX(A, B, C) ((A >= B && A >= C) ? A : (B >= A && B >= C) ? B : C)
#define MIN(A, B, C) ((A <= B && A <= C) ? A : (B <= A && B <= C) ? B : C)
#define MY(WHAT, J) (mydata->WHAT[mydata->i * mydata->img.w + J])
#define MYP1(WHAT, J) (mydata->WHAT[(mydata->i+1) * mydata->img.w + J])
#define DISTRIBUTE_ERR(ERR, J)\
    do {\
        if(J < mydata->img.w - 1)\
            MY(err, J + 1) += ERR * 7.f/16.f;\
        if(mydata->i < mydata->img.h - 1) {\
            if(J > 0) \
                MYP1(err, J - 1) += ERR * 3.f/16.f;\
            MYP1(err, J) += ERR * 5.f/16.f;\
            if(J < mydata->img.w - 1)\
                MYP1(err, J + 1) += ERR * 1.f/16.f;\
        }\
    } while(0)
#define DISTRIBUTE_ERR2(ERR, J)\
    do {\
        if(J < mydata->img.w - 1)\
            MY(err2, J + 1) += ERR * 7.f/16.f;\
        if(mydata->i < mydata->img.h - 1) {\
            if(J > 0) \
                MYP1(err2, J - 1) += ERR * 3.f/16.f;\
            MYP1(err2, J) += ERR * 5.f/16.f;\
            if(J < mydata->img.w - 1)\
                MYP1(err2, J + 1) += ERR * 1.f/16.f;\
        }\
    } while(0)
#define DISTRIBUTE_ERR3(ERR, J)\
    do {\
        if(J < mydata->img.w - 1)\
            MY(err3, J + 1) += ERR * 7.f/16.f;\
        if(mydata->i < mydata->img.h - 1) {\
            if(J > 0) \
                MYP1(err3, J - 1) += ERR * 3.f/16.f;\
            MYP1(err3, J) += ERR * 5.f/16.f;\
            if(J < mydata->img.w - 1)\
                MYP1(err3, J + 1) += ERR * 1.f/16.f;\
        }\
    } while(0)


static void _proc_toHSL(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        pixel_t ip = A(mydata->img, mydata->i, j);
        float l = (ip.r + ip.g + ip.b) / 3.f / 255.f * 2.f - 1.f;
        float rm, gc;
        if(!opt_calt) {
            rm = (!opt_alt ? (ip.r+ip.b)/2.f : ip.r);
            gc = (!opt_alt ? ip.g : (ip.g+ip.b)/2.f);
        } else {
            rm = (!opt_alt ? (ip.r+ip.g)/2.f : (ip.r + ip.b)/2.f);
            gc = (!opt_alt ? ip.b : ip.g);
        }

        float c = (gc - rm)/(gc + rm);

        MY(c, j) = c;
        MY(l, j) = l;
    }
}

inline void _proc_ditherLuma(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float Nu = MY(err, j);
        float old = MY(l, j);
        float new = round(CLAMP(old + Nu, -1.f, 1.f));
        float err = old - new;
        DISTRIBUTE_ERR(err, j);
        MY(l, j) = new;
    }
}

inline void _proc_ditherChroma(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float Nu = MY(err, j);
        float old = MY(c, j);
        float new = round(CLAMP(old + Nu, -1.f, 1.f));
        float err = old - new;
        float altNu = MY(err2, j);
        float altNew = round(CLAMP( (old + Nu)*0.5f+0.5f, 0.f, 1.f ));
        float err2 = old*0.5f+0.5f - altNew;
        pixel_t ip = A(mydata->img, mydata->i, j);
        float l = (ip.r + ip.g + ip.b) / 3.f / 255.f;
        float lNu = MY(err2, j);
        float lNew = round(CLAMP(l + lNu, 0.f, 1.f));
        float lErr = l - lNew;
        if(new == 0.f||1) {
            new = 2.f*altNew-1.f;
        }
        DISTRIBUTE_ERR(err, j);
        DISTRIBUTE_ERR2(err2, j);
        DISTRIBUTE_ERR3(lNew, j);
        MY(c, j) = new;
    }
}

static void _output_layer(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        pixel_t p = A(mydata->img, mydata->i, j);

        if(MY(l, j) < -0.5f) {
            p.r = p.g = p.b = 0;
        } else if(MY(l, j) > 0.5f) {
            p.r = p.g = p.b = 255;
            p.b *= !opt_alt;
        } else {
            if(MY(c, j) == 100.f) {
                p.r = p.g = p.b = 0;
            } else {
                p.r = (MY(c, j) < .5f) * 255;
                p.g = (MY(c, j) > -.5f) * 255;
                p.b = (!opt_alt) * 255;
            }
        }

        A(mydata->img, mydata->i, j) = p;
    }
}

/*
   Multi-step Algorithm

   No. Step                         Colour Space
   0. in RGB                        (r[], g[], b[])
   1. RGB -> LC                     (c[], s[], v[])
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
img_t cgaditherfs2(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;
    tddata_t* ddatas;

    float* c = (float*)malloc(img.w * img.h * sizeof(float));
    float* s = (float*)malloc(img.w * img.h * sizeof(float));
    float* l = (float*)malloc(img.w * img.h * sizeof(float));
    float* err = (float*)malloc(img.w * img.h * sizeof(float));
    float* err2 = (float*)malloc(img.w * img.h * sizeof(float));
    float* err3 = (float*)malloc(img.w * img.h * sizeof(float));

    // process
    // 1. compute HSV
    ddatas = (tddata_t*)malloc(img.h * sizeof(tddata_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = img;
        data->c = c;
        data->l = l;
        data->err = err;
        data->err2 = err2;
        data->err3 = err3;
        _proc_toHSL(data);
    }

    // 2. dither colors to magenta or cyan
    memset(err, 0, img.w * img.h * sizeof(float));
    memset(err2, 0, img.w * img.h * sizeof(float));
    // can't omp because each pixel depends on previous 4
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_ditherLuma(data);
    }
    memset(err, 0, img.w * img.h * sizeof(float));
    memset(err2, 0, img.w * img.h * sizeof(float));
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_ditherChroma(data);
    }

    // 6. distribute pixels to C, M, black or white
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = ret;
        _output_layer(data);
    }

    free(c);
    free(l);
    free(err);
    free(err2);
    free(err3);
    free(ddatas);
    
    return ret;
}
