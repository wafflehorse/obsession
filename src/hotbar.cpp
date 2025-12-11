#include "game.h"

void hotbar_init(HotBar* hotbar) {
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        hotbar->items[i].entity_handle.generation = -1;
    }
}

bool hotbar_should_persist_entity(EntityType entity_type) {
    return is_set(entity_info[entity_type].flags, ENTITY_INFO_F_PERSIST_IN_INVENTORY);
}

bool hotbar_contains_item(HotBar* hotbar, EntityType entity_type, uint32 quantity) {
    uint32 quantity_found = 0;
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        if (hotbar->items[i].entity_type == entity_type) {
            quantity_found += hotbar->items[i].stack_size;
        }
    }

    return quantity_found >= quantity;
}

void hotbar_slot_remove_item(InventoryItem* hotbar_slot, uint32 quantity) {
    hotbar_slot->stack_size = w_clamp_min(hotbar_slot->stack_size - quantity, 0);

    if (hotbar_slot->stack_size == 0) {
        hotbar_slot->entity_type = ENTITY_TYPE_UNKNOWN;
        hotbar_slot->entity_handle.generation = -1;
    }
}

void hotbar_remove_item(HotBar* hotbar, EntityType entity_type, uint32 quantity) {
    uint32 quantity_remaining = quantity;

    while (quantity_remaining > 0) {
        InventoryItem* min_stack_size_slot = NULL;
        for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
            InventoryItem* slot = &hotbar->items[i];
            if (slot->entity_type == entity_type) {
                if (!min_stack_size_slot || min_stack_size_slot->stack_size > slot->stack_size) {
                    min_stack_size_slot = slot;
                }
            }
        }

        ASSERT(min_stack_size_slot, "Trying to remove hot bar items that don't exist");

        uint32 tmp_stack_size = min_stack_size_slot->stack_size;
        hotbar_slot_remove_item(min_stack_size_slot, quantity_remaining);
        quantity_remaining -= w_min(tmp_stack_size, quantity_remaining);
    }
}

bool hotbar_slot_can_take_item(EntityType entity_type, uint32 quantity, InventoryItem* slot) {
    return slot->stack_size == 0 || (slot->entity_type == entity_type && !hotbar_should_persist_entity(entity_type) &&
                                     (slot->stack_size + quantity) < MAX_ITEM_STACK_SIZE);
}

InventoryItem* hotbar_available_slot(HotBar* hotbar, EntityType entity_type, uint32 quantity) {
    InventoryItem* open_slot = NULL;
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        InventoryItem* slot = &hotbar->items[i];
        if (hotbar_slot_can_take_item(entity_type, quantity, slot)) {
            open_slot = slot;
            break;
        }
    }

    return open_slot;
}

void hotbar_add_item(HotBar* hotbar, EntityType entity_type, uint32 quantity) {
    InventoryItem* open_slot = hotbar_available_slot(hotbar, entity_type, quantity);

    ASSERT(open_slot, "Trying to add item to hotbar, but there's not space");

    open_slot->entity_type = entity_type;
    open_slot->stack_size += quantity;
}

void hotbar_add_item(Entity* item, HotBar* hotbar, EntityData* entity_data) {
    InventoryItem* open_slot = hotbar_available_slot(hotbar, item->type, 1);

    if (open_slot) {
        if (hotbar_should_persist_entity(item->type)) {
            open_slot->entity_handle = entity_to_handle(item, entity_data);
        } else {
            set(item->flags, ENTITY_F_MARK_FOR_DELETION);
        }

        open_slot->entity_type = item->type;
        open_slot->stack_size += item->stack_size;
    }
}

InventoryItem* hotbar_active_slot(HotBar* hotbar) {
    return &hotbar->items[hotbar->active_item_idx];
}

bool hotbar_space_for_item(EntityType entity_type, uint32 quantity, HotBar* hotbar) {
    InventoryItem* open_slot = hotbar_available_slot(hotbar, entity_type, quantity);

    if (open_slot) {
        return true;
    }

    return false;
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

    InventoryItem* slot = &hotbar->items[hotbar->active_item_idx];

    Entity* slot_entity = entity_find(slot->entity_handle, &game_state->entity_data);
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

void hotbar_render(GameState* game_state, RenderGroup* render_group) {
    UIElement* container = ui_create_container({.padding = 0.5, .child_gap = 0.2, .opts = UI_ELEMENT_F_CONTAINER_ROW},
                                               &game_state->frame_arena);

    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        float slot_padding = 0.5f;
        Vec2 slot_size = {pixels_to_units(8) + (2 * slot_padding), pixels_to_units(8) + (2 * slot_padding)};
        UIElement* slot_container =
            ui_create_container({.padding = slot_padding,
                                 .min_size = slot_size,
                                 .max_size = slot_size,
                                 .opts = UI_ELEMENT_F_CONTAINER_ROW | UI_ELEMENT_F_DRAW_BACKGROUND,
                                 .background_rgba = COLOR_BLACK},
                                &game_state->frame_arena);

        InventoryItem* slot = &game_state->hotbar.items[i];

        if (slot->stack_size > 0) {
            EntityHandle entity_handle = slot->entity_handle;
            Entity* entity = entity_find(entity_handle, &game_state->entity_data);
            Sprite sprite;
            if (entity) {
                ASSERT(entity->sprite_id != SPRITE_UNKNOWN, "The hot bar only supports rendering the entity sprite id");
                sprite = sprite_table[entity->sprite_id];
            } else {
                SpriteID sprite_id = entity_info[slot->entity_type].default_sprite;
                sprite = sprite_table[sprite_id];
            }

            UIElement* sprite_element = ui_create_sprite(sprite, &game_state->frame_arena);
            ui_push_centered(slot_container, sprite_element);

            if (slot->stack_size > 1) {
                char stack_size_str[3] = {};
                snprintf(stack_size_str, 3, "%i", slot->stack_size);
                UIElement* stack_size_element =
                    ui_create_text(stack_size_str, COLOR_WHITE, 0.5, &game_state->frame_arena);

                Vec2 stack_size_rel_position = {slot_container->size.x - stack_size_element->size.x -
                                                    pixels_to_units(2),
                                                -stack_size_element->size.y / 2};

                ui_push_abs_position(slot_container, stack_size_element, stack_size_rel_position);
            }
        }

        ui_push(container, slot_container);
    }

    Vec2 container_top_left = {game_state->camera.position.x - (container->size.x / 2),
                               game_state->camera.position.y - (game_state->camera.size.y / 2) + container->size.y};

    ui_draw_element(container, container_top_left, render_group);
}

void hotbar_validate(HotBar* hotbar) {
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        InventoryItem slot = hotbar->items[i];

        ASSERT(slot.stack_size >= 0, "hotbar slot stacksize must be >= 0");

        if (slot.stack_size > 0) {
            ASSERT(slot.entity_type != ENTITY_TYPE_UNKNOWN, "hotbar slot has a stacksize but no entity type");
        } else {
            ASSERT(slot.entity_type == ENTITY_TYPE_UNKNOWN,
                   "hotbar slot has non-zero stack size and no entity type assigned");
        }
    }
}
