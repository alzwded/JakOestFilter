#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "common.h"

/** abritrary maximum number of rules */
#define MAXRULES 10

/** represents a rule:
  greater and lower are the colours you're targeting
  factor is the factor which will grow one of the colours */
typedef struct {
    RC_RGB_t greater, lower;
    float factor;
} rule_t;

/** the rules */
static rule_t rules[MAXRULES];
static size_t nrules = 0;

/** used to add a new rule */
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
        // if the factor is >1, raise the greater component
        // else lower the lower component by 1/factor amount
        if(rule.factor >= 1.0) {
            uint8_t greater = GET(*p, rule.greater);
            uint8_t lower = GET(pin, rule.lower);
            if(greater > lower) {
                float diff = rule.factor * (float)(greater - lower);
                //printf(" from %d", GET(pin, rule.greater));
                GET(*p, rule.greater) = (uint8_t)SUP(lower + diff, 255.f);
                //printf(" to %d by %f\n", GET(*p, rule.greater), diff);
            }
        } else {
            uint8_t greater = GET(pin, rule.greater);
            uint8_t lower = GET(*p, rule.lower);
            if(greater > lower) {
                float diff = (1.f / rule.factor) * (float)(greater - lower);
                GET(*p, rule.lower) = (uint8_t)INF(greater - diff, 0.f);
            }
        }
    }
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
        _process(A(mydata->in, mydata->i, j), &A(mydata->out, mydata->i, j));
    }

    free(mydata);
}

/** apply a colour transformation based on relationships between colour
  components (RGB)
  rules are added with recolour_addRule */
img_t recolour(img_t const img)
{
    img_t ret = { img.w, img.h, (pixel_t*)malloc(img.w * img.h * sizeof(pixel_t)) };

    size_t i, j;

#pragma omp parallel for
    for(i = 0; i < img.h; ++i) {
        tdata_t* data = (tdata_t*)malloc(sizeof(tdata_t));
        data->i = i;
        data->in = img;
        data->out = ret;
        _tprocess(data);
    }
    
    return ret;
}
