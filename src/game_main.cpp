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
#include "sprite_assets.h"

char* g_base_path;
uint32 g_pixels_per_unit;
double g_sim_dt_s;

#define WORLD_TILE_WIDTH 8
#define WORLD_TILE_HEIGHT 8 

uint32 get_viewport_scale_factor(Vec2 screen_size) {
	uint32 width_scale = screen_size.x / BASE_RESOLUTION_WIDTH;
	uint32 height_scale = screen_size.y / BASE_RESOLUTION_HEIGHT;

	return w_min(width_scale, height_scale);
}

Vec2 get_viewport(uint32 scale) {
	return {
		(float)scale * BASE_RESOLUTION_WIDTH,
		(float)scale * BASE_RESOLUTION_HEIGHT
	};
}

Entity* get_new_entity(EntityArray* entity_array) {
	Entity* entity = &entity_array->entities[entity_array->count++];
	memset(entity, 0, sizeof(Entity));
	set(entity->flags, ENTITY_F_ACTIVE);

	ASSERT(entity_array->count < entity_array->cap, "MAX_ENTITIES has been reached!");

	return entity;
}

uint32 create_player_entity(EntityArray* entity_array, Vec2 position) {
	Entity* entity = get_new_entity(entity_array);
	
	entity->position = position;
	entity->anim_state = {
		ANIM_HERO_IDLE,
		0
	};

	return entity_array->count - 1;
}

RenderQuad* render_sprite(Vec2 world_position, uint32 sprite_id, RenderGroup* render_group) {
	RenderQuad* quad = get_next_quad(render_group);
	quad->world_position = world_position;
	Sprite sprite = sprite_table[sprite_id];

	quad->world_size = w_vec_mult((Vec2){ sprite.w, sprite.h }, 1.0 / BASE_PIXELS_PER_UNIT);
	quad->sprite_position = { sprite.x, sprite.y };
	quad->sprite_size = { sprite.w, sprite.h };

	return quad;
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
	GameState* game_state = (GameState*)game_memory->memory;
	g_base_path = game_memory->base_path;
	g_sim_dt_s = frame_dt_s;

	game_state->viewport_scale_factor = get_viewport_scale_factor(game_memory->screen_size);
	g_pixels_per_unit = BASE_PIXELS_PER_UNIT * game_state->viewport_scale_factor;

	Vec2 viewport = get_viewport(game_state->viewport_scale_factor);

	game_memory->set_viewport(viewport, game_memory->screen_size);

	w_init_waffle_lib(g_base_path);
	w_init_animation(animation_table);

	RenderGroup background_render_group = {};
	background_render_group.id = RENDER_GROUP_ID_BACKGROUND;
	RenderGroup main_render_group = {};
	main_render_group.id = RENDER_GROUP_ID_MAIN;

#ifdef DEBUG
	RenderGroup debug_render_group = {};
	debug_render_group.id = RENDER_GROUP_ID_DEBUG;
#endif

	if(!is_set(game_state->flags, GAME_STATE_F_INITIALIZED)) {
		set(game_state->flags, GAME_STATE_F_INITIALIZED);

		game_state->main_arena.size = game_memory->size - sizeof(GameState);
		game_state->main_arena.data = (char*)game_memory->memory + sizeof(GameState);
		game_state->main_arena.next = game_state->main_arena.data;

		game_state->frame_arena.size = Megabytes(10);
		game_state->frame_arena.data = w_arena_alloc(&game_state->main_arena, game_state->frame_arena.size);
		game_state->frame_arena.next = game_state->frame_arena.data;

		game_state->camera.position = { 0, 0 };
		game_state->camera.size = {
			BASE_RESOLUTION_WIDTH / BASE_PIXELS_PER_UNIT,
			BASE_RESOLUTION_HEIGHT / BASE_PIXELS_PER_UNIT
		};

		game_state->entity_array.cap = MAX_ENTITIES;

		game_memory->initialize_renderer(game_memory->screen_size, viewport, game_state->camera.size, &game_state->main_arena);
		game_memory->load_texture(TEXTURE_ID_FONT, "resources/assets/font_texture.png");
		game_memory->load_texture(TEXTURE_ID_SPRITE, "resources/assets/sprite_atlas.png");

		game_memory->init_audio(&game_state->audio_player);
		setup_sound_from_wav(SOUND_BACKGROUND_MUSIC, "resources/assets/background_music_1.wav", 0.60, game_state->sounds, &game_state->main_arena);

		// TODO: The arena marker / restore interface can be improved. Some way to defer?
		char* arena_marker = w_arena_marker(&game_state->main_arena);
		FileContents font_data_file_contents;
		w_read_file("resources/assets/font_data", &font_data_file_contents, &game_state->main_arena);
		memcpy(&game_state->font_data, font_data_file_contents.data, sizeof(FontData));
		w_arena_restore(&game_state->main_arena, arena_marker);

		create_player_entity(&game_state->entity_array, { 0, 0 });

		play_sound(&game_state->sounds[SOUND_BACKGROUND_MUSIC], &game_state->audio_player);
	}

	init_ui(&game_state->font_data, BASE_PIXELS_PER_UNIT);
	game_state->frame_arena.next = game_state->frame_arena.data;

	background_render_group.size = 5000;
	background_render_group.quads = (RenderQuad*)w_arena_alloc(&game_state->frame_arena, background_render_group.size * sizeof(RenderQuad));
	main_render_group.size = 5000;
	main_render_group.quads = (RenderQuad*)w_arena_alloc(&game_state->frame_arena, main_render_group.size * sizeof(RenderQuad));

	uint32 tilemap[WORLD_TILE_WIDTH * WORLD_TILE_HEIGHT] = {
		0, 0, 0, 0, 0, 0, 2, 0,
		0, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 2, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 2, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
	};

	Vec2 world_top_left_tile_position = {
		-WORLD_TILE_WIDTH / 2 + 0.5,
		WORLD_TILE_HEIGHT / 2 - 0.5
	};

	{
		RenderQuad* ground = get_next_quad(&background_render_group);
		ground->world_position = { 0, 0 };
		ground->world_size = { 64, 64 };
		Sprite ground_sprite = sprite_table[SPRITE_GROUND_1];
		ground->sprite_position = { ground_sprite.x, ground_sprite.y };
		ground->sprite_size = { ground_sprite.w, ground_sprite.h };
	}

	for(int i = 0; i < WORLD_TILE_WIDTH * WORLD_TILE_HEIGHT; i++) {
		uint32 col = i % WORLD_TILE_WIDTH;
		uint32 row = i / WORLD_TILE_HEIGHT;
		Vec2 position = {
			world_top_left_tile_position.x + col,
			world_top_left_tile_position.y - row
		};
		switch(tilemap[i]) {
			case 1: {
				render_sprite(position, SPRITE_BLOCK_1, &background_render_group);
				break;
			}
			case 2: {
				render_sprite(position, SPRITE_PLANT_1, &main_render_group);
				break;
			}
		}
	}

	for(int i = 0; i < game_state->entity_array.count; i++) {
		Entity* entity = &game_state->entity_array.entities[i];
		Vec2 new_position {};
		new_position = w_vec_add(w_vec_mult(w_vec_mult(entity->acceleration, 0.5), w_square(g_sim_dt_s)), new_position);
		new_position = w_vec_add(w_vec_mult(entity->velocity, g_sim_dt_s), new_position);
		new_position = w_vec_add(new_position, entity->position);
		entity->position = new_position;
		entity->velocity = w_vec_add(w_vec_mult(entity->acceleration, g_sim_dt_s), entity->velocity);

		SpriteID sprite = (SpriteID)w_update_animation(&entity->anim_state, g_sim_dt_s);
		render_sprite(entity->position, (uint32)sprite, &main_render_group);
	}

	game_memory->push_audio_samples(&game_state->audio_player);

	game_memory->push_render_group(background_render_group.quads, background_render_group.count, game_state->camera.position, background_render_group.opts);
	game_memory->push_render_group(main_render_group.quads, main_render_group.count, game_state->camera.position, main_render_group.opts);

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

	game_memory->push_render_group(debug_render_group.quads, debug_render_group.count, game_state->camera.position, debug_render_group.opts);
#endif
}
