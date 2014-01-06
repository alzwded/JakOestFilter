#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"

extern pixel_t* readPixels(char const*);
extern int savePixels(pixel_t const*, char const*);

extern pixel_t* getSquare(pixel_t const*);
extern pixel_t* downSample800(pixel_t const*);
extern pixel_t* recolour(pixel_t const*);

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

void process(char const* file)
{
    pixel_t* img = readPixels(file);
    pixel_t* alt = img;

    img = getSquare(img);
    free(alt);
    alt = img;

    img = downSample800(img);
    free(alt);
    alt = img;

    img = recolour(img);
    free(alt);

    char* outFile = getOutFileName(file);
    savePixels(img, outFile);
    free(outFile);
    free(img);
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
