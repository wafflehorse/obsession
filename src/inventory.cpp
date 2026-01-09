#include "game.h"
#include "crafting.h"

#define INVENTORY_BASE_SLOT_DIMENSION 1

void inventory_init(Inventory* inventory, uint32 item_count) {
    for (int i = 0; i < item_count; i++) {
        inventory->items[i].entity_handle.generation = -1;
    }
}

bool inventory_should_persist_entity(EntityType entity_type) {
    return is_set(entity_info[entity_type].flags, ENTITY_INFO_F_PERSIST_IN_INVENTORY);
}

void inventory_item_clear(InventoryItem* item) {
    item->stack_size = 0;
    item->entity_handle = entity_null_handle;
    item->entity_type = ENTITY_TYPE_UNKNOWN;
}

void inventory_remove_items_by_index(Inventory* inventory, uint32 index, uint32 quantity) {
    InventoryItem* item = &inventory->items[index];

    ASSERT(item->stack_size >= quantity, "inventory_move: stack size is less than quantity being moved");

    item->stack_size -= quantity;

    if (item->stack_size <= 0) {
        inventory_item_clear(item);
    }
}

void inventory_remove_items(Inventory* inventory, EntityType entity_type, uint32 quantity) {
    uint32 quantity_remaining = quantity;

    while (quantity_remaining > 0) {
        InventoryItem* min_stack_size_slot = NULL;
        int min_stack_size_index = -1;
        for (int i = 0; i < inventory->capacity; i++) {
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
    for (int i = 0; i < inventory->capacity; i++) {
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

void inventory_add_entity_item(Inventory* inventory, Entity* entity) {
    InventoryItem* open_slot = inventory_find_available_slot(inventory, entity->type, 1);

    if (open_slot) {
        if (inventory_should_persist_entity(entity->type)) {
            open_slot->entity_handle = entity_to_handle(entity);
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
            Entity* entity = entity_find(item.entity_handle);

            ASSERT(entity, "entity in inventory does not exist");

            inventory_add_entity_item(dest_inventory, entity);
        } else {
            inventory_add_item(dest_inventory, item.entity_type, quantity);
        }
    }

    return result;
}

Vec2 inventory_get_dimensions_from_capacity(uint32 capacity, uint32 num_columns) {
    Vec2 result = {0, 0};

    if (capacity > 0 && num_columns > 0) {
        result.x = w_ceilf((float)capacity / (float)num_columns);
        result.y = num_columns;
    }

    return result;
}

Vec2 inventory_ui_get_size(Vec2 dimensions, float padding, float slot_gap, float scale) {
    Vec2 inventory_ui_size;
    if (dimensions.x <= 0 || dimensions.y <= 0) {
        inventory_ui_size = {0, 0};
    } else {
        inventory_ui_size = {.x = dimensions.y * (INVENTORY_BASE_SLOT_DIMENSION * scale) +
                                  (slot_gap * (dimensions.y - 1)) + (padding * 2),
                             .y = dimensions.x * (INVENTORY_BASE_SLOT_DIMENSION * scale) +
                                  (slot_gap * (dimensions.x - 1)) + (padding * 2)};
    }

    return inventory_ui_size;
}

struct InventoryInput {
    int idx_hovered;
    int idx_clicked;
};

#define INVENTORY_RENDER_F_SLOTS_MOUSE_DISABLED (1 << 0)
#define INVENTORY_RENDER_F_FOR_CRAFTING (1 << 1)

struct InventoryRenderOptions {
    flags flags;
    float scale;
    float slot_gap;
    Vec4 background_rgba;
    CraftingRecipeBook recipe_book_type;
};

// NOTE: There is some ui hackiness going on here.
// 1. in order to get mouse hover and click, we need to prepend the item_row_container to the container before
// the item_row_container has been filled with children, so we have to precalculate the size of it before adding it to
// the container so that the cursor position updates correctly
// 2. Really, we shouldn't have the row container at all. We should just have a new line function that moves the cursor
// to the right place for the next row of children. We could then more easily avoid having to precompute the size,
// because we could then just have a second pass through the children to check for mouse interaction and we can more
// easily attribute the ui element to an inventory item.
InventoryInput inventory_render(UIElement* container, Vec2 container_position, Inventory* inventory, Vec2 dimensions,
                                GameState* game_state, GameInput* game_input, InventoryRenderOptions opts) {

    ASSERT(!is_set(opts.flags, INVENTORY_RENDER_F_FOR_CRAFTING) ||
               opts.recipe_book_type != CRAFTING_RECIPE_BOOK_UNKNOWN,
           "Must specify a recipe book if inventory render is for crafting");

    InventoryInput input = {-1, -1};
    Vec2 slot_size = {1, 1};
    slot_size = w_vec_mult(slot_size, opts.scale);

    Vec2 row_container_size = {.x = (slot_size.x * dimensions.y) + ((dimensions.y - 1) * opts.slot_gap),
                               .y = slot_size.y};

    for (int row = 0; row < dimensions.x; row++) {
        UIElement* item_row_container = ui_create_container({
            .padding = 0,
            .min_size = row_container_size,
            .max_size = row_container_size,
            .child_gap = opts.slot_gap,
            .opts = UI_ELEMENT_F_CONTAINER_ROW,
        });

        ui_push(container, item_row_container);

        for (int col = 0; col < dimensions.y; col++) {
            uint32 item_idx = row * dimensions.y + col;

            if (item_idx < inventory->capacity) {
                InventoryItem* item = NULL;
                item = &inventory->items[item_idx];

                UIElement* item_slot =
                    ui_create_container({.min_size = slot_size,
                                         .max_size = slot_size,
                                         .background_rgba = opts.background_rgba,
                                         .opts = UI_ELEMENT_F_CONTAINER_ROW | UI_ELEMENT_F_DRAW_BACKGROUND});

                if (item && item->entity_type != ENTITY_TYPE_UNKNOWN) {
                    Sprite sprite = entity_get_default_sprite(item->entity_type);
                    Vec2 sprite_world_size = {.x = pixels_to_units(sprite.w), .y = pixels_to_units(sprite.h)};
                    Vec2 sprite_size = w_scale_to_fit(sprite_world_size, slot_size);

                    UICreateSpriteOpts sprite_opts = {.size = sprite_size};

                    if (is_set(opts.flags, INVENTORY_RENDER_F_FOR_CRAFTING) &&
                        !crafting_can_craft_item(opts.recipe_book_type, item->entity_type,
                                                 &game_state->player->inventory)) {
                        set(sprite_opts.flags, UI_CREATE_SPRITE_OPTS_F_APPLY_TINT);
                        sprite_opts.tint = {1, 1, 1, 0.4};
                    }

                    UIElement* item_sprite = ui_create_sprite(sprite, sprite_opts);
                    ui_push_centered(item_slot, item_sprite);

                    if (item->stack_size > 1) {
                        char stack_size_str[3] = {};
                        snprintf(stack_size_str, 3, "%i", item->stack_size);
                        UIElement* stack_size_element =
                            ui_create_text(stack_size_str, {.rgba = COLOR_WHITE, .font_scale = 0.5});

                        Vec2 stack_size_rel_position = {item_slot->size.x - stack_size_element->size.x -
                                                            pixels_to_units(2),
                                                        -stack_size_element->size.y / 2};

                        ui_push_abs_position(item_slot, stack_size_element, stack_size_rel_position);
                    }
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
        }

        ui_container_size_update(container);
    }

    return input;
}
