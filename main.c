#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "common.h"
#include "tga.h"

// alternate CGA pallette in the cgadither filters
int opt_alt = 0;
// extended range on CGA pallette for FS dithering
int opt_balt = 0;
// alternate color bias in FS ditherers
int opt_calt = 0;

// external subroutines
extern img_t readPixels(char const*);
extern int savePixels(img_t const, char const*);

extern img_t getSquare(img_t const);
extern img_t downSample800(img_t const);
extern img_t recolour(img_t const);
extern img_t frame(img_t const);

extern img_t mosaic(img_t const);

extern img_t mobord(img_t const);

extern img_t faith(img_t const);
extern img_t rgfilter(img_t const);

static int isCGADither = 0;
extern img_t cgadither(img_t const);
extern img_t cgadither2(img_t const);
extern img_t cgaditherfs(img_t const);
extern img_t cgaditherfs2(img_t const);
extern img_t cgaditherfs3(img_t const);

// fwd decl
static void randomizer(char const*);
static void process(char const*);

img_t (*rec_fn)(img_t const) = NULL;
void (*processfn)(char const*) = process;


/** generate the output file name */
static char* getOutFileName(char const* file, const char* ext)
{
    size_t len = strlen(file);
    char const* i = file + len - 1;
    for(; i - file >= 0; --i) {
        if(*i == '/') break;
        if(*i == '.') {
            char* ret = (char*)malloc((i - file) + strlen(ext) + 1);
            strncpy(ret, file, i - file);
            strcpy(ret + (i - file), ext);
            return ret;
        }
    }
    char* ret = (char*)malloc(len + strlen(ext));
    strcpy(ret, file);
    strcpy(ret + len, ext);
    return ret;
}

/** initialise the recolouring engine */
static void init_recolour()
{
    if(rec_fn == recolour || processfn == randomizer) {
        recolour_addRule(RC_G, RC_R, 1.29);
        recolour_addRule(RC_G, RC_B, 1.25);
        recolour_addRule(RC_R, RC_B, 0.7);
        recolour_addRule(RC_B, RC_G, 0.9);
        recolour_addRule(RC_R, RC_G, 1.1);
    }
    if(rec_fn == mobord || processfn == randomizer) {
        mosaic_addColour(20, 20, 60);
        mosaic_addColour(60, 60, 100);
        mosaic_addColour(128, 128, 192); // soft blue
        mosaic_addColour(0xC0, 0x80, 0x80); // deep purple
        mosaic_addColour(250, 127, 127); // soft bright red
        mosaic_addColour(195, 230, 135); // light green
        mosaic_addColour(225, 237, 147); // yellow
        mosaic_addColour(250, 250, 142); // yellow2
    }
    if(rec_fn == mosaic || processfn == randomizer) {
        mosaic_addColour(0, 0, 0);
        mosaic_addColour(0x20, 0x20, 0x20);
        mosaic_addColour(0x40, 0x40, 0x40);
        mosaic_addColour(0x80, 0x80, 0x80);
        mosaic_addColour(0xA0, 0xA0, 0xA0);

        mosaic_addColour(128, 128, 192); // soft blue
        mosaic_addColour(0xC0, 0x80, 0x80); // deep purple
        mosaic_addColour(250, 127, 127); // soft bright red
        mosaic_addColour(195, 230, 135); // light green
        mosaic_addColour(222, 222, 155); // yellow

        mosaic_addColour(0xC0, 0xC0, 0xC0);
        mosaic_addColour(255, 255, 255);
    }
}

/** apply transformations on a file */
static void process(char const* file)
{
    printf("%s: reading pixels\n", file);
    img_t img = readPixels(file);
    img_t alt = img;

    if(!isCGADither) {
        printf("%s: getting square\n", file);
        img = getSquare(img);
        free(alt.pixels);
        alt = img;

        printf("%s: downsampling\n", file);
        img = downSample800(img);
        free(alt.pixels);
        alt = img;
    }

    printf("%s: recolouring\n", file);
    img = rec_fn(img);
    free(alt.pixels);
    alt = img;

    if(rec_fn == recolour) {
        printf("%s: framing\n", file);
        img = frame(img);
        free(alt.pixels);
        //alt = img;
    }

    if(!isCGADither) {
        char* outFile = getOutFileName(file, ".out.jpg");
        printf("%s: saving as %s\n", file, outFile);
        savePixels(img, outFile);
        free(outFile);
    } else {
        char* outFile = getOutFileName(file, ".out.tga");
        printf("%s: saving as %s\n", file, outFile);
        tga_write(img, outFile);
        free(outFile);
    }
    free(img.pixels);
}

void randomizer(char const* file)
{
    typedef img_t (*fn_t)(img_t const);
    fn_t fns[] = {
        mosaic,
        mobord,
        faith,
        recolour,
        rgfilter,
        cgadither,
        cgadither2,
        cgaditherfs,
        cgaditherfs2,
        cgaditherfs3,
    };
    int const size = sizeof(fns)/sizeof(fns[0]);

    int idx = (int)(rand() % size);
    int rrr = (int)rand();
    opt_alt =  (rrr & 0x1) >> 0;
    opt_balt = (rrr & 0x2) >> 1;
    opt_calt = (rrr & 0x4) >> 2;
    isCGADither = idx > 4;

    rec_fn = fns[idx];


    return process(file);
}

void usage(char const* name)
{
    fprintf(stderr, "usage: %s <filter> pic1.jpg pic2.jpg pic3.jpg ...\n", name);
    fprintf(stderr, "    filter may be: -1  (original)\n");
    fprintf(stderr, "                   -2  (mosaic)\n");
    fprintf(stderr, "                   -3  (mobord)\n");
    fprintf(stderr, "                   -4  (faith)\n");
    fprintf(stderr, "                   -5  (rgfilter)\n");
    fprintf(stderr, "                   -6  (cgadither)\n");
    fprintf(stderr, "                   -6a (cgadither with RYGb pallette)\n");
    fprintf(stderr, "                   -7  (cgadither2)\n");
    fprintf(stderr, "                   -7a (cgadither2 with RYGb pallette)\n");
    fprintf(stderr, "                   -8[aec] (cgaditherfs)\n");
    fprintf(stderr, "                     a = use alternate color pallette\n");
    fprintf(stderr, "                     e = use expanded color pallette\n");
    fprintf(stderr, "                     c = alternative color bias\n");
    fprintf(stderr, "                   -9[acb] (cgaditherfs2)\n");
    fprintf(stderr, "                     a = use alternate color pallette\n");
    fprintf(stderr, "                     c = alternative color bias\n");
    fprintf(stderr, "                     b = another alternative color bias\n");
    fprintf(stderr, "                   -C[acb] (cgaditherfs3)\n");
    fprintf(stderr, "                     a = use alternate color pallette\n");
    fprintf(stderr, "                     c = alternative color bias\n");
    fprintf(stderr, "                     b = another alternative color bias\n");
    fprintf(stderr, "                   -r  (random filter)\n");
    exit(255);
}

int main(int argc, char* argv[])
{
    int i;

#ifdef VERSION
    printf("Jak's Oesterreich photo filter %s\n", VERSION);
#endif
    printf("(C) Vlad Mesco, all rights reserved.\n");
    printf("See LICENSE for information on the terms this software is provided under\n");

    if(argc <= 2
            || argv[1][0] != '-')
    {
        usage(argv[0]);
    }

    if(strlen(argv[1]) < 2) usage(argv[0]);

    switch(argv[1][1]) {
    case '1':
        rec_fn = recolour;
        break;
    case '2':
        rec_fn = mosaic;
        break;
    case '3':
        rec_fn = mobord;
        break;
    case '4':
        rec_fn = faith;
        break;
    case '5':
        rec_fn = rgfilter;
        break;
    case '6':
        rec_fn = cgadither;
        isCGADither = 1;
        if(argv[1][2] == 'a') opt_alt = 1;
        break;
    case '7':
        rec_fn = cgadither2;
        isCGADither = 1;
        if(argv[1][2] == 'a') opt_alt = 1;
        break;
    case '8':
        rec_fn = cgaditherfs;
        isCGADither = 1;
        {
            for(int i = 2; argv[1][i]; ++i) {
                if(argv[1][i] == 'a') opt_alt = 1;
                if(argv[1][i] == 'e') opt_balt = 1;
                if(argv[1][i] == 'c') opt_calt = 1;
            }
        }
        break;
    case '9':
        rec_fn = cgaditherfs2;
        isCGADither = 1;
        {
            for(int i = 2; argv[1][i]; ++i) {
                if(argv[1][i] == 'a') opt_alt = 1;
                if(argv[1][i] == 'c') opt_calt = 1;
                if(argv[1][i] == 'b') opt_balt = 1;
            }
            if(opt_calt && opt_balt) {
                fprintf(stderr, "WARN: option 9c overrides 9b!\n");
            }
        }
        break;
    case 'C':
        rec_fn = cgaditherfs3;
        isCGADither = 1;
        {
            for(int i = 2; argv[1][i]; ++i) {
                if(argv[1][i] == 'a') opt_alt = 1;
                if(argv[1][i] == 'c') opt_calt = 1;
                if(argv[1][i] == 'b') opt_balt = 1;
                if(argv[1][i] == 'A') opt_alt = 2;
            }
            if(opt_calt && opt_balt) {
                fprintf(stderr, "WARN: option 9c overrides 9b!\n");
            }
        }
        break;
    case 'r':
        processfn = randomizer;
        break;
    default:
        usage(argv[0]);
    }

    srand(time(NULL));

    init_recolour();

    for(i = 2; i < argc; processfn(argv[i++]));

    return 0;
}
