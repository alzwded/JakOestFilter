#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "common.h"

// FIXME backport fixes to dither_s and dither_m_c here

extern int opt_alt;

static float PI = acos(-1);

typedef struct {
    short hue;
    float saturation, value;
} hsv_t;

typedef struct {
    size_t w, h;
    hsv_t* pixels;
} hsvimg_t;

#define MAX(A, B, C) ((A >= B && A >= C) ? A : (B >= A && B >= C) ? B : C)
#define MIN(A, B, C) ((A <= B && A <= C) ? A : (B <= A && B <= C) ? B : C)

static inline hsv_t _toHSL(pixel_t ip)
{
    hsv_t ret;
    float r = (float)ip.r / 255.f, g = (float)ip.g / 255.f, b = (float)ip.b / 255.f;
    float max = MAX(r, g, b);
    float min = MIN(r, g, b);
    float ax, bx;
    ret.value = (max + min) / 2.f;

    if(max == min){
        ret.hue = ret.saturation = 0.f; // achromatic
    }else{
        float C = max - min;
        ret.saturation = C / (1.f - fabs(2.f * ret.value - 1.f));
        if(max == r) {
            ret.hue = 60.f * fmodf((g - b) / C, 6.f);
        } else if(max == g) {
            ret.hue = 60.f * ((b - r) / C + 2.f);
        } else {
            ret.hue = 60.f * ((r - g) / C + 4.f);
        }
    }

    // BLOCK: from HSV to HSL
    ax = ret.value * (1.f - ret.saturation / 2.f);
    bx = (ax < 1 - ax) ? ax : 1 - ax;
    if(bx >= 0.00001f) bx = (ret.value - ax) / bx;
    else bx = 0.f;
    ret.value = ax;
    ret.saturation = bx;
    // END BLOCK

    return ret;
}

static inline pixel_t _fromHSL(hsv_t p)
{
    struct { float r, g, b; } ret;

    float ax, bx;
    
    // BLOCK: from HSL to HSV
    bx = (p.value < 1.f - p.value) ? p.value : 1.f - p.value;
    ax = p.value + p.saturation * bx;
    if(bx == 0.f) p.saturation = 0.f;
    else p.saturation = 2.f * (1.f - p.value / bx);
    // END BLOCK

    if(p.saturation == 0.f) {
        pixel_t ret = { p.value * 255.f, p.value * 255.f, p.value * 255.f };
        return ret;
    }

    float C = (1.f - fabsf(2.f * p.value - 1.f)) * p.saturation;
    float X = C * (1.f - fabsf(fmodf(p.hue / 60.f, 2.f) - 1.f));
    float m = 1.f * (p.value - 0.5f * C);

    if(p.hue < 0.f) {
        ret.r = ret.g = ret.b = m;
    } else if(p.hue < 60.f) {
        ret.r = C + m;
        ret.g = X + m;
        ret.b = m;
    } else if(p.hue < 120.f) {
        ret.r = X + m;
        ret.g = C + m;
        ret.b = m;
    } else if(p.hue < 180.f) {
        ret.r = m;
        ret.g = C + m;
        ret.b = X + m;
    } else if(p.hue < 240.f) {
        ret.r = m;
        ret.g = X + m;
        ret.b = C + m;
    } else if(p.hue < 300.f) {
        ret.r = X + m;
        ret.g = m;
        ret.b = C + m;
    } else if(p.hue < 360.f) {
        ret.r = C + m;
        ret.g = m;
        ret.b = X + m;
    } else {
        ret.r = ret.g = ret.b = m;
    }


    pixel_t realRet = {
        ret.r * 255.f,
        ret.g * 255.f,
        ret.b * 255.f
    };

    return realRet;
}

typedef struct {
    size_t i;
    union {
        img_t asRGB;
        hsvimg_t asHSV;
    } in;
    union {
        img_t asRGB;
        hsvimg_t asHSV;
    } out;
} tdata_t;

typedef struct {
    size_t i;
    union {
        img_t asRGB;
        hsvimg_t asHSV;
    } img;
    uint8_t* isMagenta;
    uint8_t* isGray;
    uint8_t* isWhite;
    int* randomness;
} tddata_t;

static void _proc_toHSL(void* data)
{
    tdata_t* mydata = (tdata_t*)data;
    size_t j;

    for(j = 0; j < mydata->in.asRGB.w; ++j) {
        A(mydata->out.asHSV, mydata->i, j) = 
            _toHSL(A(mydata->in.asRGB, mydata->i, j));
    }
}

static void _normalize(hsvimg_t img)
{
    float min = 1.f, max = 0.f;
    float mins = 1.f, maxs = 0.f;
    size_t i, j;

    // get normalization extents
    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            if(A(img, i, j).value < min) min = A(img, i, j).value;
            if(A(img, i, j).value > max) max = A(img, i, j).value;
            if(A(img, i, j).saturation < mins) mins = A(img, i, j).saturation;
            if(A(img, i, j).saturation > maxs) maxs = A(img, i, j).saturation;
        }
    }

    // normalize and partition
#pragma parallel for
    for(i = 0; i < img.h; ++i) {
        int k = 0;
        for(k = 0; k < img.w; ++k) {
            A(img, i, k).value = (A(img, i, k).value - min) / max;
            A(img, i, k).saturation = (A(img, i, k).saturation - mins) / maxs;
        }
    }
}

static inline void _genRandom(int n, int* a)
{
    int i;
    for(i = 0; i < n; ++i) {
        a[i] = rand();
    }
}


static void _dither_m_c(void* data)
{
#define MY(A, J) (mydata->A[mydata->img.asHSV.w*mydata->i+J])
    tddata_t* mydata = (tddata_t*)data;
    size_t j;
    short dC, dM;
    short ah, bh;
    float median, Nu;

#define MAGENTA 300 
#define CYAN 180
#define MINUS_MAGENTA 60
#define MINUS_CYAN 180
#define MINUS(s) (360 - s)

    for(j = 0; j < mydata->img.asHSV.w; ++j) {
        hsv_t p = A(mydata->img.asHSV, mydata->i, j);
        
        ah = abs((p.hue + MINUS_CYAN) % 360);
        bh = abs((CYAN + MINUS(p.hue)) % 360);
        dC = (ah < bh) ? ah : bh;

        ah = abs((p.hue + MINUS_MAGENTA) % 360);
        bh = abs((MAGENTA + MINUS(p.hue)) % 360);
        dM = (ah < bh) ? ah : bh;

        median = 1.f - ((float)dM) / ((float)dM + (float)dC);
        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
#if 0
        //Nu = mydata->randomness[mydata->img.asHSV.w * mydata->i + j] / RAND_MAX;

        MY(isMagenta, j) = (Nu < median);
        //mydata->isMagenta[mydata->img.asHSV.w * mydata->i + j] = (Nu < median);
#endif
        float x = (Nu < median) ? sqrtf(Nu * median) : (1.f - sqrtf((1.f-Nu)*(1.f - median)));
        median = 2.f * median - 1.f;
        x = 2.f * x - 1.f;
        MY(isMagenta, j) = median + x > 0.f;
    }

#undef MAGENTA
#undef CYAN
#undef MY
}

static void _dither_s(void* data)
{
#define MY(A, J) (mydata->A[mydata->img.asHSV.w*mydata->i+J])
    tddata_t* mydata = (tddata_t*)data;
    size_t j;
    float median, Nu;

    for(j = 0; j < mydata->img.asHSV.w; ++j) {
        hsv_t p = A(mydata->img.asHSV, mydata->i, j);

#if 0
        median = 1.f - p.saturation;
        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
        //Nu = mydata->randomness[mydata->img.asHSV.w * mydata->i + j] / RAND_MAX;

        MY(isGray, j) = (Nu < median + 0.00001f);
        //mydata->isGray = (Nu < median + 0.00001f);
#else
        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
        median = 1.f - p.saturation; // c
        float x = (Nu < median) ? sqrtf(Nu * median) : (1.f - sqrtf((1.f-Nu)*(1.f - median)));
        x = 2.f * x - 1.f;
        median = 2.f * median - 1.f;
        MY(isGray, j) = median + x > 0.f;
#endif
    }
#undef MY
}

static void _dither_l(void* data)
{
#define MY(A, J) (mydata->A[mydata->img.asHSV.w*mydata->i+J])
    tddata_t* mydata = (tddata_t*)data;
    size_t j;
    float median, Nu;

    for(j = 0; j < mydata->img.asHSV.w; ++j) {
        hsv_t p = A(mydata->img.asHSV, mydata->i, j);

#if 0
        median = 1.f - (1.f - p.value); // the formula says 1-X, but we need to reverse the logic
        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
        //Nu = mydata->randomness[mydata->img.asHSV.w * mydata->i + j] / RAND_MAX;

        MY(isWhite, j) = (Nu < median + 0.00001f);
        //mydata->isWhite = (Nu < median + 0.00001f);
#endif
        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
        median = 1.f - (1.f - p.value); // c
        float x = (Nu < median) ? sqrtf(Nu * median) : (1.f - sqrtf((1.f-Nu)*(1.f - median)));
        x = 2.f * x - 1.f;
        median = 2.f * median - 1.f;
        MY(isWhite, j) = median + x > 0.f;
    }
#undef MY
}

static void _output_layer(void* data)
{
#define MY(A, J) (mydata->A[mydata->img.asRGB.w*mydata->i+J])
    tddata_t* mydata = (tddata_t*)data;
    size_t j;
    float median, Nu;

    for(j = 0; j < mydata->img.asRGB.w; ++j) {
        pixel_t p = A(mydata->img.asRGB, mydata->i, j);

        if(MY(isGray, j)) {
            if(MY(isWhite, j)) {
                p.r = p.g = 255;
                p.b = (!opt_alt) * 255;
            } else {
                // black is always black :-)
                p.r = p.g = p.b = 0;
            }
        } else {
            if(MY(isMagenta, j)) {
                p.r = 255;  p.g = 0;    p.b = (!opt_alt) * 255;
            } else {
                p.r = 0;    p.g = 255;  p.b = (!opt_alt) * 255;
            }
        }
#if 0
        printf("%3dx%3d: MGW, RGB = \n\t%d %d %d\n\t%x %x %x\n",
                mydata->i, j,
                MY(isMagenta, j), MY(isGray, j), MY(isWhite, j),
                p.r, p.g, p.b);
#endif

        A(mydata->img.asRGB, mydata->i, j) = p;
    }
#undef MY
}

/*
   Multi-step Algorithm

   No. Step                         Colour Space
   0. in RGB                        (r[], g[], b[])
   1. RGB -> HSL                    (h[], s[], l[])
   2. dither_m_c                    (isMagenta?, s[], l[])
   3. dither_s                      (isMagenta?, isGray?, l[])
   4. dither_l                      (isMagenta?, isGray?, isWhite?)
   5. output                        (r[], g[], b[])
        isGray?
            isWhite?
                out = FFFFFF
            else
                out = 000000
        else
            isMagenta?
                out = FF00FF
            else
                out = 00FFFF
 */
img_t cgadither(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };
    hsvimg_t hsvimg = { img.w, img.h, (hsv_t*)malloc(img.w * img.h * sizeof(hsvimg_t)) };

    size_t i, j;
    tdata_t* datas;
    tddata_t* ddatas;

    // randomness for our threads
    int* randomness = (int*)malloc(img.w * img.h * sizeof(int));
    // make it predictable
    srand(0);

    // process
    // 1. toHSL
    datas = (tdata_t*)malloc(img.h * sizeof(tdata_t));
    ddatas = (tddata_t*)malloc(img.h * sizeof(tddata_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = &datas[i];
        data->i = i;
        data->in.asRGB = img;
        data->out.asHSV = hsvimg;
        _proc_toHSL(data);
    }

    // 1.5 normalize to get a more full colour space
    _normalize(hsvimg);

    // bulk processing
    // 2. dither colors to magenta or cyan
    _genRandom(img.w * img.h, randomness);
    uint8_t* isMagenta = (uint8_t*)malloc(img.w * img.h * sizeof(uint8_t));
    uint8_t* isGray = (uint8_t*)malloc(img.w * img.h * sizeof(uint8_t));
    uint8_t* isWhite = (uint8_t*)malloc(img.w * img.h * sizeof(uint8_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->randomness = randomness;
        data->i = i;
        data->img.asHSV = hsvimg;
        data->isMagenta = isMagenta;
        _dither_m_c(data);
    }

    // 3. dither saturation to color or gray
    _genRandom(img.w * img.h, randomness);
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->randomness = randomness;
        data->i = i;
        data->img.asHSV = hsvimg;
        data->isGray = isGray;
        _dither_s(data);
    }

    // 4. dither luma to white or black
    _genRandom(img.w * img.h, randomness);
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->randomness = randomness;
        data->i = i;
        data->img.asHSV = hsvimg;
        data->isWhite = isWhite;
        _dither_l(data);
    }

    // 5. distribute pixels to C, M, black or white
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img.asRGB = ret;
        data->isMagenta = isMagenta;
        data->isGray = isGray;
        data->isWhite = isWhite;
        _output_layer(data);
    }

    free(randomness);
    free(isMagenta);
    free(isGray);
    free(isWhite);
    free(ddatas);
    free(datas);
    free(hsvimg.pixels);
    
    return ret;
}
