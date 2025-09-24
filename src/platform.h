#pragma once

#include <limits.h>

#ifdef _WIN32
#define W_PATH_MAX 500//260 is standard 
#else 
#define W_PATH_MAX PATH_MAX
#endif

#define DEFAULT_ALIGNMENT (uint32)(2 * sizeof(void*))

#include "waffle_lib.h"
#include "renderer.h"
#include "audio.h"

enum ProfileTimerID {
	ProfileTimerID_GameStateInitialization,
	ProfileTimerIDCount
};

struct ProfileTimer {
	uint64 ticks_elapsed;
	double time_elapsed_ms;
	uint32 hit_count;
};

#ifdef DEBUG
	#define StartTimedBlock(ID) uint64 start_perf_counter_##ID = debug_game_memory->get_performance_counter();
	#define EndTimedBlock(ID) uint64 elapsed_perf_counter_##ID = debug_game_memory->get_performance_counter() - start_perf_counter_##ID; \
	debug_game_memory->debug_info.profile_timers[ProfileTimerID_##ID].ticks_elapsed += elapsed_perf_counter_##ID; \
	debug_game_memory->debug_info.profile_timers[ProfileTimerID_##ID].time_elapsed_ms += (elapsed_perf_counter_##ID / (double)debug_game_memory->performance_frequency) * 1000; \
	debug_game_memory->debug_info.profile_timers[ProfileTimerID_##ID].hit_count++;
#else
	#define StartTimedBlock(ID)
	#define EndTimedBlock(ID)
#endif

#define GET_PERFORMANCE_COUNTER(name) uint64 name()
typedef GET_PERFORMANCE_COUNTER(GetPerformanceCounter);

#define START_TEXT_INPUT(name) bool name()
typedef START_TEXT_INPUT(StartTextInput);

#define STOP_TEXT_INPUT(name) bool name()
typedef STOP_TEXT_INPUT(StopTextInput);

#define FRAME_TIME_HISTORY_MAX_COUNT 120

struct DebugInfo {
	double rendered_dt_history[FRAME_TIME_HISTORY_MAX_COUNT];
	uint32 rendered_dt_history_count;
	double prebuffer_swap_dt_history[FRAME_TIME_HISTORY_MAX_COUNT];
	uint32 prebuffer_swap_dt_history_count;
	ProfileTimer profile_timers[ProfileTimerIDCount];
};

struct GameMemory {
    void* memory;
    long long size;
    uint64 performance_frequency;
    Vec2 screen_size;
    char base_path[W_PATH_MAX];

    InitializeRenderer* initialize_renderer;
	LoadTexture* load_texture;
    InitializeRenderGroup* initialize_render_group;
    PushRenderGroup* push_render_group;
    InitAudio* init_audio;
    PushAudioSamples* push_audio_samples;
    GetPerformanceCounter* get_performance_counter;
    StartTextInput* start_text_input;
    StopTextInput* stop_text_input;
	DebugInfo debug_info;
};

struct KeyInputState
{
    bool is_pressed;
    bool is_held;
};

enum KeyInput
{
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_RETURN,
    KEY_ESCAPE,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_SPACE,
    KEY_APOSTROPHE,
    KEY_COMMA,
    KEY_MINUS,
    KEY_PERIOD,
    KEY_BACKSLASH,
    KEY_SLASH,
    KEY_SEMICOLON,
    KEY_L_SHIFT,
    KEY_R_SHIFT,
    KEY_L_CTRL,
    KEY_R_CTRL,
    KEY_L_ALT,
    KEY_R_ALT,
    KEY_LEFT_BRACKET,
    KEY_RIGHT_BRACKET,
    KEY_L_GUI,
    KEY_R_GUI,
    KEY_GRAVE,
    KEY_INPUT_COUNT
};

enum TextInput {
    TEXT_SPACE = 32,  // ' '
    TEXT_EXCLAMATION = 33,  // '!'
    TEXT_QUOTE = 34,  // '"'
    TEXT_HASH = 35,  // '#'
    TEXT_DOLLAR = 36,  // '$'
    TEXT_PERCENT = 37,  // '%'
    TEXT_AMPERSAND = 38,  // '&'
    TEXT_APOSTROPHE = 39,  // '\''
    TEXT_LEFT_PAREN = 40,  // '('
    TEXT_RIGHT_PAREN = 41,  // ')'
    TEXT_ASTERISK = 42,  // '*'
    TEXT_PLUS = 43,  // '+'
    TEXT_COMMA = 44,  // ','
    TEXT_MINUS = 45,  // '-'
    TEXT_PERIOD = 46,  // '.'
    TEXT_SLASH = 47,  // '/'

    TEXT_0 = 48,  // '0'
    TEXT_1 = 49,
    TEXT_2 = 50,
    TEXT_3 = 51,
    TEXT_4 = 52,
    TEXT_5 = 53,
    TEXT_6 = 54,
    TEXT_7 = 55,
    TEXT_8 = 56,
    TEXT_9 = 57,

    TEXT_COLON = 58,  // ':'
    TEXT_SEMICOLON = 59,  // ';'
    TEXT_LESS_THAN = 60,  // '<'
    TEXT_EQUALS = 61,  // '='
    TEXT_GREATER_THAN = 62,  // '>'
    TEXT_QUESTION = 63, // '?'
    TEXT_AT = 64, // '@'

    TEXT_A = 65, TEXT_B, TEXT_C, TEXT_D, TEXT_E, TEXT_F, TEXT_G,
    TEXT_H, TEXT_I, TEXT_J, TEXT_K, TEXT_L, TEXT_M, TEXT_N,
    TEXT_O, TEXT_P, TEXT_Q, TEXT_R, TEXT_S, TEXT_T, TEXT_U,
    TEXT_V, TEXT_W, TEXT_X, TEXT_Y, TEXT_Z,  // 65–90

    TEXT_LEFT_BRACKET = 91,  // '['
    TEXT_BACKSLASH = 92,  // '\\'
    TEXT_RIGHT_BRACKET = 93,  // ']'
    TEXT_CARET = 94,  // '^'
    TEXT_UNDERSCORE = 95,  // '_'
    TEXT_GRAVE = 96,  // '`'

    TEXT_a = 97, TEXT_b, TEXT_c, TEXT_d, TEXT_e, TEXT_f, TEXT_g,
    TEXT_h, TEXT_i, TEXT_j, TEXT_k, TEXT_l, TEXT_m, TEXT_n,
    TEXT_o, TEXT_p, TEXT_q, TEXT_r, TEXT_s, TEXT_t, TEXT_u,
    TEXT_v, TEXT_w, TEXT_x, TEXT_y, TEXT_z,  // 97–122

    TEXT_LEFT_BRACE = 123, // '{'
    TEXT_PIPE = 124, // '|'
    TEXT_RIGHT_BRACE = 125, // '}'
    TEXT_TILDE = 126,  // '~'
    TEXT_INPUT_COUNT
};

enum MouseInput {
    MOUSE_LEFT_BUTTON,
    MOUSE_RIGHT_BUTTON,
    MOUSE_MIDDLE_BUTTON,
    MOUSE_INPUT_COUNT
};

struct MouseState {
    union {
        Vec2 position;
        struct {
            float x;
            float y;
        };
    };

    KeyInputState input_states[MOUSE_INPUT_COUNT];
};

struct GameInput {
    KeyInputState key_input_states[KEY_INPUT_COUNT];
    bool text_input_states[TEXT_INPUT_COUNT]; // not sure if we still need this
    char text_buffer[100];
    MouseState mouse_state;
};

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory *game_memory, GameInput *game_input, double frame_dt_s)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
