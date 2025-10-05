#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define ASE_DATA_FILE_PATH "./resources/assets/packed_sprite_data.json"
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION

// this surpressed the unused function warning in the lib
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma clang diagnostic pop

#define WAFFLE_LIB_IMPLEMENTATION
#include "waffle_lib.h"
#undef WAFFLE_LIB_IMPLEMENTATION

#define MAX_KEY_LENGTH 128
#define MAX_VALUE_LENGTH 256

#define MAX_BITMAPS 1000
#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024

struct AnimationExtraction {
    char animation_label[256];
    char sprite_labels[256][256];
    uint32 frame_durations[256];
    uint32 frame_count;
};

struct AsepriteAnimationExtract {
    uint32 frame_durations[256];
    uint32 frame_count;
    Animation animations[128];
    uint32 animation_count;
};

void aseprite_anim_parse(FileContents* file_contents, const char* entity_name, AnimationExtraction* anim_extractions, uint32* anim_count) {
    char* cursor = file_contents->data;
    uint32 all_frame_durations[512] = {};
    uint32 all_frames_count = 0;

    while ((cursor = strstr(cursor, "duration")) != NULL) {
        cursor += w_str_len("duration");
        char duration_str[16] = {};
        uint32 length = 0;
        char* value_cursor = cursor;
        while (!isdigit(*value_cursor)) {
            value_cursor++;
        }
        while (isdigit(*value_cursor)) {
            duration_str[length++] = *value_cursor;
            value_cursor++;
        }
        char* end;
        all_frame_durations[all_frames_count++] = strtoul(duration_str, &end, 10);
    }

    printf("Processing %i frames...\n", all_frames_count);

    cursor = file_contents->data;
    while ((cursor = strstr(cursor, "name")) != NULL) {
        char name[64] = {};
        uint32 name_length = 0;
        cursor = strstr(cursor, ":");
        while (*cursor != '"') { cursor++; }
        cursor++;
        while (*cursor != '"') {
            name[name_length++] = *cursor;
            cursor++;
        }

        char from_str[64] = {};
        uint32 from_length = 0;
        cursor = strstr(cursor, "from");

        while (!isdigit(*cursor)) { cursor++; }
        while (isdigit(*cursor)) {
            from_str[from_length++] = *cursor;
            cursor++;
        }

        char* end;
        uint32 start_idx = strtoul(from_str, &end, 10);

        char to_str[64] = {};
        uint32 to_length = 0;
        cursor = strstr(cursor, "to");

        while (!isdigit(*cursor)) { cursor++; }
        while (isdigit(*cursor)) {
            to_str[to_length++] = *cursor;
            cursor++;
        }

        uint32 end_idx = strtoul(to_str, &end, 10);

        AnimationExtraction* anim_extract = &anim_extractions[(*anim_count)++];
        char* animation_label = anim_extract->animation_label;
        snprintf(animation_label, sizeof(anim_extract->animation_label), "ANIM_%s_%s", entity_name, name);
        w_to_uppercase(animation_label);

        for (int i = start_idx; i < end_idx + 1; i++) {
            anim_extract->frame_durations[anim_extract->frame_count] = all_frame_durations[i];
            char* sprite_label = anim_extract->sprite_labels[anim_extract->frame_count];
            w_str_copy(sprite_label, "SPRITE_");
            w_str_concat(sprite_label, entity_name);
            w_str_concat(sprite_label, "_");
            w_str_concat(sprite_label, name);
            char frame_index_str[64] = {};
            snprintf(frame_index_str, 64, "_%i", anim_extract->frame_count);
            w_str_concat(sprite_label, frame_index_str);
            w_to_uppercase(sprite_label);
            anim_extract->frame_count++;
        }
    }
}
void enum_begin(const char* enum_name, FILE* file) {
	char enum_declaration[64] = {}; 
	snprintf(enum_declaration, 64, "enum %s {\n", enum_name);
    fwrite(enum_declaration, w_str_len(enum_declaration), sizeof(char), file);
}

void enum_add(const char* enum_value, FILE* file) {
	char enum_label[256] = {};
	snprintf(enum_label, 256, "\t%s,\n", enum_value);
	fwrite(enum_label, w_str_len(enum_label), sizeof(char), file);
}

void enum_close(FILE* file) {
	const char* enum_close = "};\n\n";
	fwrite(enum_close, w_str_len(enum_close), sizeof(char), file);
}


bool trim(unsigned char* bitmap, int num_channels, int bitmap_width, int bitmap_height,
    unsigned char** trimmed_bitmap, int* trimmed_width, int* trimmed_height) {
    if (bitmap_width < 1 || bitmap_height < 1) {
        return false;
    }
    int min_x = bitmap_width;
    int max_x = 0;
    int min_y = bitmap_height;
    int max_y = 0;
    bool is_occupied = false;

    for (int i = 0; i < bitmap_width * bitmap_height; i++) {
        unsigned char* pixel_alpha = bitmap + (i * num_channels) + (num_channels - 1);
        int x = i % bitmap_width;
        int y = i / bitmap_width;
        if (*pixel_alpha != 0) {
            min_x = w_min(min_x, x);
            max_x = w_max(max_x, x);
            min_y = w_min(min_y, y);
            max_y = w_max(max_y, y);
            is_occupied = true;
        }
    }

    *trimmed_bitmap = bitmap + (min_y * num_channels * bitmap_width) + (min_x * num_channels);
    *trimmed_width = max_x - min_x + 1;
    *trimmed_height = max_y - min_y + 1;

    return is_occupied;
}

int filepath_sort_cmp(const void* a, const void* b) {
    return w_str_cmp((const char*)a, (const char*)b);
}

int main(int argc, char* argv[]) {
    Arena arena;
    arena.size = Megabytes(100);
    arena.data = (char*)malloc(arena.size);
    arena.next = arena.data;

    //~~~~~~~~~~~~~~~~~~Gather and Sort Bitmap Files~~~~~~~~~~~~~~~~~~~~~//

    char (*bitmap_filepaths)[W_PATH_MAX] = (char(*)[W_PATH_MAX])w_arena_alloc(&arena, MAX_BITMAPS * W_PATH_MAX * sizeof(char));
    uint32 bitmap_file_count = 0;

    const char* asset_dir = "./assets/bitmaps";
    DIR* bitmap_dir = opendir(asset_dir);
    if (!bitmap_dir) {
        fprintf(stderr, "Failed to open directory");
        return 0;
    }

    dirent* bitmap_entry;
    while ((bitmap_entry = readdir(bitmap_dir)) != NULL) {
        if (bitmap_entry->d_type == DT_DIR) {
            continue;
        }

        char* ext = strrchr(bitmap_entry->d_name, '.');
        if (w_str_match(ext, ".png")) {
            w_get_absolute_path(bitmap_filepaths[bitmap_file_count++], "./assets/bitmaps/", bitmap_entry->d_name);
        }
    }

    qsort(bitmap_filepaths, bitmap_file_count, W_PATH_MAX, filepath_sort_cmp);

    //~~~~~~~~~~~~~~~~~~Pack Bitmaps into Atlas Texture~~~~~~~~~~~~~~~~~~~~~//

    stbrp_rect bitmap_rects[MAX_BITMAPS];

    stbrp_context pack_context = {};
    int num_nodes = ATLAS_WIDTH;
    stbrp_node* nodes = (stbrp_node*)w_arena_alloc(&arena, num_nodes * sizeof(stbrp_node));
    stbrp_init_target(&pack_context, ATLAS_WIDTH, ATLAS_HEIGHT, nodes, num_nodes);

    int bitmap_count = 0;

    for (int i = 0; i < bitmap_file_count; i++) {
        int bitmap_width, bitmap_height, nrChannels;

        unsigned char* bitmap_data = stbi_load(bitmap_filepaths[i], &bitmap_width, &bitmap_height, &nrChannels, 0);

        char channel_assert_message[W_PATH_MAX];
        snprintf(channel_assert_message, W_PATH_MAX, "Bitmap %s had %i channels instead of the expected 4", bitmap_filepaths[i], nrChannels);
        // ASSERT(nrChannels == 4, channel_assert_message);
        if (nrChannels != 4) {
            printf("%s\n", channel_assert_message);
        }

        unsigned char* trimmed_bitmap = NULL;
        int trimmed_width, trimmed_height;
        if (trim(bitmap_data, nrChannels, bitmap_width, bitmap_height, &trimmed_bitmap, &trimmed_width, &trimmed_height)) {
            bitmap_rects[bitmap_count].w = trimmed_width;
            bitmap_rects[bitmap_count].h = trimmed_height;
            bitmap_rects[bitmap_count].id = i;
            bitmap_count++;
        }
        else {
            char empty_bmp_message[W_PATH_MAX] = {};
            snprintf(empty_bmp_message, W_PATH_MAX, "Empty bitmap found and skipped: %s", bitmap_filepaths[i]);
            printf("%s\n", empty_bmp_message);
        }
        free(bitmap_data);
    }

    printf("Found %i valid bitmaps to pack out of %i\n", bitmap_count, bitmap_file_count);

    if (stbrp_pack_rects(&pack_context, bitmap_rects, bitmap_count) == 0) {
        fprintf(stderr, "Failed to pack rects!\n");
    }

    uint32 atlas_size_bytes = ATLAS_WIDTH * ATLAS_HEIGHT * 4 * sizeof(unsigned char);
    unsigned char* pixels = (unsigned char*)w_arena_alloc(&arena, atlas_size_bytes);
    printf("Allocated memory for atlas of size %i x %i for total of %i bytes\n", ATLAS_WIDTH, ATLAS_HEIGHT, atlas_size_bytes);

    for (int i = 0; i < bitmap_count; i++) {
        int bitmap_width, bitmap_height, nrChannels;

        int filepath_index = bitmap_rects[i].id;
        unsigned char* bitmap_data = stbi_load(bitmap_filepaths[filepath_index], &bitmap_width, &bitmap_height, &nrChannels, 0);

        stbrp_rect rect = bitmap_rects[i];

        if (!rect.was_packed) {
            char not_packed_message[256] = {};
            snprintf(not_packed_message, 256, "%s is not packed!", bitmap_filepaths[filepath_index]);
            printf("%s\n", not_packed_message);
            continue;
        }

        unsigned char* trimmed_bitmap = NULL;
        int trimmed_width, trimmed_height;
        trim(bitmap_data, nrChannels, bitmap_width, bitmap_height, &trimmed_bitmap, &trimmed_width, &trimmed_height);

        unsigned char* src_row_start = trimmed_bitmap;
        unsigned char* dest_row_start = pixels + (4 * ATLAS_WIDTH * rect.y) + (4 * rect.x);

        for (int row = 0; row < trimmed_height; row++) {
            memcpy(dest_row_start, src_row_start, trimmed_width * 4);
            dest_row_start += (4 * ATLAS_WIDTH);
            src_row_start += (4 * bitmap_width);
        }

        free(bitmap_data);
    }

    stbi_write_png("./assets/sprite_atlas.png", ATLAS_WIDTH, ATLAS_HEIGHT, 4, pixels, ATLAS_WIDTH * 4);

    //~~~~~~~~~~~~~~~~~~~~Construct sprite_assets.h file with sprite and animation data ~~~~~~~~~~~~~~~//

		FILE* file = fopen("./assets/asset_ids.h", "wb");
	{
		const char* includes_section = "//This file was autogenerated via game_tools/asset_manager/pack\n#pragma once\n\n";
		fwrite(includes_section, w_str_len(includes_section), sizeof(char), file);
	}

	//~~~~~~~~~~~~~~~~~~~~~~ Sprite ID writing ~~~~~~~~~~~~~~~~~~~//
	
	enum_begin("SpriteID", file);

    char (*enum_labels)[256] = (char(*)[256])w_arena_alloc(&arena, MAX_BITMAPS * 256 * sizeof(char));

    for (int i = 0; i < bitmap_count; i++) {
        char* bitmap_filepath = bitmap_filepaths[bitmap_rects[i].id];
        char filepath_copy[W_PATH_MAX] = {};
        w_str_copy(filepath_copy, bitmap_filepath);

        char* token;
        char* filename;
        char* cursor = filepath_copy;
        while ((token = w_next_token(&cursor, "/")) != NULL) {
            filename = token;
        }

        cursor = filename;
        char* filename_no_ext = w_next_token(&cursor, ".");

        char* enum_str = enum_labels[i];
        w_str_copy(enum_str, "SPRITE_");
        cursor = filename_no_ext;
        while ((token = w_next_token(&cursor, "__")) != NULL) {
            w_to_uppercase(token);
            w_str_concat(enum_str, token);
            w_str_concat(enum_str, "_");
        }
        enum_str[w_str_len(enum_str) - 1] = '\0';

		enum_add(enum_labels[i], file);
    }

	enum_add("SPRITE_COUNT", file);
	enum_close(file);

	//~~~~~~~~~~~~~~~ Extract animation data ~~~~~~~~~~~~~~~~~~~//

    char (*json_filepaths)[W_PATH_MAX] = (char(*)[W_PATH_MAX])w_arena_alloc(&arena, W_PATH_MAX * 256 * sizeof(char));
    int json_file_count = 0;

    const char* aseprite_data_dir_path = "./assets/aseprite_data";
    DIR* aseprite_data_dir = opendir(aseprite_data_dir_path);
    if (!aseprite_data_dir) {
        fprintf(stderr, "Failed to open directory");
        return 0;
    }

    dirent* aseprite_data_entry;
    while ((aseprite_data_entry = readdir(aseprite_data_dir)) != NULL) {
        if (aseprite_data_entry->d_type == DT_DIR) {
            continue;
        }

        char* ext = strrchr(aseprite_data_entry->d_name, '.');
        if (w_str_match(ext, ".json")) {
            w_get_absolute_path(json_filepaths[json_file_count++], "./assets/aseprite_data/", aseprite_data_entry->d_name);
        }
    }

    printf("Found %i aseprite data files\n", json_file_count);

    AnimationExtraction* animation_extracts = (AnimationExtraction*)w_arena_alloc(&arena, 256 * sizeof(AnimationExtraction));
    uint32 anim_count = 0;

    for (int i = 0; i < json_file_count; i++) {
        char* marker = w_arena_marker(&arena);
        FileContents file_contents;

        w_read_file_abs(json_filepaths[i], &file_contents, &arena);

        char filename_no_ext[64] = {};
        w_filename_no_ext_from_path(json_filepaths[i], filename_no_ext);
		char* cursor = filename_no_ext;
		char* entity_name = w_next_token(&cursor, "_anim");

        aseprite_anim_parse(&file_contents, entity_name, animation_extracts, &anim_count);

        w_arena_restore(&arena, marker);
    }

	//~~~~~~~~~~~~~~~~~~~~ Write Animation IDs ~~~~~~~~~~~~~~~~~~~//

	enum_begin("AnimationID", file);

    for (int i = 0; i < anim_count; i++) {
        AnimationExtraction* anim_ext = &animation_extracts[i];
		enum_add(anim_ext->animation_label, file);
    }

	enum_add("ANIM_COUNT", file);
	enum_close(file);

	fclose(file);

    file = fopen("./assets/asset_tables.h", "wb");

	{
		const char* includes_section = "//This file was autogenerated via game_tools/asset_manager/pack\n#pragma once\n\n"
			"#include \"waffle_lib.h\"\n\n";
		fwrite(includes_section, w_str_len(includes_section), sizeof(char), file);
	}

	//~~~~~~~~~~~~~~ Sprite Table ~~~~~~~~~~~~~~~~~~~~~~//
    const char* sprite_table_declaration = "Sprite sprite_table[SPRITE_COUNT] = {\n";
    fwrite(sprite_table_declaration, w_str_len(sprite_table_declaration), sizeof(char), file);

    for (int i = 0; i < bitmap_count; i++) {
        char sprite_init[256] = {};
        stbrp_rect rect = bitmap_rects[i];
        snprintf(sprite_init, 256, "\t[%s] = {\n\t\t%i, %i, %i, %i\n\t},\n", enum_labels[i], rect.x, rect.y, rect.w, rect.h);
        fwrite(sprite_init, w_str_len(sprite_init), sizeof(char), file);
    }

    const char* sprite_table_close = "};\n\n";
    fwrite(sprite_table_close, w_str_len(sprite_table_close), sizeof(char), file);

	//~~~~~~~~~~~~~~~~~~~ Write Animation Table ~~~~~~~~~~~~~~~~~~~~~//

    const char* anim_table_definition = "Animation animation_table[ANIM_COUNT] = {\n";
    fwrite(anim_table_definition, w_str_len(anim_table_definition), sizeof(char), file);

    for (int i = 0; i < anim_count; i++) {
        AnimationExtraction* anim_ext = &animation_extracts[i];
        char anim_index[128] = {};
        snprintf(anim_index, 128, "\t[%s] = {\n", anim_ext->animation_label);
        fwrite(anim_index, w_str_len(anim_index), sizeof(char), file);

        char frames_init[256] = {};
        w_str_copy(frames_init, "\t\t.frames = {\n");
        for (int j = 0; j < anim_ext->frame_count; j++) {
            char frame_label[64] = {};
            snprintf(frame_label, 64, "\t\t\t{ %s, %i },\n", anim_ext->sprite_labels[j], anim_ext->frame_durations[j]);
            w_str_concat(frames_init, frame_label);
        }
        int frames_init_length = w_str_len(frames_init);
        frames_init[frames_init_length - 2] = '\0';
        w_str_concat(frames_init, "\n\t\t},\n");

        fwrite(frames_init, w_str_len(frames_init), sizeof(char), file);

        char frame_count_str[128] = {};
        snprintf(frame_count_str, 128, "\t\t.frame_count = %i\n", anim_ext->frame_count);
        fwrite(frame_count_str, w_str_len(frame_count_str), sizeof(char), file);

        char anim_index_close[16] = {};
        w_str_copy(anim_index_close, "\t}");
        if (i != (anim_count - 1)) {
            w_str_concat(anim_index_close, ",");
        }
        w_str_concat(anim_index_close, "\n");
        fwrite(anim_index_close, w_str_len(anim_index_close), sizeof(char), file);
    }

    const char* anim_table_close = "};";
    fwrite(anim_table_close, w_str_len(anim_table_close), sizeof(char), file);

    fclose(file);

    return 0;
}

