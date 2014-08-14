#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "JakWorkers.h"

#define SATTHRESH (0.1f)
#define VALTHRESH (0.05f)

typedef struct {
    short hue;
    float saturation, value;
} hsv_t;

typedef struct {
    size_t w, h;
    hsv_t* pixels;
} hsvimg_t;

#define ABS(X) ((X < 0) ? -X : X)

static short _partitionedHues[] = {
    0,
    60,
    120,
    180,
    240,
};

static struct {
    float max;
    size_t partition;
} _partitions[] = {
    { 30.f, 0 },
    { 90.f, 1 },
    { 150.f, 2 },
    { 210.f, 3 },
    { 300.f, 4 },
    { 999.f, 0 },
};

static short C1, C2, C3, C4;

static inline int _underThresh(hsv_t p)
{
    if(p.saturation < SATTHRESH) return 1;
    if(p.value < VALTHRESH) return 1;
    return 0;
}

static inline hsv_t _toHSV(pixel_t p)
{
    uint8_t M, m, C;
    float Hp;
    hsv_t ret;
    enum {
        UNDEF,
        MAXR,
        MAXB,
        MAXG
    } state;

    if(p.r > p.g && p.b > p.g) {
        m = p.g;
        if(p.r > p.b) {
            M = p.r;
            state = MAXR;
        } else {
            M = p.b;
            state = MAXB;
        }
    } else if(p.r > p.b && p.g > p.b) {
        m = p.b;
        if(p.r > p.g) {
            M = p.r;
            state = MAXR;
        } else {
            M = p.g;
            state = MAXG;
        }
    } else if(p.b > p.r && p.g > p.r) {
        m = p.r;
        if(p.b > p.g) {
            M = p.b;
            state = MAXB;
        } else {
            M = p.g;
            state = MAXG;
        }
    }

    if((C = M - m) == 0) {
        state = UNDEF;
    }

    switch(state) {
    case MAXR:
        Hp = (((float)p.g - p.b) / (float)C);
        if(Hp > 6.f) Hp -= 6.f;
        break;
    case MAXG:
        Hp = (((float)p.b - p.r) / (float)C) + 2;
        break;
    case MAXB:
        Hp = (((float)p.r - p.g) / (float)C) + 4;
        break;
    default:
        Hp = 0.f;
        break;
    }

    ret.hue = SUP(60.f * Hp, 360.f);
    ret.value = M;
    ret.saturation = (ret.value == 0) ? 0.f : ((float)C / ret.value);

    return ret;
}

static inline pixel_t _fromHSV(hsv_t p)
{
    int Hp;
	float f, P, Q, T;
    struct { float r, g, b; } ret;

	if(p.saturation < 1.e-5f) {
        pixel_t ret;
        ret.r = ret.g = ret.b = p.value;
		return ret;
	}

	Hp = floorf(p.hue / 60.f);
	f = (p.hue / 60.f) - Hp;
	P = p.value * (1 - p.saturation);
	Q = p.value * (1 - p.saturation * f);
	T = p.value * (1 - p.saturation * (1 - f));

	switch(Hp) {
	case 0:
		ret.r = p.value;
		ret.g = T;
		ret.b = P;
		break;
	case 1:
		ret.r = Q;
		ret.g = p.value;
		ret.b = P;
		break;
	case 2:
		ret.r = P;
		ret.g = p.value;
		ret.b = T;
		break;
	case 3:
		ret.r = P;
		ret.g = Q;
		ret.b = p.value;
		break;
	case 4:
		ret.r = T;
		ret.g = P;
		ret.b = p.value;
		break;
	case 5:
		ret.r = p.value;
		ret.g = P;
		ret.b = Q;
		break;
    default:
        ret.r = ret.g = ret.b = 0;
        break;
	}

    pixel_t pret = { ret.r, ret.g, ret.b };

    return pret;
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
    short r1 = ABS(p1 - p2);
    short r2 = (r1 + 180) % 360;
    return (r1 < r2) ? r1 : r2;
}

static void _preproc(hsvimg_t img)
{
    float min = 1.f, max = 0.f;
    size_t i, j;
    size_t partitions[5] = { 0, 0, 0, 0, 0 };
    size_t C1partition = 999;
    int hh = 0, n = 0;

    // get normalization extents
    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            if(A(img, i, j).value < min) min = A(img, i, j).value;
            if(A(img, i, j).value > max) max = A(img, i, j).value;
        }
    }

    // normalize and partition
    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            A(img, i, j).value = (A(img, i, j).value - min) / max;
            if(_underThresh(A(img, i, j))) continue;
            partitions[PARTITION(A(img, i, j).hue)]++;
        }
    }

    // get dominant color
    j = -1;
    for(i = 0; i < 5; ++i) {
        if(partitions[i] > j) {
            j = partitions[i];
            C1 = _partitionedHues[i];
            C1partition = i;
        }
    }

    // get secondary color
    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            if(PARTITION(A(img, i, j).hue) == C1partition) continue;
            if(_underThresh(A(img, i, j))) continue;
            hh += A(img, i, j).hue;
            ++n;
        }
    }
    //C2 = _partitionedHues[PARTITION(hh / n)];
    if(n) C2 = hh / n;
    else C2 = (C1 + 180) % 360;

    // determine 3rd point
    {
        short p1 = (C2 + C1) / 2;
        short p2 = ((int)C1 + 360 + C2) / 2 % 360;
        if(dist(p1, C1) < dist(p2, C1)) {
            C3 = p2;
            if(dist(C1, C2) >= 30) {
                C4 = p1;
            } else {
                C4 = C3;
            }
        } else {
            C3 = p1;
            if(dist(C1, C2) >= 30) {
                C4 = p2;
            } else {
                C4 = C3;
            }
        }
    }
}

static inline float _redistribVal(float p)
{
    return sinf(p * 3.14159f / 2.f);
}

static void _proc_bulk(void* data)
{
    tdata_t* mydata = (tdata_t*)data;
    size_t j;

    for(j = 0; j < mydata->in.asHSV.w; ++j) {
        hsv_t p = A(mydata->in.asHSV, mydata->i, j);
        
        A(mydata->out.asRGB, mydata->i, j) = _fromHSV(p);
        continue;
        
        short dC1 = dist(p.hue, C1),
              dC2 = dist(p.hue, C2),
              dC3 = dist(p.hue, C3),
              dC4 = dist(p.hue, C4);

        printf("%d %f %f\n", p.hue, p.saturation, p.value);
        printf("%dx%d:: %d %d %d %d\n", mydata->i, j, dC1, dC2, dC3, dC4);

        if(_underThresh(p)) {
            p.saturation = 0.f;
        } else if(dC1 <= dC2 && dC1 <= dC3 && dC1 <= dC4) {
#ifdef ALT_C1
            // bad alternative
            p.hue = C1;
            p.value = 0.5f;
            p.saturation = p.saturation / 2.f + 0.5f;
#else
            // better alternative?
            p.hue = C1;
            # define SATAMOUNT 8.f
            # define TAILSAT / SATAMOUNT + (SATAMOUNT - 1.f) / SATAMOUNT;
            p.saturation = p.saturation TAILSAT;
            //p.value = _redistribVal(p.value);
#endif
        } else if(dC2 <= dC1 && dC2 <= dC3 && dC2 <= dC4) {
            p.hue = C2;
            p.saturation = p.saturation / 2.f + 0.5f;
            p.value = _redistribVal(p.value);
        } else /*dC3 or dC4 are min*/ {
            p.hue = 0;
            p.saturation = 0.f;
            //p.value = _redistribVal(p.value);
            p.value = _redistribVal(_redistribVal(p.value)); // favor white
        }
        
        A(mydata->out.asRGB, mydata->i, j) = _fromHSV(p);
    }
}

img_t faith(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };
    hsvimg_t hsvimg = { img.w, img.h, (hsv_t*)malloc(img.w * img.h * sizeof(hsvimg_t)) };

    size_t i, j;
    tdata_t* datas;

    jw_config_t conf = JW_CONFIG_INITIALIZER;

    // process
    jw_init(conf);
    // toHSV
    datas = (tdata_t*)malloc(img.h * sizeof(tdata_t));
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = &datas[i];
        data->i = i;
        data->in.asRGB = img;
        data->out.asHSV = hsvimg;
        jw_add_job(&_proc_toHSV, data);
    }
    jw_main();
    // normalize
    _preproc(hsvimg);
    // bulk processing
    jw_init(conf);
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = &datas[i];
        data->i = i;
        data->in.asHSV = hsvimg;
        data->out.asRGB = ret;
        jw_add_job(&_proc_bulk, data);
    }
    jw_main();

    free(datas);
    free(hsvimg.pixels);
    
    return ret;
}
