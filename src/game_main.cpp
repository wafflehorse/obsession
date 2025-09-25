#include <stdio.h>
#include "platform.h"
#define WAFFLE_LIB_IMPLEMENTATION
#include "waffle_lib.h"
#undef WAFFLE_LIB_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "game.h"
#include "audio.h"
#include "renderer.cpp"
#include "ui.cpp"
#include "sound.cpp"

char* g_base_path;
uint32 g_pixels_per_unit;

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
	GameState* game_state = (GameState*)game_memory->memory;
	g_base_path = game_memory->base_path;
	g_pixels_per_unit = 32;
	PushRenderGroup* push_render_group = game_memory->push_render_group;

	w_init_waffle_lib(g_base_path);

#ifdef DEBUG
	RenderGroup debug_render_group = {};
	debug_render_group.id = RENDER_GROUP_ID_DEBUG;
#endif

	if(!is_set(game_state->flags, GAME_STATE_F_INITIALIZED)) {
		set(game_state->flags, GAME_STATE_F_INITIALIZED);

		game_state->main_arena.size = game_memory->size - sizeof(GameState);
		game_state->main_arena.data = (char*)game_memory->memory + sizeof(GameState);
		game_state->main_arena.next = game_state->main_arena.data;

		game_state->frame_arena.size = MAX_ENTITIES * 100;
		game_state->frame_arena.data = w_arena_alloc(&game_state->main_arena, game_state->frame_arena.size);
		game_state->frame_arena.next = game_state->frame_arena.data;

		game_state->camera.position = { 0, 0 };
		game_state->camera.size = {
			game_memory->screen_size.x / g_pixels_per_unit,
			game_memory->screen_size.y / g_pixels_per_unit
		};

		game_memory->initialize_renderer(game_memory->screen_size, game_state->camera.size, &game_state->main_arena);
		game_memory->load_texture(TEXTURE_ID_FONT, "resources/assets/font_texture.png");

		game_memory->init_audio(&game_state->audio_player);
		setup_sound_from_wav(SOUND_BACKGROUND_MUSIC, "resources/assets/background_music_1.wav", 0.60, game_state->sounds, &game_state->main_arena);

		// TODO: The arena marker / restore interface can be improved. Some way to defer?
		char* arena_marker = w_get_arena_marker(&game_state->main_arena);
		FileContents font_data_file_contents;
		w_read_file("resources/assets/font_data", &font_data_file_contents, &game_state->main_arena);
		memcpy(&game_state->font_data, font_data_file_contents.data, sizeof(FontData));
		w_arena_restore(&game_state->main_arena, arena_marker);

		play_sound(&game_state->sounds[SOUND_BACKGROUND_MUSIC], &game_state->audio_player);
	}

	init_ui(&game_state->font_data, g_pixels_per_unit);

	game_state->frame_arena.next = game_state->frame_arena.data;

	game_memory->push_audio_samples(&game_state->audio_player);

#ifdef DEBUG
	debug_render_group.size = 500;
	debug_render_group.quads = (RenderQuad*)w_arena_alloc(&game_state->frame_arena, debug_render_group.size * sizeof(RenderQuad));
	DebugInfo* debug_info = &game_memory->debug_info;
	double avg_rendered_frame_time_s = w_avg(debug_info->rendered_dt_history, w_min(debug_info->rendered_dt_history_count, FRAME_TIME_HISTORY_MAX_COUNT));
	double fps = 0; 
	if(avg_rendered_frame_time_s != 0) {
		fps = 1 / avg_rendered_frame_time_s;
	}

	double avg_preswap_dt_ms = w_avg(debug_info->prebuffer_swap_dt_history, w_min(debug_info->prebuffer_swap_dt_history_count, FRAME_TIME_HISTORY_MAX_COUNT)) * 1000;

	char fps_str[32] = {};
	char avg_preswap_dt_str[32] = {};
	snprintf(fps_str, 32, "FPS: %.0f", w_round((float)fps));
	snprintf(avg_preswap_dt_str, 32, "Frame ms: %.3f", avg_preswap_dt_ms); 

	UIElement* debug_text_container = ui_create_container(0.5, 0.1, UI_CONTAINER_DIRECTION_COL, 2, &game_state->frame_arena);
	ui_add_text_element(debug_text_container, fps_str, 1, COLOR_WHITE);
	ui_add_text_element(debug_text_container, avg_preswap_dt_str, 1, COLOR_WHITE);
	Vec2 debug_text_container_size = ui_get_container_size(debug_text_container);
	Vec2 camera_top_left = {
		game_state->camera.position.x - (game_state->camera.size.x / 2),
		game_state->camera.position.y + (game_state->camera.size.y / 2)
	};
	debug_text_container->container_data.position = {
		camera_top_left.x + (debug_text_container_size.x / 2),
		camera_top_left.y - (debug_text_container_size.y / 2)
	};
	ui_draw_container(debug_text_container, &debug_render_group, UI_CONTAINER_DRAW_F_LEFT_ALIGN);

	push_render_group(debug_render_group.quads, debug_render_group.count, game_state->camera.position, debug_render_group.opts);
#endif
}
