#include "imgui.h"
#include "game.h"

void sprite_to_uv_coordinates(SpriteID sprite_id, Vec2 texture_size, Vec2* uv0, Vec2* uv1) {
    Sprite sprite = sprite_table[sprite_id];

    uv0->x = sprite.x / texture_size.x;
    uv0->y = 1 - (sprite.y / texture_size.y);

    uv1->x = (sprite.x + sprite.w) / texture_size.x;
    uv1->y = 1 - ((sprite.y + sprite.h) / texture_size.y);
}

void tools_update_input(GameInput* game_input, GameState* game_state) {
    KeyInputState* key_input_states = game_input->key_input_states;

    if (key_input_states[KEY_L_CTRL].is_held || key_input_states[KEY_R_CTRL].is_held) {
        if (key_input_states[KEY_E].is_pressed) {
            game_state->tools.is_panel_open = !game_state->tools.is_panel_open;
        }

        if (key_input_states[KEY_MINUS].is_pressed) {
            game_state->camera.zoom = w_max(game_state->camera.zoom * 0.90f, 0.1f);
        }

        if (key_input_states[KEY_EQUALS].is_pressed) {
            game_state->camera.zoom = w_min(game_state->camera.zoom * 1.10f, 1.0f);
        }

        if (key_input_states[KEY_0].is_pressed) {
            game_state->camera.zoom = 1.0f;
        }
    }
}

void tools_render_panel(GameMemory* game_memory, GameState* game_state, GameInput* game_input) {
    if (game_state->tools.is_panel_open) {
        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        int panel_width = 260;
        int panel_height = 420;
        ImGui::SetNextWindowSize(ImVec2(panel_width, panel_height), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x, main_viewport->WorkPos.y + 80),
                                ImGuiCond_FirstUseEver);

        uint32 panel_flags = 0;
        panel_flags |= ImGuiWindowFlags_NoCollapse;
        if (!ImGui::Begin("Tools", &game_state->tools.is_panel_open, panel_flags)) {
            ImGui::End();
            return;
        }

        Vec2 mouse_world_position =
            get_mouse_world_position(&game_state->camera, game_input, game_memory->window.size_px);

        ImGui::Text("Mouse coord: %.3f, %.3f", mouse_world_position.x, mouse_world_position.y);

        if (ImGui::CollapsingHeader("Entity")) {
            ImGui::Text("Entity count: %i", game_state->entity_data.entity_count);
            ImGui::Text("Max entity count: %i", MAX_ENTITIES);
            ImGui::Checkbox("World init", &game_state->tools.entity_palette_should_add_to_init);

            ImGui::SameLine();

            if (ImGui::Button("Clear")) {
                game_state->tools.selected_entity = ENTITY_TYPE_UNKNOWN;
            }

            float available_width = 256; // ImGui::GetContentRegionAvail().x;
            float x = 0;
            Vec2 sprite_texture_size = {(float)game_state->sprite_texture_info.width,
                                        (float)game_state->sprite_texture_info.height};
            float spacing = ImGui::GetStyle().ItemSpacing.x;

            for (int i = 1; i < ENTITY_TYPE_COUNT; i++) {
                ImGui::PushID(i);

                SpriteID sprite_id = entity_default_sprites[i];
                Sprite sprite = sprite_table[sprite_id];

                Vec2 image_size = w_vec_mult((Vec2){sprite.w, sprite.h}, 2);

                Vec2 player_uv0 = {};
                Vec2 player_uv1 = {};
                sprite_to_uv_coordinates(sprite_id, sprite_texture_size, &player_uv0, &player_uv1);

                if (x + image_size.x + spacing > available_width - 8) {
                    ImGui::NewLine();
                    x = 0;
                } else if (x > 0.0f) {
                    ImGui::SameLine();
                }

                x += image_size.x + spacing;

                bool is_clicked = ImGui::ImageButton(
                    "some_id", game_state->sprite_texture_info.id, ImVec2(image_size.x, image_size.y),
                    ImVec2(player_uv0.x, player_uv0.y), ImVec2(player_uv1.x, player_uv1.y));

                if (is_clicked) {
                    game_state->tools.selected_entity = (EntityType)i;
                }

                if (i == game_state->tools.selected_entity) {
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                        IM_COL32(255, 255, 0, 255), // yellow
                                                        0.0f, 0, 2.0f);
                }

                ImGui::PopID();
            }

            if (ImGui::Button("Save world init")) {
                char new_filename[256];
                char timestamp[256];
                char filepath[PATH_MAX];

                w_timestamp_str(timestamp, 256);
                snprintf(new_filename, 256, "world_init_%s", timestamp);

                w_get_absolute_path(filepath, ABS_PROJECT_RESOURCE_PATH, new_filename);
                w_str_copy(game_state->game_init_config.default_world_init_path, filepath);

                w_file_write_bin(filepath, (char*)&game_state->world_init, sizeof(WorldInit));
                w_file_write_bin(ABS_GAME_INIT_PATH, (char*)&game_state->game_init_config, sizeof(GameInit));
            };

            ImGui::Text("Current World Init: %s", game_state->game_init_config.default_world_init_path);
        }

        if (ImGui::CollapsingHeader("Proc Gen")) {
            if (ImGui::TreeNode("Iron Ore")) {
                FBMContext* fbm_context = &game_state->world_gen_context.ore_fbm_context;
                ImGuiSliderFlags slider_flags = 0;

                ImGui::DragInt("Octaves", (int*)&fbm_context->octaves, 0.1f, 1, 16, "%i", slider_flags);
                ImGui::DragFloat("Amp", &fbm_context->amp, 0.1f, 0, 16, "%.4f", slider_flags);
                ImGui::DragFloat("Freq", &fbm_context->freq, 0.001f, 0, 4, "%.4f", slider_flags);
                ImGui::DragFloat("Gain", &fbm_context->gain, 0.1f, 0, 8, "%.4f", slider_flags);
                ImGui::DragFloat("Lacunarity", &fbm_context->lacunarity, 0.1f, 0, 8, "%.4f", slider_flags);
                if (ImGui::Button("Gen")) {
                    proc_gen_iron_ore(&game_state->entity_data, &game_state->world_gen_context.ore_fbm_context);
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Plants")) {
                FBMContext* fbm_context = &game_state->world_gen_context.plant_fbm_context;
                ImGuiSliderFlags slider_flags = 0;

                ImGui::DragInt("Octaves", (int*)&fbm_context->octaves, 0.1f, 1, 16, "%i", slider_flags);
                ImGui::DragFloat("Amp", &fbm_context->amp, 0.1f, 0, 16, "%.4f", slider_flags);
                ImGui::DragFloat("Freq", &fbm_context->freq, 0.001f, 0, 4, "%.4f", slider_flags);
                ImGui::DragFloat("Gain", &fbm_context->gain, 0.1f, 0, 8, "%.4f", slider_flags);
                ImGui::DragFloat("Lacunarity", &fbm_context->lacunarity, 0.1f, 0, 8, "%.4f", slider_flags);
                if (ImGui::Button("Gen")) {
                    proc_gen_plants(&game_state->decoration_data, fbm_context);
                }

                ImGui::TreePop();
            }
        }

        if (ImGui::CollapsingHeader("Render groups")) {
        }

        ImGui::End();
    } else {
        game_state->tools.selected_entity = ENTITY_TYPE_UNKNOWN;
    }
}

void tools_update_and_render(GameMemory* game_memory, GameState* game_state, GameInput* game_input,
                             RenderGroup* render_group) {
    ImGui::SetCurrentContext(game_memory->imgui_context);
    ImGuiIO* imgui_io = &ImGui::GetIO();
    // Note: tells imgui not to save ini file
    imgui_io->IniFilename = NULL;

    if (imgui_io->WantCaptureMouse || game_state->tools.selected_entity != ENTITY_TYPE_UNKNOWN) {
        set(game_state->tools.flags, TOOLS_F_CAPTURING_MOUSE_INPUT);
    } else {
        unset(game_state->tools.flags, TOOLS_F_CAPTURING_MOUSE_INPUT);
    }

    if (imgui_io->WantCaptureKeyboard) {
        set(game_state->tools.flags, TOOLS_F_CAPTURING_KEYBOARD_INPUT);
    } else {
        unset(game_state->tools.flags, TOOLS_F_CAPTURING_KEYBOARD_INPUT);
    }

    tools_update_input(game_input, game_state);

    Vec2 mouse_world_position = get_mouse_world_position(&game_state->camera, game_input, game_memory->window.size_px);
    WorldInitEntity* entity_inits = game_state->world_init.entity_inits;
    int hovered_over_entity_init = -1;

    if (!imgui_io->WantCaptureMouse) {
        for (int i = 0; i < game_state->world_init.entity_init_count; i++) {
            SpriteID sprite_id = entity_default_sprites[entity_inits[i].type];
            Vec2 target_position = entity_sprite_world_position(sprite_id, entity_inits[i].position, 0, false);
            Sprite sprite = entity_get_default_sprite(entity_inits[i].type);
            Rect target = {target_position.x, target_position.y, pixels_to_units(sprite.w), pixels_to_units(sprite.h)};
            if (w_check_point_in_rect(target, mouse_world_position)) {
                hovered_over_entity_init = i;
                break;
            }
        }

        if (game_input->mouse_state.input_states[MOUSE_LEFT_BUTTON].is_pressed) {
            if (game_state->tools.entity_palette_should_add_to_init) {
                if (hovered_over_entity_init != -1) {
                    if (entity_inits[hovered_over_entity_init].type != ENTITY_TYPE_PLAYER) {
                        entity_inits[hovered_over_entity_init] =
                            entity_inits[game_state->world_init.entity_init_count-- - 1];
                    }
                } else if (game_state->tools.selected_entity != ENTITY_TYPE_UNKNOWN) {
                    entity_inits[game_state->world_init.entity_init_count++] = {
                        .type = game_state->tools.selected_entity, .position = mouse_world_position};
                }
            } else {
                entity_create(&game_state->entity_data, game_state->tools.selected_entity, mouse_world_position);
            }
        }
    }

    if (game_state->tools.entity_palette_should_add_to_init) {
        for (int i = 0; i < game_state->world_init.entity_init_count; i++) {
            SpriteID sprite_id = entity_default_sprites[entity_inits[i].type];
            Vec4 tint = {1, 1, 1, 0.3};
            if (hovered_over_entity_init == i) {
                tint.x = 4;
            }

            Vec2 sprite_position = entity_sprite_world_position(sprite_id, entity_inits[i].position, 0, false);
            render_sprite(sprite_position, sprite_id, render_group, {.tint = tint, .opts = RENDER_SPRITE_OPT_TINT_SET});
        }
    }

    tools_render_panel(game_memory, game_state, game_input);
}
