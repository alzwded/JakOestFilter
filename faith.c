#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "common.h"

#define SATTHRESH (0.1f)
#define VALTHRESH (0.05f)
#define VALSUPTHRESH (0.87f)

typedef struct {
    short hue;
    float saturation, value;
} hsv_t;

typedef struct {
    size_t w, h;
    hsv_t* pixels;
} hsvimg_t;

static short _partitionedHues[] = {
    0,
    90,
    185,
};
#define NUMPART (sizeof(_partitionedHues)/sizeof(_partitionedHues[0]))

static struct {
    float max;
    size_t partition;
} _partitions[] = {
    { 30.f, 0 },
    { 90.f, 1 },
    { 240.f, 2 },
    { 999.f, 0 },
};

static short C1, C2, C3, C4;

static inline int _underThresh(hsv_t p)
{
    if(p.saturation < SATTHRESH) return 1;
    if(p.value < VALTHRESH) return 1;
    if(p.value > VALSUPTHRESH) return 1;
    return 0;
}

#define MAX(A, B, C) ((A >= B && A >= C) ? A : (B >= A && B >= C) ? B : C)
#define MIN(A, B, C) ((A <= B && A <= C) ? A : (B <= A && B <= C) ? B : C)

static inline hsv_t _toHSV(pixel_t ip)
{
    hsv_t ret;
    float r = (float)ip.r / 255.f, g = (float)ip.g / 255.f, b = (float)ip.b / 255.f;
    float max = MAX(r, g, b);
    float min = MIN(r, g, b);
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

    return ret;
}

static inline pixel_t _fromHSV(hsv_t p)
{
    struct { float r, g, b; } ret;

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

static void _proc_toHSV(void* data)
{
    tdata_t* mydata = (tdata_t*)data;
    size_t j;

    for(j = 0; j < mydata->in.asRGB.w; ++j) {
        A(mydata->out.asHSV, mydata->i, j) = 
            _toHSV(A(mydata->in.asRGB, mydata->i, j));
    }
}

static inline size_t PARTITION(float X)
{
    size_t ii;
    for(ii = 0; ii < 6; ++ii) {
        if(X < _partitions[ii].max) {
            return _partitions[ii].partition;
        }
    }
}

static inline short dist(short p1, short p2)
{
    if(p1 > p2) {
        short t = p1;
        p1 = p2;
        p2 = t;
    }
    short r1 = abs(p2 - p1);
    short r2 = abs(p1 + 360 - p2);
    return (r1 < r2) ? r1 : r2;
}

static inline void _incPartition(double* val, hsv_t p)
{
    if(p.value < 0.5) {
        *val += sqrt((double)p.saturation * sin(p.value * 3.14159));
    } else {
        double b = p.value;
        b = 4.0 * (b * b - 2.0 * b + 1.0);
        b *= b;
        *val += sqrt((double)p.saturation * b);
    }
}

static void _preprocRG(hsvimg_t img)
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
    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            A(img, i, j).value = (A(img, i, j).value - min) / max;
            //A(img, i, j).saturation = (A(img, i, j).saturation - mins) / maxs;
        }
    }

    C1 = 0;
    C2 = 139;
    //C1 = 120;
    //C2 = 0;
    C3 = 240;
    C4 = 60;

    printf("color points: %d %d %d %d\n", C1, C2, C3, C4);
}

static void _preproc(hsvimg_t img)
{
    float min = 1.f, max = 0.f;
    float mins = 1.f, maxs = 0.f;
    size_t i, j;
    double partitions[NUMPART];
    size_t C1partition = 999;

    memset(partitions, 0, NUMPART * sizeof(double));

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
    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            A(img, i, j).value = (A(img, i, j).value - min) / max;
            //A(img, i, j).saturation = (A(img, i, j).saturation - mins) / maxs;
            if(_underThresh(A(img, i, j))) continue;
            _incPartition(&partitions[PARTITION(A(img, i, j).hue)], A(img, i, j));
        }
    }

    //printf("partitions: R:%.3f Y:%.3f G:%.3f b:%.3f B:%.3f\n", partitions[0], partitions[1], partitions[2], partitions[3], partitions[4]);
    printf("partitions: R:%.3f Y:%.3f G:%.3f b:%.3f\n", partitions[0], partitions[1], partitions[2], partitions[3]);

    // get dominant color
    double k =1e27f;
    for(i = 0; i < NUMPART; ++i) {
        if(partitions[i] < k) {
            k = partitions[i];
            C1 = _partitionedHues[i];
            C1partition = i;
        }
    }

    // get secondary color
    if(partitions[(C1partition + 1) % NUMPART]
            <= partitions[(C1partition + NUMPART - 1) % NUMPART])
    {
        C2 = _partitionedHues[(C1partition + 1) % NUMPART];
    } else {
        C2 = _partitionedHues[(C1partition + NUMPART - 1) % NUMPART];
    }

    // determine 3rd and 4th points
    {
        short p1 = (C2 + C1) / 2;
        short p2 = (p1 + 180) % 360;
        if(dist(p1, C1) < dist(p2, C1)) {
            C3 = p2;
            C4 = p1;
        } else {
            C3 = p1;
            C4 = p2;
        }
    }

    printf("color points: %d %d %d %d\n", C1, C2, C3, C4);
}

static inline float _redistribVal(float p)
{
    float s = sinf(p * 3.14159f / 2.f);
#ifdef NOTSOBRIGHT
    float t = 0.05 + (0.65 * s + 0.15 * s * s); // add to 0.9
#else
    float t = 0.02 + (0.66 * s + 0.3 * sinf(s * 3.14159f / 2.f));
#endif
    return t;
}

static inline float applyMode(int mode, float t)
{
    switch(mode) {
    case 0:
        return 0.5f * t + 0.5f * _redistribVal(t);
    case 1:
        return 0.5f * t + 0.5f * (1.f - _redistribVal(1.f - t));
    default:
        return t;
    }
}

static inline float fixHue(float hue)
{
    int mode = 0;
    float clor1 = C1, clor2 = C2;
    if(abs(clor2 - clor1) == dist(clor1, clor2)) {
        if(C2 > C1) {
            mode = 1;
            clor1 = C1;
            clor2 = C2;
        } else {
            mode = 0;
            clor1 = C2;
            clor2 = C1;
        }
    } else {
        if(C2 > C1) {
            clor1 = C2;
            clor2 = C1 + 360.f;
            mode = 0;
        } else {
            clor1 = C1;
            clor2 = C2 + 360.f;
            mode = 1;
        }
    }

    if(dist(hue, C4) < dist(hue, C3)) {
        float t = (float)dist(hue, clor1) / (float)(dist(hue, clor1) + dist(hue, clor2));
        t = applyMode(mode, t);
        hue = clor1 + t * (float)dist(clor1, clor2);
    } else {
        hue = fmodf((hue + 180.f), 360);
        float t = (float)dist(hue, clor1) / (float)(dist(hue, clor1) + dist(hue, clor2));
        t = 1.f - t;
        if(mode == 0) {
            t = _redistribVal(t);
        } else if(mode == 1) {
            t = 1.f - _redistribVal(1.f - t);
        }
        hue = clor1 + t * (float)dist(clor1, clor2);
    }

    return fmodf(hue, 360.f);
}

static void _proc_bulk(void* data)
{
    tdata_t* mydata = (tdata_t*)data;
    size_t j;

    for(j = 0; j < mydata->in.asHSV.w; ++j) {
        hsv_t p = A(mydata->in.asHSV, mydata->i, j);

        short dC1 = dist(p.hue, C1),
              dC2 = dist(p.hue, C2),
              dC3 = dist(p.hue, C3),
              dC4 = dist(p.hue, C4);

        if(dC3 < dC4) {
            p.hue = fixHue(p.hue);

            // since it's outside of our color arc, desaturate it a bit
            float t = (1.f - _redistribVal(1.f - p.saturation));
            p.saturation = t;//(0.3f * p.saturation + 0.7f * t);
        } else /*if(dC4 < dC3)*/ {
            p.hue = fixHue(p.hue);

            if(dC1 < dC2) {
                p.saturation = _redistribVal(p.saturation);
            } else {
                float t = _redistribVal(p.saturation);
                p.saturation = 0.4f * p.saturation + 0.6f * t;
            }
        }

        p.value = _redistribVal(p.value);
        p.value = 0.4f * p.value + 0.6f * _redistribVal(p.value);

        A(mydata->out.asRGB, mydata->i, j) = _fromHSV(p);
    }
}


static img_t _faithProc(img_t const img, void (*preproc)(hsvimg_t))
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };
    hsvimg_t hsvimg = { img.w, img.h, (hsv_t*)malloc(img.w * img.h * sizeof(hsvimg_t)) };

    size_t i, j;
    tdata_t* datas;

    // process
    // toHSV
    datas = (tdata_t*)malloc(img.h * sizeof(tdata_t));
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = &datas[i];
        data->i = i;
        data->in.asRGB = img;
        data->out.asHSV = hsvimg;
        _proc_toHSV(data);
    }

    // normalize
    preproc(hsvimg);
    // bulk processing
#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = &datas[i];
        data->i = i;
        data->in.asHSV = hsvimg;
        data->out.asRGB = ret;
        _proc_bulk(data);
    }

    free(datas);
    free(hsvimg.pixels);
    
    return ret;
}

img_t faith(img_t const img)
{
    return _faithProc(img, _preproc);
}

img_t rgfilter(img_t const img)
{
    return _faithProc(img, _preprocRG);
}
