#pragma once

enum CraftingRecipeBook {
    CRAFTING_RECIPE_BOOK_UNKNOWN,
    CRAFTING_RECIPE_BOOK_GENERAL,
    CRAFTING_RECIPE_BOOK_STRUCTURES,
    CRAFTING_RECIPE_BOOK_COUNT
};

bool crafting_can_craft_item(CraftingRecipeBook recipe_book_type, EntityType entity_type, Inventory* inventory);
