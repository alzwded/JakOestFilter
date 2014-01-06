#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"

extern img_t readPixels(char const*);
extern int savePixels(img_t const, char const*);

extern img_t getSquare(img_t const);
extern img_t downSample800(img_t const);
extern img_t recolour(img_t const);
extern img_t frame(img_t const);

char* getOutFileName(char const* file)
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

static void init_recolour()
{
    recolour_addRule(RC_G, RC_R, 1.29);
    recolour_addRule(RC_G, RC_B, 1.25);
    recolour_addRule(RC_R, RC_B, 1.3);
    recolour_addRule(RC_B, RC_G, 0.7);
    recolour_addRule(RC_R, RC_G, 1.1);
}

void process(char const* file)
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

    init_recolour();

    printf("%s: recolouring\n", file);
    img = recolour(img);
    free(alt.pixels);
    alt = img;

    printf("%s: framing\n", file);
    img = frame(img);
    free(alt.pixels);
    //alt = img;

    char* outFile = getOutFileName(file);
    printf("%s: saving as %s\n", file, outFile);
    savePixels(img, outFile);
    free(outFile);
    free(img.pixels);
}

int main(int argc, char* argv[])
{
    int i;

    if(argc <= 1) {
        fprintf(stderr, "usage: %s pic1.jpg pic2.jpg pic3.jpg ...\n", argv[0]);
        return 255;
    }

    for(i = 1; i < argc; ++i) {
        process(argv[i]);
    }

    return 0;
}
