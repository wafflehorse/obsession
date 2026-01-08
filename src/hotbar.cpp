#include "game.h"

void hotbar_init(HotBar* hotbar) {
    hotbar->inventory.row_count = 1;
    hotbar->inventory.col_count = HOTBAR_MAX_SLOTS;

    inventory_init(&hotbar->inventory, HOTBAR_MAX_SLOTS);
}

InventoryItem* hotbar_active_slot(HotBar* hotbar) {
    return &hotbar->inventory.items[hotbar->active_item_idx];
}

Vec2 hotbar_placeable_position(Vec2 player_position, Vec2 aim_vec) {
    Vec2 placement_position = player_position;
    float placement_offset = 1.0f;

    if (aim_vec.y >= aim_vec.x && aim_vec.y >= -aim_vec.x) {
        placement_position.y += placement_offset;
    } else if (aim_vec.y <= aim_vec.x && aim_vec.y <= -aim_vec.x) {
        placement_position.y -= placement_offset;
    } else if (aim_vec.y <= aim_vec.x && aim_vec.y >= -aim_vec.x) {
        placement_position.x += placement_offset;
    } else {
        placement_position.x -= placement_offset;
    }

    return placement_position;
}

void hotbar_render_placement_indicator(Vec2 player_position, Vec2 aim_vec, RenderGroup* render_group) {
    Vec2 placement_position = hotbar_placeable_position(player_position, aim_vec);
    Vec2 placement_indicator_size = {1, 1};
    render_borders(pixels_to_units(1), COLOR_WHITE, placement_position, placement_indicator_size, render_group);
}

void hotbar_render_item(GameState* game_state, Vec2 player_aim_vec, RenderGroup* render_group) {
    Entity* player = game_state->player;
    HotBar* hotbar = &game_state->hotbar;

    InventoryItem* slot = &hotbar->inventory.items[hotbar->active_item_idx];

    Entity* slot_entity = entity_find(slot->entity_handle);
    if (!slot_entity && slot->stack_size > 0) {
        float z_pos, z_index;
        Vec2 item_position = entity_held_item_position(player, &z_pos, &z_index);

        SpriteID sprite_id = entity_info[slot->entity_type].default_sprite;
        item_position.y += z_pos;

        render_sprite(item_position, sprite_id, render_group, {.z_index = z_index});

        if (is_set(entity_info[slot->entity_type].flags, ENTITY_INFO_F_PLACEABLE)) {
            hotbar_render_placement_indicator(game_state->player->position, player_aim_vec, render_group);
        }
    }
}

void hotbar_render(GameState* game_state, GameInput* game_input, RenderGroup* render_group) {
    float padding = 0.5f;

    UIElement* container =
        ui_create_container({.padding = padding, .opts = UI_ELEMENT_F_CONTAINER_ROW | UI_ELEMENT_F_DRAW_BACKGROUND});

    flags inventory_render_option_flags = 0;
    if (!is_set(game_state->ui_mode.flags, UI_MODE_F_INVENTORY_ACTIVE)) {
        inventory_render_option_flags |= INVENTORY_RENDER_F_SLOTS_MOUSE_DISABLED;
    }

    InventoryRenderOptions inventory_opts = {.scale = 1.5,
                                             .slot_gap = pixels_to_units(8),
                                             .background_rgba = COLOR_BLACK,
                                             .flags = inventory_render_option_flags};

    Vec2 hotbar_ui_size =
        inventory_ui_get_size(&game_state->hotbar.inventory, padding, inventory_opts.slot_gap, inventory_opts.scale);

    Vec2 container_top_left = {game_state->camera.position.x - (hotbar_ui_size.x / 2),
                               game_state->camera.position.y - (game_state->camera.size.y / 2) + hotbar_ui_size.y};

    InventoryInput inventory_input = inventory_render(container, container_top_left, &game_state->hotbar.inventory,
                                                      game_state, game_input, inventory_opts);

    if (is_set(game_state->ui_mode.flags, UI_MODE_F_INVENTORY_ACTIVE) && inventory_input.idx_clicked != -1) {
        InventoryItem* item = &game_state->hotbar.inventory.items[inventory_input.idx_clicked];
        Entity* entity_with_open_inventory = entity_find(game_state->ui_mode.entity_handle);

        ASSERT(entity_with_open_inventory, "entity_handle must point to entity if inventory is active");

        if (item->entity_type != ENTITY_TYPE_UNKNOWN) {
            inventory_move_items(inventory_input.idx_clicked, item->stack_size, &game_state->hotbar.inventory,
                                 &entity_with_open_inventory->inventory, &game_state->entity_data);
        }
    }

    ui_draw_element(container, container_top_left, render_group);
}

void hotbar_validate(HotBar* hotbar) {
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        InventoryItem slot = hotbar->inventory.items[i];

        ASSERT(slot.stack_size >= 0, "hotbar slot stacksize must be >= 0");

        if (slot.stack_size > 0) {
            ASSERT(slot.entity_type != ENTITY_TYPE_UNKNOWN, "hotbar slot has a stacksize but no entity type");
        } else {
            ASSERT(slot.entity_type == ENTITY_TYPE_UNKNOWN,
                   "hotbar slot has non-zero stack size and no entity type assigned");
        }
    }
}
