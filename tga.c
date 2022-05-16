#include "tga.h"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>

struct TGAHeader {
    int8_t idLen;                           // 0
    int8_t colorMapType;                    // 1
    int8_t imageType;                       // 2
        int16_t firstEntryIndex;            // 3
        int16_t colorMapLength;             // 5
        int8_t mapEntrySize;                // 7
        int16_t x, y, w, h;                 // 8
        int8_t pixelDepth;                  // 16
        int8_t alphaChannelDepthAndDirection; // 17; bits 0-3 are alpha channel, 4-5 are direction
}__attribute__((packed));
_Static_assert(sizeof(struct TGAHeader) == 18, "failed to pack");

void tga_write(img_t img, const char* outfile)
{
    struct TGAHeader header;
    FILE* g = fopen(outfile, "w");
    uint32_t buf[8096];
    int i, j;

    pixel_t p;
    header.idLen = 0;
    header.colorMapType = 0;
    header.imageType = 2; // uncompressed true-color image
    header.firstEntryIndex = 0;
    header.colorMapLength = 0;
    header.mapEntrySize = 0;
    header.x = 0;
    header.y = 0;
    header.w = img.w;
    header.h = img.h;
    header.pixelDepth = 32;
    header.alphaChannelDepthAndDirection = (2 << 4) // top left
                                                   //| (8 << 0) // 8bits of alpha
                                                   ;
    fwrite(&header, sizeof(struct TGAHeader), 1, g);
    printf("total pixels: %" PRIu64 "\n", (uint64_t)img.w*img.h);
    for(i = 0, j = 0; i < img.w*img.h; ++i) {
        p = img.pixels[i];
        buf[j++] = p.b | (p.g << 8) | (p.r << 16) | (0xFF << 24); // force alpha 1
        if(j >= sizeof(buf)/sizeof(buf[0])) {
            //printf("wrote %d\n", j);
            fwrite(buf, sizeof(buf)/sizeof(buf[0]), sizeof(buf[0]), g);
            j = 0;
        }
    }
    if(j > 0) {
        //printf("wrote %d\n", j);
        fwrite(buf, j, sizeof(buf[0]), g);
    }
#if 0
    // byte order: BGRA, i.e. a<<24|r<<16|g<<8|r<<0
    unsigned a = (255 << 24) | 20;
    unsigned b = (255 << 24) | 255;
    unsigned test[] = {
        a, b, b, a,
        b, a, a, b,
        b, b, a, a,
        b, b, a, a,
    };
    fwrite(test, sizeof(unsigned), 16, g);
#endif
    fclose(g);
}
