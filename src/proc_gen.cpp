
bool w_is_local_maximum(FBMContext* fbm_context, Vec2 position, float radius, float peak_threshold) {
    float center =
        w_fbm_noise2_seed(position.x * fbm_context->freq, position.y * fbm_context->freq, fbm_context->lacunarity,
                          fbm_context->gain, fbm_context->octaves, fbm_context->seed);

    if (center < peak_threshold) {
        return false;
    }

    Vec2 directions[8] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};

    for (int i = 0; i < 8; i++) {
        Vec2 sample_delta = w_vec_mult(directions[i], radius);
        Vec2 sample_position = w_vec_add(position, sample_delta);

        float sample_value =
            w_fbm_noise2_seed(sample_position.x * fbm_context->freq, sample_position.y * fbm_context->freq,
                              fbm_context->lacunarity, fbm_context->gain, fbm_context->octaves, fbm_context->seed);

        if (sample_value >= center) {
            return false;
        }
    }

    return true;
}

void try_spawn_entity_patch(Vec2 patch_coord, Vec2 patch_position, EntityType entity_type, EntityData* entity_data,
                            Rect chunk_bounds, FBMContext* fbm_context) {
    if (w_is_local_maximum(fbm_context, patch_coord, 1, 0.0f)) {
        // patch exists at this spot
        uint32 patch_seed = w_vec_hash(patch_coord, fbm_context->seed);

        int MIN_NODES = 3;
        int MAX_NODES = 6;
        float MIN_RADIUS = 5;
        float MAX_RADIUS = 10;

        int count = w_rng_range_i32(&patch_seed, MIN_NODES, MAX_NODES);
        float max_radius = w_rng_range_f32(&patch_seed, MIN_RADIUS, MAX_RADIUS);

        for (int i = 0; i < count; i++) {
            float angle = w_rng_f32(&patch_seed) * 2 * W_PI;
            float radius = w_rng_f32(&patch_seed) * max_radius;

            Vec2 offset = {cosf(angle) * radius, sinf(angle) * radius};

            Vec2 item_position = w_vec_add(patch_position, offset);

            if (w_check_point_in_rect(chunk_bounds, item_position)) {
                entity_create(entity_data, entity_type, item_position);
            }
        }
    }
}

void proc_gen_entity_patches(EntityData* entity_data, EntityType entity_type, FBMContext* fbm_context) {
    for (int i = 0; i < entity_data->entity_count; i++) {
        if (entity_data->entities[i].type == entity_type) {
            set(entity_data->entities[i].flags, ENTITY_F_MARK_FOR_DELETION);
        }
    }

    float CHUNK_SIZE = 512.0f;
    Vec2 chunk_position = {0, 0};

    float MAX_PATCH_RADIUS = 10;
    float PATCH_SPACING = 20;

    float chunk_min_x = chunk_position.x - (CHUNK_SIZE / 2);
    float chunk_max_x = chunk_min_x + CHUNK_SIZE;
    float chunk_min_y = chunk_position.y - (CHUNK_SIZE / 2);
    float chunk_max_y = chunk_min_y + CHUNK_SIZE;

    Rect chunk_bounds = {.x = chunk_position.x, .y = chunk_position.y, .w = CHUNK_SIZE, .h = CHUNK_SIZE};

    float gen_min_x = chunk_min_x - MAX_PATCH_RADIUS;
    float gen_max_x = chunk_max_x + MAX_PATCH_RADIUS;
    float gen_min_y = chunk_min_y - MAX_PATCH_RADIUS;
    float gen_max_y = chunk_max_y + MAX_PATCH_RADIUS;

    int patch_min_x = (int)w_floorf(gen_min_x / PATCH_SPACING);
    int patch_max_x = (int)w_floorf(gen_max_x / PATCH_SPACING);
    int patch_min_y = (int)w_floorf(gen_min_y / PATCH_SPACING);
    int patch_max_y = (int)w_floorf(gen_max_y / PATCH_SPACING);

    for (int py = patch_min_y; py < patch_max_y; py++) {
        for (int px = patch_min_x; px < patch_max_x; px++) {
            Vec2 patch_coord = {(float)px, (float)py};
            Vec2 patch_position = {px * PATCH_SPACING, py * PATCH_SPACING};

            try_spawn_entity_patch(patch_coord, patch_position, entity_type, entity_data, chunk_bounds, fbm_context);
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
