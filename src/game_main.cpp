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
char* g_debug_project_dir;
uint32 g_pixels_per_unit;
double g_sim_dt_s;
RenderGroup* g_debug_render_group;

#define DEFAULT_WORLD_WIDTH 256
#define DEFAULT_WORLD_HEIGHT 256

Vec2 get_world_top_left_tile_position() {
    return {-DEFAULT_WORLD_WIDTH / 2 + 0.5, DEFAULT_WORLD_HEIGHT / 2 - 0.5};
}

#define DEFAULT_Z_INDEX 1.0f

struct PlayerInputAction {
    bool is_held;
    bool was_pressed;
    bool was_released;
    float held_duration_s;
};

struct PlayerInputWorld {
    Vec2 movement_vec;
    Vec2 aim_vec;
    PlayerInputAction use_held_item;
    PlayerInputAction interact;
    PlayerInputAction open_inventory;
    bool drop_item;
};

struct PlayerInputUI {
    PlayerInputAction select;
    PlayerInputAction close;
};

struct PlayerInput {
    PlayerInputWorld world;
    PlayerInputUI ui;
};

Vec2 random_point_near_position(Vec2 position, float x_range, float y_range) {
    float random_x = w_random_between(-x_range, x_range);
    float random_y = w_random_between(-y_range, y_range);
    Vec2 result = {.x = position.x + random_x, .y = position.y + random_y};

    return result;
}

#define RENDER_SPRITE_OPT_FLIP_X (1 << 0)
#define RENDER_SPRITE_OPT_TINT_SET (1 << 1)

struct RenderSpriteOptions {
    Vec4 tint;
    float rotation_rads;
    float z_index;
    Vec2 size;
    flags flags;
};

RenderQuad* render_sprite(Vec2 world_position, SpriteID sprite_id, RenderGroup* render_group,
                          RenderSpriteOptions opts) {
    ASSERT(sprite_id != SPRITE_UNKNOWN, "sprite unknown passed to render sprite");
    RenderQuad* quad = get_next_quad(render_group);
    quad->world_position = world_position;
    Sprite sprite = sprite_table[sprite_id];

    Vec2 world_size = opts.size;
    if (w_vec_length(world_size) == 0) {
        world_size = w_vec_mult((Vec2){sprite.w, sprite.h}, 1.0 / BASE_PIXELS_PER_UNIT);
    }

    quad->world_size = world_size;
    quad->sprite_position = {sprite.x, sprite.y};
    quad->sprite_size = {sprite.w, sprite.h};
    quad->rotation_rads = opts.rotation_rads;
    quad->z_index = opts.z_index;

    if (is_set(opts.flags, RENDER_SPRITE_OPT_TINT_SET)) {
        quad->tint = opts.tint;
    }

    if (is_set(opts.flags, RENDER_SPRITE_OPT_FLIP_X)) {
        quad->flip_x = 1;
    }

    return quad;
}

RenderQuad* render_sprite(Vec2 world_position, SpriteID sprite_id, RenderGroup* render_group, float z_index) {
    return render_sprite(world_position, sprite_id, render_group, {.z_index = z_index});
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

#include "entity.cpp"
#include "collision.cpp"
#include "inventory.cpp"
#include "hotbar.cpp"
#include "brain.cpp"
#include "proc_gen.cpp"
#include "crafting.cpp"

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

void player_action_update_from_input(PlayerInputAction* action, KeyInputState key_input_state, float sim_dt_s) {
    if (key_input_state.is_held) {
        action->held_duration_s += sim_dt_s;
    } else {
        action->held_duration_s = 0;
    }

    action->is_held = key_input_state.is_held;
    action->was_pressed = key_input_state.is_pressed;
    action->was_released = key_input_state.is_released;
}

void update_player_input_no_ui(GameInput* game_input, GameState* game_state, float sim_dt_s, PlayerInput* input) {
    if (game_input->active_input_type == INPUT_TYPE_KEYBOARD_MOUSE) {
        KeyInputState* key_input_states = game_input->key_input_states;
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
            input->world.drop_item = true;
        }

        input->world.movement_vec = w_vec_norm(movement_vec);

#ifdef DEBUG
        if (!is_set(game_state->tools.flags, TOOLS_F_CAPTURING_MOUSE_INPUT)) {
            player_action_update_from_input(&input->world.use_held_item,
                                            game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON], sim_dt_s);
        }
#else
        player_action_update_from_input(&input->world.use_held_item,
                                        game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON]);
#endif

        Vec2 mouse_world_position = game_state->world_input.mouse_position_world;
        Vec2 player_position = game_state->player->position;
        input->world.aim_vec = w_vec_norm(
            {mouse_world_position.x - player_position.x, mouse_world_position.y - (player_position.y + 0.5f)});

        player_action_update_from_input(&input->world.interact, key_input_states[KEY_E], sim_dt_s);
        player_action_update_from_input(&input->world.open_inventory, key_input_states[KEY_I], sim_dt_s);
    } else if (game_input->active_input_type == INPUT_TYPE_GAMEPAD) {
        GamepadState* gamepad_state = &game_input->gamepad_state;
        input->world.movement_vec = w_vec_norm(
            {gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_X], -gamepad_state->axes[GAMEPAD_AXIS_LEFT_STICK_Y]});
    }
}

void update_player_input_ui(GameInput* game_input, PlayerInput* input, float sim_dt_s) {
    KeyInputState* key_input_states = game_input->key_input_states;
    player_action_update_from_input(&input->ui.select, game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON],
                                    sim_dt_s);
    player_action_update_from_input(&input->ui.close, key_input_states[KEY_E], sim_dt_s);
}

PlayerInput player_input_get(GameInput* game_input, GameState* game_state, float sim_dt_s) {
    PlayerInput input = {};
    if (game_state->ui_mode.state == UI_STATE_NONE) {
        update_player_input_no_ui(game_input, game_state, sim_dt_s, &input);
    } else {
        update_player_input_ui(game_input, &input, sim_dt_s);
    }

    return input;
}

SpriteID player_hunger_to_ui_sprite[MAX_HUNGER_PLAYER + 1] = {
    [0] = SPRITE_PLAYER_HUNGER_UI_10, [1] = SPRITE_PLAYER_HUNGER_UI_9, [2] = SPRITE_PLAYER_HUNGER_UI_8,
    [3] = SPRITE_PLAYER_HUNGER_UI_7,  [4] = SPRITE_PLAYER_HUNGER_UI_6, [5] = SPRITE_PLAYER_HUNGER_UI_5,
    [6] = SPRITE_PLAYER_HUNGER_UI_4,  [7] = SPRITE_PLAYER_HUNGER_UI_3, [8] = SPRITE_PLAYER_HUNGER_UI_2,
    [9] = SPRITE_PLAYER_HUNGER_UI_1,  [10] = SPRITE_PLAYER_HUNGER_UI_0};

SpriteID player_hp_to_ui_sprite[MAX_HP_PLAYER + 1] = {
    [0] = SPRITE_PLAYER_HP_UI_10, [1] = SPRITE_PLAYER_HP_UI_9, [2] = SPRITE_PLAYER_HP_UI_8, [3] = SPRITE_PLAYER_HP_UI_7,
    [4] = SPRITE_PLAYER_HP_UI_6,  [5] = SPRITE_PLAYER_HP_UI_5, [6] = SPRITE_PLAYER_HP_UI_4, [7] = SPRITE_PLAYER_HP_UI_3,
    [8] = SPRITE_PLAYER_HP_UI_2,  [9] = SPRITE_PLAYER_HP_UI_1, [10] = SPRITE_PLAYER_HP_UI_0};

void render_player_health_ui(Entity* player, Camera camera, RenderGroup* render_group) {
    SpriteID hp_sprite_id = player_hp_to_ui_sprite[player->hp];
    SpriteID hunger_sprite_id = player_hunger_to_ui_sprite[player->hunger];
    Sprite hp_sprite = sprite_table[hp_sprite_id];
    Sprite hunger_sprite = sprite_table[hunger_sprite_id];

    Vec2 camera_top_left = {camera.position.x - (camera.size.x / 2), camera.position.y + (camera.size.y / 2)};

    UIElement* container =
        ui_create_container({.padding = 0.5f, .child_gap = 0.5f, .opts = UI_ELEMENT_F_CONTAINER_COL});

    UIElement* hp_sprite_element = ui_create_sprite(hp_sprite);
    UIElement* hunger_sprite_element = ui_create_sprite(hunger_sprite);

    ui_push(container, hp_sprite_element);
    ui_push(container, hunger_sprite_element);

    ui_draw_element(container, camera_top_left, render_group);
}

void render_player_party_ui(Entity* player, GameState* game_state, GameInput* game_input, RenderGroup* render_group) {
    Vec2 dimensions = {(float)player->entity_party.capacity, 1};
    float padding = pixels_to_units(8);
    float slot_gap = pixels_to_units(8);
    float scale = 2.0f;
    Vec2 ui_size = inventory_ui_get_size(dimensions, padding, slot_gap, scale);

    Camera camera = game_state->camera;
    Vec2 ui_position = {camera.position.x - (camera.size.x / 2), camera.position.y + (ui_size.y / 2)};

    UIElement* container = ui_create_container({.padding = padding,
                                                .child_gap = slot_gap,
                                                .min_size = ui_size,
                                                .max_size = ui_size,
                                                .opts = UI_ELEMENT_F_CONTAINER_COL});

    Inventory inventory = {};
    inventory.capacity = player->entity_party.capacity;
    for (int i = 0; i < inventory.capacity; i++) {
        Entity* entity = entity_find(player->entity_party.handles[i]);
        if (entity) {
            inventory.items[i].entity_type = entity->type;
            inventory.items[i].stack_size = 1;
        }
    }

    InventoryInput input = inventory_render(container, ui_position, &inventory, dimensions, game_state, game_input,
                                            {.scale = scale,
                                             .background_rgba = COLOR_BLACK,
                                             .slot_gap = slot_gap,
                                             .flags = INVENTORY_RENDER_F_SLOTS_MOUSE_DISABLED});

    ui_draw_element(container, ui_position, render_group);
}

void debug_render_entity_colliders(Entity* entity, bool has_collided) {
#ifdef DEBUG
    if (!is_set(entity->flags, ENTITY_F_NONSPACIAL)) {
        RenderQuad* quad = get_next_quad(g_debug_render_group);

        WorldCollider collider = entity_get_world_collider(entity);
        quad->world_position = collider.position;
        quad->world_size = collider.size;
        quad->draw_colored_rect = 1;
        quad->rgba = {0, 0, 255, 0.5};
        if (has_collided) {
            quad->rgba = {255, 0, 0, 0.5};
        }
        quad->z_index = entity->z_index + 1;
    }
#endif
}

void entity_command_center_ui_render(Entity* entity, GameState* game_state, RenderGroup* render_group,
                                     GameInput* game_input) {
    Vec2 inventory_ui_size = {13, 16};
    Vec2 ui_position = game_state->camera.position;
    ui_position.x += game_state->camera.size.x / 4;
    ui_position = w_rect_center_to_top_left(ui_position, inventory_ui_size);

    UIElement* container = ui_create_container({.min_size = inventory_ui_size,
                                                .max_size = inventory_ui_size,
                                                .padding = pixels_to_units(8),
                                                .child_gap = pixels_to_units(8),
                                                .background_rgba = COLOR_BLACK,
                                                .opts = UI_ELEMENT_F_CONTAINER_COL | UI_ELEMENT_F_DRAW_BACKGROUND});

    UICommandCenterTab active_tab = game_state->ui_mode.active_command_center_tab;

    UIElement* button_container = ui_create_container({.min_size = {inventory_ui_size.x, 1.25},
                                                       .max_size = {inventory_ui_size.x, 1.25},
                                                       .child_gap = pixels_to_units(8),
                                                       .opts = UI_ELEMENT_F_CONTAINER_ROW});

    ui_push(container, button_container);

    UIElement* inventory_button = ui_create_button("Inventory", ui_position, button_container);
    UIElement* structures_button = ui_create_button("Structures", ui_position, button_container);

    if (ui_button_pressed(ui_position, inventory_button)) {
        game_state->ui_mode.active_command_center_tab = UI_COMMAND_CENTER_TAB_INVENTORY;
    }

    if (ui_button_pressed(ui_position, structures_button)) {
        game_state->ui_mode.active_command_center_tab = UI_COMMAND_CENTER_TAB_STRUCTURES;
    }

    ui_container_size_update(container);

    if (active_tab == UI_COMMAND_CENTER_TAB_INVENTORY) {
        float slot_gap = pixels_to_units(8);
        Vec2 inventory_dimensions = inventory_get_dimensions_from_capacity(entity->inventory.capacity, 8);
        InventoryInput inventory_input =
            inventory_render(container, ui_position, &entity->inventory, inventory_dimensions, game_state, game_input,
                             {.scale = 1, .slot_gap = slot_gap, .background_rgba = COLOR_GRAY});

        if (inventory_input.idx_clicked > -1) {
            InventoryItem slot = entity->inventory.items[inventory_input.idx_clicked];
            if (slot.entity_type != ENTITY_TYPE_UNKNOWN) {
                inventory_move_items(inventory_input.idx_clicked, slot.stack_size, &entity->inventory,
                                     &game_state->player->inventory);
            }
        }
    } else if (active_tab == UI_COMMAND_CENTER_TAB_STRUCTURES) {
        uint32 structure_slot_count = 10;
        Vec2 structure_inventory_dimensions = inventory_get_dimensions_from_capacity(structure_slot_count, 5);

        Inventory structure_inventory = recipes_to_inventory(CRAFTING_RECIPE_BOOK_STRUCTURES, structure_slot_count);

        InventoryInput inventory_input = inventory_render(container, ui_position, &structure_inventory,
                                                          structure_inventory_dimensions, game_state, game_input,
                                                          {.scale = 2,
                                                           .slot_gap = pixels_to_units(4),
                                                           .background_rgba = COLOR_GRAY,
                                                           .recipe_book_type = CRAFTING_RECIPE_BOOK_STRUCTURES,
                                                           .flags = INVENTORY_RENDER_F_FOR_CRAFTING,
                                                           .crafting_from_inventory = &entity->inventory});

        if (inventory_input.idx_clicked >= 0) {
            EntityType selected_entity_type = structure_inventory.items[inventory_input.idx_clicked].entity_type;
            if (selected_entity_type != ENTITY_TYPE_UNKNOWN &&
                crafting_can_craft_item(CRAFTING_RECIPE_BOOK_STRUCTURES, selected_entity_type, &entity->inventory)) {
                game_state->ui_mode.placing_structure_type = selected_entity_type;
                game_state->ui_mode.state = UI_STATE_STRUCTURE_PLACEMENT;
                game_state->ui_mode.camera_position = entity->position;
                game_state->ui_mode.camera_zoom = 0.5f;
                set(game_state->ui_mode.flags, UI_MODE_F_CAMERA_OVERRIDE);
            }
        }

        if (inventory_input.idx_hovered >= 0) {
            crafting_recipe_info_render(container, inventory_ui_size, CRAFTING_RECIPE_BOOK_STRUCTURES,
                                        inventory_input.idx_hovered);
        }
    }
    ui_draw_element(container, ui_position, render_group);
};

void entity_command_center_placement_ui_update_render(GameState* game_state, PlayerInput* player_input,
                                                      RenderGroup* render_group) {
    UIMode* ui_mode = &game_state->ui_mode;
    set(ui_mode->flags, UI_MODE_F_CAMERA_OVERRIDE);
    Entity* command_center = entity_find(ui_mode->entity_handle);
    ui_mode->camera_position = command_center->position;

    Vec2 mouse_world_position = game_state->world_input.mouse_position_world;
    SpriteID structure_sprite_id = entity_info[ui_mode->placing_structure_type].default_sprite;
    Sprite structure_sprite = sprite_table[structure_sprite_id];

    bool valid_placement = true;
    Collider subject_collider = entity_get_collider(ui_mode->placing_structure_type);

    Rect subject_rect = {.x = mouse_world_position.x,
                         .y = mouse_world_position.y,
                         .w = subject_collider.size.x,
                         .h = subject_collider.size.y};

    for (int i = 0; i < game_state->entity_data.entity_count; i++) {
        Entity* entity = &game_state->entity_data.entities[i];
        WorldCollider target_collider = entity_get_world_collider(entity);

        Rect target_rect = {.x = target_collider.position.x,
                            .y = target_collider.position.y,
                            .w = target_collider.size.x,
                            .h = target_collider.size.y};

        if (w_check_aabb_overlap(subject_rect, target_rect)) {
            valid_placement = false;
        }
    }

    Vec4 tint = {1, 1, 1, 0.3};

    if (!valid_placement) {
        tint.x += 3;
    }

    render_sprite(mouse_world_position, structure_sprite_id, render_group,
                  {.tint = tint, .flags = RENDER_SPRITE_OPT_TINT_SET});

    if (player_input->ui.select.was_pressed && valid_placement &&
        crafting_can_craft_item(CRAFTING_RECIPE_BOOK_STRUCTURES, ui_mode->placing_structure_type,
                                &command_center->inventory)) {
        crafting_consume_ingredients(&command_center->inventory, CRAFTING_RECIPE_BOOK_STRUCTURES,
                                     ui_mode->placing_structure_type);
        entity_create(ui_mode->placing_structure_type,
                      {mouse_world_position.x, mouse_world_position.y - (pixels_to_units(structure_sprite.h) / 2)});
    }
}

void entity_robot_interact_ui_render(Entity* entity, GameState* game_state, RenderGroup* render_group,
                                     GameInput* game_input) {
    float padding = pixels_to_units(16);
    float slot_gap = pixels_to_units(8);

    Vec2 inventory_ui_size = {8, 8};
    Vec2 inventory_dimensions = {2, 2};

    ASSERT(inventory_dimensions.x * inventory_dimensions.y == entity->inventory.capacity,
           "entity_robot_interact_ui_render dimensions much match capacity");

    Vec2 inventory_ui_position = game_state->camera.position;
    inventory_ui_position.x -= (inventory_ui_size.x / 2);
    inventory_ui_position.y += (inventory_ui_size.y / 2) + 5;

    UIElement* container = ui_create_container({.min_size = inventory_ui_size,
                                                .max_size = inventory_ui_size,
                                                .padding = padding,
                                                .child_gap = slot_gap,
                                                .background_rgba = COLOR_BLACK,
                                                .opts = UI_ELEMENT_F_CONTAINER_COL | UI_ELEMENT_F_DRAW_BACKGROUND});

    InventoryInput input =
        inventory_render(container, inventory_ui_position, &entity->inventory, inventory_dimensions, game_state,
                         game_input, {.scale = 1, .slot_gap = slot_gap, .background_rgba = COLOR_GRAY});

    UIElement* start_button = ui_create_button("Start", inventory_ui_position, container);

    if (ui_button_pressed(inventory_ui_position, start_button)) {
        entity->brain.ai_state = AI_STATE_SEARCHING;
    }

    ui_draw_element(container, inventory_ui_position, render_group);

    if (input.idx_clicked > -1) {
        InventoryItem slot = entity->inventory.items[input.idx_clicked];
        if (slot.entity_type != ENTITY_TYPE_UNKNOWN) {
            inventory_move_items(input.idx_clicked, slot.stack_size, &entity->inventory,
                                 &game_state->player->inventory);
        }
    }
}

void entity_inventory_render(Entity* entity, GameState* game_state, RenderGroup* render_group, GameInput* game_input) {
    float padding = pixels_to_units(16);
    float slot_gap = pixels_to_units(8);

    Vec2 inventory_dimensions = inventory_get_dimensions_from_capacity(entity->inventory.capacity, 4);
    Vec2 inventory_ui_size = inventory_ui_get_size(inventory_dimensions, padding, slot_gap, 1);

    Vec2 inventory_ui_position = game_state->camera.position;
    inventory_ui_position.x -= (inventory_ui_size.x / 2);
    inventory_ui_position.y += (inventory_ui_size.y / 2) + 5;

    UIElement* container = ui_create_container({.padding = padding,
                                                .child_gap = slot_gap,
                                                .background_rgba = COLOR_BLACK,
                                                .opts = UI_ELEMENT_F_CONTAINER_COL | UI_ELEMENT_F_DRAW_BACKGROUND});

    InventoryInput input =
        inventory_render(container, inventory_ui_position, &entity->inventory, inventory_dimensions, game_state,
                         game_input, {.scale = 1, .slot_gap = slot_gap, .background_rgba = COLOR_GRAY});

    ui_draw_element(container, inventory_ui_position, render_group);

    if (input.idx_clicked > -1) {
        InventoryItem slot = entity->inventory.items[input.idx_clicked];
        if (slot.entity_type != ENTITY_TYPE_UNKNOWN) {
            inventory_move_items(input.idx_clicked, slot.stack_size, &entity->inventory,
                                 &game_state->player->inventory);
        }
    }
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
                                                .opts = UI_ELEMENT_F_CONTAINER_COL | UI_ELEMENT_F_DRAW_BACKGROUND});

    UIElement* title = ui_create_text("Crafting", {.rgba = COLOR_WHITE, .font_scale = 1.0f});
    ui_push(container, title);

    Vec2 inventory_dimensions = {2, 6};
    uint32 inventory_capacity = inventory_dimensions.x * inventory_dimensions.y;

    Inventory inventory = recipes_to_inventory(CRAFTING_RECIPE_BOOK_GENERAL, inventory_capacity);

    InventoryInput inventory_input =
        inventory_render(container, crafting_menu_position, &inventory, inventory_dimensions, game_state, game_input,
                         {.scale = 1,
                          .slot_gap = pixels_to_units(8),
                          .background_rgba = COLOR_GRAY,
                          .recipe_book_type = CRAFTING_RECIPE_BOOK_GENERAL,
                          .flags = INVENTORY_RENDER_F_FOR_CRAFTING,
                          .crafting_from_inventory = &game_state->player->inventory});

    ui_container_size_update(container);

    if (inventory_input.idx_hovered > -1) {
        crafting_recipe_info_render(container, crafting_menu_size, CRAFTING_RECIPE_BOOK_GENERAL,
                                    inventory_input.idx_hovered);
    }

    if (inventory_input.idx_clicked > -1) {
        CraftingRecipe* recipe = crafting_recipe_find(CRAFTING_RECIPE_BOOK_GENERAL, inventory_input.idx_clicked);
        if (recipe) {
            crafting_craft_item(&game_state->entity_data, recipe->entity_type, CRAFTING_RECIPE_BOOK_GENERAL,
                                &game_state->player->inventory);
        }
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

bool debug_entity_is_penetrating_blocker(Entity* entity, EntityData* entity_data) {
    WorldCollider subject_collider = entity_get_world_collider(entity);
    Rect subject = {subject_collider.position.x, subject_collider.position.y, subject_collider.size.x,
                    subject_collider.size.y};

    for (int i = 0; i < entity_data->entity_count; i++) {
        Entity* target = &entity_data->entities[i];
        if (is_set(target->flags, ENTITY_F_BLOCKER) && target->id != entity->id) {
            WorldCollider target_collider = entity_get_world_collider(target);
            Rect target_rect = {target_collider.position.x, target_collider.position.y, target_collider.size.x,
                                target_collider.size.y};
            if (w_check_aabb_overlap(subject, target_rect)) {
                return true;
            }
        }
    }

    return false;
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
    entity_init(&game_state->entity_data);
    crafting_init();

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
        setup_sound_from_wav(SOUND_SONG_JAN_7, "resources/assets/song_jan_7.wav", 0.80, game_state->sounds,
                             &game_state->main_arena);
        setup_sound_from_wav(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_01.wav", 0.2,
                             game_state->sounds, &game_state->main_arena);
        add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_02.wav", game_state);
        add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_03.wav", game_state);
        add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_04.wav", game_state);
        add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_05.wav", game_state);
        add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_06.wav", game_state);
        add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_07.wav", game_state);
        add_sound_variant(SOUND_BASIC_GUN_SHOT, "resources/assets/handgun_sci-fi_a_shot_single_08.wav", game_state);

        // play_sound(&game_state->sounds[SOUND_SONG_JAN_7], &game_state->audio_player);

        // TODO: The arena marker / restore interface can be improved. Some way to defer?
        char* arena_marker = w_arena_marker(&game_state->main_arena);
        FileContents font_data_file_contents;
        w_read_file("resources/assets/font_data", &font_data_file_contents, &game_state->main_arena);
        memcpy(&game_state->font_data, font_data_file_contents.data, sizeof(FontData));
        w_arena_restore(&game_state->main_arena, arena_marker);

        {
            char* main_marker = w_arena_marker(&game_state->main_arena);

            FileContents game_init_file_contents;
            char game_init_file_path[PATH_MAX] = {};
            w_get_absolute_path(game_init_file_path, g_base_path, "../resources/game.init");
            if (w_read_file_abs(game_init_file_path, &game_init_file_contents, &game_state->main_arena) != 0) {
                w_str_copy(game_state->game_init_config.default_world_init_path, "../resources/world_0.init");

                bool result =
                    w_file_write_bin(game_init_file_path, (char*)&game_state->game_init_config, sizeof(GameInit));
                ASSERT(result == 0, "failed to open file resources/game.init");
            } else {
                memcpy(&game_state->game_init_config, game_init_file_contents.data, sizeof(GameInit));
            }

            FileContents world_init_file_contents;
            char world_init_file_path[PATH_MAX] = {};
            w_get_absolute_path(world_init_file_path, g_base_path,
                                game_state->game_init_config.default_world_init_path);
            if (w_read_file_abs(world_init_file_path, &world_init_file_contents, &game_state->main_arena) != 0) {
                game_state->world_init.world_size = {DEFAULT_WORLD_WIDTH, DEFAULT_WORLD_HEIGHT};
                game_state->world_init.entity_inits[0].type = ENTITY_TYPE_PLAYER;
                game_state->world_init.entity_inits[0].position = {0, 0};
                game_state->world_init.entity_init_count = 1;

                bool result = w_file_write_bin(world_init_file_path, (char*)&game_state->world_init, sizeof(WorldInit));
                ASSERT(result == 0, "failed to open file resources/world_0.init");
            } else {
                memcpy(&game_state->world_init, world_init_file_contents.data, sizeof(WorldInit));
            }

            w_arena_restore(&game_state->main_arena, main_marker);
        }

        init_entity_data(&game_state->entity_data);
        tools_init(&game_state->tools);
        Entity* command_center = NULL;

        {
            WorldInitEntity* entity_inits = game_state->world_init.entity_inits;
            for (int i = 0; i < game_state->world_init.entity_init_count; i++) {
                EntityHandle handle = entity_create(entity_inits[i].type, entity_inits[i].position);
                if (entity_inits[i].type == ENTITY_TYPE_LANDING_POD_YELLOW) {
                    command_center = entity_find(handle);
                }
            }
        }

        game_state->player = entity_find_first_of_type(ENTITY_TYPE_PLAYER);
        ASSERT(game_state->player, "Player must exist at initialization");

        game_state->world_seed = 12756671;

        proc_gen_init_chunk_states({DEFAULT_WORLD_WIDTH, DEFAULT_WORLD_HEIGHT}, &game_state->chunk_spawn,
                                   game_state->world_seed);

        game_state->world_gen_context.plant_fbm_context = {.amp = 1.0f,
                                                           .octaves = 4,
                                                           .freq = 0.2f,
                                                           .lacunarity = 2.0f,
                                                           .gain = 0.90f,
                                                           .seed = w_fmix32(game_state->world_seed ^ SEED_PLANT)};

        proc_gen_plants(&game_state->decoration_data, &game_state->world_gen_context.plant_fbm_context);

        if (command_center) {
            inventory_add_item(&command_center->inventory, ENTITY_TYPE_IRON, 99);
            inventory_add_item(&command_center->inventory, ENTITY_TYPE_COAL, 99);
        }
    }

    init_ui(&game_state->font_data, BASE_PIXELS_PER_UNIT, &game_state->frame_arena, &game_state->world_input,
            game_input);
    game_state->frame_arena.next = game_state->frame_arena.data;
    game_state->frame_flags = 0;

    memset(&game_state->render_groups, 0, sizeof(RenderGroups));
    game_state->render_groups.hud.id = RENDER_GROUP_ID_HUD;
    game_state->render_groups.hud.size = 250;
    game_state->render_groups.hud.quads =
        (RenderQuad*)w_arena_alloc(&game_state->frame_arena, game_state->render_groups.hud.size * sizeof(RenderQuad));

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

    Entity* closest_interactable_entity = entity_closest_player_interactable(game_state->player);

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

    PlayerInput player_input = player_input_get(game_input, game_state, g_sim_dt_s);

    for (int i = 0; i < game_state->entity_data.entity_count; i++) {
        Entity* entity = &game_state->entity_data.entities[i];

        // ~~~~~~~~~~~~~~ Update entity intentions (brain / player input) ~~~~~~~~~~~~~~~~ //

        brain_update(entity, game_state, &player_input, g_sim_dt_s);

        Vec2 starting_position = entity->position;

        // bool has_collided = false;
        // Note: This is an optimization
        if (!is_set(entity->flags, ENTITY_F_NONSPACIAL)) {
            double t_remaining = 1.0f;
            for (int attempts = 0; attempts < 4 && t_remaining > 0.0f; attempts++) {
                double t_min = 1.0f;
                Vec2 collision_normal = {0, 0};
                Entity* entity_collided_with = NULL;

                WorldCollider subject_collider = entity_get_world_collider(entity);
                Vec2 subject_delta = w_calc_position_delta(entity->acceleration, entity->velocity,
                                                           subject_collider.position, t_remaining * g_sim_dt_s);

                for (int j = 0; j < game_state->entity_data.entity_count; j++) {
                    Entity* target_entity = &game_state->entity_data.entities[j];

                    if (should_collide(entity, target_entity, game_state->collision_rule_hash)) {
                        Rect subject = {subject_collider.position.x, subject_collider.position.y,
                                        subject_collider.size.x, subject_collider.size.y};

                        WorldCollider target_collider = entity_get_world_collider(target_entity);
                        Vec2 target_delta = w_calc_position_delta(target_entity->acceleration, target_entity->velocity,
                                                                  target_collider.position, g_sim_dt_s);

                        Rect target = {target_collider.position.x, target_collider.position.y, target_collider.size.x,
                                       target_collider.size.y};

                        double prev_t_min = t_min;

                        w_rect_collision(subject, subject_delta, target, target_delta, &t_min, &collision_normal);

                        if (t_min != prev_t_min) {
                            entity_collided_with = target_entity;
                        }
                    }
                }

                if (t_min < 1) {
                    t_min = w_max(0.0, t_min - 0.8); // epsilon adjustment
                }

                double t_effective = t_min * t_remaining;
                t_remaining -= t_effective;
                double effective_dt_s = t_effective * g_sim_dt_s;

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

        if (is_set(entity->flags, ENTITY_F_ITEM) && !is_set(entity->flags, ENTITY_F_OWNED) &&
            entity->item_drop_pickup_cooldown_s <= 0) {
            Entity* collector = NULL;
            float distance_from_collector = ITEM_PICKUP_RANGE;

            for (int i = 0; i < game_state->entity_data.entity_count; i++) {
                Entity* possible_collector = &game_state->entity_data.entities[i];

                if (is_set(possible_collector->flags, ENTITY_F_COLLECTS_ITEMS)) {
                    float distance = w_euclid_dist(possible_collector->position, entity->position);

                    if (distance <= distance_from_collector) {
                        distance_from_collector = distance;
                        collector = possible_collector;
                    }
                }
            }

            if (collector) {
                if (distance_from_collector < 0.1 &&
                    inventory_space_for_item(entity->type, entity->stack_size, &game_state->player->inventory)) {
                    set(entity->flags, ENTITY_F_OWNED);
                    entity->owner_handle = entity_to_handle(game_state->player);
                    entity->velocity = {};
                    entity->acceleration = {};
                    Inventory* target_inventory = &collector->inventory;
                    if (collector->type == ENTITY_TYPE_PLAYER) {
                        target_inventory = &game_state->player->inventory;
                    }
                    inventory_add_entity_item(target_inventory, entity);
                } else if (distance_from_collector < ITEM_PICKUP_RANGE &&
                           inventory_space_for_item(entity->type, entity->stack_size, &game_state->player->inventory)) {
                    Vec2 collector_offset = w_vec_sub(collector->position, entity->position);
                    Vec2 collector_direction_norm = w_vec_norm(collector_offset);
                    float current_velocity_mag = w_vec_length(entity->velocity);
                    if (current_velocity_mag == 0) {
                        current_velocity_mag = 3;
                    }
                    entity->velocity = w_vec_mult(collector_direction_norm, current_velocity_mag);
                    entity->acceleration = w_vec_mult(collector_direction_norm, 15.0f);
                }
            }

            if (!is_set(entity->flags, ENTITY_F_ITEM_SPAWNING)) {
                entity->item_floating_anim_timer_s += g_sim_dt_s;
                entity->z_pos = w_anim_sine(entity->item_floating_anim_timer_s, 4.0f, 0.10f);
            }
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

        Entity* owner = entity_find(entity->owner_handle);
        if (is_set(entity->flags, ENTITY_F_OWNED) && owner == NULL) {
            set(entity->flags, ENTITY_F_MARK_FOR_DELETION);
        }

        EntityHandle entity_handle = entity_to_handle(entity);
        InventoryItem* active_hotbar_slot =
            hotbar_active_slot(&game_state->player->inventory, game_state->hotbar.active_item_idx);

        if (entity->type == ENTITY_TYPE_PLAYER && player_input.world.drop_item && active_hotbar_slot->stack_size > 0) {
            float z_index;
            Vec3 item_position_3d = {};
            Vec2 item_position = entity_held_item_position(entity, &item_position_3d.z, &z_index);
            item_position_3d.x = item_position.x;
            item_position_3d.y = item_position.y;

            entity_inventory_spawn_world_item(active_hotbar_slot, item_position_3d, z_index,
                                              entity->facing_direction.x);
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

                    if (player_input.world.use_held_item.was_pressed) {
                        Vec2 velocity_unit = w_vec_unit_from_radians(rotation_rads);
                        Vec2 velocity = w_vec_mult(velocity_unit, 30.0f);
                        Vec2 projectile_position = {entity->position.x, entity->position.y + entity->z_pos};
                        EntityHandle projectile_handle =
                            entity_create_projectile(projectile_position, rotation_rads, velocity);
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

        entity->damage_taken_tint_cooldown_s = w_clamp_min(entity->damage_taken_tint_cooldown_s - g_sim_dt_s, 0);
        entity->item_drop_pickup_cooldown_s = w_clamp_min(entity->item_drop_pickup_cooldown_s - g_sim_dt_s, 0);

        if (is_set(entity->flags, ENTITY_F_GETS_HUNGERY) && !game_state->tools.disable_hunger) {
            entity->hunger_cooldown_s = w_clamp_min(entity->hunger_cooldown_s - g_sim_dt_s, 0);
            if (entity->hunger_cooldown_s <= 0) {
                entity->hunger = w_clamp_min((int)entity->hunger - 1, 0);
                entity->hunger_cooldown_s = HUNGER_TICK_COOLDOWN_S;
            }

            if (entity->hunger <= 0) {
                entity->hp = 0;
            }
        }

        entity_death(entity);

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
                        WorldCollider target_collider = entity_get_world_collider(target_entity);

                        Rect target = {target_collider.position.x, target_collider.position.y, target_collider.size.x,
                                       target_collider.size.y};

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

    proc_gen_update_chunk_states(game_state->player->position, game_state, g_sim_dt_s);

    unset(game_state->ui_mode.flags, UI_MODE_F_INVENTORY_ACTIVE);
    unset(game_state->ui_mode.flags, UI_MODE_F_CAMERA_OVERRIDE);

    if (player_input.world.open_inventory.was_pressed) {
        game_state->ui_mode.state = UI_STATE_PLAYER_INVENTORY;
        game_state->ui_mode.entity_handle = entity_to_handle(game_state->player);
    } else if (player_input.world.interact.was_pressed && closest_interactable_entity) {
        game_state->ui_mode.state = UI_STATE_ENTITY_UI;
        game_state->ui_mode.entity_handle = entity_to_handle(closest_interactable_entity);
    }

    if (player_input.ui.close.was_pressed) {
        game_state->ui_mode.state = UI_STATE_NONE;
        game_state->ui_mode.entity_handle = entity_null_handle;
        game_state->ui_mode.camera_position = {};
        game_state->ui_mode.camera_zoom = 1.0f;
        game_state->ui_mode.placing_structure_type = ENTITY_TYPE_UNKNOWN;
        game_state->ui_mode.active_command_center_tab = UI_COMMAND_CENTER_TAB_INVENTORY;
    }

    if (game_state->ui_mode.state == UI_STATE_ENTITY_UI) {
        Entity* ui_entity = entity_find(game_state->ui_mode.entity_handle);
        if (ui_entity->inventory.capacity > 0) {
            set(game_state->ui_mode.flags, UI_MODE_F_INVENTORY_ACTIVE);
        }
        if (ui_entity->type == ENTITY_TYPE_ROBOT_GATHERER) {
            entity_robot_interact_ui_render(ui_entity, game_state, &render_group_ui, game_input);
        } else if (ui_entity->type == ENTITY_TYPE_LANDING_POD_YELLOW) {
            entity_command_center_ui_render(ui_entity, game_state, &render_group_ui, game_input);
        } else {
            entity_inventory_render(ui_entity, game_state, &render_group_ui, game_input);
        }
    } else if (game_state->ui_mode.state == UI_STATE_STRUCTURE_PLACEMENT) {
        entity_command_center_placement_ui_update_render(game_state, &player_input, &render_group_ui);
    } else if (game_state->ui_mode.state == UI_STATE_PLAYER_INVENTORY) {
        player_inventory_render(game_state, &render_group_ui, game_input);
    }

    {
        InventoryItem* active_hotbar_slot =
            hotbar_active_slot(&game_state->player->inventory, game_state->hotbar.active_item_idx);
        EntityType active_hotbar_entity_type = active_hotbar_slot->entity_type;
        if (player_input.world.use_held_item.is_held) {
            EntityInfo* e_info = &entity_info[active_hotbar_entity_type];
            if (is_set(e_info->flags, ENTITY_INFO_F_PLACEABLE) && player_input.world.use_held_item.was_pressed) {
                Vec2 placement_position =
                    hotbar_placeable_position(game_state->player->position, player_input.world.aim_vec);
                entity_create(active_hotbar_entity_type, placement_position);
                inventory_remove_items_by_index(&game_state->player->inventory, game_state->hotbar.active_item_idx, 1);
            } else if (is_set(e_info->flags, ENTITY_INFO_F_FOOD) && player_input.world.use_held_item.was_pressed) {
                game_state->player->hunger =
                    w_clamp_max(game_state->player->hunger + e_info->hunger_gain, MAX_HUNGER_PLAYER);
                inventory_remove_items_by_index(&game_state->player->inventory, game_state->hotbar.active_item_idx, 1);
            }
        }
    }

#ifdef DEBUG
    hotbar_validate(&game_state->player->inventory);
#endif

    for (int i = 0; i < game_state->entity_data.entity_count; i++) {
        Entity* entity = &game_state->entity_data.entities[i];
        if (is_set(entity->flags, ENTITY_F_MARK_FOR_DELETION)) {
            ASSERT(entity->type != ENTITY_TYPE_PLAYER, "Player entity should never be freed");
            remove_collision_rules(entity->id, game_state->collision_rule_hash, &game_state->collision_rule_free_list);
            entity_free(entity->id);
            // Ensures we process the element that was inserted to replace the freed entity
            i--;
        }
    }

    Vec2 camera_target_position = game_state->player->position;
    float camera_target_zoom = 1.0f;

    if (is_set(game_state->ui_mode.flags, UI_MODE_F_CAMERA_OVERRIDE)) {
        camera_target_position = game_state->ui_mode.camera_position;
        camera_target_zoom = game_state->ui_mode.camera_zoom;
    } else {
#ifdef DEBUG
        camera_target_zoom = game_state->tools.camera_zoom;
#endif
    }

    float smoothing_factor = 8;
    Vec2 camera_to_target_offset = w_vec_sub(camera_target_position, game_state->camera.position);
    Vec2 camera_delta = w_vec_mult(camera_to_target_offset, smoothing_factor);
    camera_delta = w_vec_mult(camera_delta, g_sim_dt_s);
    game_state->camera.position = w_vec_add(game_state->camera.position, camera_delta);

    float camera_zoom_offset = camera_target_zoom - game_state->camera.zoom;
    game_state->camera.zoom += camera_zoom_offset * smoothing_factor * g_sim_dt_s;

    hotbar_render_item(game_state, player_input.world.aim_vec, &main_render_group);
    hotbar_render(game_state, game_input, &game_state->render_groups.hud);
    render_player_health_ui(game_state->player, game_state->camera, &game_state->render_groups.hud);
    render_player_party_ui(game_state->player, game_state, game_input, &game_state->render_groups.hud);
    game_memory->push_audio_samples(&game_state->audio_player);

    sort_render_group(&main_render_group);

    Vec2 shake_offset = update_and_get_camera_shake(&game_state->camera.shake, g_sim_dt_s);
    Vec2 rendered_camera_position = w_vec_add(game_state->camera.position, shake_offset);

    game_memory->set_projection(game_state->camera.size.x, game_state->camera.size.y, game_state->camera.zoom);

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

    UIElement* debug_text_container =
        ui_create_container({.padding = 0.25, .child_gap = 0, .opts = UI_ELEMENT_F_CONTAINER_COL});
    UIElement* fps_text_element = ui_create_text(fps_str, {.rgba = COLOR_WHITE, .font_scale = 0.5});
    ui_push(debug_text_container, fps_text_element);
    UIElement* frame_time_text_element = ui_create_text(avg_preswap_dt_str, {.rgba = COLOR_WHITE, .font_scale = 0.5});
    ui_push(debug_text_container, frame_time_text_element);

    Vec2 camera_top_right = {game_state->camera.position.x + (game_state->camera.size.x / 2),
                             game_state->camera.position.y + (game_state->camera.size.y / 2)};

    Vec2 debug_text_container_position = {camera_top_right.x - debug_text_container->size.x, camera_top_right.y};

    // Note: technically, maybe this shouldn't be in hud? Maybe we should have a debug hud or something?
    ui_draw_element(debug_text_container, debug_text_container_position, &game_state->render_groups.hud);

    game_memory->push_render_group(render_group_tools.quads, render_group_tools.count, game_state->camera.position,
                                   render_group_tools.opts);
    game_memory->push_render_group(debug_render_group.quads, debug_render_group.count, game_state->camera.position,
                                   debug_render_group.opts);
#endif
    game_memory->set_projection(game_state->camera.size.x, game_state->camera.size.y, 1.0f);
    game_memory->set_projection(game_state->camera.size.x, game_state->camera.size.y, 1.0f);

    game_memory->push_render_group(game_state->render_groups.hud.quads, game_state->render_groups.hud.size,
                                   game_state->camera.position, game_state->render_groups.hud.opts);

    game_memory->push_render_group(game_state->render_groups.hud.quads, game_state->render_groups.hud.size,
                                   game_state->camera.position, game_state->render_groups.hud.opts);
}
