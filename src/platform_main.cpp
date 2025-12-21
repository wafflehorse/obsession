#include "platform.h"
#define WAFFLE_LIB_IMPLEMENTATION
#include "waffle_lib.h"
#undef WAFFLE_LIB_IMPLEMENTATION

#ifdef _WIN32
#define GAME_CODE_LIB "game_main.dll"
#define TEMP_GAME_CODE_LIB "temp_game_main.dll"
#elif __linux__
#define GAME_CODE_LIB "game_main.so"
#define TEMP_GAME_CODE_LIB "temp_game_main.so"
#else
#define GAME_CODE_LIB "game_main.dylib"
#define TEMP_GAME_CODE_LIB "temp_game_main.dylib"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define stat _stat
#endif

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include "math.h"
#include "audio.h"
#include "sdl_audio.cpp"
#include "opengl_renderer.cpp"

#ifdef DEBUG

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#endif

static char base_path[W_PATH_MAX];

#define MAX_FRAME_RATE 120.0f
#define MIN_FRAME_TIME_S 1 / MAX_FRAME_RATE
#define MAX_FRAME_TIME_S 1 / 30.f

#define MIN_SCREEN_WIDTH 640
#define MIN_SCREEN_HEIGHT 360

static SDL_Window* window;

struct GameCode {
    SDL_SharedObject* game_code_lib;
    GameUpdateAndRender* game_update_and_render;

    time_t last_write_time;
};

GameCode load_game_code() {
    char source_dylib[W_PATH_MAX];
    w_get_absolute_path(source_dylib, base_path, GAME_CODE_LIB);
    char temp_dylib[W_PATH_MAX];
    w_get_absolute_path(temp_dylib, base_path, TEMP_GAME_CODE_LIB);

    GameCode game;
    if (!SDL_CopyFile(source_dylib, temp_dylib)) {
        printf("Failed to copy game code library: %s\n", SDL_GetError());
    }
    game.game_code_lib = SDL_LoadObject(temp_dylib);

    if (!game.game_code_lib) {
        fprintf(stderr, "Failed to load game_main shared lib: %s\n", SDL_GetError());
    } else {
        game.game_update_and_render =
            (GameUpdateAndRender*)SDL_LoadFunction(game.game_code_lib, "game_update_and_render");

        if (!game.game_update_and_render) {
            fprintf(stderr, "Failed to function pointer to game_update_and_render: %s\n", SDL_GetError());
        }
    }

    game.last_write_time = get_last_file_write_time(GAME_CODE_LIB);

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

KeyInput sdl_scancode_to_key[SDL_SCANCODE_COUNT] = {
    [SDL_SCANCODE_A] = KEY_A,
    [SDL_SCANCODE_B] = KEY_B,
    [SDL_SCANCODE_C] = KEY_C,
    [SDL_SCANCODE_D] = KEY_D,
    [SDL_SCANCODE_E] = KEY_E,
    [SDL_SCANCODE_F] = KEY_F,
    [SDL_SCANCODE_G] = KEY_G,
    [SDL_SCANCODE_H] = KEY_H,
    [SDL_SCANCODE_I] = KEY_I,
    [SDL_SCANCODE_J] = KEY_J,
    [SDL_SCANCODE_K] = KEY_K,
    [SDL_SCANCODE_L] = KEY_L,
    [SDL_SCANCODE_M] = KEY_M,
    [SDL_SCANCODE_N] = KEY_N,
    [SDL_SCANCODE_O] = KEY_O,
    [SDL_SCANCODE_P] = KEY_P,
    [SDL_SCANCODE_Q] = KEY_Q,
    [SDL_SCANCODE_R] = KEY_R,
    [SDL_SCANCODE_S] = KEY_S,
    [SDL_SCANCODE_T] = KEY_T,
    [SDL_SCANCODE_U] = KEY_U,
    [SDL_SCANCODE_V] = KEY_V,
    [SDL_SCANCODE_W] = KEY_W,
    [SDL_SCANCODE_X] = KEY_X,
    [SDL_SCANCODE_Y] = KEY_Y,
    [SDL_SCANCODE_Z] = KEY_Z,

    [SDL_SCANCODE_0] = KEY_0,
    [SDL_SCANCODE_1] = KEY_1,
    [SDL_SCANCODE_2] = KEY_2,
    [SDL_SCANCODE_3] = KEY_3,
    [SDL_SCANCODE_4] = KEY_4,
    [SDL_SCANCODE_5] = KEY_5,
    [SDL_SCANCODE_6] = KEY_6,
    [SDL_SCANCODE_7] = KEY_7,
    [SDL_SCANCODE_8] = KEY_8,
    [SDL_SCANCODE_9] = KEY_9,

    [SDL_SCANCODE_RETURN] = KEY_RETURN,
    [SDL_SCANCODE_ESCAPE] = KEY_ESCAPE,
    [SDL_SCANCODE_BACKSPACE] = KEY_BACKSPACE,
    [SDL_SCANCODE_CAPSLOCK] = KEY_CAPSLOCK,
    [SDL_SCANCODE_TAB] = KEY_TAB,
    [SDL_SCANCODE_SPACE] = KEY_SPACE,

    [SDL_SCANCODE_APOSTROPHE] = KEY_APOSTROPHE,
    [SDL_SCANCODE_COMMA] = KEY_COMMA,
    [SDL_SCANCODE_MINUS] = KEY_MINUS,
    [SDL_SCANCODE_EQUALS] = KEY_EQUALS,
    [SDL_SCANCODE_PERIOD] = KEY_PERIOD,
    [SDL_SCANCODE_BACKSLASH] = KEY_BACKSLASH,
    [SDL_SCANCODE_SLASH] = KEY_SLASH,
    [SDL_SCANCODE_SEMICOLON] = KEY_SEMICOLON,
    [SDL_SCANCODE_LEFTBRACKET] = KEY_LEFT_BRACKET,
    [SDL_SCANCODE_RIGHTBRACKET] = KEY_RIGHT_BRACKET,
    [SDL_SCANCODE_GRAVE] = KEY_GRAVE,

    [SDL_SCANCODE_LSHIFT] = KEY_L_SHIFT,
    [SDL_SCANCODE_RSHIFT] = KEY_R_SHIFT,
    [SDL_SCANCODE_LCTRL] = KEY_L_CTRL,
    [SDL_SCANCODE_RCTRL] = KEY_R_CTRL,
    [SDL_SCANCODE_LALT] = KEY_L_ALT,
    [SDL_SCANCODE_RALT] = KEY_R_ALT,

    [SDL_SCANCODE_LGUI] = KEY_L_GUI,
    [SDL_SCANCODE_RGUI] = KEY_R_GUI,
};

void clear_inputs(GameInput* game_input) {
    for (int i = 0; i < KEY_INPUT_COUNT; i++) {
        game_input->key_input_states[i].is_pressed = false;
        game_input->key_input_states[i].is_released = false;
    }

    for (int i = 0; i < TEXT_INPUT_COUNT; i++) {
        game_input->text_input_states[i] = false;
    }

    for (int i = 0; i < MOUSE_INPUT_COUNT; i++) {
        game_input->mouse_state.input_states[i].is_pressed = false;
        game_input->mouse_state.input_states[i].is_released = false;
    }

    game_input->text_buffer[0] = '\0';

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        game_input->gamepad_state.buttons[i].is_pressed = false;
        game_input->gamepad_state.buttons[i].is_released = false;
    }
}

void handle_sdl_keyboard_mouse_event(SDL_Event* event, GameInput* game_input) {
    KeyInputState* key_input_states = game_input->key_input_states;
    bool* text_input_states = game_input->text_input_states;
    MouseState* mouse_state = &game_input->mouse_state;
    game_input->active_input_type = INPUT_TYPE_KEYBOARD_MOUSE;

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        bool is_down = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        switch (event->button.button) {
        case SDL_BUTTON_LEFT:
            mouse_state->input_states[MOUSE_LEFT_BUTTON].is_held = is_down;
            mouse_state->input_states[MOUSE_LEFT_BUTTON].is_pressed = is_down;
            mouse_state->input_states[MOUSE_LEFT_BUTTON].is_released = !is_down;
            break;
        case SDL_BUTTON_RIGHT:
            mouse_state->input_states[MOUSE_RIGHT_BUTTON].is_held = is_down;
            mouse_state->input_states[MOUSE_RIGHT_BUTTON].is_pressed = is_down;
            mouse_state->input_states[MOUSE_RIGHT_BUTTON].is_released = !is_down;
            break;
        case SDL_BUTTON_MIDDLE:
            mouse_state->input_states[MOUSE_MIDDLE_BUTTON].is_held = is_down;
            mouse_state->input_states[MOUSE_MIDDLE_BUTTON].is_pressed = is_down;
            mouse_state->input_states[MOUSE_MIDDLE_BUTTON].is_released = !is_down;
            break;
        }
    } else if ((event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) &&
               sdl_scancode_to_key[event->key.scancode] != KEY_UNKNOWN && !event->key.repeat) {
        bool is_down = event->type == SDL_EVENT_KEY_DOWN;
        KeyInput key = sdl_scancode_to_key[event->key.scancode];
        key_input_states[key].is_held = is_down;
        key_input_states[key].is_pressed = is_down;
        key_input_states[key].is_released = !is_down;
    } else if (event->type == SDL_EVENT_TEXT_INPUT) {
        const char* input = event->text.text;
        // the first condition here is just to protect against weirdness
        // the second condition ensures we only process single byte chars
        // This might make for a shitty typing experience in which case we will have to handle multi-byte input which
        // could be multi-byte chars that we'd have to filter out.
        if (input[0] != '\0' && input[1] == '\0') {
            if (input[0] >= 32 && input[0] < TEXT_INPUT_COUNT) {
                text_input_states[(int)input[0]] = true;
                w_str_concat(game_input->text_buffer, (char*)input);
            }
        }
    }
}

// Note: We probably want to switch to handling SDL_GamepadButtonEvent's so that we
// can capture is_released on these gamepad button states
void update_gamepad_state(GameInput* game_input, SDL_Gamepad* gamepad) {
    GamepadState* gamepad_state = &game_input->gamepad_state;

    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_SOUTH].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_SOUTH].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_NORTH].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_NORTH].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_EAST].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_EAST].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_WEST].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_WEST].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_LEFT_STICK].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_LEFT_STICK].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_RIGHT_STICK].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_RIGHT_STICK].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_LEFT_SHOULDER].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_LEFT_SHOULDER].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_RIGHT_SHOULDER].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_RIGHT_SHOULDER].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_UP].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_UP].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_DOWN].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_DOWN].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_LEFT].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_LEFT].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_RIGHT].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_DPAD_RIGHT].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_START].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_START].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_BACK].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_BACK].is_held = true;
    }
    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_GUIDE)) {
        gamepad_state->buttons[GAMEPAD_BUTTON_GUIDE].is_pressed = true;
        gamepad_state->buttons[GAMEPAD_BUTTON_GUIDE].is_held = true;
    }

    float stick_deadzone = 8000;
    float trigger_deadzone = 800;

    float left_stick_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    float left_stick_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    float right_stick_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    float right_stick_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
    float left_trigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    float right_trigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    float left_stick_mag = w_vec_length({left_stick_x, left_stick_y});
    float right_stick_mag = w_vec_length({right_stick_x, right_stick_y});

    if (left_stick_mag < stick_deadzone) {
        left_stick_x = 0;
        left_stick_y = 0;
    }
    if (right_stick_mag < stick_deadzone) {
        right_stick_x = 0;
        right_stick_y = 0;
    }
    if (left_trigger < trigger_deadzone) {
        left_trigger = 0;
    }
    if (right_trigger < trigger_deadzone) {
        right_trigger = 0;
    }

    gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_X] = left_stick_x;
    gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_Y] = left_stick_y;
    gamepad_state->axes[GAMEPAD_AXIS_RIGHT_STICK_X] = right_stick_x;
    gamepad_state->axes[GAMEPAD_AXIS_RIGHT_STICK_Y] = right_stick_y;
    gamepad_state->axes[GAMEPAD_AXIS_LEFT_TRIGGER] = left_trigger;
    gamepad_state->axes[GAMEPAD_AXIS_RIGHT_TRIGGER] = right_trigger;
}

int main(int argc, char* argv[]) {
    w_str_copy(base_path, (char*)SDL_GetBasePath());

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init Video error: %s\n", SDL_GetError());
        return 1;
    }

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init Audio error: %s\n", SDL_GetError());
        return 1;
    }

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init Gamepad error: %s\n", SDL_GetError());
        return 1;
    }

    // assumes game code is using rand()
    srand(time(NULL));

    GameMemory game_memory = {};
    game_memory.size = Megabytes(100);
    game_memory.memory = malloc(game_memory.size);
    memset(game_memory.memory, 0, game_memory.size);
    game_memory.initialize_renderer = initialize_renderer;
    game_memory.set_viewport = set_viewport;
    game_memory.set_projection = set_projection;
    game_memory.load_texture = load_texture;
    game_memory.push_render_group = push_render_group;
    game_memory.get_performance_counter = get_performance_counter;
    game_memory.start_text_input = start_text_input;
    game_memory.stop_text_input = stop_text_input;
    game_memory.init_audio = init_audio;
    game_memory.push_audio_samples = push_audio_samples;
    game_memory.performance_frequency = SDL_GetPerformanceFrequency();
    game_memory.window.size = {MIN_SCREEN_WIDTH * 2.1, MIN_SCREEN_HEIGHT * 2.1};
    w_str_copy(game_memory.base_path, (char*)SDL_GetBasePath());

    w_init_waffle_lib(game_memory.base_path);

    // TODO: will probably want to query screen size and then create window?
    window = SDL_CreateWindow("Obsession", game_memory.window.size.x, game_memory.window.size.y,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_FOCUS |
                                  SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowMinimumSize(window, MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
    {
        int screen_size_x, screen_size_y;
        SDL_GetWindowSizeInPixels(window, &screen_size_x, &screen_size_y);
        game_memory.window.size_px = {(float)screen_size_x, (float)screen_size_y};
    }

    float pt_per_px = game_memory.window.size_px.x / game_memory.window.size.x;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "Failed to load OpenGL context: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Vsync
    if (!SDL_GL_SetSwapInterval(1)) {
        fprintf(stderr, "Failed to turn on vsync!");
    }

    GameCode game;
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return 1;
    }

#ifdef DEBUG
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init();
    game_memory.imgui_context = ImGui::GetCurrentContext();
#endif

    game = load_game_code();

    SDL_Gamepad* gamepad;
    GameInput game_input;
    memset(&game_input, 0, sizeof(GameInput));

    uint64 frame_start_count = SDL_GetPerformanceCounter();
    uint64 frame_end_count = frame_start_count;
    double frame_dt_s = 0;
    bool running = true;

    while (running) {
        time_t last_game_write = get_last_file_write_time(GAME_CODE_LIB);
        if (last_game_write != game.last_write_time) {
            printf("reloading game code...\n");
            unload_game_code(&game);
            game = load_game_code();
        }
#ifdef DEBUG
        // memset(game_memory.debug_info.profile_timers, 0, sizeof(game_memory.debug_info.profile_timers));
#endif

        clear_inputs(&game_input);

        SDL_GetMouseState(&game_input.mouse_state.position.x, &game_input.mouse_state.position.y);
        game_input.mouse_state.position_px.x = game_input.mouse_state.position.x * pt_per_px;
        game_input.mouse_state.position_px.y = game_input.mouse_state.position.y * pt_per_px;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#ifdef DEBUG
            ImGui_ImplSDL3_ProcessEvent(&event);
#endif
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                game_memory.window.size = {(float)event.window.data1, (float)event.window.data2};
                int screen_size_x, screen_size_y;
                SDL_GetWindowSizeInPixels(window, &screen_size_x, &screen_size_y);
                game_memory.window.size_px.x = (float)screen_size_x;
                game_memory.window.size_px.y = (float)screen_size_y;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                handle_sdl_keyboard_mouse_event(&event, &game_input);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                gamepad = SDL_OpenGamepad(event.gdevice.which);
                printf("gamepad added!\n");
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                SDL_CloseGamepad(gamepad);
                memset(&game_input.gamepad_state, 0, sizeof(GamepadState));
                gamepad = NULL;
                printf("gamepad removed!\n");
                break;
            case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
                update_gamepad_state(&game_input, gamepad);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                game_input.active_input_type = INPUT_TYPE_GAMEPAD;
                break;
            }
        }

        render_begin_frame();

#ifdef DEBUG
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        // ImGui::ShowDemoWindow();
#endif

        game.game_update_and_render(&game_memory, &game_input, frame_dt_s);

        DebugInfo* debug_info = &game_memory.debug_info;
        double pre_render_frame_end_count = SDL_GetPerformanceCounter();
        double preswap_dt_s =
            ((pre_render_frame_end_count - frame_start_count) / (double)game_memory.performance_frequency);
        debug_info
            ->prebuffer_swap_dt_history[debug_info->prebuffer_swap_dt_history_count++ % FRAME_TIME_HISTORY_MAX_COUNT] =
            preswap_dt_s;

#ifdef DEBUG
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

        SDL_GL_SwapWindow(window);

        frame_end_count = SDL_GetPerformanceCounter();
        frame_dt_s = (frame_end_count - frame_start_count) / (double)game_memory.performance_frequency;
        while (frame_dt_s < MIN_FRAME_TIME_S) {
            frame_end_count = SDL_GetPerformanceCounter();
            frame_dt_s = (frame_end_count - frame_start_count) / (double)game_memory.performance_frequency;
        }

        debug_info->rendered_dt_history[debug_info->rendered_dt_history_count++ % FRAME_TIME_HISTORY_MAX_COUNT] =
            frame_dt_s;

        frame_dt_s = w_min(frame_dt_s, MAX_FRAME_TIME_S);
        frame_start_count = frame_end_count;
    }

#ifdef DEBUG
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
#endif

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
