#include "platform.h"
#define WAFFLE_LIB_IMPLEMENTATION
#include "waffle_lib.h"
#undef WAFFLE_LIB_IMPLEMENTATION

#ifdef _WIN32
#define GAME_CODE_LIB "game_main.dll"
#define TEMP_GAME_CODE_LIB "temp_game_main.dll"
#else
#define GAME_CODE_LIB "game_main.dylib"
#define TEMP_GAME_CODE_LIB "temp_game_main.dylib"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#define stat _stat
#endif

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <time.h>
#include "math.h"
#include "audio.h"

char base_path[W_PATH_MAX];

#include "sdl_audio.cpp"

#define MAX_FRAME_RATE 120.0f
#define MIN_FRAME_TIME_S 1 / MAX_FRAME_RATE
#define MAX_FRAME_TIME_S 1 / 30.f

static SDL_Window* window;

#include "opengl_renderer.cpp"

struct GameCode {
    SDL_SharedObject* game_code_lib;
    GameUpdateAndRender* game_update_and_render;

    time_t last_write_time;
};

time_t get_last_write_time(const char* file_path) {
    struct stat file_stat;

    if (stat(file_path, &file_stat) == 0) {
        return file_stat.st_mtime;
    }
    else {
        return -1;
    }
}

GameCode load_game_code(char* source_dylib, char* temp_dylib) {
    GameCode game;
    if (!SDL_CopyFile(source_dylib, temp_dylib)) {
        printf("Failed to copy game code library: %s\n", SDL_GetError());
    }
    game.game_code_lib = SDL_LoadObject(temp_dylib);

    if (!game.game_code_lib) {
        fprintf(stderr, "Failed to load game_main shared lib: %s\n", SDL_GetError());
    }
    else {
        game.game_update_and_render = (GameUpdateAndRender*)SDL_LoadFunction(game.game_code_lib, "game_update_and_render");

        if (!game.game_update_and_render) {
            fprintf(stderr, "Failed to function pointer to game_update_and_render: %s\n", SDL_GetError());
        }
    }

    game.last_write_time = get_last_write_time(source_dylib);

    return game;
}

void unload_game_code(GameCode* game) {
    if (game->game_code_lib) {
        SDL_UnloadObject(game->game_code_lib);
    }
    game->game_code_lib = NULL;
    game->game_update_and_render = NULL;
    game->last_write_time = -1;
}

GET_PERFORMANCE_COUNTER(get_performance_counter) {
    return SDL_GetPerformanceCounter();
}

START_TEXT_INPUT(start_text_input) {
    return SDL_StartTextInput(window);
}

STOP_TEXT_INPUT(stop_text_input) {
    return SDL_StopTextInput(window);
}

void read_inputs(GameInput* game_input, bool* running) {
    KeyInputState* key_input_states = game_input->key_input_states;
    bool* text_input_states = game_input->text_input_states;
    MouseState* mouse_state = &game_input->mouse_state;

    SDL_GetMouseState(&mouse_state->position.x, &mouse_state->position.y);

    for (int i = 0; i < KEY_INPUT_COUNT; i++) {
        key_input_states[i].is_pressed = false;
    }

    for (int i = 0; i < TEXT_INPUT_COUNT; i++) {
        text_input_states[i] = false;
    }

    for (int i = 0; i < MOUSE_INPUT_COUNT; i++) {
        mouse_state->input_states[i].is_pressed = false;
    }

    game_input->text_buffer[0] = '\0';

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            *running = false;
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            bool is_down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            switch (event.button.button) {
            case SDL_BUTTON_LEFT:
                mouse_state->input_states[MOUSE_LEFT_BUTTON].is_held = is_down;
                mouse_state->input_states[MOUSE_LEFT_BUTTON].is_pressed = is_down;
                break;
            case SDL_BUTTON_RIGHT:
                mouse_state->input_states[MOUSE_RIGHT_BUTTON].is_held = is_down;
                mouse_state->input_states[MOUSE_RIGHT_BUTTON].is_pressed = is_down;
                break;
            case SDL_BUTTON_MIDDLE:
                mouse_state->input_states[MOUSE_MIDDLE_BUTTON].is_held = is_down;
                mouse_state->input_states[MOUSE_MIDDLE_BUTTON].is_held = is_down;
                break;
            }
        }
        else if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && !event.key.repeat) {
            bool is_down = event.type == SDL_EVENT_KEY_DOWN;
            switch (event.key.key) {
            case SDLK_A:
                key_input_states[KEY_A].is_held = is_down;
                key_input_states[KEY_A].is_pressed = is_down;
                break;
            case SDLK_B:
                key_input_states[KEY_B].is_held = is_down;
                key_input_states[KEY_B].is_pressed = is_down;
                break;
            case SDLK_C:
                key_input_states[KEY_C].is_held = is_down;
                key_input_states[KEY_C].is_pressed = is_down;
                break;
            case SDLK_D:
                key_input_states[KEY_D].is_held = is_down;
                key_input_states[KEY_D].is_pressed = is_down;
                break;
            case SDLK_E:
                key_input_states[KEY_E].is_held = is_down;
                key_input_states[KEY_E].is_pressed = is_down;
                break;
            case SDLK_F:
                key_input_states[KEY_F].is_held = is_down;
                key_input_states[KEY_F].is_pressed = is_down;
                break;
            case SDLK_G:
                key_input_states[KEY_G].is_held = is_down;
                key_input_states[KEY_G].is_pressed = is_down;
                break;
            case SDLK_H:
                key_input_states[KEY_H].is_held = is_down;
                key_input_states[KEY_H].is_pressed = is_down;
                break;
            case SDLK_I:
                key_input_states[KEY_I].is_held = is_down;
                key_input_states[KEY_I].is_pressed = is_down;
                break;
            case SDLK_J:
                key_input_states[KEY_J].is_held = is_down;
                key_input_states[KEY_J].is_pressed = is_down;
                break;
            case SDLK_K:
                key_input_states[KEY_K].is_held = is_down;
                key_input_states[KEY_K].is_pressed = is_down;
                break;
            case SDLK_L:
                key_input_states[KEY_L].is_held = is_down;
                key_input_states[KEY_L].is_pressed = is_down;
                break;
            case SDLK_M:
                key_input_states[KEY_M].is_held = is_down;
                key_input_states[KEY_M].is_pressed = is_down;
                break;
            case SDLK_N:
                key_input_states[KEY_N].is_held = is_down;
                key_input_states[KEY_N].is_pressed = is_down;
                break;
            case SDLK_O:
                key_input_states[KEY_O].is_held = is_down;
                key_input_states[KEY_O].is_pressed = is_down;
                break;
            case SDLK_P:
                key_input_states[KEY_P].is_held = is_down;
                key_input_states[KEY_P].is_pressed = is_down;
                break;
            case SDLK_Q:
                key_input_states[KEY_Q].is_held = is_down;
                key_input_states[KEY_Q].is_pressed = is_down;
                break;
            case SDLK_R:
                key_input_states[KEY_R].is_held = is_down;
                key_input_states[KEY_R].is_pressed = is_down;
                break;
            case SDLK_S:
                key_input_states[KEY_S].is_held = is_down;
                key_input_states[KEY_S].is_pressed = is_down;
                break;
            case SDLK_T:
                key_input_states[KEY_T].is_held = is_down;
                key_input_states[KEY_T].is_pressed = is_down;
                break;
            case SDLK_U:
                key_input_states[KEY_U].is_held = is_down;
                key_input_states[KEY_U].is_pressed = is_down;
                break;
            case SDLK_V:
                key_input_states[KEY_V].is_held = is_down;
                key_input_states[KEY_V].is_pressed = is_down;
                break;
            case SDLK_W:
                key_input_states[KEY_W].is_held = is_down;
                key_input_states[KEY_W].is_pressed = is_down;
                break;
            case SDLK_X:
                key_input_states[KEY_X].is_held = is_down;
                key_input_states[KEY_X].is_pressed = is_down;
                break;
            case SDLK_Y:
                key_input_states[KEY_Y].is_held = is_down;
                key_input_states[KEY_Y].is_pressed = is_down;
                break;
            case SDLK_Z:
                key_input_states[KEY_Z].is_held = is_down;
                key_input_states[KEY_Z].is_pressed = is_down;
                break;
            case SDLK_0:
                key_input_states[KEY_0].is_held = is_down;
                key_input_states[KEY_0].is_pressed = is_down;
                break;
            case SDLK_1:
                key_input_states[KEY_1].is_held = is_down;
                key_input_states[KEY_1].is_pressed = is_down;
                break;
            case SDLK_2:
                key_input_states[KEY_2].is_held = is_down;
                key_input_states[KEY_2].is_pressed = is_down;
                break;
            case SDLK_3:
                key_input_states[KEY_3].is_held = is_down;
                key_input_states[KEY_3].is_pressed = is_down;
                break;
            case SDLK_4:
                key_input_states[KEY_4].is_held = is_down;
                key_input_states[KEY_4].is_pressed = is_down;
                break;
            case SDLK_5:
                key_input_states[KEY_5].is_held = is_down;
                key_input_states[KEY_5].is_pressed = is_down;
                break;
            case SDLK_6:
                key_input_states[KEY_6].is_held = is_down;
                key_input_states[KEY_6].is_pressed = is_down;
                break;
            case SDLK_7:
                key_input_states[KEY_7].is_held = is_down;
                key_input_states[KEY_7].is_pressed = is_down;
                break;
            case SDLK_8:
                key_input_states[KEY_8].is_held = is_down;
                key_input_states[KEY_8].is_pressed = is_down;
                break;
            case SDLK_9:
                key_input_states[KEY_9].is_held = is_down;
                key_input_states[KEY_9].is_pressed = is_down;
                break;
            case SDLK_RETURN:
                key_input_states[KEY_RETURN].is_held = is_down;
                key_input_states[KEY_RETURN].is_pressed = is_down;
                break;
            case SDLK_ESCAPE:
                key_input_states[KEY_ESCAPE].is_held = is_down;
                key_input_states[KEY_ESCAPE].is_pressed = is_down;
                break;
            case SDLK_BACKSPACE:
                key_input_states[KEY_BACKSPACE].is_held = is_down;
                key_input_states[KEY_BACKSPACE].is_pressed = is_down;
                break;
            case SDLK_TAB:
                key_input_states[KEY_TAB].is_held = is_down;
                key_input_states[KEY_TAB].is_pressed = is_down;
                break;
            case SDLK_SPACE:
                key_input_states[KEY_SPACE].is_held = is_down;
                key_input_states[KEY_SPACE].is_pressed = is_down;
                break;
            case SDLK_APOSTROPHE:
                key_input_states[KEY_APOSTROPHE].is_held = is_down;
                key_input_states[KEY_APOSTROPHE].is_pressed = is_down;
                break;
            case SDLK_COMMA:
                key_input_states[KEY_COMMA].is_held = is_down;
                key_input_states[KEY_COMMA].is_pressed = is_down;
                break;
            case SDLK_MINUS:
                key_input_states[KEY_MINUS].is_held = is_down;
                key_input_states[KEY_MINUS].is_pressed = is_down;
                break;
            case SDLK_PERIOD:
                key_input_states[KEY_PERIOD].is_held = is_down;
                key_input_states[KEY_PERIOD].is_pressed = is_down;
                break;
            case SDLK_BACKSLASH:
                key_input_states[KEY_BACKSLASH].is_held = is_down;
                key_input_states[KEY_BACKSLASH].is_pressed = is_down;
                break;
            case SDLK_SLASH:
                key_input_states[KEY_SLASH].is_held = is_down;
                key_input_states[KEY_SLASH].is_pressed = is_down;
                break;
            case SDLK_SEMICOLON:
                key_input_states[KEY_SEMICOLON].is_held = is_down;
                key_input_states[KEY_SEMICOLON].is_pressed = is_down;
                break;
            case SDLK_LSHIFT:
                key_input_states[KEY_L_SHIFT].is_held = is_down;
                key_input_states[KEY_L_SHIFT].is_pressed = is_down;
                break;
            case SDLK_RSHIFT:
                key_input_states[KEY_R_SHIFT].is_held = is_down;
                key_input_states[KEY_R_SHIFT].is_pressed = is_down;
                break;
            case SDLK_LCTRL:
                key_input_states[KEY_L_CTRL].is_held = is_down;
                key_input_states[KEY_L_CTRL].is_pressed = is_down;
                break;
            case SDLK_RCTRL:
                key_input_states[KEY_R_CTRL].is_held = is_down;
                key_input_states[KEY_R_CTRL].is_pressed = is_down;
                break;
            case SDLK_LALT:
                key_input_states[KEY_L_ALT].is_held = is_down;
                key_input_states[KEY_L_ALT].is_pressed = is_down;
                break;
            case SDLK_RALT:
                key_input_states[KEY_R_ALT].is_held = is_down;
                key_input_states[KEY_R_ALT].is_pressed = is_down;
                break;
            case SDLK_LEFTBRACKET:
                key_input_states[KEY_LEFT_BRACKET].is_held = is_down;
                key_input_states[KEY_LEFT_BRACKET].is_pressed = is_down;
                break;
            case SDLK_RIGHTBRACKET:
                key_input_states[KEY_RIGHT_BRACKET].is_held = is_down;
                key_input_states[KEY_RIGHT_BRACKET].is_pressed = is_down;
                break;
            case SDLK_LGUI:
                key_input_states[KEY_L_GUI].is_held = is_down;
                key_input_states[KEY_L_GUI].is_pressed = is_down;
                break;
            case SDLK_RGUI:
                key_input_states[KEY_R_GUI].is_held = is_down;
                key_input_states[KEY_R_GUI].is_pressed = is_down;
                break;
            case SDLK_GRAVE:
                key_input_states[KEY_GRAVE].is_held = is_down;
                key_input_states[KEY_GRAVE].is_pressed = is_down;
                break;
            }
        }
        else if (event.type == SDL_EVENT_TEXT_INPUT) {
            const char* input = event.text.text;
            // the first condition here is just to protect against weirdness
            // the second condition ensures we only process single byte chars
            // This might make for a shitty typing experience in which case we will have to handle multi-byte input which could be multi-byte chars that we'd have to filter out.
            if (input[0] != '\0' && input[1] == '\0') {
                if (input[0] >= 32 && input[0] < TEXT_INPUT_COUNT) {
                    text_input_states[(int)input[0]] = true;
                    w_str_concat(game_input->text_buffer, (char*)input);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    w_str_copy(base_path, (char*)SDL_GetBasePath());

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL_Init Video error: %s\n", SDL_GetError());
        return 1;
    }

    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        fprintf(stderr, "SDL_Init Audio error: %s\n", SDL_GetError());
        return 1;
    }

    // assumes game code is using rand()
    srand(time(NULL));

    GameMemory game_memory = {};
    game_memory.size = Megabytes(100);
    game_memory.memory = malloc(game_memory.size);
    memset(game_memory.memory, 0, game_memory.size);
    game_memory.initialize_renderer = initialize_renderer;
	game_memory.load_texture = load_texture;
    game_memory.push_render_group = push_render_group;
    game_memory.get_performance_counter = get_performance_counter;
    game_memory.start_text_input = start_text_input;
    game_memory.stop_text_input = stop_text_input;
    game_memory.init_audio = init_audio;
    game_memory.push_audio_samples = push_audio_samples;
    game_memory.performance_frequency = SDL_GetPerformanceFrequency();
    game_memory.screen_size = { 960, 540 };
    w_str_copy(game_memory.base_path, (char*)SDL_GetBasePath());

    window = SDL_CreateWindow("Obsession", game_memory.screen_size.x, game_memory.screen_size.y, SDL_WINDOW_OPENGL);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
    {
        fprintf(stderr, "Failed to load OpenGL context: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Vsync
    if (!SDL_GL_SetSwapInterval(1))
    {
        fprintf(stderr, "Failed to turn on vsync!");
    }

    GameCode game;
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return 1;
    }

    char source_game_dylib[W_PATH_MAX];
    w_get_absolute_path(source_game_dylib, base_path, GAME_CODE_LIB);
    char temp_game_dylib[W_PATH_MAX];
    w_get_absolute_path(temp_game_dylib, base_path, TEMP_GAME_CODE_LIB);
    game = load_game_code(source_game_dylib, temp_game_dylib);

    GameInput game_input;
    memset(&game_input, 0, sizeof(GameInput));

    uint64 frame_start_count = SDL_GetPerformanceCounter();
    uint64 frame_end_count = frame_start_count;
    double frame_dt_s = 0;
    bool running = true;

    while (running) {
        time_t last_game_write = get_last_write_time(source_game_dylib);
        if (last_game_write != game.last_write_time) {
            printf("reloading game code...\n");
            unload_game_code(&game);
            game = load_game_code(source_game_dylib, temp_game_dylib);
        }
#ifdef DEBUG
		// memset(game_memory.debug_info.profile_timers, 0, sizeof(game_memory.debug_info.profile_timers));
#endif

        read_inputs(&game_input, &running);

        render_begin_frame();

        game.game_update_and_render(&game_memory, &game_input, frame_dt_s);

        DebugInfo* debug_info = &game_memory.debug_info;
		double pre_render_frame_end_count = SDL_GetPerformanceCounter();
		double preswap_dt_s = ((pre_render_frame_end_count - frame_start_count) / (double)game_memory.performance_frequency);
		debug_info->prebuffer_swap_dt_history[debug_info->prebuffer_swap_dt_history_count++ % FRAME_TIME_HISTORY_MAX_COUNT] = preswap_dt_s;

        SDL_GL_SwapWindow(window);

        frame_end_count = SDL_GetPerformanceCounter();
        double frame_dt_s = (frame_end_count - frame_start_count) / (double)game_memory.performance_frequency;
        while (frame_dt_s < MIN_FRAME_TIME_S) {
            frame_end_count = SDL_GetPerformanceCounter();
            frame_dt_s = (frame_end_count - frame_start_count) / (double)game_memory.performance_frequency;
        }

        debug_info->rendered_dt_history[debug_info->rendered_dt_history_count++ % FRAME_TIME_HISTORY_MAX_COUNT] = frame_dt_s;

        frame_dt_s = w_min(frame_dt_s, MAX_FRAME_TIME_S);
        frame_start_count = frame_end_count;
    }

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
