#include "game.h"

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
