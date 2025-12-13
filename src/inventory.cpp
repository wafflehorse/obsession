#include "game.h"

#define INVENTORY_BASE_SLOT_DIMENSION 1

void inventory_init(Inventory* inventory, uint32 item_count) {
    for (int i = 0; i < item_count; i++) {
        inventory->items[i].entity_handle.generation = -1;
    }
}

bool inventory_should_persist_entity(EntityType entity_type) {
    return is_set(entity_info[entity_type].flags, ENTITY_INFO_F_PERSIST_IN_INVENTORY);
}

void inventory_remove_items_by_index(Inventory* inventory, uint32 index, uint32 quantity) {
    // ASSERT(index < inventory->item_count, "inventory_remove_items: index is greater than item count");

    InventoryItem* item = &inventory->items[index];

    ASSERT(item->stack_size >= quantity, "inventory_move: stack size is less than quantity being moved");

    item->stack_size -= quantity;

    if (item->stack_size <= 0) {
        item->entity_type = ENTITY_TYPE_UNKNOWN;
        item->entity_handle = entity_null_handle;
    }
}

void inventory_remove_items(Inventory* inventory, EntityType entity_type, uint32 quantity) {
    uint32 quantity_remaining = quantity;

    while (quantity_remaining > 0) {
        InventoryItem* min_stack_size_slot = NULL;
        int min_stack_size_index = -1;
        uint32 capacity = inventory->row_count * inventory->col_count;
        for (int i = 0; i < capacity; i++) {
            InventoryItem* slot = &inventory->items[i];
            if (slot->entity_type == entity_type) {
                if (!min_stack_size_slot || min_stack_size_slot->stack_size > slot->stack_size) {
                    min_stack_size_slot = slot;
                    min_stack_size_index = i;
                }
            }
        }

        ASSERT(min_stack_size_slot, "Trying to remove hot bar items that don't exist");

        uint32 quantity_to_remove = w_min(quantity_remaining, min_stack_size_slot->stack_size);
        inventory_remove_items_by_index(inventory, min_stack_size_index, quantity_to_remove);
        quantity_remaining -= quantity_to_remove;
    }
}

bool inventory_slot_can_take_item(EntityType entity_type, uint32 quantity, InventoryItem* slot) {
    return slot->stack_size == 0 ||
           (slot->entity_type == entity_type && !inventory_should_persist_entity(entity_type) &&
            (slot->stack_size + quantity) < MAX_ITEM_STACK_SIZE);
}

InventoryItem* inventory_find_available_slot(Inventory* inventory, EntityType entity_type, uint32 quantity) {
    InventoryItem* open_slot = NULL;
    uint32 capacity = inventory->col_count * inventory->row_count;
    for (int i = 0; i < capacity; i++) {
        InventoryItem* slot = &inventory->items[i];
        if (inventory_slot_can_take_item(entity_type, quantity, slot)) {
            if (!open_slot || (open_slot->entity_type != entity_type && slot->entity_type == entity_type) ||
                ((open_slot->entity_type == entity_type && slot->entity_type == entity_type) &&
                 slot->stack_size > open_slot->stack_size)) {
                open_slot = slot;
            }
        }
    }

    return open_slot;
}

void inventory_add_item(Inventory* inventory, EntityType entity_type, uint32 quantity) {
    InventoryItem* open_slot = inventory_find_available_slot(inventory, entity_type, quantity);

    ASSERT(open_slot, "Trying to add item to hotbar, but there's not space");

    open_slot->entity_type = entity_type;
    open_slot->stack_size += quantity;
}

void inventory_add_entity_item(Inventory* inventory, Entity* entity, EntityData* entity_data) {
    InventoryItem* open_slot = inventory_find_available_slot(inventory, entity->type, 1);

    if (open_slot) {
        if (inventory_should_persist_entity(entity->type)) {
            open_slot->entity_handle = entity_to_handle(entity, entity_data);
        } else {
            set(entity->flags, ENTITY_F_MARK_FOR_DELETION);
        }

        open_slot->entity_type = entity->type;
        open_slot->stack_size += entity->stack_size;
    }
}

bool inventory_contains_item(Inventory* inventory, EntityType entity_type, uint32 quantity) {
    uint32 quantity_found = 0;
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        if (inventory->items[i].entity_type == entity_type) {
            quantity_found += inventory->items[i].stack_size;
        }
    }

    return quantity_found >= quantity;
}

bool inventory_item_is_entity(InventoryItem item) {
    return item.entity_handle.generation != -1;
}

bool inventory_space_for_item(EntityType entity_type, uint32 quantity, Inventory* inventory) {
    InventoryItem* open_slot = inventory_find_available_slot(inventory, entity_type, quantity);

    if (open_slot) {
        return true;
    }

    return false;
}

bool inventory_move_items(uint32 index, uint32 quantity, Inventory* source_inventory, Inventory* dest_inventory,
                          EntityData* entity_data) {
    bool result = false;

    InventoryItem item = source_inventory->items[index];

    if (inventory_space_for_item(item.entity_type, quantity, dest_inventory)) {
        inventory_remove_items_by_index(source_inventory, index, quantity);

        if (inventory_item_is_entity(item)) {
            Entity* entity = entity_find(item.entity_handle, entity_data);

            ASSERT(entity, "entity in inventory does not exist");

            inventory_add_entity_item(dest_inventory, entity, entity_data);
        } else {
            inventory_add_item(dest_inventory, item.entity_type, quantity);
        }
    }

    return result;
}

Vec2 inventory_ui_get_size(Inventory* inventory, float padding, float slot_gap, float scale) {
    Vec2 inventory_ui_size = {.x = inventory->col_count * (INVENTORY_BASE_SLOT_DIMENSION * scale) +
                                   (slot_gap * (inventory->col_count - 1)) + (padding * 2),
                              .y = inventory->row_count * (INVENTORY_BASE_SLOT_DIMENSION * scale) +
                                   (slot_gap * (inventory->row_count - 1)) + (padding * 2)};

    return inventory_ui_size;
}

struct InventoryInput {
    int idx_hovered;
    int idx_clicked;
};

#define INVENTORY_RENDER_F_SLOTS_MOUSE_DISABLED (1 << 0)

struct InventoryRenderOptions {
    flags flags;
    float scale;
    float slot_gap;
    Vec4 background_rgba;
};

InventoryInput inventory_render(UIElement* container, Vec2 container_position, Inventory* inventory,
                                GameState* game_state, GameInput* game_input, InventoryRenderOptions opts) {
    InventoryInput input = {-1, -1};

    for (int row = 0; row < inventory->row_count; row++) {
        UIElement* item_row_container = ui_create_container(
            {.padding = 0, .child_gap = opts.slot_gap, .opts = UI_ELEMENT_F_CONTAINER_ROW}, &game_state->frame_arena);

        ui_push(container, item_row_container);

        for (int col = 0; col < inventory->col_count; col++) {
            uint32 item_idx = row * inventory->col_count + col;
            InventoryItem* item = NULL;
            item = &inventory->items[item_idx];

            Vec2 slot_size = {1, 1};
            slot_size = w_vec_mult(slot_size, opts.scale);

            UIElement* item_slot =
                ui_create_container({.min_size = slot_size,
                                     .max_size = slot_size,
                                     .background_rgba = opts.background_rgba,
                                     .opts = UI_ELEMENT_F_CONTAINER_ROW | UI_ELEMENT_F_DRAW_BACKGROUND},
                                    &game_state->frame_arena);

            if (item && item->entity_type != ENTITY_TYPE_UNKNOWN) {
                if (item->stack_size > 1) {
                    char stack_size_str[3] = {};
                    snprintf(stack_size_str, 3, "%i", item->stack_size);
                    UIElement* stack_size_element =
                        ui_create_text(stack_size_str, COLOR_WHITE, 0.5, &game_state->frame_arena);

                    Vec2 stack_size_rel_position = {item_slot->size.x - stack_size_element->size.x - pixels_to_units(2),
                                                    -stack_size_element->size.y / 2};

                    ui_push_abs_position(item_slot, stack_size_element, stack_size_rel_position);
                }

                Sprite sprite = entity_get_default_sprite(item->entity_type);
                UIElement* item_sprite = ui_create_sprite(sprite, &game_state->frame_arena);
                ui_push_centered(item_slot, item_sprite);
            }

            ui_push(item_row_container, item_slot);

            Vec2 slot_world_position_top_left = ui_abs_position_get(container_position, item_slot);
            Vec2 slot_world_position_center = w_rect_top_left_to_center(slot_world_position_top_left, slot_size);

            Rect slot_rect = {slot_world_position_center.x, slot_world_position_center.y, slot_size.x, slot_size.y};

            if (w_check_point_in_rect(slot_rect, game_state->world_input.mouse_position_world) &&
                !is_set(opts.flags, INVENTORY_RENDER_F_SLOTS_MOUSE_DISABLED)) {
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
