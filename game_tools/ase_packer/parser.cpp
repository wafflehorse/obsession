#include <ctype.h>
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

static Arena arena = {};

int parse_string(FileContents* file_contents, int start_idx, char* string) {
	ASSERT(file_contents->data[start_idx] == '"', "Keys must start with \"");

	int i = start_idx + 1;
	char character = file_contents->data[i++];
	uint32 string_length = 0;

	while(character != '"') {
		string[string_length++] = character;
		character = file_contents->data[i++];
	}

	return i + 1;
}

int parse_array(FileContents* file_contents, int i) {
	ASSERT(file_contents->data[i] == '[', "Arrays must start with [");

	while(file_contents->data[i++] != ']') {
				
	}

	return i + 1;
}

int parse_numeric(FileContents* file_contents, int i, float* number) {
	ASSERT(isdigit(file_contents->data[i]), "numerics must start with a digit");

	char number_string[MAX_VALUE_LENGTH];
	number_string[i] = file_contents->data[i];
	uint32 num_str_len = 0;

	while(isdigit(file_contents->data[i]) || file_contents->data[i] == '.') {
		number_string[num_str_len++] = file_contents->data[i++]; 
	} 
	number_string[num_str_len] = '\0';

	char* end;
	*number = strtof(number_string, &end);

	return i + 1;
}

int parse_object(FileContents* file_contents, int start_idx) {
	ASSERT(file_contents->data[start_idx] == '{', "Objects must start with {");
	bool left_side = true;
	int i = start_idx + 1;

	while(file_contents->data[i] != '}') {
		if(file_contents->data[i] == '"' && left_side) {
			char string[MAX_KEY_LENGTH] = {};
			i = parse_string(file_contents, i, string);		
			left_side = false;
			printf("key: %s\n", string);
		}
		else if(file_contents->data[i] == '"' && !left_side) {
			char string[MAX_VALUE_LENGTH] = {};
			i = parse_string(file_contents, i, string);
			left_side = true;
			printf("value: %s\n", string);
		}
		else if(file_contents->data[i] == '{' && !left_side) {
			i = parse_object(file_contents, i);
			left_side = true;
			printf("parsing an object\n");
		}
		else if(isdigit(file_contents->data[i]) && !left_side) {
			float number;
			i = parse_numeric(file_contents, i, &number); 
			left_side = true;
			printf("numeric: %f\n", number);
		}
		i++;
	}

	return i + 1;
}

void parse(FileContents* file_contents, int start_idx) {
	int i = start_idx;
	while(i < file_contents->size_bytes) {
		if(file_contents->data[i] == '{') {
			i = parse_object(file_contents, i);
		}
		else {
			i++;
		}
	}
}

bool trim(unsigned char* bitmap, int num_channels, int bitmap_width, int bitmap_height, 
		  unsigned char** trimmed_bitmap, int* trimmed_width, int* trimmed_height) {
	if(bitmap_width < 1 || bitmap_height < 1) {
		return false;
	}
	int min_x = bitmap_width;
	int max_x = 0;
	int min_y = bitmap_height;
	int max_y = 0;
	bool is_occupied = false;

	for(int i = 0; i < bitmap_width * bitmap_height; i++) {
		unsigned char* pixel_alpha = bitmap + (i * num_channels) + (num_channels - 1);		
		int x = i % bitmap_width;
		int y = i / bitmap_width;
		if(*pixel_alpha != 0) {
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

int main(int argc, char* argv[]) {
	arena.size = Megabytes(100);
	arena.data = (char*)malloc(arena.size);
	arena.next = arena.data;

// struct FileContents {
//     char* data;
//     uint64 size_bytes;
// };
	const char* asset_dir = "./assets/bitmaps";
	DIR* dir = opendir(asset_dir);
	if(!dir) {
		fprintf(stderr, "Failed to open directory");
		return 1;
	}

	char bitmap_filepaths[MAX_BITMAPS][W_PATH_MAX] = {};
	uint32 bitmap_file_count = 0;

	dirent* entry;
	while((entry = readdir(dir)) != NULL) {
		if(entry->d_type == DT_DIR) {
			continue;
		}

		char* ext = strrchr(entry->d_name, '.');
		if(w_str_match(ext, ".png")) {
			printf("found png: %s\n", entry->d_name);
			w_get_absolute_path(bitmap_filepaths[bitmap_file_count++], "./assets/bitmaps/", entry->d_name);
		}
	}

	stbrp_rect bitmap_rects[MAX_BITMAPS];

	stbrp_context pack_context = {};
	int num_nodes = ATLAS_WIDTH;
	stbrp_node *nodes = (stbrp_node*)w_arena_alloc(&arena, num_nodes * sizeof(stbrp_node));
	stbrp_init_target(&pack_context, ATLAS_WIDTH, ATLAS_HEIGHT, nodes, num_nodes);

	int bitmap_count = 0;

	for(int i = 0; i < bitmap_file_count; i++) {
		int bitmap_width, bitmap_height, nrChannels;

		unsigned char* bitmap_data = stbi_load(bitmap_filepaths[i], &bitmap_width, &bitmap_height, &nrChannels, 0);

		char channel_assert_message[W_PATH_MAX];
		snprintf(channel_assert_message, W_PATH_MAX, "Bitmap %s had %i channels instead of the expected 4", bitmap_filepaths[i], nrChannels);
		// ASSERT(nrChannels == 4, channel_assert_message);
		if(nrChannels != 4) {
			printf("%s\n", channel_assert_message);
		}

		unsigned char* trimmed_bitmap = NULL;
		int trimmed_width, trimmed_height;
		if(trim(bitmap_data, nrChannels, bitmap_width, bitmap_height, &trimmed_bitmap, &trimmed_width, &trimmed_height)) {
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

	if(stbrp_pack_rects(&pack_context, bitmap_rects, bitmap_count) == 0) {
		fprintf(stderr, "Failed to pack rects!\n");
	}

	uint32 atlas_size_bytes = ATLAS_WIDTH * ATLAS_HEIGHT * 4 * sizeof(unsigned char);
	unsigned char* pixels = (unsigned char*)w_arena_alloc(&arena, atlas_size_bytes);
	memset(pixels, 0, atlas_size_bytes);
	printf("Allocated memory for atlas of size %i x %i for total of %i bytes\n", ATLAS_WIDTH, ATLAS_HEIGHT, atlas_size_bytes);

	for(int i = 0; i < bitmap_count; i++) {
		int bitmap_width, bitmap_height, nrChannels;

		int filepath_index = bitmap_rects[i].id;
		unsigned char* bitmap_data = stbi_load(bitmap_filepaths[filepath_index], &bitmap_width, &bitmap_height, &nrChannels, 0);

		stbrp_rect rect = bitmap_rects[i];
        
        if(!rect.was_packed) {
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

		for(int row = 0; row < trimmed_height; row++) {
			memcpy(dest_row_start, src_row_start, trimmed_width * 4);
			dest_row_start += (4 * ATLAS_WIDTH);
			src_row_start += (4 * bitmap_width);
		}
			
		free(bitmap_data);
	}

	stbi_write_png("./assets/sprite_atlas.png", ATLAS_WIDTH, ATLAS_HEIGHT, 4, pixels, ATLAS_WIDTH * 4);

	



	
	// FileContents file_contents;
	//
	// w_read_file_abs(ASE_DATA_FILE_PATH, &file_contents, &arena);
	//
	// parse(&file_contents, 0);

	return 0;
}

// #include "sprites.h"
//
// const Sprite sprite_table[SPRITE_COUNT] = {
//     [SPRITE_HERO_IDLE_DOWN_0] = {
//         "hero_idle_down_0", { 0, 0, 16, 16 }, 100.0f, 0
//     },
//     [SPRITE_HERO_IDLE_DOWN_1] = {
//         "hero_idle_down_1", { 16, 0, 16, 16 }, 100.0f, 0
//     },
//     [SPRITE_HERO_IDLE_DOWN_2] = {
//         "hero_idle_down_2", { 32, 0, 16, 16 }, 100.0f, 0
//     },
//     [SPRITE_ZOMBIE_WALK_LEFT_0] = {
//         "zombie_walk_left_0", { 0, 16, 16, 16 }, 120.0f, 0
//     },
//     [SPRITE_ZOMBIE_WALK_LEFT_1] = {
//         "zombie_walk_left_1", { 16, 16, 16, 16 }, 120.0f, 0
//     },
//     [SPRITE_BULLET] = {
//         "bullet", { 0, 32, 8, 8 }, 0.0f, 0
//     },
// };
//
// const Animation animation_table[ANIM_COUNT] = {
//     [ANIM_HERO_IDLE_DOWN] = {
//         "hero_idle_down",
//         ANIM_HERO_IDLE_DOWN,
//         {
//             SPRITE_HERO_IDLE_DOWN_0,
//             SPRITE_HERO_IDLE_DOWN_1,
//             SPRITE_HERO_IDLE_DOWN_2
//         },
//         3,
//         true
//     },
//     [ANIM_ZOMBIE_WALK_LEFT] = {
//         "zombie_walk_left",
//         ANIM_ZOMBIE_WALK_LEFT,
//         {
//             SPRITE_ZOMBIE_WALK_LEFT_0,
//             SPRITE_ZOMBIE_WALK_LEFT_1
//         },
//         2,
//         true
//     },
// };
