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
#include "asset_ids.h"
#include "asset_tables.h"

char* g_base_path;
uint32 g_pixels_per_unit;
double g_sim_dt_s;

#define WORLD_TILE_WIDTH 8
#define WORLD_TILE_HEIGHT 8 

#define DEFAULT_Z_INDEX 1.0f

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

void init_entity_data(EntityData* entity_data) {
	for(int i = 0; i < MAX_ENTITIES; i++) {
		entity_data->entity_ids[i] = i;

		EntityLookup* lookup = &entity_data->entity_lookups[i];
		lookup->generation = 0;
		lookup->idx = i;
	}
}

Entity* get_new_entity(EntityData* entity_data) {
	uint32 idx = entity_data->entity_count;
	Entity* entity = &entity_data->entities[entity_data->entity_count++];
	memset(entity, 0, sizeof(Entity));

	ASSERT(entity_data->entity_count < MAX_ENTITIES, "MAX_ENTITIES has been reached!");

	entity->id = entity_data->entity_ids[idx];
	entity->z_index = 1;

	return entity;
}

void free_entity(uint32 id, EntityData* entity_data) {
	EntityLookup* freed_lookup = &entity_data->entity_lookups[id];

	uint32 last_idx = entity_data->entity_count - 1;
	Entity* last_entity = &entity_data->entities[last_idx];
	if(last_entity->id != id) {
		entity_data->entities[freed_lookup->idx] = *last_entity;

		entity_data->entity_ids[freed_lookup->idx] = last_entity->id;
		entity_data->entity_ids[last_idx] = id;

		EntityLookup* last_lookup = &entity_data->entity_lookups[last_entity->id];
		last_lookup->idx = freed_lookup->idx;

		freed_lookup->idx = last_idx;
	}

	freed_lookup->generation++;

	entity_data->entity_count--;
}

EntityHandle get_entity_handle(Entity* entity, EntityData* entity_data) {
	EntityHandle handle = {
		.id = entity->id,
		.generation = entity_data->entity_lookups[entity->id].generation,
	};

	return handle;
}

EntityHandle create_player_entity(EntityData* entity_data, Vec2 position) {
	Entity* entity = get_new_entity(entity_data);
	
	entity->type = ENTITY_TYPE_PLAYER;
	entity->position = position;
	entity->facing_direction.x = 1;
	entity->facing_direction.y = 0;

	return get_entity_handle(entity, entity_data);
}

EntityHandle create_gun_entity(EntityData* entity_data, EntityHandle owner) {
	Entity* entity = get_new_entity(entity_data);

	entity->type = ENTITY_TYPE_GUN;
	set(entity->flags, ENTITY_FLAG_OWNED);
	set(entity->flags, ENTITY_FLAG_EQUIPPED);
	entity->owner_handle = owner;
	entity->sprite_id = SPRITE_GUN_GREEN;

	return get_entity_handle(entity, entity_data);
}

EntityHandle create_projectile_entity(EntityData* entity_data, Vec2 position, float rotation_rads, Vec2 velocity) {
	Entity* entity = get_new_entity(entity_data);

	entity->type = ENTITY_TYPE_PROJECTILE;
	entity->sprite_id = SPRITE_GREEN_BULLET_STRETCHED_1;
	entity->position = position;
	entity->rotation_rads = rotation_rads;
	entity->velocity = velocity;

	return get_entity_handle(entity, entity_data);
}

Entity* get_entity(EntityHandle handle, EntityData* entity_data) {
	ASSERT(handle.id < MAX_ENTITIES, "Entity handle has id greater than max entities");
	Entity* entity = NULL;

	EntityLookup lookup = entity_data->entity_lookups[handle.id];
	if(lookup.generation == handle.generation) {
		entity = &entity_data->entities[lookup.idx];
	}

	return entity;
}

#define RENDER_SPRITE_OPT_FLIP_X (1 << 0)

RenderQuad* render_sprite(Vec2 world_position, SpriteID sprite_id, RenderGroup* render_group, float rotation_rads, float z_index, uint32 opts) { 
	ASSERT(sprite_id != SPRITE_UNKNOWN, "sprite unknown passed to render sprite");
	RenderQuad* quad = get_next_quad(render_group);
	quad->world_position = world_position;
	Sprite sprite = sprite_table[sprite_id];

	quad->world_size = w_vec_mult((Vec2){ sprite.w, sprite.h }, 1.0 / BASE_PIXELS_PER_UNIT);
	quad->sprite_position = { sprite.x, sprite.y };
	quad->sprite_size = { sprite.w, sprite.h };
	quad->rotation_rads = rotation_rads;
	quad->z_index = z_index;

	if(is_set(opts, RENDER_SPRITE_OPT_FLIP_X)) {
		quad->flip_x = 1;
	}

	return quad;
}

RenderQuad* render_sprite(Vec2 world_position, SpriteID sprite_id, RenderGroup* render_group, float z_index) {
	return render_sprite(world_position, sprite_id, render_group, 0, z_index, 0);
}

RenderQuad* render_animation_sprite(Vec2 world_position, AnimationState* anim_state, RenderGroup* render_group, float z_index) {
	ASSERT(anim_state->animation_id != ANIM_UNKNOWN, "anim unknown passed to render animation sprite");

	flags opts = {};
	if(is_set(anim_state->flags, ANIMATION_STATE_F_FLIP_X)) {
		set(opts, RENDER_SPRITE_OPT_FLIP_X);
	}

	// TODO: might want to create util methods for this with assertions and bounds checking?
	Animation animation = animation_table[anim_state->animation_id];
	SpriteID sprite_id = animation.frames[anim_state->current_frame].sprite_id;
	
	return render_sprite(world_position, sprite_id, render_group, 0, z_index, opts);
} 

RenderQuad* render_entity_sprite(Entity* entity, RenderGroup* render_group) {
	uint32 sprite_opts = 0;
	if(is_set(entity->flags, ENTITY_FLAG_SPRITE_FLIP_X)) {
		set(sprite_opts, RENDER_SPRITE_OPT_FLIP_X);
	}
	return render_sprite(entity->position, entity->sprite_id, render_group, entity->rotation_rads, entity->z_index, sprite_opts);
}

void start_camera_shake(Camera* camera, float magnitude, float frequency, float duration) {
	CameraShake* shake = &camera->shake;
	shake->magnitude = magnitude;
	shake->frequency = frequency;
	shake->duration_s = duration;
	shake->timer_s = duration;
	shake->seed = rand() / RAND_MAX * 1000.0f;
}

Vec2 update_and_get_camera_shake(CameraShake* shake, double dt_s) {
	if(shake->timer_s <= 0) {
		return { 0, 0 };
	}

	shake->timer_s = w_clamp_min(shake->timer_s - dt_s, 0);

	float decay = shake->timer_s / shake->duration_s; 
	float t = (shake->duration_s - shake->timer_s) * shake->frequency;

	Vec2 shake_offset = {
		sinf(t * 15.23 + shake->seed) * shake->magnitude * decay,
		cosf(t * 12.95 + shake->seed) * shake->magnitude * decay
	};

	return shake_offset;
}

Vec2 get_discrete_facing_direction(Vec2 facing_direction, Vec2 velocity) {
	float mag_y_direction = w_abs(facing_direction.y);
	float mag_x_direction = w_abs(facing_direction.x);
	Vec2 result = {};

	if(w_vec_length(velocity) >= 0.1) {
		if(mag_y_direction > mag_x_direction) {
			if(facing_direction.y > 0) {
				result.y = 1;
			}
			else {
				result.y = -1;
			}
		}
		else {
			if(facing_direction.x > 0) {
				result.x = 1;
			}
			else {
				result.x = -1;
			}
		}
	}
	else {
		if(facing_direction.x > 0) {
			result.x = 1;
		}
		else {
			result.x = -1;
		}
	}

	return result;
}

Vec2 get_discrete_facing_direction(Vec2 facing_direction) {
	float mag_y_direction = w_abs(facing_direction.y);
	float mag_x_direction = w_abs(facing_direction.x);
	Vec2 result = {};

	if(mag_y_direction > mag_x_direction) {
		if(facing_direction.y > 0) {
			result.y = 1;
		}
		else {
			result.y = -1;
		}
	}
	else {
		if(facing_direction.x > 0) {
			result.x = 1;
		}
		else {
			result.x = -1;
		}
	}

	return result;
}

struct PlayerWorldInput {
	Vec2 movement_vec;
	Vec2 aim_vec;
	bool shoot;
};

void update_player_world_input(GameInput* game_input, PlayerWorldInput* input, Vec2 player_position, Vec2 screen_size) {
	if(game_input->active_input_type == INPUT_TYPE_KEYBOARD_MOUSE) {
		KeyInputState* key_input_states = game_input->key_input_states;
		Vec2 movement_vec = {};
		if(key_input_states[KEY_W].is_held) {
			movement_vec.y = 1;
		}
		if(key_input_states[KEY_A].is_held) {
			movement_vec.x = -1;
		}
		if(key_input_states[KEY_S].is_held) {
			movement_vec.y = -1;
		}
		if(key_input_states[KEY_D].is_held) {
			movement_vec.x = 1;
		}
		input->movement_vec = w_vec_norm(movement_vec);

		Vec2 mouse_world_position = {
			(game_input->mouse_state.position.x / g_pixels_per_unit) - (screen_size.x / g_pixels_per_unit / 2),
			-(game_input->mouse_state.position.y / g_pixels_per_unit) + (screen_size.y / g_pixels_per_unit / 2)
		};
		input->aim_vec = {
			mouse_world_position.x - player_position.x,
			mouse_world_position.y - player_position.y
		};

		input->shoot = game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON].is_pressed;
	}
	else if(game_input->active_input_type == INPUT_TYPE_GAMEPAD) {
		GamepadState* gamepad_state = &game_input->gamepad_state;
		input->movement_vec = w_vec_norm({ gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_X], -gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_Y] });
	}
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

		game_memory->initialize_renderer(game_memory->screen_size, viewport, game_state->camera.size, &game_state->main_arena);
		game_memory->load_texture(TEXTURE_ID_FONT, "resources/assets/font_texture.png");
		game_memory->load_texture(TEXTURE_ID_SPRITE, "resources/assets/sprite_atlas.png");

		game_memory->init_audio(&game_state->audio_player);
		setup_sound_from_wav(SOUND_BACKGROUND_MUSIC, "resources/assets/background_music_1.wav", 0.60, game_state->sounds, &game_state->main_arena);
		setup_sound_from_wav(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_01.wav", 0.4, game_state->sounds, &game_state->main_arena);
		add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_02.wav", game_state);
		add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_03.wav", game_state);
		add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_04.wav", game_state);
		add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_05.wav", game_state);
		add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_06.wav", game_state);
		add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_07.wav", game_state);
		add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_08.wav", game_state);

		// TODO: The arena marker / restore interface can be improved. Some way to defer?
		char* arena_marker = w_arena_marker(&game_state->main_arena);
		FileContents font_data_file_contents;
		w_read_file("resources/assets/font_data", &font_data_file_contents, &game_state->main_arena);
		memcpy(&game_state->font_data, font_data_file_contents.data, sizeof(FontData));
		w_arena_restore(&game_state->main_arena, arena_marker);

		init_entity_data(&game_state->entity_data);

		EntityHandle player_handle = create_player_entity(&game_state->entity_data, { 0, 0 });
		create_gun_entity(&game_state->entity_data, player_handle);
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
				render_sprite(position, SPRITE_BLOCK_1, &background_render_group, DEFAULT_Z_INDEX);
				break;
			}
			case 2: {
				render_sprite(position, SPRITE_PLANT_1, &main_render_group, DEFAULT_Z_INDEX);
				break;
			}
		}
	}

	PlayerWorldInput player_world_input = {};

	for(int i = 0; i < game_state->entity_data.entity_count; i++) {
		Entity* entity = &game_state->entity_data.entities[i];
		if(entity->type == ENTITY_TYPE_PLAYER) {
			update_player_world_input(game_input, &player_world_input, entity->position, game_memory->screen_size);
			float acceleration_mag = 30;
			Vec2 acceleration = w_vec_mult(w_vec_norm(player_world_input.movement_vec), acceleration_mag);
			entity->acceleration = w_vec_add(acceleration, w_vec_mult(entity->velocity, -5.0));
		}

		//~~~~~~~~~~~ Physics update ~~~~~~~~~~~//
		Vec2 new_position =	w_calc_position(entity->acceleration, entity->velocity, entity->position, g_sim_dt_s);
		Vec2 new_velocity = w_vec_add(w_vec_mult(entity->acceleration, g_sim_dt_s), entity->velocity);

		if(entity->type == ENTITY_TYPE_PROJECTILE) {
			Vec2 position_delta = w_vec_sub(new_position, entity->position);
			entity->distance_traveled += w_vec_length(position_delta);

			if(entity->distance_traveled > MAX_PROJECTILE_DISTANCE) {
				set(entity->flags, ENTITY_FLAG_MARK_FOR_DELETION);
			}
		}

		entity->position = new_position;
		entity->velocity = new_velocity;

		//~~~~~~~~~~~ Facing direction update ~~~~~~~~~ //

		if(entity->type == ENTITY_TYPE_PLAYER) {
			entity->facing_direction = w_vec_norm(player_world_input.aim_vec);
			Vec2 disc_facing_direction = get_discrete_facing_direction(entity->facing_direction, entity->velocity);

			if(w_vec_length(entity->velocity) >= 0.1) {
				if(disc_facing_direction.x > 0) {
					w_play_animation(ANIM_HERO_MOVE_LEFT, &entity->anim_state, ANIMATION_STATE_F_FLIP_X);		
				}
				else if(disc_facing_direction.x < 0) {
					w_play_animation(ANIM_HERO_MOVE_LEFT, &entity->anim_state);		
				}
				else if(disc_facing_direction.y > 0) {
					w_play_animation(ANIM_HERO_MOVE_UP, &entity->anim_state);		
				}
				else {
					w_play_animation(ANIM_HERO_MOVE_DOWN, &entity->anim_state);	
				}
			}
			else {
				if(disc_facing_direction.x > 0) {
					w_play_animation(ANIM_HERO_IDLE, &entity->anim_state, ANIMATION_STATE_F_FLIP_X);
				}
				else {
					w_play_animation(ANIM_HERO_IDLE, &entity->anim_state);
				}
			}
		}

		if(is_set(entity->flags, ENTITY_FLAG_OWNED) && is_set(entity->flags, ENTITY_FLAG_EQUIPPED)) {
			Entity* owner = get_entity(entity->owner_handle, &game_state->entity_data);

			Vec2 aim_vec_rel_owner = player_world_input.aim_vec;

			float rotation_rads = atan2(aim_vec_rel_owner.y, aim_vec_rel_owner.x);
			entity->rotation_rads = rotation_rads;

			if(aim_vec_rel_owner.x < 0) {
				entity->position = { owner->position.x - 0.3f, owner->position.y };
				set(entity->flags, ENTITY_FLAG_SPRITE_FLIP_X);
				// Note: This fixes gun rotation when rads are negative from negative aim vector x
				entity->rotation_rads = M_PI + entity->rotation_rads;
			}
			else {
				entity->position = { owner->position.x + 0.3f, owner->position.y };
				unset(entity->flags, ENTITY_FLAG_SPRITE_FLIP_X);
			}

			entity->position = w_rotate_around_pivot(entity->position, owner->position, entity->rotation_rads);

			Vec2 owner_facing_direction = get_discrete_facing_direction(owner->facing_direction, owner->velocity);
			if(owner_facing_direction.y > 0) {
				entity->z_index = owner->z_index - 0.1;
			}
			else {
				entity->z_index = owner->z_index + 0.1;
			}

			if(player_world_input.shoot) {
				Vec2 velocity_unit = w_vec_unit_from_radians(rotation_rads);
				Vec2 velocity = w_vec_mult(velocity_unit, 30.0f);
				create_projectile_entity(&game_state->entity_data, entity->position, rotation_rads, velocity);

				play_sound_rand(&game_state->sounds[SOUND_BASIC_GUN_SHOT], &game_state->audio_player);
				start_camera_shake(&game_state->camera, 0.1, 10, 0.08);
			}
		}

		w_update_animation(&entity->anim_state, g_sim_dt_s);

		ASSERT(entity->sprite_id != SPRITE_UNKNOWN || entity->anim_state.animation_id != ANIM_UNKNOWN, "Cannot determine entity sprite");

		if(entity->anim_state.animation_id != ANIM_UNKNOWN) {
			render_animation_sprite(entity->position, &entity->anim_state, &main_render_group, entity->z_index);
		}
		else {
			render_entity_sprite(entity, &main_render_group);
		}
	}

	for(int i = 0; i < game_state->entity_data.entity_count; i++) {
		Entity* entity = &game_state->entity_data.entities[i];
		if(is_set(entity->flags, ENTITY_FLAG_MARK_FOR_DELETION)) {
			free_entity(entity->id, &game_state->entity_data);
			// Ensures we process the element that was inserted to replace the freed entity
			i--;
		}
	}

	game_memory->push_audio_samples(&game_state->audio_player);

	sort_render_group(&main_render_group);

	Vec2 shake_offset = update_and_get_camera_shake(&game_state->camera.shake, g_sim_dt_s);
	Vec2 rendered_camera_position = w_vec_add(game_state->camera.position, shake_offset);

	game_memory->push_render_group(background_render_group.quads, background_render_group.count, rendered_camera_position, background_render_group.opts);
	game_memory->push_render_group(main_render_group.quads, main_render_group.count, rendered_camera_position, main_render_group.opts);

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
