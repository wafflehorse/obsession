#include "crafting.h"

#define CRAFTING_MAX_INGREDIENTS 8

struct CraftingIngredient {
    EntityType entity_type;
    uint32 quantity;
};

struct CraftingRecipe {
    EntityType entity_type;
    CraftingIngredient ingredients[CRAFTING_MAX_INGREDIENTS];
};

static CraftingRecipe crafting_recipe_books[CRAFTING_RECIPE_BOOK_COUNT][ENTITY_TYPE_COUNT];

void crafting_init() {
    CraftingRecipe* general_recipes = crafting_recipe_books[CRAFTING_RECIPE_BOOK_GENERAL];
    uint32 general_recipes_count = 0;
    general_recipes[general_recipes_count++] = {.entity_type = ENTITY_TYPE_CHEST_IRON,
                                                .ingredients = {{.entity_type = ENTITY_TYPE_IRON, .quantity = 4}}};

    ASSERT(general_recipes_count < ENTITY_TYPE_COUNT, "too many general recipes");

    CraftingRecipe* structure_recipes = crafting_recipe_books[CRAFTING_RECIPE_BOOK_STRUCTURES];
    uint32 structure_recipes_count = 0;
    structure_recipes[structure_recipes_count++] = {.entity_type = ENTITY_TYPE_ROBOTICS_FACTORY,
                                                    .ingredients = {{.entity_type = ENTITY_TYPE_IRON, .quantity = 20},
                                                                    {.entity_type = ENTITY_TYPE_COAL, .quantity = 40}}};

    ASSERT(structure_recipes_count < ENTITY_TYPE_COUNT, "too many structure recipes");
};

CraftingRecipe* crafting_recipe_find(CraftingRecipeBook recipe_book_type, EntityType entity_type) {
    CraftingRecipe* recipe_book = crafting_recipe_books[recipe_book_type];
    CraftingRecipe* recipe = NULL;

    for (int i = 0; i < ENTITY_TYPE_COUNT; i++) {
        if (recipe_book[i].entity_type == entity_type) {
            recipe = &recipe_book[i];
        }
    }

    return recipe;
}

CraftingRecipe* crafting_recipe_find(CraftingRecipeBook recipe_book_type, uint32 idx) {
    ASSERT(idx < ENTITY_TYPE_COUNT, "crafting recipe_find index out of range");

    CraftingRecipe* recipe = &crafting_recipe_books[recipe_book_type][idx];
    if (recipe->entity_type == ENTITY_TYPE_UNKNOWN) {
        recipe = NULL;
    }

    return recipe;
}

bool crafting_can_craft_item(CraftingRecipeBook recipe_book_type, EntityType entity_type, Inventory* inventory) {
    CraftingRecipe* recipe = crafting_recipe_find(recipe_book_type, entity_type);

    if (!recipe) {
        return false;
    }

    for (int i = 0; i < CRAFTING_MAX_INGREDIENTS; i++) {
        CraftingIngredient* ingredient = &recipe->ingredients[i];
        if (ingredient->entity_type != ENTITY_TYPE_UNKNOWN &&
            !inventory_contains_item(inventory, ingredient->entity_type, ingredient->quantity)) {
            return false;
        }
    }

    return true;
}

void crafting_consume_ingredients(Inventory* inventory, CraftingRecipeBook recipe_book_type, EntityType entity_type) {
    CraftingRecipe* recipe = crafting_recipe_find(recipe_book_type, entity_type);
    ASSERT(recipe, "crafting_consume_ingredients - recipe not found!");
    if (crafting_can_craft_item(recipe_book_type, entity_type, inventory)) {
        for (int i = 0; i < CRAFTING_MAX_INGREDIENTS && recipe->ingredients[i].entity_type != ENTITY_TYPE_UNKNOWN;
             i++) {
            CraftingIngredient ingredient = recipe->ingredients[i];
            inventory_remove_items(inventory, ingredient.entity_type, ingredient.quantity);
        }
    }
}

void crafting_craft_item(EntityData* entity_data, EntityType entity_type, CraftingRecipeBook recipe_book_type,
                         Inventory* inventory) {
    CraftingRecipe* recipe = crafting_recipe_find(recipe_book_type, entity_type);

    ASSERT(recipe, "Attempting to craft an item without a recipe");

    if (inventory_space_for_item(entity_type, 1, inventory) &&
        crafting_can_craft_item(recipe_book_type, entity_type, inventory)) {
        for (int i = 0; i < CRAFTING_MAX_INGREDIENTS && recipe->ingredients[i].entity_type != ENTITY_TYPE_UNKNOWN;
             i++) {
            CraftingIngredient ingredient = recipe->ingredients[i];
            inventory_remove_items(inventory, ingredient.entity_type, ingredient.quantity);
        }

        inventory_add_item(inventory, entity_type, 1);
    }
}

Inventory recipes_to_inventory(CraftingRecipeBook recipe_book_type, int row_count, int col_count) {
    ASSERT(row_count != 0 && col_count != 0, "recipes_to_inventory - row_cound and col_count must be non-zero");

    CraftingRecipe* recipes = crafting_recipe_books[recipe_book_type];
    Inventory inventory = {};

    for (int i = 0; i < ENTITY_TYPE_COUNT; i++) {
        CraftingRecipe recipe = recipes[i];

        if (recipe.entity_type != ENTITY_TYPE_UNKNOWN) {
            inventory.items[i].entity_type = recipe.entity_type;
            inventory.items[i].stack_size = 1;
        }
    }

    inventory.row_count = row_count;
    inventory.col_count = col_count;

    ASSERT(inventory.row_count * inventory.col_count < ENTITY_TYPE_COUNT,
           "crafting inventory size must be less than entity type count");

    return inventory;
}

void crafting_recipe_info_render(UIElement* container, Vec2 container_size, CraftingRecipeBook recipe_book_type,
                                 uint32 recipe_idx) {
    CraftingRecipe* recipe = crafting_recipe_find(recipe_book_type, recipe_idx);
    if (!recipe) {
        return;
    }

    EntityInfo* e_info = &entity_info[recipe->entity_type];

    UIElement* title =
        ui_create_text(e_info->type_name_string, {.rgba = COLOR_WHITE, .font_scale = 1, .max_width = container_size.x});
    UIElement* description =
        ui_create_text(e_info->description, {.rgba = COLOR_WHITE, .font_scale = 1, .max_width = container_size.x});

    ui_push(container, title);
    ui_push(container, description);

    for (int i = 0; i < CRAFTING_MAX_INGREDIENTS && recipe->ingredients[i].entity_type != ENTITY_TYPE_UNKNOWN; i++) {
        CraftingIngredient ingredient = recipe->ingredients[i];

        UIElement* ingredient_container =
            ui_create_container({.child_gap = pixels_to_units(8), .opts = UI_ELEMENT_F_CONTAINER_ROW});
        UIElement* ingredient_sprite_element = ui_create_sprite(entity_get_default_sprite(ingredient.entity_type));

        const char* entity_name_string = entity_info[ingredient.entity_type].type_name_string;
        char ingredient_text[64] = {};
        snprintf(ingredient_text, 64, "%i %s", ingredient.quantity, entity_name_string);
        UIElement* ingredient_text_element = ui_create_text(ingredient_text, {.rgba = COLOR_WHITE, .font_scale = 1});

        ui_push(ingredient_container, ingredient_sprite_element);
        ui_push(ingredient_container, ingredient_text_element);

        ui_push(container, ingredient_container);
    }
}
