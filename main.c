#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"

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

img_t (*rec_fn)(img_t const) = NULL;

/** generate the output file name */
static char* getOutFileName(char const* file)
{
    size_t len = strlen(file);
    char const* i = file + len - 1;
    for(; i - file >= 0; --i) {
        if(*i == '/') break;
        if(*i == '.') {
            char* ret = (char*)malloc((i - file) + 9);
            strncpy(ret, file, i - file);
            strcpy(ret + (i - file), ".out.jpg");
            return ret;
        }
    }
    char* ret = (char*)malloc(len + 8);
    strcpy(ret, file);
    strcpy(ret + len, ".out.jpg");
    return ret;
}

/** initialise the recolouring engine */
static void init_recolour()
{
    if(rec_fn == recolour) {
        recolour_addRule(RC_G, RC_R, 1.29);
        recolour_addRule(RC_G, RC_B, 1.25);
        recolour_addRule(RC_R, RC_B, 0.7);
        recolour_addRule(RC_B, RC_G, 0.9);
        recolour_addRule(RC_R, RC_G, 1.1);
    } else if(rec_fn == mobord) {
        mosaic_addColour(20, 20, 60);
        mosaic_addColour(60, 60, 100);
        mosaic_addColour(128, 128, 192); // soft blue
        mosaic_addColour(0xC0, 0x80, 0x80); // deep purple
        mosaic_addColour(250, 127, 127); // soft bright red
        mosaic_addColour(195, 230, 135); // light green
        mosaic_addColour(225, 237, 147); // yellow
        mosaic_addColour(250, 250, 142); // yellow2
    } else if(rec_fn == mosaic) {
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

    printf("%s: getting square\n", file);
    img = getSquare(img);
    free(alt.pixels);
    alt = img;

    printf("%s: downsampling\n", file);
    img = downSample800(img);
    free(alt.pixels);
    alt = img;

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

    char* outFile = getOutFileName(file);
    printf("%s: saving as %s\n", file, outFile);
    savePixels(img, outFile);
    free(outFile);
    free(img.pixels);
}

void usage(char const* name)
{
    fprintf(stderr, "usage: %s <filter> pic1.jpg pic2.jpg pic3.jpg ...\n", name);
    fprintf(stderr, "    filter may be: -1 (original)\n");
    fprintf(stderr, "                   -2 (mosaic)\n");
    fprintf(stderr, "                   -3 (mobord)\n");
    fprintf(stderr, "                   -4 (faith)\n");
    fprintf(stderr, "                   -5 (rgfilter)\n");
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

    if(strlen(argv[1]) != 2) usage(argv[0]);

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
    default:
        usage(argv[0]);
    }

    init_recolour();

    for(i = 2; i < argc; process(argv[i++]));

    return 0;
}
