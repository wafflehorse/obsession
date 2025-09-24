#include <stdio.h>
#include <stdlib.h>
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define BAKED_FONT_SIZE 8
#define TTF_FILE_PATH "/Users/parkerpestana/Library/Fonts/PressStart2P-Regular.ttf"
#define OUTPUT_TEXTURE_FILE_PATH "./resources/assets/font_texture.png"
#define OUTPUT_FONT_DATA_BIN_PATH "./resources/assets/font_data"

struct Vec2 {
    float x;
    float y;
};

struct FontData {
    int ascent;
    int descent;
    int line_gap;
    float baked_pixel_size;
    Vec2 texture_dimensions;
    stbtt_packedchar char_data[95];
};

int main(int argc, char* argv[]) {
    stbtt_pack_context spc;
    unsigned char* pixels = (unsigned char*)malloc(TEXTURE_WIDTH * TEXTURE_HEIGHT);

    if (stbtt_PackBegin(&spc, pixels, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, 1, NULL) == 0) {
        fprintf(stderr, "Failed to PackBegin\n");
        return 1;
    }

    // Makes smaller text look better
    // Might want to adjust this if text doesn't look good in game
    // This fucks up pixel art. So not using it here
    // stbtt_PackSetOversampling(&spc, 2, 2);

    // 1 mb buffer
    unsigned char ttf_buffer[1 << 20];
    // "b" here reads it in binary mode
    FILE* ttf_file = fopen(TTF_FILE_PATH, "rb");
    if (!ttf_file) {
        fprintf(stderr, "Failed to load in ttf file\n");
        return 1;
    }

    unsigned long bytes_read = fread(ttf_buffer, 1, 1 << 20, ttf_file);
    printf("Read in %lu bytes from ttf file\n", bytes_read);

    fclose(ttf_file);

    stbtt_fontinfo font_info;
    if (stbtt_InitFont(&font_info, (unsigned char*)ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0)) == 0) {
        fprintf(stderr, "Failed to initialize font info\n");
        return 1;
    }
    int font_count = stbtt_GetNumberOfFonts(ttf_buffer);
    printf("Number of fonts in file: %d\n", font_count);

    // int advance_space, lsb_space;
    // stbtt_GetCodepointHMetrics(&font_info, ' ', &advance_space, &lsb_space);

    FontData font_data;

    font_data.baked_pixel_size = BAKED_FONT_SIZE;
    font_data.texture_dimensions = { (float)TEXTURE_WIDTH, (float)TEXTURE_HEIGHT };

    if (stbtt_PackFontRange(&spc, ttf_buffer, 0, font_data.baked_pixel_size, 32, 95, font_data.char_data) == 0) {
        fprintf(stderr, "Failed to pack font range\n");
        return 1;
    }

    stbtt_PackEnd(&spc);

    stbtt_GetFontVMetrics(&font_info, &font_data.ascent, &font_data.descent, &font_data.line_gap);

    FILE* file = fopen(OUTPUT_FONT_DATA_BIN_PATH, "wb");
    fwrite(&font_data, 1, sizeof(font_data), file);
    fclose(file);

    stbi_write_png(OUTPUT_TEXTURE_FILE_PATH, TEXTURE_WIDTH, TEXTURE_HEIGHT, 1, pixels, TEXTURE_WIDTH);

    printf("Done processing fonts\n");
    return 0;
}
