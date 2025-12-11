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
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

char* g_base_path;
char* g_debug_project_dir;
uint32 g_pixels_per_unit;
double g_sim_dt_s;
RenderGroup* g_debug_render_group;

#define DEFAULT_WORLD_WIDTH 128
#define DEFAULT_WORLD_HEIGHT 128

Vec2 get_world_top_left_tile_position() {
    return {-DEFAULT_WORLD_WIDTH / 2 + 0.5, DEFAULT_WORLD_HEIGHT / 2 - 0.5};
}

#define DEFAULT_Z_INDEX 1.0f

#ifdef DEBUG
#define ABS_PROJECT_RESOURCE_PATH "/Users/parkerpestana/dev/obsession/resources/"
#define ABS_GAME_INIT_PATH "/Users/parkerpestana/dev/obsession/resources/game.init"
#endif

struct PlayerInputWorld {
    Vec2 movement_vec;
    Vec2 aim_vec;
    bool use_held_item;
    bool drop_item;
    bool interact;
};

struct PlayerInputInventory {
    bool select;
};

struct PlayerInput {
    PlayerInputWorld world;
    PlayerInputInventory inventory;
};

Vec2 random_point_near_position(Vec2 position, float x_range, float y_range) {
    float random_x = w_random_between(-x_range, x_range);
    float random_y = w_random_between(-y_range, y_range);
    Vec2 result = {.x = position.x + random_x, .y = position.y + random_y};

    return result;
}

#define RENDER_SPRITE_OPT_FLIP_X (1 << 0)
#define RENDER_SPRITE_OPT_TINT_SET (1 << 1)

// TODO: remove this interface and replace with params version
RenderQuad* render_sprite(Vec2 world_position, SpriteID sprite_id, RenderGroup* render_group, Vec4 tint,
                          float rotation_rads, float z_index, uint32 opts) {
    ASSERT(sprite_id != SPRITE_UNKNOWN, "sprite unknown passed to render sprite");
    RenderQuad* quad = get_next_quad(render_group);
    quad->world_position = world_position;
    Sprite sprite = sprite_table[sprite_id];

    quad->world_size = w_vec_mult((Vec2){sprite.w, sprite.h}, 1.0 / BASE_PIXELS_PER_UNIT);
    quad->sprite_position = {sprite.x, sprite.y};
    quad->sprite_size = {sprite.w, sprite.h};
    quad->rotation_rads = rotation_rads;
    quad->z_index = z_index;

    if (is_set(opts, RENDER_SPRITE_OPT_TINT_SET)) {
        quad->tint = tint;
    }

    if (is_set(opts, RENDER_SPRITE_OPT_FLIP_X)) {
        quad->flip_x = 1;
    }

    return quad;
}

RenderQuad* render_sprite(Vec2 world_position, SpriteID sprite_id, RenderGroup* render_group, float z_index) {
    return render_sprite(world_position, sprite_id, render_group, {1, 1, 1, 1}, 0, z_index, 0);
}

struct RenderSpriteParams {
    uint32 opts;
    float rotation_rads;
    float z_index;
    Vec4 tint;
};

RenderQuad* render_sprite(Vec2 world_position, SpriteID sprite_id, RenderGroup* render_group,
                          RenderSpriteParams params) {
    return render_sprite(world_position, sprite_id, render_group, params.tint, params.rotation_rads, params.z_index,
                         params.opts);
}

Vec2 get_mouse_world_position(Camera* camera, GameInput* game_input, Vec2 window_size_px) {
    Vec2 mouse_screen_position = {(game_input->mouse_state.position_px.x - (window_size_px.x / 2)) / g_pixels_per_unit,
                                  -(game_input->mouse_state.position_px.y - (window_size_px.y / 2)) /
                                      g_pixels_per_unit};
    mouse_screen_position = w_vec_mult(mouse_screen_position, 1.0f / camera->zoom);
    Vec2 mouse_world_position = w_vec_add(mouse_screen_position, camera->position);

    return mouse_world_position;
}

float pixels_to_units(float pixels) {
    return pixels / BASE_PIXELS_PER_UNIT;
}

Vec2 pixels_to_units(Vec2 pixels) {
    return {pixels.x / BASE_PIXELS_PER_UNIT, pixels.y / BASE_PIXELS_PER_UNIT};
}

Vec2 sprite_get_world_size(SpriteID sprite_id) {
    Sprite sprite = sprite_table[sprite_id];

    return {sprite.w / BASE_PIXELS_PER_UNIT, sprite.h / BASE_PIXELS_PER_UNIT};
}

#include "entity.cpp"
#include "hotbar.cpp"

void proc_gen_iron_ore(EntityData* entity_data, FBMContext* fbm_context) {
    for (int i = 0; i < entity_data->entity_count; i++) {
        if (entity_data->entities[i].type == ENTITY_TYPE_IRON_DEPOSIT) {
            set(entity_data->entities[i].flags, ENTITY_F_MARK_FOR_DELETION);
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
            entity_create_ore_deposit(entity_data, ENTITY_TYPE_IRON_DEPOSIT, position, SPRITE_ORE_IRON_0);
        }
        // else if (noise_val < 0.61f && noise_val > 0.6f) {
        // 	create_prop_entity(&game_state->entity_data, position, SPRITE_BLOCK_1);
        // }
    }
}

void create_decoration(DecorationData* decoration_data, DecorationType type, Vec2 position, SpriteID sprite_id) {
    Decoration* decorations = decoration_data->decorations;
    Decoration* decoration = NULL;
    for (int i = 0; i < MAX_DECORATIONS; i++) {
        if (decorations[i].type == DECORATION_TYPE_NONE) {
            decoration = &decorations[i];
        }
    }

    ASSERT(decoration, "MAX_DECORATIONS has been reached");

    decoration->position = position;
    decoration->sprite_id = sprite_id;
    decoration->type = type;
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

#ifdef DEBUG
#include "engine_tools.cpp"
#endif

uint32 get_viewport_scale_factor(Vec2 screen_size) {
    uint32 width_scale = screen_size.x / BASE_RESOLUTION_WIDTH;
    uint32 height_scale = screen_size.y / BASE_RESOLUTION_HEIGHT;

    return w_min(width_scale, height_scale);
}

Vec2 get_viewport(uint32 scale) {
    return {(float)scale * BASE_RESOLUTION_WIDTH, (float)scale * BASE_RESOLUTION_HEIGHT};
}

void init_entity_data(EntityData* entity_data) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entity_data->entity_ids[i] = i;

        EntityLookup* lookup = &entity_data->entity_lookups[i];
        lookup->generation = 0;
        lookup->idx = i;
    }
}

bool inventory_ui_open(GameState* game_state) {
    return is_set(game_state->flags, GAME_STATE_F_INVENTORY_OPEN) ||
           entity_find(game_state->open_entity_inventory, &game_state->entity_data);
}

PlayerInput player_input_get(GameInput* game_input, GameState* game_state) {
    PlayerInput input = {};
    bool inventory_ui_is_open = inventory_ui_open(game_state);
    if (game_input->active_input_type == INPUT_TYPE_KEYBOARD_MOUSE) {
        KeyInputState* key_input_states = game_input->key_input_states;
        if (!inventory_ui_is_open) {
            Vec2 movement_vec = {};
            if (key_input_states[KEY_W].is_held) {
                movement_vec.y = 1;
            }
            if (key_input_states[KEY_A].is_held) {
                movement_vec.x = -1;
            }
            if (key_input_states[KEY_S].is_held) {
                movement_vec.y = -1;
            }

            if (key_input_states[KEY_D].is_held) {
                movement_vec.x = 1;
            }

            for (int i = KEY_1; i < KEY_9; i++) {
                if (key_input_states[i].is_pressed) {
                    int hotbar_idx = i - KEY_1;
                    game_state->hotbar.active_item_idx = hotbar_idx;
                }
            }

            if (key_input_states[KEY_G].is_pressed) {
                input.world.drop_item = true;
            }

            input.world.movement_vec = w_vec_norm(movement_vec);

#ifdef DEBUG
            if (!is_set(game_state->tools.flags, TOOLS_F_CAPTURING_MOUSE_INPUT)) {
                input.world.use_held_item = game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON].is_pressed;
            }
#else
            input.world.use_held_item = game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON].is_pressed;
#endif

            Vec2 mouse_world_position = game_state->world_input.mouse_position_world;
            Vec2 player_position = game_state->player->position;
            input.world.aim_vec = w_vec_norm(
                {mouse_world_position.x - player_position.x, mouse_world_position.y - (player_position.y + 0.5f)});
        }

        if (key_input_states[KEY_E].is_pressed) {
            input.world.interact = true;
        }

        if (key_input_states[KEY_I].is_pressed) {
            toggle(game_state->flags, GAME_STATE_F_INVENTORY_OPEN);
        }

    } else if (game_input->active_input_type == INPUT_TYPE_GAMEPAD) {
        GamepadState* gamepad_state = &game_input->gamepad_state;
        input.world.movement_vec = w_vec_norm(
            {gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_X], -gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_Y]});
    }

    return input;
}

uint32 get_next_attack_id(uint32* attack_id_next) {
    uint32 result = (*attack_id_next)++;

    if (*attack_id_next > ATTACK_ID_LAST) {
        *attack_id_next = ATTACK_ID_START;
    }

    return result;
}

// TODO: make this faster?
void remove_collision_rules(uint32 id, CollisionRule** hash, CollisionRule** free_list) {
    for (int i = 0; i < MAX_COLLISION_RULES; i++) {
        for (CollisionRule** rule = &hash[i]; *rule;) {
            if ((*rule)->a_id == id || (*rule)->b_id == id) {
                CollisionRule* free_rule = *rule;
                *rule = (*rule)->next_rule;

                free_rule->next_rule = *free_list;
                *free_list = &(*free_rule);
            } else {
                rule = &(*rule)->next_rule;
            }
        }
    }
}

uint32 sort_and_get_collision_hash_bucket(uint32* a_id, uint32* b_id) {
    if (*a_id > *b_id) {
        uint32 temp = *a_id;
        *a_id = *b_id;
        *b_id = temp;
    }

    uint32 hash_bucket = *a_id & (MAX_COLLISION_RULES - 1);
    return hash_bucket;
}

CollisionRule* find_collision_rule(uint32 a_id, uint32 b_id, GameState* game_state) {
    CollisionRule** hash = game_state->collision_rule_hash;

    uint32 hash_bucket = sort_and_get_collision_hash_bucket(&a_id, &b_id);

    CollisionRule* rule = 0;
    CollisionRule* found = 0;
    for (rule = hash[hash_bucket]; rule; rule = rule->next_rule) {
        if (rule->a_id == a_id && rule->b_id == b_id) {
            found = rule;
            break;
        }
    }

    return found;
}

void add_collision_rule(uint32 a_id, uint32 b_id, bool should_collide, GameState* game_state) {
    CollisionRule** hash = game_state->collision_rule_hash;
    CollisionRule** free_list = &game_state->collision_rule_free_list;

    CollisionRule* found = find_collision_rule(a_id, b_id, game_state);
    uint32 hash_bucket = sort_and_get_collision_hash_bucket(&a_id, &b_id);

    if (!found) {
        found = *free_list;
        if (!found) {
            found = (CollisionRule*)w_arena_alloc(&game_state->main_arena, sizeof(CollisionRule));
        } else {
            *free_list = found->next_rule;
        }
        found->a_id = a_id;
        found->b_id = b_id;
        found->next_rule = hash[hash_bucket];
        hash[hash_bucket] = found;
    }

    found->should_collide = should_collide;
}

// TODO: Do we need to make sure that entities marked for deletion don't collide?
bool should_collide(Entity* entity_a, Entity* entity_b, CollisionRule** hash) {
    float distance_between = w_euclid_dist(entity_a->position, entity_b->position);
    if (entity_a->id == entity_b->id || is_set(entity_a->flags, ENTITY_F_NONSPACIAL) ||
        is_set(entity_b->flags, ENTITY_F_NONSPACIAL) || distance_between > 10) {
        return false;
    }

    ASSERT(entity_a->collider.shape != COLLIDER_SHAPE_UNKNOWN, "unknown collider shape on spacial entity")

    bool should_collide = true;
    if (is_set(entity_a->flags, ENTITY_F_KILLABLE) && !is_set(entity_b->flags, ENTITY_F_BLOCKER)) {
        should_collide = false;
    }

    if (entity_a->id > entity_b->id) {
        Entity* temp = entity_a;
        entity_a = entity_b;
        entity_b = temp;
    }

    uint32 hash_bucket = entity_a->id & (MAX_COLLISION_RULES - 1);
    CollisionRule* rule = 0;
    for (rule = hash[hash_bucket]; rule; rule = rule->next_rule) {
        if (rule->a_id == entity_a->id && rule->b_id == entity_b->id) {
            should_collide = rule->should_collide;
            break;
        }
    }

    return should_collide;
}

void handle_collision(Entity* subject, Entity* target, Vec2 collision_normal, float dt_collision_s,
                      bool* can_move_freely, GameState* game_state) {
    *can_move_freely = true;

    if (subject->type == ENTITY_TYPE_PROJECTILE) {
        if (is_set(target->flags, ENTITY_F_KILLABLE)) {
            entity_deal_damage(target, 1, game_state);
        }

        if (is_set(target->flags, ENTITY_F_KILLABLE) || is_set(target->flags, ENTITY_F_BLOCKER)) {
            set(subject->flags, ENTITY_F_MARK_FOR_DELETION);
        }

        add_collision_rule(subject->id, target->id, false, game_state);
    }

    if (is_set(target->flags, ENTITY_F_BLOCKER)) {
        subject->position =
            w_calc_position(subject->acceleration, subject->velocity, subject->position, dt_collision_s);
        float collision_normal_velocity_penetration = w_dot_product(subject->velocity, collision_normal);
        Vec2 collision_normal_velocity = w_vec_mult(collision_normal, collision_normal_velocity_penetration);
        subject->velocity = w_vec_sub(subject->velocity, collision_normal_velocity);

        float collision_normal_acceleration_penetration = w_dot_product(subject->acceleration, collision_normal);
        Vec2 collision_normal_acceleration = w_vec_mult(collision_normal, collision_normal_acceleration_penetration);
        subject->acceleration = w_vec_sub(subject->acceleration, collision_normal_acceleration);
        *can_move_freely = false;
    }
}

void update_brain(Entity* entity, GameState* game_state, double dt_s) {
    Entity* player = game_state->player;
    Brain* brain = &entity->brain;
    brain->cooldown_s = w_clamp_min(brain->cooldown_s - dt_s, 0);
    float distance_to_player = w_euclid_dist(entity->position, player->position);
    EntityAnimations animations = entity_info[entity->type].animations;
    if (brain->type == BRAIN_TYPE_WARRIOR) {
        if (distance_to_player < 5 && brain->ai_state != AI_STATE_ATTACK && brain->ai_state != AI_STATE_DEAD) {
            brain->ai_state = AI_STATE_CHASE;
        }
        switch (brain->ai_state) {
        case AI_STATE_IDLE:
            if (brain->cooldown_s <= 0) {
                brain->target_position = random_point_near_position(entity->position, 5, 5);
                brain->cooldown_s = 10;
                brain->ai_state = AI_STATE_WANDER;
            }
            entity->velocity = {0, 0};
            entity_play_animation_with_direction(animations.idle, &entity->anim_state, entity->facing_direction);
            break;
        case AI_STATE_WANDER: {
            if (w_euclid_dist(entity->position, brain->target_position) <= 1.0f || brain->cooldown_s <= 0) {
                brain->ai_state = AI_STATE_IDLE;
                brain->cooldown_s = w_random_between(1, 6);
            }

            Vec2 wander_direction = w_vec_norm(w_vec_sub(brain->target_position, entity->position));
            entity->velocity = w_vec_mult(wander_direction, 1.0f);
            entity->facing_direction = w_vec_norm(entity->velocity);
            entity_play_animation_with_direction(animations.move, &entity->anim_state, entity->facing_direction);
            break;
        }
        case AI_STATE_CHASE:
            if (distance_to_player >= 5) {
                brain->ai_state = AI_STATE_IDLE;
                brain->cooldown_s = w_random_between(1, 6);
            } else if (distance_to_player < 0.5) {
                brain->ai_state = AI_STATE_ATTACK;
            } else {
                brain->target_position = player->position;
                Vec2 chase_direction = w_vec_norm(w_vec_sub(brain->target_position, entity->position));
                entity->velocity = w_vec_mult(chase_direction, 5.0f);
            }
            entity->facing_direction = w_vec_norm(entity->velocity);
            entity_play_animation_with_direction(animations.move, &entity->anim_state, entity->facing_direction);
            break;
        case AI_STATE_ATTACK: {
            entity->velocity = {0, 0};
            if (player->position.x < entity->position.x) {
                entity->facing_direction.x = -1;
            } else {
                entity->facing_direction.x = 1;
            }
            entity_play_animation_with_direction(animations.attack, &entity->anim_state, entity->facing_direction);
            if (entity->attack_id == 0) {
                entity->attack_id = get_next_attack_id(&game_state->attack_id_next);
            }
            if (w_animation_complete(&entity->anim_state, dt_s)) {
                entity->attack_id = 0;
                remove_collision_rules(entity->attack_id, game_state->collision_rule_hash,
                                       &game_state->collision_rule_free_list);
                brain->ai_state = AI_STATE_CHASE;
                brain->cooldown_s = w_random_between(1, 6);
            }
            break;
        }
        case AI_STATE_DEAD:
            entity->velocity = {0, 0};
            set(entity->flags, ENTITY_F_NONSPACIAL);
            set(entity->flags, ENTITY_F_DELETE_AFTER_ANIMATION);
            if (animations.death != ANIM_UNKNOWN) {
                entity_play_animation_with_direction(animations.death, &entity->anim_state, entity->facing_direction);
            }
            break;
        }
    } else if (brain->type == BRAIN_TYPE_BOAR) {
        switch (brain->ai_state) {
        case AI_STATE_IDLE:
            if (brain->cooldown_s <= 0) {
                brain->target_position = random_point_near_position(entity->position, 5, 5);
                brain->cooldown_s = 10;
                brain->ai_state = AI_STATE_WANDER;
            }
            entity->velocity = {0, 0};
            entity_play_animation_with_direction(animations.idle, &entity->anim_state, entity->facing_direction);
            break;
        case AI_STATE_WANDER: {
            if (w_euclid_dist(entity->position, brain->target_position) <= 1.0f || brain->cooldown_s <= 0) {
                brain->ai_state = AI_STATE_IDLE;
                brain->cooldown_s = w_random_between(1, 6);
            }

            Vec2 wander_direction = w_vec_norm(w_vec_sub(brain->target_position, entity->position));
            entity->velocity = w_vec_mult(wander_direction, 1.0f);
            entity->facing_direction = w_vec_norm(entity->velocity);
            entity_play_animation_with_direction(animations.move, &entity->anim_state, entity->facing_direction);
            break;
        }
        case AI_STATE_DEAD:
            entity->velocity = {0, 0};
            set(entity->flags, ENTITY_F_NONSPACIAL);
            set(entity->flags, ENTITY_F_DELETE_AFTER_ANIMATION);
            if (animations.death != ANIM_UNKNOWN) {
                entity_play_animation_with_direction(animations.death, &entity->anim_state, entity->facing_direction);
            }
            break;
        default:
            break;
        }
    }
}

SpriteID player_hp_to_ui_sprite[MAX_HP_PLAYER + 1] = {
    [0] = SPRITE_PLAYER_HP_UI_10, [1] = SPRITE_PLAYER_HP_UI_9, [2] = SPRITE_PLAYER_HP_UI_8, [3] = SPRITE_PLAYER_HP_UI_7,
    [4] = SPRITE_PLAYER_HP_UI_6,  [5] = SPRITE_PLAYER_HP_UI_5, [6] = SPRITE_PLAYER_HP_UI_4, [7] = SPRITE_PLAYER_HP_UI_3,
    [8] = SPRITE_PLAYER_HP_UI_2,  [9] = SPRITE_PLAYER_HP_UI_1, [10] = SPRITE_PLAYER_HP_UI_0};

void render_player_hp_ui(int hp, Camera camera, RenderGroup* render_group) {
    SpriteID ui_sprite = player_hp_to_ui_sprite[hp];
    Sprite sprite = sprite_table[ui_sprite];

    Vec2 camera_top_left = {camera.position.x - (camera.size.x / 2), camera.position.y + (camera.size.y / 2)};

    Vec2 ui_position = {camera_top_left.x + (pixels_to_units(sprite.w) / 2) + 0.5f,
                        camera_top_left.y - (pixels_to_units(sprite.h) / 2) - 0.5f};

    render_sprite(ui_position, ui_sprite, render_group, DEFAULT_Z_INDEX);
}

void debug_render_entity_colliders(Entity* entity, bool has_collided) {
#ifdef DEBUG
    if (!is_set(entity->flags, ENTITY_F_NONSPACIAL)) {
        RenderQuad* quad = get_next_quad(g_debug_render_group);

        Collider collider = entity->collider;
        Vec2 collider_position = w_vec_add(entity->position, collider.offset);
        quad->world_position = collider_position;
        quad->world_size = {collider.width, collider.height};
        quad->draw_colored_rect = 1;
        quad->rgba = {0, 0, 255, 0.5};
        if (has_collided) {
            quad->rgba = {255, 0, 0, 0.5};
        }
        quad->z_index = entity->z_index + 1;
    }
#endif
}

#define CRAFTING_MAX_INGREDIENTS 8

struct CraftingIngredient {
    EntityType entity_type;
    uint32 quantity;
};

struct CraftingRecipe {
    EntityType entity_type;
    CraftingIngredient ingredients[CRAFTING_MAX_INGREDIENTS];
};

CraftingRecipe crafting_recipes[ENTITY_TYPE_COUNT] = {
    {.entity_type = ENTITY_TYPE_CHEST_IRON, .ingredients = {{.entity_type = ENTITY_TYPE_IRON, .quantity = 4}}}};

CraftingRecipe* crafting_recipe_find(EntityType entity_type) {
    CraftingRecipe* recipe = NULL;

    for (int i = 0; i < ENTITY_TYPE_COUNT; i++) {
        if (crafting_recipes[i].entity_type == entity_type) {
            recipe = &crafting_recipes[i];
        }
    }

    return recipe;
}

bool crafting_can_craft_item(EntityType entity_type, HotBar* hotbar) {
    CraftingRecipe* recipe = crafting_recipe_find(entity_type);

    if (!recipe) {
        return false;
    }

    for (int i = 0; i < CRAFTING_MAX_INGREDIENTS; i++) {
        CraftingIngredient* ingredient = &recipe->ingredients[i];
        if (ingredient->entity_type != ENTITY_TYPE_UNKNOWN &&
            !hotbar_contains_item(hotbar, ingredient->entity_type, ingredient->quantity)) {
            return false;
        }
    }

    return true;
}

void crafting_craft_item(EntityData* entity_data, EntityType entity_type, HotBar* hotbar) {
    CraftingRecipe* recipe = crafting_recipe_find(entity_type);

    ASSERT(recipe, "Attempting to craft an item without a recipe");

    if (hotbar_space_for_item(entity_type, 1, hotbar) && crafting_can_craft_item(entity_type, hotbar)) {
        for (int i = 0; i < CRAFTING_MAX_INGREDIENTS && recipe->ingredients[i].entity_type != ENTITY_TYPE_UNKNOWN;
             i++) {
            CraftingIngredient ingredient = recipe->ingredients[i];
            hotbar_remove_item(hotbar, ingredient.entity_type, ingredient.quantity);
        }

        hotbar_add_item(hotbar, entity_type, 1);
    }
}

struct InventoryInput {
    int idx_hovered;
    int idx_clicked;
};

InventoryInput inventory_render(UIElement* container, Vec2 container_position, InventoryItem* inventory_items,
                                uint32 item_count, uint32 num_rows, uint32 num_cols, GameState* game_state,
                                GameInput* game_input) {
    InventoryInput input = {-1, -1};

    ASSERT(num_rows * num_cols >= item_count, "Too many items provided based on inventory size");

    for (int row = 0; row < num_rows; row++) {
        UIElement* item_row_container =
            ui_create_container({.padding = 0, .child_gap = pixels_to_units(8), .opts = UI_ELEMENT_F_CONTAINER_ROW},
                                &game_state->frame_arena);

        ui_push(container, item_row_container);

        for (int col = 0; col < num_cols; col++) {
            uint32 item_idx = row * num_cols + col;
            InventoryItem* item = NULL;
            if (item_idx < item_count) {
                item = &inventory_items[item_idx];
            }

            Vec2 slot_size = {1, 1};

            UIElement* item_slot =
                ui_create_container({.min_size = slot_size,
                                     .max_size = slot_size,
                                     .background_rgba = COLOR_GRAY,
                                     .opts = UI_ELEMENT_F_CONTAINER_ROW | UI_ELEMENT_F_DRAW_BACKGROUND},
                                    &game_state->frame_arena);

            if (item && item->entity_type != ENTITY_TYPE_UNKNOWN) {
                Sprite sprite = entity_get_default_sprite(item->entity_type);
                UIElement* item_sprite = ui_create_sprite(sprite, &game_state->frame_arena);
                ui_push_centered(item_slot, item_sprite);
            }

            ui_push(item_row_container, item_slot);

            Vec2 slot_world_position_top_left = ui_abs_position_get(container_position, item_slot);
            Vec2 slot_world_position_center = w_rect_top_left_to_center(slot_world_position_top_left, slot_size);

            Rect slot_rect = {slot_world_position_center.x, slot_world_position_center.y, slot_size.x, slot_size.y};

            if (w_check_point_in_rect(slot_rect, game_state->world_input.mouse_position_world)) {
                item_slot->border_width = pixels_to_units(1);
                item_slot->border_color = COLOR_WHITE;

                if (item) {
                    input.idx_hovered = item_idx;

                    if (game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON].is_pressed) {
                        input.idx_clicked = item_idx;
                    }
                }
            }
        }

        ui_container_size_update(container);
    }

    return input;
}

void entity_inventory_render(Entity* entity, GameState* game_state, RenderGroup* render_group, GameInput* game_input) {
    float padding = pixels_to_units(16);
    float child_gap = pixels_to_units(8);

    // NOTE: this is assuming the slot size is 1 which is hard coded in the inventory_render. It
    // will break if that changes
    Vec2 inventory_ui_size = {
        .x = entity->inventory_cols * 1 + (child_gap * (entity->inventory_cols - 1)) + (padding * 2),
        .y = entity->inventory_rows * 1 + (child_gap * (entity->inventory_rows - 1)) + (padding * 2)};

    Vec2 inventory_ui_position = game_state->camera.position;
    inventory_ui_position.x -= (inventory_ui_size.x / 2);
    inventory_ui_position.y += (inventory_ui_size.y / 2) + 5;

    UIElement* container = ui_create_container({.padding = padding,
                                                .child_gap = child_gap,
                                                .background_rgba = COLOR_BLACK,
                                                .opts = UI_ELEMENT_F_CONTAINER_COL | UI_ELEMENT_F_DRAW_BACKGROUND},
                                               &game_state->frame_arena);

    InventoryInput inventory_input =
        inventory_render(container, inventory_ui_position, entity->inventory, entity->inventory_count,
                         entity->inventory_rows, entity->inventory_cols, game_state, game_input);

    ui_draw_element(container, inventory_ui_position, render_group);
}

void player_inventory_render(GameState* game_state, RenderGroup* render_group, GameInput* game_input) {
    Vec2 crafting_menu_size = {13, 10};
    Vec2 crafting_menu_position = game_state->camera.position;
    crafting_menu_position.x -= (game_state->camera.size.x / 4) + (crafting_menu_size.x / 2);
    crafting_menu_position.y += (crafting_menu_size.y / 2);

    UIElement* container = ui_create_container({.padding = pixels_to_units(8),
                                                .child_gap = pixels_to_units(8),
                                                .min_size = crafting_menu_size,
                                                .max_size = crafting_menu_size,
                                                .background_rgba = COLOR_BLACK,
                                                .opts = UI_ELEMENT_F_CONTAINER_COL | UI_ELEMENT_F_DRAW_BACKGROUND},
                                               &game_state->frame_arena);

    UIElement* title = ui_create_text("Crafting", COLOR_WHITE, 1.0f, &game_state->frame_arena);
    ui_push(container, title);

    InventoryItem items[ENTITY_TYPE_COUNT] = {};
    uint32 item_count = 0;

    for (int i = 0; i < ENTITY_TYPE_COUNT; i++) {
        CraftingRecipe recipe = crafting_recipes[i];

        if (recipe.entity_type != ENTITY_TYPE_UNKNOWN) {
            items[i].entity_type = recipe.entity_type;
            items[i].stack_size = 1;
            item_count++;
        }
    }

    InventoryInput inventory_input =
        inventory_render(container, crafting_menu_position, items, item_count, 2, 8, game_state, game_input);

    ui_container_size_update(container);

    if (inventory_input.idx_hovered > -1) {
        CraftingRecipe recipe = crafting_recipes[inventory_input.idx_hovered];

        for (int i = 0; i < CRAFTING_MAX_INGREDIENTS && recipe.ingredients[i].entity_type != ENTITY_TYPE_UNKNOWN; i++) {
            CraftingIngredient ingredient = recipe.ingredients[i];

            UIElement* ingredient_container = ui_create_container(
                {.child_gap = pixels_to_units(8), .opts = UI_ELEMENT_F_CONTAINER_ROW}, &game_state->frame_arena);
            UIElement* ingredient_sprite_element =
                ui_create_sprite(entity_get_default_sprite(ingredient.entity_type), &game_state->frame_arena);

            const char* entity_name_string = entity_info[ingredient.entity_type].type_name_string;
            char ingredient_text[64] = {};
            snprintf(ingredient_text, 64, "%i %s", ingredient.quantity, entity_name_string);
            UIElement* ingredient_text_element =
                ui_create_text(ingredient_text, COLOR_WHITE, 1, &game_state->frame_arena);

            ui_push(ingredient_container, ingredient_sprite_element);
            ui_push(ingredient_container, ingredient_text_element);

            ui_push(container, ingredient_container);
        }
    }

    if (inventory_input.idx_clicked > -1) {
        CraftingRecipe recipe = crafting_recipes[inventory_input.idx_hovered];
        crafting_craft_item(&game_state->entity_data, recipe.entity_type, &game_state->hotbar);
    }

    ui_draw_element(container, crafting_menu_position, render_group);
}

void debug_render_rect(Vec2 position, Vec2 size, Vec4 color) {
#ifdef DEBUG
    RenderQuad* quad = get_next_quad(g_debug_render_group);

    quad->world_position = position;
    quad->world_size = size;
    quad->draw_colored_rect = 1;
    quad->rgba = color;
    quad->z_index = ENTITY_MAX_Z + 1;
#endif
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
    if (shake->timer_s <= 0) {
        return {0, 0};
    }

    shake->timer_s = w_clamp_min(shake->timer_s - dt_s, 0);

    float decay = shake->timer_s / shake->duration_s;
    float t = (shake->duration_s - shake->timer_s) * shake->frequency;

    Vec2 shake_offset = {sinf(t * 15.23 + shake->seed) * shake->magnitude * decay,
                         cosf(t * 12.95 + shake->seed) * shake->magnitude * decay};

    return shake_offset;
}

extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render) {
    // ~~~~~~~~~~~~~~~~~~ Debug Flags ~~~~~~~~~~~~~~~~~~~~ //
    GameState* game_state = (GameState*)game_memory->memory;
    g_base_path = game_memory->base_path;

    g_sim_dt_s = frame_dt_s;

    game_state->viewport_scale_factor = get_viewport_scale_factor(game_memory->window.size_px);
    g_pixels_per_unit = BASE_PIXELS_PER_UNIT * game_state->viewport_scale_factor;

    Vec2 viewport = get_viewport(game_state->viewport_scale_factor);

    game_memory->set_viewport(viewport, game_memory->window.size_px);

    game_state->world_input.mouse_position_world =
        get_mouse_world_position(&game_state->camera, game_input, game_memory->window.size_px);
    w_init_waffle_lib(g_base_path);
    w_init_animation(animation_table);
    entity_init();

    // TODO: should background just be merged with decorations?
    RenderGroup background_render_group = {};
    background_render_group.id = RENDER_GROUP_ID_BACKGROUND;
    RenderGroup render_group_decorations = {};
    render_group_decorations.id = RENDER_GROUP_ID_DECORATIONS;
    RenderGroup main_render_group = {};
    main_render_group.id = RENDER_GROUP_ID_MAIN;
    RenderGroup render_group_ui = {};
    render_group_ui.id = RENDER_GROUP_ID_UI;
#ifdef DEBUG
    RenderGroup render_group_tools = {};
    render_group_tools.id = RENDER_GROUP_ID_TOOLS;
    RenderGroup debug_render_group = {};
    debug_render_group.id = RENDER_GROUP_ID_DEBUG;
#endif

    if (!is_set(game_state->flags, GAME_STATE_F_INITIALIZED)) {
        set(game_state->flags, GAME_STATE_F_INITIALIZED);

        game_state->main_arena.size = game_memory->size - sizeof(GameState);
        game_state->main_arena.data = (char*)game_memory->memory + sizeof(GameState);
        game_state->main_arena.next = game_state->main_arena.data;

        game_state->frame_arena.size = Megabytes(10);
        game_state->frame_arena.data = w_arena_alloc(&game_state->main_arena, game_state->frame_arena.size);
        game_state->frame_arena.next = game_state->frame_arena.data;

        game_state->camera.position = {0, 0};
        game_state->camera.size = {BASE_RESOLUTION_WIDTH / BASE_PIXELS_PER_UNIT,
                                   BASE_RESOLUTION_HEIGHT / BASE_PIXELS_PER_UNIT};
        game_state->camera.zoom = 1;

        game_memory->initialize_renderer(game_memory->window.size_px, viewport, game_state->camera.size,
                                         game_state->camera.zoom, &game_state->main_arena);
        game_memory->load_texture(TEXTURE_ID_FONT, "resources/assets/font_texture.png", &game_state->font_texture_info);
        game_memory->load_texture(TEXTURE_ID_SPRITE, "resources/assets/sprite_atlas.png",
                                  &game_state->sprite_texture_info);

        game_memory->init_audio(&game_state->audio_player);
        setup_sound_from_wav(SOUND_BACKGROUND_MUSIC, "resources/assets/background_music_1.wav", 0.60,
                             game_state->sounds, &game_state->main_arena);
        setup_sound_from_wav(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_01.wav", 0.2,
                             game_state->sounds, &game_state->main_arena);
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

        {
            char* main_marker = w_arena_marker(&game_state->main_arena);

            FileContents game_init_file_contents;
            if (w_read_file_abs(ABS_GAME_INIT_PATH, &game_init_file_contents, &game_state->main_arena) != 0) {
                w_get_absolute_path(game_state->game_init_config.default_world_init_path, ABS_PROJECT_RESOURCE_PATH,
                                    "world_0.init");

                bool result =
                    w_file_write_bin(ABS_GAME_INIT_PATH, (char*)&game_state->game_init_config, sizeof(GameInit));
                ASSERT(result == 0, "failed to open file resources/game.init");
            } else {
                memcpy(&game_state->game_init_config, game_init_file_contents.data, sizeof(GameInit));
            }

            FileContents world_init_file_contents;
            if (w_read_file_abs(game_state->game_init_config.default_world_init_path, &world_init_file_contents,
                                &game_state->main_arena) != 0) {
                game_state->world_init.world_size = {DEFAULT_WORLD_WIDTH, DEFAULT_WORLD_HEIGHT};
                game_state->world_init.entity_inits[0].type = ENTITY_TYPE_PLAYER;
                game_state->world_init.entity_inits[0].position = {0, 0};
                game_state->world_init.entity_init_count = 1;

                bool result = w_file_write_bin(game_state->game_init_config.default_world_init_path,
                                               (char*)&game_state->world_init, sizeof(WorldInit));
                ASSERT(result == 0, "failed to open file resources/world_0.init");
            } else {
                memcpy(&game_state->world_init, world_init_file_contents.data, sizeof(WorldInit));
            }

            w_arena_restore(&game_state->main_arena, main_marker);
        }

        init_entity_data(&game_state->entity_data);
        game_state->open_entity_inventory = entity_null_handle;
        hotbar_init(&game_state->hotbar);

        game_state->entity_item_spawn_info[ENTITY_TYPE_IRON_DEPOSIT] = {
            .spawned_entity_type = ENTITY_TYPE_IRON, .damage_required_to_spawn = 3.0f, .spawn_chance = 0.5f};
        game_state->entity_item_spawn_info[ENTITY_TYPE_BOAR] = {.spawned_entity_type = ENTITY_TYPE_BOAR_MEAT,
                                                                .damage_required_to_spawn = MAX_HP_BOAR,
                                                                .spawn_chance = 1.0f};
        game_state->entity_item_spawn_info[ENTITY_TYPE_PLANT_CORN] = {.spawned_entity_type = ENTITY_TYPE_ITEM_CORN,
                                                                      .damage_required_to_spawn = MAX_HP_PLANT_CORN,
                                                                      .spawn_chance = 1.0f};

        {
            WorldInitEntity* entity_inits = game_state->world_init.entity_inits;
            for (int i = 0; i < game_state->world_init.entity_init_count; i++) {
                entity_create(&game_state->entity_data, entity_inits[i].type, entity_inits[i].position);
            }
        }

        game_state->player = entity_find_first_of_type(&game_state->entity_data, ENTITY_TYPE_PLAYER);
        ASSERT(game_state->player, "Player must exist at initialization");

        FBMContext* ore_fbm_context = &game_state->world_gen_context.ore_fbm_context;

        *ore_fbm_context = {.amp = 1.0f, .octaves = 4, .freq = 0.12f, .lacunarity = 2.0f, .gain = 0.45f};

        w_perlin_seed(&ore_fbm_context->perlin_context, 12756671);

        FBMContext* plant_fbm_context = &game_state->world_gen_context.plant_fbm_context;

        *plant_fbm_context = {.amp = 1.0f, .octaves = 4, .freq = 0.2f, .lacunarity = 2.0f, .gain = 0.90f};

        w_perlin_seed(&plant_fbm_context->perlin_context, 32756671);

        proc_gen_iron_ore(&game_state->entity_data, ore_fbm_context);
        proc_gen_plants(&game_state->decoration_data, plant_fbm_context);
    }

    game_memory->set_projection(game_state->camera.size.x, game_state->camera.size.y, game_state->camera.zoom);
    init_ui(&game_state->font_data, BASE_PIXELS_PER_UNIT);
    game_state->frame_arena.next = game_state->frame_arena.data;
    game_state->frame_flags = 0;

    background_render_group.size = DEFAULT_WORLD_WIDTH * DEFAULT_WORLD_HEIGHT;
    background_render_group.quads =
        (RenderQuad*)w_arena_alloc(&game_state->frame_arena, background_render_group.size * sizeof(RenderQuad));
    render_group_decorations.size = MAX_DECORATIONS;
    render_group_decorations.quads =
        (RenderQuad*)w_arena_alloc(&game_state->frame_arena, render_group_decorations.size * sizeof(RenderQuad));
    main_render_group.size = MAX_ENTITIES * 2;
    main_render_group.quads =
        (RenderQuad*)w_arena_alloc(&game_state->frame_arena, main_render_group.size * sizeof(RenderQuad));
    render_group_ui.size = 250;
    render_group_ui.quads =
        (RenderQuad*)w_arena_alloc(&game_state->frame_arena, render_group_ui.size * sizeof(RenderQuad));

#ifdef DEBUG
    render_group_tools.size = MAX_WORLD_ENTITY_INITS + 1000;
    render_group_tools.quads =
        (RenderQuad*)w_arena_alloc(&game_state->frame_arena, render_group_tools.size * sizeof(RenderQuad));
    debug_render_group.size = MAX_ENTITIES * 2;
    debug_render_group.quads =
        (RenderQuad*)w_arena_alloc(&game_state->frame_arena, debug_render_group.size * sizeof(RenderQuad));
    g_debug_render_group = &debug_render_group;

    tools_update_and_render(game_memory, game_state, game_input, &render_group_tools);
#endif

    Entity* closest_interactable_entity =
        entity_closest_player_interactable(&game_state->entity_data, game_state->player);

    // ~~~~~~~~~~~~~~~~~~ Render decorations ~~~~~~~~~~~~~~~~~~~~~~ //
    {
        RenderQuad* ground = get_next_quad(&background_render_group);
        ground->world_position = {0, 0};
        ground->world_size = {DEFAULT_WORLD_WIDTH, DEFAULT_WORLD_HEIGHT};
        Sprite ground_sprite = sprite_table[SPRITE_GROUND_1];
        ground->sprite_position = {ground_sprite.x, ground_sprite.y};
        ground->sprite_size = {ground_sprite.w, ground_sprite.h};

        // TODO: in release, there's likely no reason to recreate quads each frame.
        // Perhaps we can have persistant render groups that don't get wiped each frame
        Decoration* decorations = game_state->decoration_data.decorations;
        for (int i = 0; i < MAX_DECORATIONS; i++) {
            if (decorations[i].type != DECORATION_TYPE_NONE) {
                Decoration* decoration = &decorations[i];
                render_sprite(decoration->position, decoration->sprite_id, &render_group_decorations, 0);
            }
        }
    }

    PlayerInput player_input = player_input_get(game_input, game_state);

    for (int i = 0; i < game_state->entity_data.entity_count; i++) {
        Entity* entity = &game_state->entity_data.entities[i];

        // ~~~~~~~~~~~~~~ Update entity intentions (brain / player input) ~~~~~~~~~~~~~~~~ //

        update_brain(entity, game_state, g_sim_dt_s);

        if (entity->type == ENTITY_TYPE_PLAYER) {
            if (entity->hp > 0) {
                entity->facing_direction = w_vec_norm(player_input.world.aim_vec);
                entity_player_movement_animation_update(entity, player_input.world.movement_vec);

                float acceleration_mag = 30;
                Vec2 acceleration = w_vec_mult(w_vec_norm(player_input.world.movement_vec), acceleration_mag);
                entity->acceleration = w_vec_add(acceleration, w_vec_mult(entity->velocity, -5.0));
            } else {
                EntityAnimations animations = entity_info[entity->type].animations;
                set(entity->flags, ENTITY_F_NONSPACIAL);
                w_play_animation(animations.death, &entity->anim_state);
                if (w_animation_complete(&entity->anim_state, g_sim_dt_s)) {
                    entity->hp = MAX_HP_PLAYER;
                    entity->position = {0, 0};
                    unset(entity->flags, ENTITY_F_NONSPACIAL);
                }
            }
        }

        Vec2 starting_position = entity->position;

        // bool has_collided = false;
        // Note: This is an optimization
        if (!is_set(entity->flags, ENTITY_F_NONSPACIAL)) {
            float t_remaining = 1.0f;
            for (int attempts = 0; attempts < 4 && t_remaining > 0.0f; attempts++) {
                float t_min = 1.0f;
                Vec2 collision_normal = {0, 0};
                Entity* entity_collided_with = NULL;

                Vec2 subject_collider_position = w_vec_add(entity->position, entity->collider.offset);
                Vec2 subject_delta = w_calc_position_delta(entity->acceleration, entity->velocity, entity->position,
                                                           t_remaining * g_sim_dt_s);

                for (int j = 0; j < game_state->entity_data.entity_count; j++) {
                    Entity* target_entity = &game_state->entity_data.entities[j];

                    if (should_collide(entity, target_entity, game_state->collision_rule_hash)) {
                        Rect subject = {subject_collider_position.x, subject_collider_position.y,
                                        entity->collider.width, entity->collider.height};

                        Vec2 target_collider_position =
                            w_vec_add(target_entity->position, target_entity->collider.offset);
                        Vec2 target_delta = w_calc_position_delta(target_entity->acceleration, target_entity->velocity,
                                                                  target_entity->position, g_sim_dt_s);

                        Rect target = {target_collider_position.x, target_collider_position.y,
                                       target_entity->collider.width, target_entity->collider.height};

                        float prev_t_min = t_min;

                        w_rect_collision(subject, subject_delta, target, target_delta, &t_min, &collision_normal);

                        if (t_min != prev_t_min) {
                            entity_collided_with = target_entity;
                        }
                    }
                }

                if (t_min < 1) {
                    t_min = w_max(0.0, t_min - 0.01); // epsilon adjustment
                }

                float t_effective = t_min * t_remaining;
                t_remaining -= t_effective;
                float effective_dt_s = t_effective * g_sim_dt_s;

                bool can_move_freely = true;
                if (entity_collided_with) {
                    handle_collision(entity, entity_collided_with, collision_normal, effective_dt_s, &can_move_freely,
                                     game_state);
                    // has_collided = true
                }

                if (can_move_freely) {
                    entity->position =
                        w_calc_position(entity->acceleration, entity->velocity, entity->position, effective_dt_s);
                    entity->velocity = w_calc_velocity(entity->acceleration, entity->velocity, effective_dt_s);
                }
            }
        } else {
            entity->position = w_calc_position(entity->acceleration, entity->velocity, entity->position, g_sim_dt_s);
            entity->velocity = w_calc_velocity(entity->acceleration, entity->velocity, g_sim_dt_s);
        }

        entity->z_pos =
            (0.5f * entity->z_acceleration * w_square(g_sim_dt_s)) + (entity->z_velocity * g_sim_dt_s) + entity->z_pos;
        entity->z_pos = w_clamp_min(entity->z_pos, 0);
        entity->z_velocity = entity->z_acceleration * g_sim_dt_s + entity->z_velocity;

        // TODO: is OWNED FLAG needed after stack implementation?
        if (is_set(entity->flags, ENTITY_F_ITEM) && !is_set(entity->flags, ENTITY_F_OWNED) &&
            !is_set(entity->flags, ENTITY_F_ITEM_SPAWNING)) {
            float distance_from_player = w_euclid_dist(game_state->player->position, entity->position);
            if (distance_from_player < 0.1 &&
                hotbar_space_for_item(entity->type, entity->stack_size, &game_state->hotbar)) {
                set(entity->flags, ENTITY_F_OWNED);
                entity->owner_handle = entity_to_handle(game_state->player, &game_state->entity_data);
                entity->velocity = {};
                entity->acceleration = {};
                hotbar_add_item(entity, &game_state->hotbar, &game_state->entity_data);
            } else if (distance_from_player < ITEM_PICKUP_RANGE &&
                       hotbar_space_for_item(entity->type, entity->stack_size, &game_state->hotbar)) {
                Vec2 player_offset = w_vec_sub(game_state->player->position, entity->position);
                Vec2 player_direction_norm = w_vec_norm(player_offset);
                float current_velocity_mag = w_vec_length(entity->velocity);
                if (current_velocity_mag == 0) {
                    current_velocity_mag = 3;
                }
                entity->velocity = w_vec_mult(player_direction_norm, current_velocity_mag);
                entity->acceleration = w_vec_mult(player_direction_norm, 15.0f);
            }

            entity->item_floating_anim_timer_s += g_sim_dt_s;
            entity->z_pos = w_anim_sine(entity->item_floating_anim_timer_s, 4.0f, 0.10f);
        }

        if (is_set(entity->flags, ENTITY_F_ITEM_SPAWNING) && entity->z_pos == 0) {
            entity->z_velocity = 0;
            entity->z_acceleration = 0;
            entity->velocity = {};
            entity->acceleration = {};
            unset(entity->flags, ENTITY_F_ITEM_SPAWNING);
        }

        entity->z_index = entity->position.y * -1;

        if (entity->type == ENTITY_TYPE_PROJECTILE) {
            Vec2 position_delta = w_vec_sub(entity->position, starting_position);
            entity->distance_traveled += w_vec_length(position_delta);

            if (entity->distance_traveled > MAX_PROJECTILE_DISTANCE) {
                set(entity->flags, ENTITY_F_MARK_FOR_DELETION);
            }
        }

        Entity* owner = entity_find(entity->owner_handle, &game_state->entity_data);
        if (is_set(entity->flags, ENTITY_F_OWNED) && owner == NULL) {
            set(entity->flags, ENTITY_F_MARK_FOR_DELETION);
        }

        EntityHandle entity_handle = entity_to_handle(entity, &game_state->entity_data);
        InventoryItem* active_hotbar_slot = hotbar_active_slot(&game_state->hotbar);

        if (entity->type == ENTITY_TYPE_PLAYER && player_input.world.drop_item && active_hotbar_slot->stack_size > 0) {
            Entity* item_entity = entity_find(active_hotbar_slot->entity_handle, &game_state->entity_data);
            if (!item_entity) {
                float z_pos, z_index;
                Vec2 item_position = entity_held_item_position(entity, &z_pos, &z_index);
                EntityHandle item_entity_handle =
                    entity_create_item(&game_state->entity_data, active_hotbar_slot->entity_type, item_position);
                item_entity = entity_find(item_entity_handle, &game_state->entity_data);
                item_entity->z_pos = z_pos;
                item_entity->z_index = z_index;
                item_entity->stack_size = active_hotbar_slot->stack_size;
            } else {
                unset(item_entity->flags, ENTITY_F_OWNED);
                item_entity->owner_handle.generation = -1;
            }

            Vec2 random_point = random_point_near_position(item_entity->position, 1, 1);
            Vec2 random_point_rel = w_vec_sub(random_point, item_entity->position);

            if ((entity->facing_direction.x > 0 && random_point_rel.x < 0) ||
                (entity->facing_direction.x < 0 && random_point_rel.x > 0)) {
                random_point_rel.x *= -1;
            }

            Vec2 velocity_direction = w_vec_norm(random_point_rel);
            float velocity_mag = w_random_between(3, 4);

            item_entity->velocity = w_vec_mult(velocity_direction, velocity_mag);

            item_entity->z_pos = 0.0001;
            item_entity->z_velocity = 10;
            item_entity->z_acceleration = -40;

            set(item_entity->flags, ENTITY_F_ITEM_SPAWNING);

            active_hotbar_slot->entity_handle.generation = -1;
            active_hotbar_slot->stack_size = 0;
            active_hotbar_slot->entity_type = ENTITY_TYPE_UNKNOWN;
        }

        // TODO: hot bar logic will have to be pulled out if we want this to allow for non-player owners
        bool is_active_hotbar_item = entity_same(entity_handle, active_hotbar_slot->entity_handle);
        if (is_active_hotbar_item) {
            if (owner) {
                Vec2 aim_vec_rel_owner = player_input.world.aim_vec;
                entity->z_pos = 0.5f;
                if (aim_vec_rel_owner.x < 0) {
                    entity->position = {owner->position.x - 0.3f, owner->position.y};
                } else {
                    entity->position = {owner->position.x + 0.3f, owner->position.y};
                }

                if (entity->type == ENTITY_TYPE_GUN) {
                    float rotation_rads = atan2(aim_vec_rel_owner.y, aim_vec_rel_owner.x);
                    entity->rotation_rads = rotation_rads;
                    Vec2 pivot = {};

                    if (aim_vec_rel_owner.x < 0) {
                        pivot = {entity->position.x + .25f, entity->position.y};
                        set(entity->flags, ENTITY_F_SPRITE_FLIP_X);
                        // Note: This fixes gun rotation when rads are negative from negative aim vector x
                        entity->rotation_rads = M_PI + entity->rotation_rads;
                    } else {
                        pivot = {entity->position.x - .25f, entity->position.y};
                        unset(entity->flags, ENTITY_F_SPRITE_FLIP_X);
                    }

                    entity->position = w_rotate_around_pivot(entity->position, pivot, entity->rotation_rads);

                    if (player_input.world.use_held_item) {
                        Vec2 velocity_unit = w_vec_unit_from_radians(rotation_rads);
                        Vec2 velocity = w_vec_mult(velocity_unit, 30.0f);
                        Vec2 projectile_position = {entity->position.x, entity->position.y + entity->z_pos};
                        EntityHandle projectile_handle = entity_create_projectile(
                            &game_state->entity_data, projectile_position, rotation_rads, velocity);
                        add_collision_rule(projectile_handle.id, entity->owner_handle.id, false, game_state);

                        play_sound_rand(&game_state->sounds[SOUND_BASIC_GUN_SHOT], &game_state->audio_player);
                        start_camera_shake(&game_state->camera, 0.1, 10, 0.08);
                    }
                }

                // TODO: maybe the facing_direction should be discrete to begin with?
                Vec2 owner_facing_direction = entity_discrete_facing_direction_4_directions(owner->facing_direction);
                if (owner_facing_direction.y > 0) {
                    entity->z_index = owner->z_index - 0.1;
                } else {
                    entity->z_index = owner->z_index + 0.1;
                }
            }
        }

        if (is_set(entity->flags, ENTITY_F_KILLABLE) && entity->hp <= 0 && entity->type != ENTITY_TYPE_PLAYER) {
            if (entity->brain.type != BRAIN_TYPE_NONE) {
                entity->brain.ai_state = AI_STATE_DEAD;
            } else {
                set(entity->flags, ENTITY_F_MARK_FOR_DELETION);
            }
        }

        entity->damage_taken_tint_cooldown_s = w_clamp_min(entity->damage_taken_tint_cooldown_s - g_sim_dt_s, 0);

        bool is_animation_complete = w_update_animation(&entity->anim_state, g_sim_dt_s);

        SpriteID sprite_id = w_animation_current_sprite(&entity->anim_state);

        Rect anim_hitbox = hitbox_table[sprite_id];

        // TODO: Maybe collapse this into the collision loop above?
        if (w_rect_has_area(anim_hitbox)) {
            anim_hitbox = w_rect_mult(anim_hitbox, 1.0 / BASE_PIXELS_PER_UNIT);

            anim_hitbox.x = entity->position.x + anim_hitbox.x;
            anim_hitbox.y = entity->position.y + anim_hitbox.y;

            Rect subject_hitbox = {
                anim_hitbox.x,
                anim_hitbox.y,
                anim_hitbox.w,
                anim_hitbox.h,
            };

            if (game_state->tools.draw_hitboxes) {
                debug_render_rect((Vec2){subject_hitbox.x, subject_hitbox.y},
                                  (Vec2){subject_hitbox.w, subject_hitbox.h}, {255, 0, 0, 0.5});
            }

            for (int j = 0; j < game_state->entity_data.entity_count; j++) {
                Entity* target_entity = &game_state->entity_data.entities[j];
                if (entity->id != target_entity->id && !is_set(entity->flags, ENTITY_F_NONSPACIAL) &&
                    !is_set(target_entity->flags, ENTITY_F_NONSPACIAL) &&
                    is_set(target_entity->flags, ENTITY_F_KILLABLE)) {

                    CollisionRule* collision_rule =
                        find_collision_rule(entity->attack_id, target_entity->id, game_state);
                    if (!collision_rule || collision_rule->should_collide) {
                        Vec2 target_collider_position =
                            w_vec_add(target_entity->position, target_entity->collider.offset);

                        Rect target = {target_collider_position.x, target_collider_position.y,
                                       target_entity->collider.width, target_entity->collider.height};

                        if (w_check_aabb_overlap(subject_hitbox, target)) {
                            entity_deal_damage(target_entity, 1, game_state);
                            add_collision_rule(entity->attack_id, target_entity->id, false, game_state);
                        }
                    }
                }
            }
        }

        if (is_set(entity->flags, ENTITY_F_DELETE_AFTER_ANIMATION) && is_animation_complete) {
            set(entity->flags, ENTITY_F_MARK_FOR_DELETION);
        }

        bool should_render = true;
        if (is_set(entity->flags, ENTITY_F_OWNED) && !is_active_hotbar_item) {
            should_render = false;
        } else if (is_set(entity->flags, ENTITY_F_MARK_FOR_DELETION)) {
            should_render = false;
        }

        if (should_render) {
            if (!is_set(entity->flags, ENTITY_F_MARK_FOR_DELETION)) {
                entity_render(entity, &main_render_group);
            }

            if (game_state->tools.draw_entity_positions) {
                debug_render_rect(entity->position, pixels_to_units({1, 1}), {0, 255, 0, 1});
            }

            if (game_state->tools.draw_colliders) {
                debug_render_entity_colliders(entity, false);
            }
        }
    }

    if (closest_interactable_entity && player_input.world.interact) {
        Entity* entity_with_open_inventory = entity_find(game_state->open_entity_inventory, &game_state->entity_data);
        if (!entity_with_open_inventory) {
            game_state->open_entity_inventory = entity_to_handle(closest_interactable_entity, &game_state->entity_data);
        } else {
            game_state->open_entity_inventory = entity_null_handle;
        }
    }

    Entity* entity_with_open_inventory = entity_find(game_state->open_entity_inventory, &game_state->entity_data);
    if (entity_with_open_inventory) {
        unset(game_state->flags, GAME_STATE_F_INVENTORY_OPEN);
        entity_inventory_render(entity_with_open_inventory, game_state, &render_group_ui, game_input);
    }

    if (is_set(game_state->flags, GAME_STATE_F_INVENTORY_OPEN)) {
        player_inventory_render(game_state, &render_group_ui, game_input);
    }

    {
        InventoryItem* active_hotbar_slot = hotbar_active_slot(&game_state->hotbar);
        EntityType active_hotbar_entity_type = active_hotbar_slot->entity_type;
        if (player_input.world.use_held_item &&
            is_set(entity_info[active_hotbar_entity_type].flags, ENTITY_INFO_F_PLACEABLE)) {
            Vec2 placement_position =
                hotbar_placeable_position(game_state->player->position, player_input.world.aim_vec);
            entity_create(&game_state->entity_data, active_hotbar_entity_type, placement_position);
            hotbar_slot_remove_item(active_hotbar_slot, 1);
        }
    }

#ifdef DEBUG
    hotbar_validate(&game_state->hotbar);
#endif

    for (int i = 0; i < game_state->entity_data.entity_count; i++) {
        Entity* entity = &game_state->entity_data.entities[i];
        if (is_set(entity->flags, ENTITY_F_MARK_FOR_DELETION)) {
            ASSERT(entity->type != ENTITY_TYPE_PLAYER, "Player entity should never be freed");
            remove_collision_rules(entity->id, game_state->collision_rule_hash, &game_state->collision_rule_free_list);
            entity_free(entity->id, &game_state->entity_data);
            // Ensures we process the element that was inserted to replace the freed entity
            i--;
        }
    }

    Vec2 camera_to_player_offset = w_vec_sub(game_state->player->position, game_state->camera.position);

    float smoothing_factor = 8;
    Vec2 camera_delta = w_vec_mult(camera_to_player_offset, smoothing_factor);
    camera_delta = w_vec_mult(camera_delta, g_sim_dt_s);
    game_state->camera.position = w_vec_add(game_state->camera.position, camera_delta);

    hotbar_render_item(game_state, player_input.world.aim_vec, &main_render_group);
    hotbar_render(game_state, &render_group_ui);
    render_player_hp_ui(game_state->player->hp, game_state->camera, &render_group_ui);
    game_memory->push_audio_samples(&game_state->audio_player);

    sort_render_group(&main_render_group);

    Vec2 shake_offset = update_and_get_camera_shake(&game_state->camera.shake, g_sim_dt_s);
    Vec2 rendered_camera_position = w_vec_add(game_state->camera.position, shake_offset);

    game_memory->push_render_group(background_render_group.quads, background_render_group.count,
                                   rendered_camera_position, background_render_group.opts);
    game_memory->push_render_group(render_group_decorations.quads, render_group_decorations.count,
                                   rendered_camera_position, render_group_decorations.opts);
    game_memory->push_render_group(main_render_group.quads, main_render_group.count, rendered_camera_position,
                                   main_render_group.opts);
    game_memory->push_render_group(render_group_ui.quads, render_group_ui.count, game_state->camera.position,
                                   render_group_ui.opts);

#ifdef DEBUG
    DebugInfo* debug_info = &game_memory->debug_info;
    double avg_rendered_frame_time_s = w_avg(
        debug_info->rendered_dt_history, w_min(debug_info->rendered_dt_history_count, FRAME_TIME_HISTORY_MAX_COUNT));
    double fps = 0;
    if (avg_rendered_frame_time_s != 0) {
        fps = 1 / avg_rendered_frame_time_s;
    }

    double avg_preswap_dt_ms = w_avg(debug_info->prebuffer_swap_dt_history,
                                     w_min(debug_info->prebuffer_swap_dt_history_count, FRAME_TIME_HISTORY_MAX_COUNT)) *
                               1000;

    char fps_str[32] = {};
    char avg_preswap_dt_str[32] = {};
    snprintf(fps_str, 32, "FPS: %.0f", w_round((float)fps));
    snprintf(avg_preswap_dt_str, 32, "Frame ms: %.3f", avg_preswap_dt_ms);

    UIElement* debug_text_container = ui_create_container(
        {.padding = 0.25, .child_gap = 0, .opts = UI_ELEMENT_F_CONTAINER_COL}, &game_state->frame_arena);
    UIElement* fps_text_element = ui_create_text(fps_str, COLOR_WHITE, 0.5, &game_state->frame_arena);
    ui_push(debug_text_container, fps_text_element);
    UIElement* frame_time_text_element = ui_create_text(avg_preswap_dt_str, COLOR_WHITE, 0.5, &game_state->frame_arena);
    ui_push(debug_text_container, frame_time_text_element);

    Vec2 camera_top_right = {game_state->camera.position.x + (game_state->camera.size.x / 2),
                             game_state->camera.position.y + (game_state->camera.size.y / 2)};

    Vec2 debug_text_container_position = {camera_top_right.x - debug_text_container->size.x, camera_top_right.y};

    ui_draw_element(debug_text_container, debug_text_container_position, &debug_render_group);

    game_memory->push_render_group(render_group_tools.quads, render_group_tools.count, game_state->camera.position,
                                   render_group_tools.opts);
    game_memory->push_render_group(debug_render_group.quads, debug_render_group.count, game_state->camera.position,
                                   debug_render_group.opts);
#endif
}
