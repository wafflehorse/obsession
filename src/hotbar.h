#pragma once

InventoryItem* hotbar_active_slot(Inventory* inventory, uint32 active_item_idx);
Vec2 hotbar_placeable_position(Vec2 player_position, Vec2 aim_vec);
void hotbar_render_item(GameState* game_state, Vec2 player_aim_vec, RenderGroup* render_group);
void hotbar_render(GameState* game_state, GameInput* game_input, RenderGroup* render_group);
void hotbar_validate(Inventory* inventory);
