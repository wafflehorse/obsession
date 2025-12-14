#ifndef WAFFLE_LIB_H
#define WAFFLE_LIB_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>
#include "math.h"
#include "asset_ids.h"

#ifdef _WIN32
#define W_PATH_MAX 500 // 260 is standard
#else
#define W_PATH_MAX PATH_MAX
#endif

#define DEFAULT_ALIGNMENT (uint32)(2 * sizeof(void*))

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)

#ifdef DEBUG
#define ASSERT(exp, explain)                                                                                           \
    if (!(exp)) {                                                                                                      \
        printf("Assertion failed: %s\n", explain);                                                                     \
        *(volatile int*)0 = 0;                                                                                         \
    }
#else
#define ASSERT(exp, explain) ((void)0)
#endif

#define ArraySize(array) (sizeof(array) / sizeof(array[0]))

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef uint32 flags;

#define is_set(flags, flag) (flags & flag)
#define set(flags, flag) (flags |= flag)
#define unset(flags, flag) (flags &= ~flag)
#define toggle(flags, flag) (flags ^= flag)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ANIMATION_MAX_FRAME_COUNT 32

struct Vec2 {
    float x;
    float y;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct Rect {
    float x;
    float y;
    float w;
    float h;
};

struct Arena {
    char* data;
    char* next;
    long long size;
};

struct Sprite {
    float x;
    float y;
    float w;
    float h;
    bool has_ground_anchor;
    Vec2 ground_anchor;
};

#define ANIMATION_STATE_F_FLIP_X (1 << 0)

struct AnimationState {
    flags flags;
    AnimationID animation_id;
    uint32 current_frame;
    uint32 elapsed_frame_ms;
};

struct AnimationFrame {
    SpriteID sprite_id;
    uint32 duration_ms;
};

struct Animation {
    AnimationFrame frames[ANIMATION_MAX_FRAME_COUNT];
    uint32 frame_count;
};

#define COLOR_WHITE ((Vec4){255, 255, 255, 1})
#define COLOR_BLACK ((Vec4){0, 0, 0, 1})
#define COLOR_GRAY ((Vec4){96, 96, 96, 1})

//~~~~~~~~~~~~~~~~~~~~~~~~ FILE UTILITY TYPES ~~~~~~~~~~~~~~~~~~~~~~~//

struct FileContents {
    char* data;
    uint64 size_bytes;
};

struct WavFileHeader {
    uint32 riff_code;
    uint32 chunk_size;
    uint32 wav_id;
};

struct WavChunkHeader {
    uint32 id;
    uint32 size_bytes;
};

struct WavFmtChunk {
    uint16 format_tag; // This tells us whether it's compressed or not
    uint16 num_channels;
    uint32 sample_rate;
    uint32 byte_rate;
    uint16 block_align;
    uint16 bits_per_sample;
    // There are more optional fields in the case that format_tag != 1
};

struct WavFile {
    WavFileHeader* header;
    WavFmtChunk* fmt_chunk;
    int16* samples;
    uint32 num_sample_bytes;
};

struct RiffIterator {
    char* at;
    char* stop;
};

#endif

#ifdef WAFFLE_LIB_IMPLEMENTATION

static char i_base_path[W_PATH_MAX] = {};

double w_max(double a, double b) {
    if ((a - b) > 0) {
        return a;
    }
    return b;
}

double w_min(double a, double b) {
    if ((a - b) > 0) {
        return b;
    }
    return a;
}

float w_abs(float a) {
    return a > 0 ? a : -a;
}

float w_square(float a) {
    return a * a;
}

double w_avg(double* nums, uint32 count) {
    double result = 0;
    for (uint32 i = 0; i < count; i++) {
        result += nums[i];
    }

    if (count != 0) {
        result = result / count;
    }

    return result;
}

float w_avg(float* nums, uint32 count) {
    float result = 0;
    for (uint32 i = 0; i < count; i++) {
        result += nums[i];
    }

    if (count != 0) {
        result = result / count;
    }

    return result;
}

Vec2 w_vec_add(Vec2 a, Vec2 b) {
    return Vec2{a.x + b.x, a.y + b.y};
}

Vec4 w_vec_add(Vec4 a, Vec4 b) {
    return Vec4{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

Vec2 w_vec_add(Vec2 a, float b) {
    return Vec2{a.x + b, a.y + b};
}

Vec4 w_vec_add(Vec4 a, float b) {
    return Vec4{a.x + b, a.y + b, a.z + b, a.w + b};
}

Vec2 w_vec_sub(Vec2 a, Vec2 b) {
    return Vec2{a.x - b.x, a.y - b.y};
}

Vec4 w_vec_sub(Vec4 a, Vec4 b) {
    return Vec4{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

Vec2 w_vec_mult(Vec2 a, float b) {
    return Vec2{a.x * b, a.y * b};
}

Vec4 w_vec_mult(Vec4 a, float b) {
    return Vec4{a.x * b, a.y * b, a.z * b, a.w * b};
}

Vec2 w_vec_invert(Vec2 a) {
    return Vec2{a.x * -1.0f, a.y * -1.0f};
}

Vec2 w_vec_norm(Vec2 a) {
    float length = sqrtf(w_square(a.x) + w_square(a.y));

    Vec2 result = {0, 0};
    if (length != 0) {
        result = {a.x / length, a.y / length};
    };

    return result;
}

Vec2 w_vec_unit_from_radians(float radians) {
    Vec2 result = {.x = cosf(radians), .y = sinf(radians)};

    return result;
}

float w_vec_length(Vec2 a) {
    return sqrtf(w_square(a.x) + w_square(a.y));
}

float w_dot_product(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

float w_euclid_dist(Vec2 a, Vec2 b) {
    return sqrtf(w_square(a.x - b.x) + w_square(a.y - b.y));
}

Rect w_rect_mult(Rect rect, float scalar) {
    return (Rect){rect.x * scalar, rect.y * scalar, rect.w * scalar, rect.h * scalar};
}

Vec2 w_rect_top_left_to_center(Vec2 top_left, Vec2 size) {
    Vec2 result = top_left;

    result.x += size.x / 2;
    result.y -= size.y / 2;

    return result;
}

int w_str_len(const char* str) {
    int result = 0;
    while (str[result] != '\0') {
        result++;
    }
    return result;
}

// TODO: make this more safe
void w_str_copy(char* dest, const char* source) {
    int i = 0;
    while (source[i] != '\0') {
        dest[i] = source[i];
        i++;
    }
    dest[i] = '\0';
}

void w_str_concat(char* dest, const char* source) {
    int old_length = w_str_len(dest);
    int i = 0;
    while (source[i] != '\0') {
        dest[old_length + i] = source[i];
        i++;
    }
    dest[old_length + i] = '\0';
}

bool w_str_match(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }

    return a[i] == b[i];
}

// 0: equal
// <0: a < b
// >0: b < a

int w_str_cmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

bool w_str_match_start(const char* source_str, const char* sub_str) {
    int sub_str_len = w_str_len(sub_str);
    if (w_str_len(source_str) < sub_str_len) {
        return false;
    }

    for (int i = 0; i < sub_str_len; i++) {
        if (source_str[i] != sub_str[i]) {
            return false;
        }
    }

    return true;
}

char* w_next_token(char** input, const char* delimiter) {
    if (!*input || **input == '\0') {
        return NULL;
    }

    char* start = *input;
    int delimiter_len = w_str_len(delimiter);

    while (**input && !w_str_match_start(*input, delimiter)) {
        (*input)++;
    }

    if (w_str_match_start(*input, delimiter)) {
        **input = '\0';
        (*input) += delimiter_len;
    }

    return start;
}

void w_filename_no_ext_from_path(char* filepath, char* filename) {
    char filepath_copy[W_PATH_MAX] = {};
    w_str_copy(filepath_copy, filepath);

    char* token;
    char* filename_copy;
    char* cursor = filepath_copy;
    while ((token = w_next_token(&cursor, "/")) != NULL) {
        filename_copy = token;
    }

    cursor = filename_copy;
    filename_copy = w_next_token(&cursor, ".");
    w_str_copy(filename, filename_copy);
}

void w_to_uppercase(char* str) {
    while (*str) {
        if (*str >= 'a' && *str <= 'z') {
            *str = *str - ('a' - 'A');
        }
        str++;
    }
}

float w_round(float a) {
    float offset = 0.5f;
    if (a < 0) {
        offset = -0.5f;
    }
    return (float)(int)(a + offset);
}

float w_deg_to_rads(float degrees) {
    return degrees * (M_PI / 180.0f);
}

float w_randf() {
    return (float)rand() / RAND_MAX;
}

float w_random_between(float a, float b) {
    return a + (float)rand() / (float)RAND_MAX * (b - a);
}

float w_clamp_max(float a, float clamp_max) {
    return w_min(a, clamp_max);
}

float w_clamp_min(float a, float clamp_min) {
    return w_max(a, clamp_min);
}

float w_clamp_between(float a, float clamp_min, float clamp_max) {
    return w_min(w_max(a, clamp_min), clamp_max);
}

float w_clamp_01(float a) {
    return w_clamp_between(a, 0, 1);
}

bool w_rect_has_area(Rect rect) {
    return (rect.w > 0 && rect.h > 0);
}

float w_lerp(float a, float b, float t) {
    return a + (t * (b - a));
}

Vec2 w_calc_position(Vec2 acceleration, Vec2 velocity, Vec2 position, double dt_s) {
    return w_vec_add(w_vec_add(w_vec_mult(acceleration, 0.5f * w_square(dt_s)), w_vec_mult(velocity, dt_s)), position);
}

Vec2 w_calc_velocity(Vec2 acceleration, Vec2 velocity, double dt_s) {
    return w_vec_add(w_vec_mult(acceleration, dt_s), velocity);
}

Vec2 w_calc_position_delta(Vec2 acceleration, Vec2 velocity, Vec2 position, double dt_s) {
    return w_vec_add(w_vec_mult(acceleration, 0.5f * w_square(dt_s)), w_vec_mult(velocity, dt_s));
}

Vec2 w_rect_top_left(Rect subject) {
    return {subject.x - (subject.w / 2), subject.y + (subject.h / 2)};
}

bool w_check_point_in_rect(Rect subject, Vec2 point) {
    Vec2 subject_top_left = w_rect_top_left(subject);

    if (point.x >= subject_top_left.x && point.x <= (subject_top_left.x + subject.w) && point.y <= subject_top_left.y &&
        point.y >= (subject_top_left.y - subject.h)) {
        return true;
    }

    return false;
}

bool w_check_aabb_overlap(Rect subject, Rect target) {
    Vec2 subject_top_left = w_rect_top_left(subject);
    Vec2 target_top_left = w_rect_top_left(target);
    if (subject_top_left.x <= target_top_left.x + target.w && subject_top_left.x + subject.w >= target_top_left.x &&
        subject_top_left.y >= target_top_left.y - target.h && subject_top_left.y - subject.h <= target_top_left.y) {
        return true;
    }

    return false;
}

bool w_test_wall_collision(float start_x, float start_y, float delta_x, float delta_y, float wall_x, float wall_min_y,
                           float wall_max_y, float* t_min) {
    if (delta_x == 0.0f) {
        return false;
    }

    float t = (wall_x - start_x) / delta_x;

    if (t < *t_min && t >= 0) {
        float collision_y = (delta_y * t) + start_y;

        if (collision_y <= wall_max_y && collision_y >= wall_min_y) {
            *t_min = w_max(0.0f, t);
            return true;
        }
    }

    return false;
}

void w_rect_collision(Rect subject, Vec2 subject_delta, Rect target, Vec2 target_delta, float* t_collision,
                      Vec2* collision_normal) {
    // Uses minkowski sums
    float wall_top_y = target.y + (target.h / 2) + (subject.h / 2);
    float wall_bottom_y = target.y - (target.h / 2) - (subject.h / 2);
    float wall_left_x = target.x - (target.w / 2) - (subject.w / 2);
    float wall_right_x = target.x + (target.w / 2) + (subject.w / 2);
    Vec2 start_pos = {subject.x, subject.y};
    Vec2 rel_delta = w_vec_sub(subject_delta, target_delta);

    if (w_test_wall_collision(start_pos.x, start_pos.y, rel_delta.x, rel_delta.y, wall_left_x, wall_bottom_y,
                              wall_top_y, t_collision)) {
        *collision_normal = Vec2{-1, 0};
    }
    if (w_test_wall_collision(start_pos.x, start_pos.y, rel_delta.x, rel_delta.y, wall_right_x, wall_bottom_y,
                              wall_top_y, t_collision)) {
        *collision_normal = Vec2{1, 0};
    }
    if (w_test_wall_collision(start_pos.y, start_pos.x, rel_delta.y, rel_delta.x, wall_top_y, wall_left_x, wall_right_x,
                              t_collision)) {
        *collision_normal = Vec2{0, 1};
    }
    if (w_test_wall_collision(start_pos.y, start_pos.x, rel_delta.y, rel_delta.x, wall_bottom_y, wall_left_x,
                              wall_right_x, t_collision)) {
        *collision_normal = Vec2{0, -1};
    }
}

Vec2 w_rotate_around_pivot(Vec2 position, Vec2 pivot, float radians) {
    // here, we are basically creating a new coordinate system as if we rotated the x and y axis by theta. We then scale
    // our new axis by the entities position which gets the new position relative to the pivot.
    Vec2 relative_pos = w_vec_sub(position, pivot);
    Vec2 basis_x = w_vec_mult((Vec2){cosf(radians), sinf(radians)}, relative_pos.x);
    Vec2 basis_y = w_vec_mult((Vec2){-sinf(radians), cosf(radians)}, relative_pos.y);
    Vec2 relative_rotated_pos = w_vec_add(basis_x, basis_y);

    return w_vec_add(pivot, relative_rotated_pos);
}

void w_get_absolute_path(char* dest, const char* base_path, const char* rel_path) {
    w_str_copy(dest, base_path);
    w_str_concat(dest, rel_path);
}

char* w_arena_alloc(Arena* arena, long long size) {
    uintptr_t block = (uintptr_t)arena->next;

    // align to default alignment for better cache usage
    uintptr_t mod = block % (uintptr_t)DEFAULT_ALIGNMENT;

    if (mod != 0) {
        block += ((uintptr_t)DEFAULT_ALIGNMENT - mod);
    }

    arena->next = (char*)(block + (uintptr_t)size);

    ASSERT((uintptr_t)arena->next <= ((uintptr_t)arena->data + arena->size), "Arena has reached max capacity");

    memset((char*)block, 0, size);

    return (char*)block;
}

char* w_arena_marker(Arena* arena) {
    return arena->next;
}

void w_arena_restore(Arena* arena, char* marker) {
    arena->next = marker;
}

// Note: t should be between 0 and 1
float w_anim_ease_out_quad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

float w_anim_sine(float t, float speed, float amp) {
    return sinf(t * speed) * amp;
}

void w_init_waffle_lib(char* base_path) {
    w_str_copy(i_base_path, base_path);
}

//~~~~~~~~~~~~~~~~ FILE UTILITIES ~~~~~~~~~~~~~~~~~~//
#define RIFF_CODE(a, b, c, d) ((uint32)a << 0 | (uint32)b << 8 | (uint32)c << 16 | (uint32)d << 24)

int w_read_file_abs(const char* file_path, FileContents* file_contents, Arena* arena) {
    FILE* file = fopen(file_path, "rb");

    if (!file) {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    uint32 file_size_bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = w_arena_alloc(arena, file_size_bytes + 1);

    uint32 bytes_read = fread(data, 1, file_size_bytes, file);
    if (bytes_read != file_size_bytes) {
        fprintf(stderr, "There was an issue reading file: %s", file_path);
        return -1;
    }

    data[file_size_bytes] = '\0';

    fclose(file);

    file_contents->data = data;
    file_contents->size_bytes = file_size_bytes;
    return 0;
}

int w_read_file(const char* rel_path, FileContents* file_contents, Arena* arena) {
    char abs_path[W_PATH_MAX];
    w_get_absolute_path(abs_path, i_base_path, rel_path);
    return w_read_file_abs(abs_path, file_contents, arena);
}

int w_file_write_bin(const char* abs_path, char* data, int size) {
    FILE* f = fopen(abs_path, "wb");

    if (!f) {
        return -1;
    }

    fwrite(data, size, 1, f);
    fclose(f);

    return 0;
}

int w_read_wav_file(const char* file_path, WavFile* wav_file, Arena* arena) {
    FileContents file_contents;
    int read_file_result = w_read_file(file_path, &file_contents, arena);
    if (read_file_result != 0) {
        fprintf(stderr, "There was an issue reading wav file: %s", file_path);
        return -1;
    }

    WavFileHeader* wav_file_header = (WavFileHeader*)file_contents.data;
    ASSERT(wav_file_header->riff_code == RIFF_CODE('R', 'I', 'F', 'F'), "Wav file riff code is incorrect\n");
    ASSERT(wav_file_header->chunk_size == (file_contents.size_bytes - 8),
           "Wav file chunk size does not match file size\n");
    ASSERT(wav_file_header->wav_id == RIFF_CODE('W', 'A', 'V', 'E'), "Wav file wav id does not match \"WAVE\"\n");

    wav_file->header = wav_file_header;

    RiffIterator riff_iterator;
    riff_iterator.stop = (file_contents.data + file_contents.size_bytes);
    riff_iterator.at = (char*)(wav_file_header + 1);
    // ASSERT(x >= 0 && y >= 0, "coordinates passed to w_perlin must be positive");

    WavChunkHeader* chunk_header;

    while (riff_iterator.at < riff_iterator.stop) {
        chunk_header = (WavChunkHeader*)riff_iterator.at;
        switch (chunk_header->id) {
        case RIFF_CODE('f', 'm', 't', ' '): {
            WavFmtChunk* wav_fmt_chunk = (WavFmtChunk*)(chunk_header + 1);
            ASSERT(wav_fmt_chunk->format_tag == 1, "wav fmt format_tag must be 1");
            ASSERT(wav_fmt_chunk->num_channels == 2 || wav_fmt_chunk->num_channels == 1,
                   "wav fmt num channels must be 1 or 2");
            ASSERT(wav_fmt_chunk->sample_rate == 44100, "wav file must have a sample rate of 44100");
            ASSERT(wav_fmt_chunk->bits_per_sample == 16, "wav file must have bits per sample of 16");
            ASSERT(wav_fmt_chunk->block_align == wav_fmt_chunk->num_channels * (wav_fmt_chunk->bits_per_sample / 8),
                   "wav file block align must be correct");
            wav_file->fmt_chunk = wav_fmt_chunk;
            break;
        }
        case RIFF_CODE('d', 'a', 't', 'a'): {
            wav_file->samples = (int16*)(chunk_header + 1);
            wav_file->num_sample_bytes = chunk_header->size_bytes;
            break;
        }
        }

        riff_iterator.at = (char*)(chunk_header + 1) + chunk_header->size_bytes;
    }

    return 0;
}

time_t get_last_file_write_time(const char* file_path_rel) {
    struct stat file_stat;

    char file_path_abs[W_PATH_MAX];
    w_get_absolute_path(file_path_abs, i_base_path, file_path_rel);

    if (stat(file_path_abs, &file_stat) == 0) {
        return file_stat.st_mtime;
    } else {
        return -1;
    }
}

void w_timestamp_str(char* timestamp_str, int size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    strftime(timestamp_str, size, "%Y%m%d_%H%M%S", t);
}

static Animation* i_animation_table;

void w_init_animation(Animation* animation_table) {
    i_animation_table = animation_table;
}

// ~~~~~~~~~~~~~~~~ Sprites & Animations ~~~~~~~~~~~~~~~~~//

SpriteID w_animation_current_sprite(AnimationState* anim_state) {
    Animation curr_anim = i_animation_table[anim_state->animation_id];
    AnimationFrame curr_frame = curr_anim.frames[anim_state->current_frame];

    return curr_frame.sprite_id;
}

bool w_animation_complete(AnimationState* anim_state, double dt_s) {
    bool animation_complete = false;

    Animation curr_anim = i_animation_table[anim_state->animation_id];
    AnimationFrame curr_frame = curr_anim.frames[anim_state->current_frame];
    if (anim_state->current_frame >= (curr_anim.frame_count - 1) &&
        anim_state->elapsed_frame_ms + (dt_s * 1000) >= curr_frame.duration_ms) {
        animation_complete = true;
    }

    return animation_complete;
}

bool w_update_animation(AnimationState* anim_state, double dt_s) {
    bool animation_complete = false;
    if (anim_state->animation_id != ANIM_UNKNOWN) {
        anim_state->elapsed_frame_ms += dt_s * 1000;
        Animation curr_anim = i_animation_table[anim_state->animation_id];
        AnimationFrame curr_frame = curr_anim.frames[anim_state->current_frame];

        if (anim_state->elapsed_frame_ms >= curr_frame.duration_ms) {
            anim_state->current_frame++;
            anim_state->elapsed_frame_ms = 0;

            if (anim_state->current_frame >= (curr_anim.frame_count)) {
                animation_complete = true;
                anim_state->current_frame = 0;
            }
        }
    }

    return animation_complete;
}

void w_play_animation(AnimationID animation_id, AnimationState* anim_state, flags anim_state_opts) {
    if (anim_state->animation_id != animation_id || anim_state_opts != anim_state->flags) {
        anim_state->animation_id = animation_id;
        anim_state->current_frame = 0;
        anim_state->elapsed_frame_ms = 0;
        anim_state->flags = anim_state_opts;
    }
}

void w_play_animation(AnimationID animation_id, AnimationState* anim_state) {
    w_play_animation(animation_id, anim_state, 0);
}
// ~~~~~~~~~~~~~~~~~~~~ XorShift Rand ~~~~~~~~~~~~~~~~~~~~~ //

struct XorShift32Context {
    uint32 last_number;
};

void w_xor_shift_seed(XorShift32Context* context, uint32 seed) {
    if (seed == 0) {
        seed = 2463534242;
    }

    context->last_number = seed;
}

uint32 w_xor_shift_next_u32(XorShift32Context* context) {
    uint32 x = context->last_number;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    context->last_number = x;
    return x;
}

float w_xor_shift_next_f32(XorShift32Context* context) {
    return (float)(w_xor_shift_next_u32(context)) / (float)UINT32_MAX;
}

// ~~~~~~~~~~~~~~~~~~~~ Perlin Noise ~~~~~~~~~~~~~~~~~~~~~~ //
// DANGER: This implementation is broken. I'm using stb_perlin.h for now and maybe forever

// NOTE: the classic perlin noise algorithm assumes that (0, 0) is in the bottom left

#define PERLIN_NOISE_PERIOD 256
#define PERLIN_NOISE_HASH_SIZE (PERLIN_NOISE_PERIOD * 2)

struct PerlinContext {
    uint32 hash[PERLIN_NOISE_HASH_SIZE];
    uint32 seed;
};

uint32 perlin_hash[PERLIN_NOISE_HASH_SIZE];

void w_perlin_seed(PerlinContext* context, uint32 seed) {

    XorShift32Context rng;
    w_xor_shift_seed(&rng, seed);

    for (int i = 0; i < PERLIN_NOISE_PERIOD; i++) {
        context->hash[i] = i;
    }

    for (int i = PERLIN_NOISE_PERIOD - 1; i > 0; i--) {
        uint32 j = w_xor_shift_next_u32(&rng) % (i + 1);
        uint32 tmp = context->hash[i];
        context->hash[i] = context->hash[j];
        context->hash[j] = tmp;
    }

    for (int i = 0; i < PERLIN_NOISE_PERIOD; i++) {
        context->hash[i + PERLIN_NOISE_PERIOD] = context->hash[i];
    }
    context->seed = seed;
}

float perlin_fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// This function determines the dot product by using the hash to define the
// gradient vector and x and y to describe the location vector
float grad(uint32 hash, float x, float y) {
    float result;
    switch (hash & 0x7) {
    case 0x0:
        result = (x + y) * 0.70710678f;
        break;
    case 0x1:
        result = (-x + y) * 0.70710678f;
        break;
    case 0x2:
        result = (x - y) * 0.70710678f;
        break;
    case 0x3:
        result = (-x - y) * 0.70710678f;
        break;
    case 0x4:
        result = x;
        break;
    case 0x5:
        result = -x;
        break;
    case 0x6:
        result = y;
        break;
    case 0x7:
        result = -y;
        break;
    default:
        result = 0;
        break;
    }

    return result;
}

float w_perlin(float x, float y, PerlinContext* context) {
    int xi = (int)floorf(x) & (PERLIN_NOISE_PERIOD - 1);
    int yi = (int)floorf(y) & (PERLIN_NOISE_PERIOD - 1);
    float xf = x - (int)floorf(x);
    float yf = y - (int)floorf(y);

    float u = perlin_fade(xf);
    float v = perlin_fade(yf);

    uint32* perlin_hash = context->hash;

    uint32 aa = perlin_hash[perlin_hash[xi] + yi];
    uint32 ab = perlin_hash[perlin_hash[xi] + yi + 1];
    uint32 ba = perlin_hash[perlin_hash[xi + 1] + yi];
    uint32 bb = perlin_hash[perlin_hash[xi + 1] + yi + 1];

    float w1 = w_lerp(grad(aa, xf, yf), grad(ab, xf, yf - 1), u);
    float w2 = w_lerp(grad(ba, xf - 1, yf), grad(bb, xf - 1, yf - 1), u);

    float result = w_lerp(w1, w2, v);
    return result;
    // return (result + 1.0f) * 0.5f;
}

// ~~~~~~~~~~~~~~ Fractal Brownian Motion ~~~~~~~~~~~~~~~~~ //
// Ref: https://thebookofshaders.com/13/

struct FBMContext {
    uint32 octaves;
    float lacunarity; // Kind of affects how noisey or gappy the noise is
    float gain;
    float amp;
    float freq;
    PerlinContext perlin_context;
};

float w_fbm(float x, float y, FBMContext* params) {
    uint32 octaves = params->octaves;
    float amp = params->amp;
    float freq = params->freq;
    float lacunarity = params->lacunarity;
    float gain = params->gain;
    float max_amp = 0.0f;

    float result = 0;
    for (int i = 0; i < octaves; i++) {
        result += amp * w_perlin(x * freq, y * freq, &params->perlin_context);
        max_amp += amp;

        freq *= lacunarity;
        amp *= gain;
    }

    return result / max_amp;
}
#endif
