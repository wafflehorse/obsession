#include "proc_gen.h"
#include "entity.h"

BiomeSpawnTable biome_spawn_tables[BIOME_COUNT] = {
    [BIOME_FART] = {.entries = {{.entity_type = ENTITY_TYPE_BOAR, .weight = 0.5, .group_min = 1, .group_max = 2},
                                {.entity_type = ENTITY_TYPE_WARRIOR, .weight = 0.25, .group_min = 1, .group_max = 3}

                    },
                    .entry_count = 2,
                    .resource_entries =
                        {
                            {.entity_type = ENTITY_TYPE_IRON_DEPOSIT,
                             .weight = 0.30f,
                             .min_patches = 1,
                             .max_patches = 2,
                             .min_nodes_per_patch = 3,
                             .max_nodes_per_patch = 6,
                             .min_patch_radius = 5,
                             .max_patch_radius = 10},

                            {.entity_type = ENTITY_TYPE_PLANT_CORN,
                             .weight = 0.50f,
                             .min_patches = 1,
                             .max_patches = 4,
                             .min_nodes_per_patch = 3,
                             .max_nodes_per_patch = 6,
                             .min_patch_radius = 5,
                             .max_patch_radius = 10},
                        },
                    .resource_entry_count = 2,
                    .max_chunk_population = 16,
                    .spawn_interval_s = 1}

};

void proc_gen_init_chunk_states(Vec2 world_dimensions, ChunkSpawn* chunk_spawn, uint32 world_seed) {
    int chunks_wide = world_dimensions.x / SPAWN_CHUNK_DIMENSION;
    int chunks_tall = world_dimensions.y / SPAWN_CHUNK_DIMENSION;

    chunk_spawn->state_count = chunks_wide * chunks_tall;
    chunk_spawn->num_rows = chunks_wide;
    chunk_spawn->num_cols = chunks_tall;

    ASSERT(chunk_spawn->state_count < MAX_CHUNK_SPAWN_STATES, "chunk spawn state count exceeds the max");

    for (int row = 0; row < chunk_spawn->num_rows; row++) {
        for (int col = 0; col < chunk_spawn->num_cols; col++) {
            uint32 idx = row * chunk_spawn->num_cols + col;
            chunk_spawn->states[idx].rng = w_vec_hash({(float)row, (float)col}, world_seed);
        }
    }
}

Vec2 proc_gen_world_position_to_chunk_position(Vec2 world_position) {
    Vec2 world_position_top_left_origin = {.x = world_position.x + (DEFAULT_WORLD_WIDTH / 2.0f),
                                           .y = (DEFAULT_WORLD_HEIGHT / 2.0f) - world_position.y};

    Vec2 result;
    result.x = (int)w_floorf(world_position_top_left_origin.x / (float)SPAWN_CHUNK_DIMENSION);
    result.y = (int)w_floorf(world_position_top_left_origin.y / (float)SPAWN_CHUNK_DIMENSION);

    return result;
}

Vec2 proc_gen_chunk_position_to_world_position_top_left(Vec2 chunk_coord) {
    Vec2 world_pos_rel_top_left = {.x = chunk_coord.x * (float)SPAWN_CHUNK_DIMENSION,
                                   .y = chunk_coord.y * (float)SPAWN_CHUNK_DIMENSION};

    Vec2 world_pos_top_left = {.x = world_pos_rel_top_left.x - (DEFAULT_WORLD_WIDTH / 2),
                               .y = -world_pos_rel_top_left.y + (DEFAULT_WORLD_HEIGHT / 2)};

    return world_pos_top_left;
}

Vec2 proc_gen_random_position_in_chunk(ChunkSpawnState* state, Vec2 chunk_coord) {
    Vec2 chunk_top_left_world = proc_gen_chunk_position_to_world_position_top_left(chunk_coord);
    Vec2 result;
    result.x = w_rng_range_f32(&state->rng, chunk_top_left_world.x, chunk_top_left_world.x + SPAWN_CHUNK_DIMENSION);
    result.y = w_rng_range_f32(&state->rng, chunk_top_left_world.y - SPAWN_CHUNK_DIMENSION, chunk_top_left_world.y);

    return result;
}

bool proc_gen_attempt_find_spawn_position(ChunkSpawnState* state, Vec2* spawn_position, Vec2 player_position,
                                          Vec2 chunk_coord) {
    bool result = false;
    for (int attempt = 0; attempt < 4; attempt++) {
        Vec2 candidate = proc_gen_random_position_in_chunk(state, chunk_coord);

        Rect player_safe_space = {
            .x = player_position.x, .y = player_position.y, .w = PLAYER_SAFE_DIMENSION, .h = PLAYER_SAFE_DIMENSION};

        if (w_check_point_in_rect(player_safe_space, candidate)) {
            continue;
        }

        *spawn_position = candidate;
        result = true;
        break;
    }

    return result;
}

Rect proc_gen_chunk_bounds(Vec2 chunk_coord) {
    Vec2 world_top_left = proc_gen_chunk_position_to_world_position_top_left(chunk_coord);

    Rect result;
    result.x = world_top_left.x + (SPAWN_CHUNK_DIMENSION / 2);
    result.y = world_top_left.y - (SPAWN_CHUNK_DIMENSION / 2);
    result.w = SPAWN_CHUNK_DIMENSION;
    result.h = SPAWN_CHUNK_DIMENSION;

    return result;
}

void proc_gen_spawn_resources_for_chunk(ChunkSpawnState* state, GameState* game_state, Vec2 chunk_coord) {
    if (is_set(state->flags, CHUNK_SPAWN_STATE_F_RESOURCES_SPAWNED)) {
        return;
    }

    BiomeSpawnTable* spawn_table = &biome_spawn_tables[BIOME_FART];
    Rect chunk_bounds = proc_gen_chunk_bounds(chunk_coord);

    for (int entry_idx = 0; entry_idx < spawn_table->resource_entry_count; entry_idx++) {
        ResourcePatchSpawnEntry spawn_entry = spawn_table->resource_entries[entry_idx];

        float weight = spawn_entry.weight;
        float rng = w_rng_f32(&state->rng);

        if (rng <= weight) {
            int count = w_rng_range_i32(&state->rng, spawn_entry.min_patches, spawn_entry.max_patches);

            for (int curr_patch = 0; curr_patch < count; curr_patch++) {
                Vec2 patch_position;
                bool found_patch_position = proc_gen_attempt_find_spawn_position(
                    state, &patch_position, game_state->player->position, chunk_coord);
                if (found_patch_position) {
                    int node_count =
                        w_rng_range_i32(&state->rng, spawn_entry.min_nodes_per_patch, spawn_entry.max_nodes_per_patch);
                    float max_radius =
                        w_rng_range_f32(&state->rng, spawn_entry.min_patch_radius, spawn_entry.max_patch_radius);

                    for (int i = 0; i < node_count; i++) {
                        float angle = w_rng_f32(&state->rng) * 2 * W_PI;
                        float radius = w_rng_f32(&state->rng) * max_radius;

                        Vec2 offset = {cosf(angle) * radius, sinf(angle) * radius};

                        Vec2 item_position = w_vec_add(patch_position, offset);

                        if (w_check_point_in_rect(chunk_bounds, item_position)) {
                            entity_create(spawn_entry.entity_type, item_position);
                        }
                    }
                }
            }
        }
    }

    set(state->flags, CHUNK_SPAWN_STATE_F_RESOURCES_SPAWNED);
}

void proc_gen_update_chunk_states(Vec2 player_position, GameState* game_state, double sim_dt_s) {
    ChunkSpawn* chunk_spawn = &game_state->chunk_spawn;
    Vec2 center_spawn_chunk = proc_gen_world_position_to_chunk_position(player_position);
    Vec2 start_spawn_chunk = w_vec_sub(center_spawn_chunk, {2, 2});

    int grid_dimension = 5;

    // loop over AxA grid centered at player chunk
    for (int row = start_spawn_chunk.y; row < (start_spawn_chunk.y + grid_dimension); row++) {
        for (int col = start_spawn_chunk.x; col < (start_spawn_chunk.x + grid_dimension); col++) {
            if (row >= chunk_spawn->num_rows || row < 0 || col >= chunk_spawn->num_cols || col < 0) {
                continue;
            }
            Vec2 chunk_coord = {(float)col, (float)row};
            uint32 state_idx = row * chunk_spawn->num_cols + col;
            ChunkSpawnState* state = &chunk_spawn->states[state_idx];

            state->respawn_timer_s += sim_dt_s;

            BiomeSpawnTable* spawn_table = &biome_spawn_tables[BIOME_FART];

            proc_gen_spawn_resources_for_chunk(state, game_state, chunk_coord);

            if (state->respawn_timer_s >= spawn_table->spawn_interval_s) {
                state->respawn_timer_s = 0;

                if (state->current_population < spawn_table->max_chunk_population) {
                    float total_spawn_entry_weight = 0;

                    for (int i = 0; i < spawn_table->entry_count; i++) {
                        total_spawn_entry_weight += spawn_table->entries[i].weight;
                    }

                    float rng = w_rng_range_f32(&state->rng, 0, total_spawn_entry_weight);

                    int chosen_entry_idx = -1;

                    for (int entry_idx = 0; entry_idx < spawn_table->entry_count; entry_idx++) {
                        SpawnEntry spawn_entry = spawn_table->entries[entry_idx];
                        if (rng <= spawn_entry.weight) {
                            chosen_entry_idx = entry_idx;
                            break;
                        } else {
                            rng -= spawn_entry.weight;
                        }
                    }

                    ASSERT(chosen_entry_idx != -1, "Did not find an entity to spawn");

                    SpawnEntry spawn_entry = spawn_table->entries[chosen_entry_idx];

                    uint32 count = w_rng_range_i32(&state->rng, spawn_entry.group_min, spawn_entry.group_max);

                    for (int i = 0; i < count; i++) {
                        Vec2 spawn_position;

                        bool spawn_position_found = proc_gen_attempt_find_spawn_position(
                            state, &spawn_position, game_state->player->position, chunk_coord);

                        if (spawn_position_found) {
                            entity_create(spawn_entry.entity_type, spawn_position);
                        }
                    }

                    state->current_population += count;
                }
            }
        }
    }
}

void proc_gen_plants(DecorationData* decoration_data, FBMContext* fbm_context) {
    Decoration* decorations = decoration_data->decorations;
    for (int i = 0; i < MAX_DECORATIONS; i++) {
        if (decorations[i].type == DECORATION_TYPE_PLANT) {
            decoration_data->decorations[i].type = DECORATION_TYPE_NONE;
        }
    }

    Vec2 world_top_left_tile_position = get_world_top_left_tile_position();

    for (int i = 0; i < DEFAULT_WORLD_WIDTH * DEFAULT_WORLD_HEIGHT; i++) {
        uint32 col = i % DEFAULT_WORLD_WIDTH;
        uint32 row = i / DEFAULT_WORLD_HEIGHT;
        Vec2 position = {world_top_left_tile_position.x + col, world_top_left_tile_position.y - row};

        float noise_val = stb_perlin_fbm_noise3(position.x * fbm_context->freq, position.y * fbm_context->freq, 0,
                                                fbm_context->lacunarity, fbm_context->gain, fbm_context->octaves);
        if (noise_val > 0.7f) {
            create_decoration(decoration_data, DECORATION_TYPE_PLANT, position, SPRITE_PLANT_1);
        }
    }
}

// bool w_is_local_maximum(FBMContext* fbm_context, Vec2 position, float radius, float peak_threshold) {
//     float center =
//         w_fbm_noise2_seed(position.x * fbm_context->freq, position.y * fbm_context->freq, fbm_context->lacunarity,
//                           fbm_context->gain, fbm_context->octaves, fbm_context->seed);
//
//     if (center < peak_threshold) {
//         return false;
//     }
//
//     Vec2 directions[8] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
//
//     for (int i = 0; i < 8; i++) {
//         Vec2 sample_delta = w_vec_mult(directions[i], radius);
//         Vec2 sample_position = w_vec_add(position, sample_delta);
//
//         float sample_value =
//             w_fbm_noise2_seed(sample_position.x * fbm_context->freq, sample_position.y * fbm_context->freq,
//                               fbm_context->lacunarity, fbm_context->gain, fbm_context->octaves, fbm_context->seed);
//
//         if (sample_value >= center) {
//             return false;
//         }
//     }
//
//     return true;
// }

// void try_spawn_entity_patch(Vec2 patch_coord, Vec2 patch_position, EntityType entity_type, EntityData* entity_data,
//                             Rect chunk_bounds, FBMContext* fbm_context) {
//     if (w_is_local_maximum(fbm_context, patch_coord, 1, 0.0f)) {
//         // patch exists at this spot
//         uint32 patch_seed = w_vec_hash(patch_coord, fbm_context->seed);
//
//         int MIN_NODES = 3;
//         int MAX_NODES = 6;
//         float MIN_RADIUS = 5;
//         float MAX_RADIUS = 10;
//
//         int count = w_rng_range_i32(&patch_seed, MIN_NODES, MAX_NODES);
//         float max_radius = w_rng_range_f32(&patch_seed, MIN_RADIUS, MAX_RADIUS);
//
//         for (int i = 0; i < count; i++) {
//             float angle = w_rng_f32(&patch_seed) * 2 * W_PI;
//             float radius = w_rng_f32(&patch_seed) * max_radius;
//
//             Vec2 offset = {cosf(angle) * radius, sinf(angle) * radius};
//
//             Vec2 item_position = w_vec_add(patch_position, offset);
//
//             if (w_check_point_in_rect(chunk_bounds, item_position)) {
//                 entity_create(entity_data, entity_type, item_position);
//             }
//         }
//     }
// }

// void proc_gen_entity_patches(EntityData* entity_data, EntityType entity_type, FBMContext* fbm_context) {
//     for (int i = 0; i < entity_data->entity_count; i++) {
//         if (entity_data->entities[i].type == entity_type) {
//             set(entity_data->entities[i].flags, ENTITY_F_MARK_FOR_DELETION);
//         }
//     }
//
//     float CHUNK_SIZE = 512.0f;
//     Vec2 chunk_position = {0, 0};
//
//     float MAX_PATCH_RADIUS = 10;
//     float PATCH_SPACING = 20;
//
//     float chunk_min_x = chunk_position.x - (CHUNK_SIZE / 2);
//     float chunk_max_x = chunk_min_x + CHUNK_SIZE;
//     float chunk_min_y = chunk_position.y - (CHUNK_SIZE / 2);
//     float chunk_max_y = chunk_min_y + CHUNK_SIZE;
//
//     Rect chunk_bounds = {.x = chunk_position.x, .y = chunk_position.y, .w = CHUNK_SIZE, .h = CHUNK_SIZE};
//
//     float gen_min_x = chunk_min_x - MAX_PATCH_RADIUS;
//     float gen_max_x = chunk_max_x + MAX_PATCH_RADIUS;
//     float gen_min_y = chunk_min_y - MAX_PATCH_RADIUS;
//     float gen_max_y = chunk_max_y + MAX_PATCH_RADIUS;
//
//     int patch_min_x = (int)w_floorf(gen_min_x / PATCH_SPACING);
//     int patch_max_x = (int)w_floorf(gen_max_x / PATCH_SPACING);
//     int patch_min_y = (int)w_floorf(gen_min_y / PATCH_SPACING);
//     int patch_max_y = (int)w_floorf(gen_max_y / PATCH_SPACING);
//
//     for (int py = patch_min_y; py < patch_max_y; py++) {
//         for (int px = patch_min_x; px < patch_max_x; px++) {
//             Vec2 patch_coord = {(float)px, (float)py};
//             Vec2 patch_position = {px * PATCH_SPACING, py * PATCH_SPACING};
//
//             try_spawn_entity_patch(patch_coord, patch_position, entity_type, entity_data, chunk_bounds, fbm_context);
//         }
//     }
// }
