#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

#define MAXRULES 10

typedef struct {
    RC_RGB_t greater, lower;
    float factor;
} rule_t;

#define SUP(X, K) ((X > K) ? K : X)
#define INF(X, K) ((X > K) ? X : K)

#define GET(P, C) (((uint8_t*)(&P))[(size_t)C])

static rule_t rules[MAXRULES];
static size_t nrules = 0;

void recolour_addRule(
        RC_RGB_t greater,
        RC_RGB_t lower,
        float factor)
{
    if(nrules < MAXRULES) {
        rules[nrules].greater = greater;
        rules[nrules].lower = lower;
        rules[nrules].factor = factor;
        nrules++;
    } else {
        fprintf(stderr, "Too many rules!\n");
        abort();
    }
}

static inline void _process(pixel_t pin, pixel_t* p)
{
    size_t i = 0;

    *p = pin;

    for(i = 0; i < nrules; ++i) {
        rule_t rule = rules[i];
        uint8_t greater = GET(pin, rule.greater);
        uint8_t lower = GET(pin, rule.lower);
        if(greater > lower)
        {
            if(rule.factor >= 1.0) {
                uint8_t greater = GET(*p, rule.greater);
                uint8_t lower = GET(pin, rule.lower);
                float diff = rule.factor * (float)(greater - lower);
                //printf(" from %d", GET(pin, rule.greater));
                GET(*p, rule.greater) = (uint8_t)SUP(lower + diff, 255.f);
                //printf(" to %d by %f\n", GET(*p, rule.greater), diff);
            } else {
                uint8_t greater = GET(pin, rule.greater);
                uint8_t lower = GET(*p, rule.lower);
                float diff = rule.factor * (float)(greater - lower);
                GET(*p, rule.lower) = (uint8_t)INF(greater - diff, 0.f);
            }
        }
    }
}

img_t recolour(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;

    //memcpy(ret.pixels, img.pixels, img.w * img.h * sizeof(pixel_t));

    for(i = 0; i < img.h; ++i) {
        for(j = 0; j < img.w; ++j) {
            _process(A(img, i, j), &A(ret, i, j));
        }
    }
    
    return ret;
}
