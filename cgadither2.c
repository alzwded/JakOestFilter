#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "common.h"

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
#define MAGENTAI 0
#define CYANI 1
#define YELLOWI 2

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
    uint8_t* color;
    uint8_t* isGray;
    uint8_t* isWhite;
    uint8_t* wcb;
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

static inline void _genRandom(int n, int* a)
{
    int i;
    for(i = 0; i < n; ++i) {
        a[i] = rand();
    }
}

static void _dither_w_c_b(void* data)
{
#define MY(A, J) (mydata->A[mydata->img.asHSV.w*mydata->i+J])
    tddata_t* mydata = (tddata_t*)data;
    size_t j;
    float median, Nu;
    int lum;
    int wcb;

    for(j = 0; j < mydata->img.asHSV.w; ++j) {
        hsv_t p = A(mydata->img.asHSV, mydata->i, j);
        //         b     c      w
        // lum 0.00..0.15..0.85..1.00
        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
        if(p.value > 0.5f) {
            const float a = 0.3, b = 1.f, c = 0.7;
            const float fc = (c-a)/(b-a);
            float lum = p.value; //(p.value < fc) ? (a + sqrtf(p.value * (b-a) * (c-a))) : (b-sqrtf((1-p.value)*(b-a)*(b-c)));
            float x = (Nu < fc) ? a + sqrtf(Nu * (b - a) * (c - a)) : (b-sqrtf(1-Nu)*(b-a)*(b-c));

            lum = 2.f * lum - 1.f;
            //printf("value = %f lum + x = %f\n", p.value, lum + x);
            //printf("lum + x = %f\n", lum + x);
            //printf(">> a %f b %f c %f fc %f Nu %f x %f\n", a, b, c, fc, Nu, x);
            wcb = (lum + x > 0.4) ? 2 : 1;
        } else {
            const float a = 0.0f, b = 0.7f, c = 0.3;
            const float fc = (c-a)/(b-a);
            float lum = p.value; //(p.value < fc) ? (a + sqrtf(p.value * (b-a) * (c-a))) : (b-sqrtf((1-p.value)*(b-a)*(b-c)));
            float x = (Nu < fc) ? a + sqrtf(Nu * (b - a) * (c - a)) : (b-sqrtf(1-Nu)*(b-a)*(b-c));

            lum = 2.f * lum - 1.f;
            wcb = (lum + x < -0.4) ? 0 : 1;
            //printf("value = %f lum + x = %f\n", p.value, lum + x);
            //printf(">> a %f b %f c %f fc %f Nu %f x %f\n", a, b, c, fc, Nu, x);
            //printf("wcb = %d\n", wcb);
        }
        MY(wcb, j) = wcb;
    }
#undef MY
}

static void _dither_m_c_y(void* data)
{
#define MY(A, J) (mydata->A[mydata->img.asHSV.w*mydata->i+J])
    tddata_t* mydata = (tddata_t*)data;
    size_t j;
    short dC, dM, dY;
    short ah, bh;
    float median, Nu;

    const short MAGENTA = (!opt_alt) ? 300 : 0;
    const short CYAN = (!opt_alt) ? 180 : 120;
#define YELLOW 60
    const short MINUS_MAGENTA = (!opt_alt) ? 60 : 0;
    const short MINUS_CYAN = (!opt_alt) ? 180 : 240;
#define MINUS_YELLOW 300
#define MINUS(s) (360 - s)

    for(j = 0; j < mydata->img.asHSV.w; ++j) {
        hsv_t p = A(mydata->img.asHSV, mydata->i, j);
        
        ah = abs((p.hue + MINUS_CYAN) % 360);
        bh = abs((CYAN + MINUS(p.hue)) % 360);
        dC = (ah < bh) ? ah : bh;

        ah = abs((p.hue + MINUS_MAGENTA) % 360);
        bh = abs((MAGENTA + MINUS(p.hue)) % 360);
        dM = (ah < bh) ? ah : bh;

        ah = abs((p.hue + MINUS_YELLOW) % 360);
        bh = abs((YELLOW + MINUS(p.hue)) % 360);
        dY = (ah < bh) ? ah : bh;

        int cas = (dM < dC) | ((dM < dY) << 1) | ((dC < dY) << 2);
        //printf("%d %d %d %d\n", dM, dC, dY, cas);
        // 012, 021, 102, 120, 201, 210

        // 210  m>c, m>y, c>y       000
        // 201  m<c, m>y, c>y       001
        // 210  m>c, m<y, c>y       010
        // 021  m<c, m<y, c>y       011
        // 120  m>c, m>y, c<y       100
        // 012  m<c, m>y, c<y       101
        // 102  m>c, m<y, c<y       110
        // 012  m<c, m<y, c<y       111
        static const uint8_t colorMappings[][2] = {
            { YELLOWI, CYANI },
            { YELLOWI, MAGENTAI },
            { YELLOWI, CYANI },
            { MAGENTAI, YELLOWI },
            { CYANI, YELLOWI },
            { MAGENTAI, CYANI },
            { CYANI, MAGENTAI },
            { MAGENTAI, CYANI }
        };
        const uint8_t distances[][2] = {
            { dY, dC },
            { dY, dM },
            { dY, dC },
            { dM, dY },
            { dC, dY },
            { dM, dC },
            { dC, dM },
            { dM, dC },
        };

        median = 1.f - ((float)distances[cas][1] / ((float)distances[cas][0] + (float)distances[cas][1]));
        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
        float x = (Nu < median) ? sqrtf(Nu * median) : (1.f - sqrtf((1.f-Nu)*(1.f - median)));
        median = 2.f * median - 1.f;
        x = 2.f * x - 1.f;
        MY(color, j) = (median + x > 0.f) ? colorMappings[cas][1] : colorMappings[cas][0];

    }

#undef MINUS
#undef YELLOW
#undef MINUS_YELLOW
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
        // project to triangle
        const float c = 0.3, b = 1.f, a = 0.f;
        float sat = 1.f - p.saturation;
        const float fc = (c-a)/(b-a);
        sat = (sat < fc) ? (a + sqrtf(sat * (b-a) * (c-a))) : (b - sqrtf((1-sat)*(b-a)*(b-c)));

        Nu = (float)MY(randomness, j) / (float)RAND_MAX;
        median = sat;
        float x = (Nu < median) ? sqrtf(Nu * median) : (1.f - sqrtf((1.f-Nu)*(1.f - median)));
        x = 2 * x - 1.f;
        median = 2.f * median - 1.f;
        MY(isGray, j) = median + x > 0.0;
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
                p.r = 255;
                p.g = 255;
                p.b = (!opt_alt) * 255;
                //printf("gray -> white\n");
            } else {
                // black is always black :-)
                p.r = p.g = p.b = 0;
                // printf("gray -> black\n");
            }
        } else {
            switch(MY(wcb, j)) {
                case 0:
                    p.r = p.g = p.b = 0;
                    break;
                case 2:
                    p.r = 255;
                    p.g = 255;
                    p.b = (!opt_alt) * 255;
                    break;
                case 1:
                    switch(MY(color, j)) {
                        case CYANI:
                            p.r = 0;
                            p.g = 255;
                            p.b = (!opt_alt) * 255;
                            break;
                        case MAGENTAI:
                            //printf("cm\n");
                            p.r = 255;
                            p.g = 0;
                            p.b = (!opt_alt) * 255;
                            break;
                        case YELLOWI:
                            //printf("cy\n");
                            p.r = 255;
                            p.g = 255;
                            p.b = (!opt_alt) * 255;
                            break;
                    }
                    break;
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
img_t cgadither2(img_t const img)
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

    // 2. dither colors to magenta or cyan
    _genRandom(img.w * img.h, randomness);
    uint8_t* color = (uint8_t*)malloc(img.w * img.h * sizeof(uint8_t));
    uint8_t* isGray = (uint8_t*)malloc(img.w * img.h * sizeof(uint8_t));
    uint8_t* isWhite = (uint8_t*)malloc(img.w * img.h * sizeof(uint8_t));
    uint8_t* wcb = (uint8_t*)malloc(img.w * img.h * sizeof(uint8_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->randomness = randomness;
        data->i = i;
        data->img.asHSV = hsvimg;
        data->color = color;
        _dither_m_c_y(data);
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

    // 5. dither luma to white, color or black
    _genRandom(img.w * img.h, randomness);
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->randomness = randomness;
        data->i = i;
        data->img.asHSV = hsvimg;
        data->wcb = wcb;
        _dither_w_c_b(data);
    }

    // 5. distribute pixels to C, M, black or white
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tddata_t* data = &ddatas[i];
        data->i = i;
        data->img.asRGB = ret;
        data->color = color;
        data->isGray = isGray;
        data->isWhite = isWhite;
        data->wcb = wcb;
        _output_layer(data);
    }

    free(randomness);
    free(color);
    free(isGray);
    free(isWhite);
    free(wcb);
    free(ddatas);
    free(datas);
    free(hsvimg.pixels);
    
    return ret;
}
