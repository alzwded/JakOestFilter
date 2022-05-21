#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "common.h"

extern int opt_alt; // RYGb
extern int opt_balt; // bias2
extern int opt_calt; // bias1

typedef struct {
    size_t i;
    img_t img;
    float* c,* l;
    float* errl,* errc;
    int* oc;
} tddata_t;

#define CLAMP(X, MIN, MAX) (X > MAX ? MAX : X < MIN ? MIN : X)
#define MAX(A, B, C) ((A >= B && A >= C) ? A : (B >= A && B >= C) ? B : C)
#define MIN(A, B, C) ((A <= B && A <= C) ? A : (B <= A && B <= C) ? B : C)
#define MY(WHAT, J) (mydata->WHAT[mydata->i * mydata->img.w + J])
#define MYP1(WHAT, J) (mydata->WHAT[(mydata->i+1) * mydata->img.w + J])
#define DISTRIBUTE_ERR(ERR, J)\
    do {\
        if(J < mydata->img.w - 1) {\
            MY(errl, J + 1) += ERR[0] * 7.f/16.f;\
            MY(errc, J + 1) += ERR[1] * 7.f/16.f;\
        }\
        if(mydata->i < mydata->img.h - 1) {\
            if(J > 0) {\
                MYP1(errl, J - 1) += ERR[0] * 3.f/16.f;\
                MYP1(errc, J - 1) += ERR[1] * 3.f/16.f;\
            }\
            MYP1(errl, J) += ERR[0] * 5.f/16.f;\
            MYP1(errc, J) += ERR[1] * 5.f/16.f;\
            if(J < mydata->img.w - 1) {\
                MYP1(errl, J + 1) += ERR[0] * 1.f/16.f;\
                MYP1(errc, J + 1) += ERR[1] * 1.f/16.f;\
            }\
        }\
    } while(0)


static void _proc_toHSL(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        pixel_t ip = A(mydata->img, mydata->i, j);
        float l = (ip.r + ip.g + ip.b) / 3.f / 255.f * 2.f - 1.f;
        float rm, gc;
        if(!opt_calt && !opt_balt) {
            rm = (!opt_alt ? (ip.r+ip.b)/2.f : ip.r);
            gc = (!opt_alt ? (ip.g+ip.b)/2.f : ip.g);
        } else if(opt_calt) {
            rm = (!opt_alt ? (ip.r+ip.g)/2.f : ip.r);
            gc = (!opt_alt ? (ip.b+ip.g)/2.f : (ip.g+ip.b)/2.f);
        } else if(opt_balt) {
            rm = (!opt_alt ? ip.r : (ip.r+ip.b)/2.f);
            gc = (!opt_alt ? ip.g : ip.g);
        }

        float c = (gc - rm)/(gc + rm);

        MY(c, j) = c;
        MY(l, j) = l;
    }
}

inline void vec_sub(const float v1[], const float v2[], float v3[])
{
    v3[0] = v1[0] - v2[0];
    v3[1] = v1[1] - v2[1];
}

inline float vec_len2(const float v[])
{
    return v[0]*v[0] + v[1]*v[1];
}

inline void _proc_dither(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        float old[] = { MY(l, j), MY(c, j) };
        float oldoff[2] = { MY(l, j) + MY(errl, j), MY(c, j) + MY(errc, j) };
        float new[2];
        float err[2];
        float dist[4];
        static const float WCMb[][2] = {
            { 1.f, 0.f }, // W
            { 0.f,-1.f }, // M
            { 0.f, 1.f }, // C
            {-1.f, 0.f }, // B
        };
        int mini = -1;
        float minlen = 9999.f;
        for(int i = 0; i < 4; ++i) {
            float v[2], len;
            vec_sub(oldoff, WCMb[i], v);
            len = vec_len2(v);
            if(len < minlen) {
                minlen = len;
                mini = i;
            }
        }
        MY(oc, j) = mini;
        vec_sub(old, WCMb[mini], err);
        DISTRIBUTE_ERR(err, j);
    }
}

static void _output_layer(tddata_t* mydata)
{
    size_t j;

    for(j = 0; j < mydata->img.w; ++j) {
        pixel_t p = A(mydata->img, mydata->i, j);

        switch(MY(oc, j)) {
        case 0:
            p.r = p.g = p.b = 255;
            p.b *= !opt_alt;
            break;
        case 1:
            p.r = p.b = 255;
            p.g = 0;
            p.b *= !opt_alt;
            break;
        case 2:
            p.g = p.b = 255;
            p.r = 0;
            p.b *= !opt_alt;
            break;
        case 3:
            p.r = p.g = p.b = 0;
            break;
        }

        A(mydata->img, mydata->i, j) = p;
    }
}

/*
   Multi-step Algorithm

   No. Step                         Colour Space
   0. in RGB                        (r[], g[], b[])
   1. RGB -> LC                     (c[], l[])
   2. dither_l                      (c[], b|c|W)
   3. dither_c                      (M|C, b|c|W)
   4. _output_layer                 (r[], g[], b[])
    opt_alt implies the blue channel is 0
 */
img_t cgaditherfs3(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;
    tddata_t* ddatas;

    float* c = (float*)malloc(img.w * img.h * sizeof(float));
    int* oc = (int*)malloc(img.w * img.h * sizeof(int));
    float* l = (float*)malloc(img.w * img.h * sizeof(float));
    float* errl = (float*)malloc(img.w * img.h * sizeof(float));
    float* errc = (float*)malloc(img.w * img.h * sizeof(float));

    // process
    // 1. compute HSV
    ddatas = (tddata_t*)malloc(img.h * sizeof(tddata_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = img;
        data->c = c;
        data->oc = oc;
        data->l = l;
        data->errl = errl;
        data->errc = errc;
        _proc_toHSL(data);
    }

    // 2. dither luma to determine if we're bright, black or one of the colours
    memset(errl, 0, img.w * img.h * sizeof(float));
    memset(errc, 0, img.w * img.h * sizeof(float));
    // can't omp because each pixel depends on previous 4
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        _proc_dither(data);
    }

    // 4. distribute pixels to C, M, black or white
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img = ret;
        _output_layer(data);
    }

    free(c);
    free(oc);
    free(l);
    free(errl);
    free(errc);
    free(ddatas);
    
    return ret;
}
