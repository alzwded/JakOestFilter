#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include <jpeglib.h>

#include "common.h"

/** read the pixels from a jpeg file */
img_t readPixels(char const* file)
{
    img_t ret = { 0, 0, NULL };

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer[1];

    FILE *infile = fopen(file, "rb");
    size_t location = 0;
    size_t i = 0;

    if(!infile) {
        printf("Error opening jpeg file %s\n!", file);
        return ret;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);

    jpeg_start_decompress(&cinfo);
    assert(cinfo.num_components == 3);

    ret.w = cinfo.output_width;
    ret.h = cinfo.output_height;
    ret.pixels = (pixel_t*)malloc(
            cinfo.output_width
            * cinfo.output_height
            * cinfo.num_components);

    row_pointer[0] = (uint8_t*)malloc(
            cinfo.output_width
            * cinfo.num_components);

    while(cinfo.output_scanline < cinfo.image_height) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        for(i = 0; i < cinfo.image_width * cinfo.num_components; i++) {
            ((uint8_t*)ret.pixels)[location++] = row_pointer[0][i];
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(row_pointer[0]);
    fclose(infile);

    return ret;
}

/** store pixels in a jpeg file */
int savePixels(img_t const img, char const* file)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer[1];
    FILE *outfile = fopen(file, "wb");

    if(!outfile) {
        printf("Error opening output jpeg file %s\n!", file);
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = img.w;        
    cinfo.image_height = img.h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);

    jpeg_start_compress(&cinfo, TRUE);
    while(cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &((uint8_t*)img.pixels)[
            cinfo.next_scanline
            * cinfo.image_width
            * cinfo.input_components];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);

    return 0;
}
