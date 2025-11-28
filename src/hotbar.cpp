#include "game.h"

void hotbar_init(HotBar* hot_bar) {
    for (int i = 0; i < MAX_HOT_BAR_SLOTS; i++) {
        hot_bar->slots[i].entity_handle.generation = -1;
    }
}

bool hotbar_slot_can_take_item(Entity* item, HotBarSlot* slot) {
    return slot->stack_size == 0 ||
           (slot->entity_type == item->type && !is_set(item->flags, ENTITY_F_ITEM_PERSIST_ENTITY) &&
            slot->stack_size < MAX_ITEM_STACK_SIZE);
}

void hotbar_add_item(Entity* item, HotBar* hot_bar, EntityData* entity_data) {
    HotBarSlot* open_slot = NULL;
    for (int i = 0; i < MAX_HOT_BAR_SLOTS; i++) {
        HotBarSlot* slot = &hot_bar->slots[i];
        if (hotbar_slot_can_take_item(item, slot)) {
            open_slot = slot;
            break;
        }
    }

    if (open_slot) {
        if (!is_set(item->flags, ENTITY_F_ITEM_PERSIST_ENTITY)) {
            set(item->flags, ENTITY_F_MARK_FOR_DELETION);
        } else {
            open_slot->entity_handle = entity_to_handle(item, entity_data);
        }

        open_slot->entity_type = item->type;
        open_slot->stack_size += item->stack_size;
    }
}

HotBarSlot* hotbar_active_slot(HotBar* hot_bar) {
    return &hot_bar->slots[hot_bar->active_item_idx];
}

bool hotbar_space_for_item(Entity* item, HotBar* hot_bar) {
    bool can_pick_up_item = false;

    for (int i = 0; i < MAX_HOT_BAR_SLOTS; i++) {
        HotBarSlot* slot = &hot_bar->slots[i];
        if (hotbar_slot_can_take_item(item, slot)) {
            can_pick_up_item = true;
        }
    }

    return can_pick_up_item;
}

void hotbar_render_item(GameState* game_state, PlayerWorldInput* player_world_input, RenderGroup* render_group) {
    Entity* player = game_state->player;
    HotBar* hot_bar = &game_state->hot_bar;

    HotBarSlot* slot = &hot_bar->slots[hot_bar->active_item_idx];

    Entity* slot_entity = entity_find(slot->entity_handle, &game_state->entity_data);
    if (!slot_entity && slot->stack_size > 0) {
        float z_pos, z_index;
        Vec2 item_position = entity_held_item_position(player, &z_pos, &z_index);

        SpriteID sprite_id = entity_default_sprites[slot->entity_type];
        item_position.y += z_pos;

        render_sprite(item_position, sprite_id, render_group, {.z_index = z_index});
    }
}

void hotbar_render(GameState* game_state, RenderGroup* render_group) {
    UIElement* container = ui_create_container({.padding = 0.5, .child_gap = 0.2, .opts = UI_ELEMENT_F_CONTAINER_ROW},
                                               &game_state->frame_arena);

    for (int i = 0; i < MAX_HOT_BAR_SLOTS; i++) {
        float slot_padding = 0.5f;
        Vec2 slot_size = {pixels_to_units(8) + (2 * slot_padding), pixels_to_units(8) + (2 * slot_padding)};
        UIElement* slot_container =
            ui_create_container({.padding = slot_padding,
                                 .min_size = slot_size,
                                 .max_size = slot_size,
                                 .opts = UI_ELEMENT_F_CONTAINER_ROW | UI_ELEMENT_F_DRAW_BACKGROUND,
                                 .background_rgba = COLOR_BLACK},
                                &game_state->frame_arena);

        HotBarSlot* slot = &game_state->hot_bar.slots[i];

        if (slot->stack_size > 0) {
            EntityHandle entity_handle = slot->entity_handle;
            Entity* entity = entity_find(entity_handle, &game_state->entity_data);
            Sprite sprite;
            if (entity) {
                ASSERT(entity->sprite_id != SPRITE_UNKNOWN, "The hot bar only supports rendering the entity sprite id");
                sprite = sprite_table[entity->sprite_id];
            } else {
                SpriteID sprite_id = entity_default_sprites[slot->entity_type];
                sprite = sprite_table[sprite_id];
            }

            UIElement* sprite_element = ui_create_sprite(sprite, &game_state->frame_arena);
            ui_push(slot_container, sprite_element);

            // TODO: this is a hack since our ui system doesn't support centering items yet
            Vec2 sprite_center_rel_pos = {slot_size.x / 2, -slot_size.y / 2};
            sprite_element->rel_position = {sprite_center_rel_pos.x - (pixels_to_units(sprite.w) / 2),
                                            sprite_center_rel_pos.y + (pixels_to_units(sprite.h) / 2)};

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
