#include "hotbar.h"

static SpriteID player_hunger_to_ui_sprite[MAX_HUNGER_PLAYER + 1] = {
    [0] = SPRITE_PLAYER_HUNGER_UI_10, [1] = SPRITE_PLAYER_HUNGER_UI_9, [2] = SPRITE_PLAYER_HUNGER_UI_8,
    [3] = SPRITE_PLAYER_HUNGER_UI_7,  [4] = SPRITE_PLAYER_HUNGER_UI_6, [5] = SPRITE_PLAYER_HUNGER_UI_5,
    [6] = SPRITE_PLAYER_HUNGER_UI_4,  [7] = SPRITE_PLAYER_HUNGER_UI_3, [8] = SPRITE_PLAYER_HUNGER_UI_2,
    [9] = SPRITE_PLAYER_HUNGER_UI_1,  [10] = SPRITE_PLAYER_HUNGER_UI_0};

static SpriteID player_hp_to_ui_sprite[MAX_HP_PLAYER + 1] = {
    [0] = SPRITE_PLAYER_HP_UI_10, [1] = SPRITE_PLAYER_HP_UI_9, [2] = SPRITE_PLAYER_HP_UI_8, [3] = SPRITE_PLAYER_HP_UI_7,
    [4] = SPRITE_PLAYER_HP_UI_6,  [5] = SPRITE_PLAYER_HP_UI_5, [6] = SPRITE_PLAYER_HP_UI_4, [7] = SPRITE_PLAYER_HP_UI_3,
    [8] = SPRITE_PLAYER_HP_UI_2,  [9] = SPRITE_PLAYER_HP_UI_1, [10] = SPRITE_PLAYER_HP_UI_0};

static void render_player_health_ui(Entity* player, Camera camera, RenderGroup* render_group) {
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

static void render_player_party_ui(Entity* player, GameState* game_state, GameInput* game_input,
                                   RenderGroup* render_group) {
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

#ifdef DEBUG
static void debug_render_stats(GameState* game_state) {
    double avg_rendered_frame_time_s =
        w_avg(g_debug_info->rendered_dt_history,
              w_min(g_debug_info->rendered_dt_history_count, FRAME_TIME_HISTORY_MAX_COUNT));
    double fps = 0;
    if (avg_rendered_frame_time_s != 0) {
        fps = 1 / avg_rendered_frame_time_s;
    }

    double avg_preswap_dt_ms =
        w_avg(g_debug_info->prebuffer_swap_dt_history,
              w_min(g_debug_info->prebuffer_swap_dt_history_count, FRAME_TIME_HISTORY_MAX_COUNT)) *
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
}
#endif

void hud_render(GameState* game_state, GameInput* game_input) {
    RenderGroup* render_group_hud = &game_state->render_groups.hud;

    hotbar_render(game_state, game_input, render_group_hud);
    render_player_health_ui(game_state->player, game_state->camera, render_group_hud);
    render_player_party_ui(game_state->player, game_state, game_input, render_group_hud);

#ifdef DEBUG
    debug_render_stats(game_state);
#endif
}
