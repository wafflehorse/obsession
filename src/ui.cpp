static FontData* i_font_data;
static uint32 i_pixels_per_unit;

enum UIPanelSlice {
    UI_PANEL_SLICE_TOP_LEFT,
    UI_PANEL_SLICE_TOP_EDGE,
    UI_PANEL_SLICE_TOP_RIGHT,
    UI_PANEL_SLICE_LEFT_EDGE,
    UI_PANEL_SLICE_CENTER,
    UI_PANEL_SLICE_RIGHT_EDGE,
    UI_PANEL_SLICE_BOTTOM_LEFT,
    UI_PANEL_SLICE_BOTTOM_EDGE,
    UI_PANEL_SLICE_BOTTOM_RIGHT,
    UI_PANEL_SLICE_COUNT
};

void init_ui(FontData* font_data, uint32 pixels_per_unit) {
    i_font_data = font_data;
	i_pixels_per_unit = pixels_per_unit;
}
// TODO: this doesn't work with wrapping
Vec2 get_text_size(const char* text, uint32 scale) {
    int i = 0;
    float pos_x = 0;
    float pos_y = 0;
    float line_height = i_font_data->baked_pixel_size + i_font_data->line_gap + 4;
    uint32 line_count = 1;

    stbtt_aligned_quad quad;
    float max_quad_x1 = 0;
    while (text[i] != '\0') {
        if (text[i] != '\n') {
            uint32 char_idx = (int)text[i] - 32;

            stbtt_GetPackedQuad(i_font_data->char_data,
                i_font_data->texture_dimensions.x,
                i_font_data->texture_dimensions.y,
                char_idx,
                &pos_x,
                &pos_y,
                &quad,
                0
            );

            max_quad_x1 = w_max(max_quad_x1, quad.x1);
        }
        else {
            pos_x = 0;
            line_count++;
        }

        i++;
    }

    return (Vec2) {
        .x = (max_quad_x1 * scale) / i_pixels_per_unit,
            .y = (line_count * line_height) / i_pixels_per_unit
    };
}

struct UIText {
    const char* text;
    Vec2 position; // top left
    uint32 font_scale;
    bool wrap;
    float max_x;
    Vec4 rgba;
};

struct DrawTextResult {
    float text_bottom_y;
    float text_right_x;
};

// TODO: should there be a single line version that's simpler and more performant?
DrawTextResult draw_text(UIText text_ui, RenderGroup* render_group) {
    float target_font_size = i_font_data->baked_pixel_size * text_ui.font_scale;
    float font_to_pixel_scale = target_font_size / (i_font_data->ascent - i_font_data->descent);
    float ascent = (font_to_pixel_scale * i_font_data->ascent) / i_pixels_per_unit;
    // Note: for the player start 2d font, the ascent_px is target font size and descent is 0.

    Vec2 text_start_pos = { text_ui.position.x, (text_ui.position.y - ascent) };
    // NOTE: pos_y must be inverted so that y increases downward and that's due to GetPackedQuad's assumptions
    float pos_x = 0;
    float pos_y = 0;

    int i = 0;
    while (text_ui.text[i] != '\0') {
        // This adjusts for the fact that our char_data starts at char 32 (ascii)
        uint32 char_index = (int)text_ui.text[i] - 32;

        if (text_ui.text[i] == (int)'\n') {
            pos_y += i_font_data->baked_pixel_size + i_font_data->line_gap + 4;
            pos_x = 0;
            i++;
            continue;
        }

        stbtt_aligned_quad quad;

        stbtt_GetPackedQuad(i_font_data->char_data,
            i_font_data->texture_dimensions.x,
            i_font_data->texture_dimensions.y,
            char_index,
            &pos_x,
            &pos_y,
            &quad,
            0
        );

        RenderQuad* render_quad = get_next_quad(render_group);;

        float world_quad_x0 = (quad.x0 * text_ui.font_scale) / i_pixels_per_unit;
        float world_quad_x1 = (quad.x1 * text_ui.font_scale) / i_pixels_per_unit;
        float world_quad_y0 = (quad.y0 * text_ui.font_scale) / i_pixels_per_unit;
        float world_quad_y1 = (quad.y1 * text_ui.font_scale) / i_pixels_per_unit;

        float world_quad_width = world_quad_x1 - world_quad_x0;
        float world_quad_height = world_quad_y1 - world_quad_y0;

		render_quad->world_position = {
            text_start_pos.x + world_quad_x0 + (world_quad_width / 2),
            text_start_pos.y - world_quad_y0 - (world_quad_height / 2),
		};
		render_quad->world_size = {
            world_quad_width,
            world_quad_height
		};

        stbtt_packedchar packed_char = i_font_data->char_data[char_index];

        render_quad->texture_unit = TEXTURE_ID_FONT;
        render_quad->sprite_position = {
            (float)packed_char.x0,
            (float)packed_char.y0
		};
		render_quad->sprite_size = {
            (float)packed_char.x1 - (float)packed_char.x0,
            (float)packed_char.y1 - (float)packed_char.y0
        };

        render_quad->rgba = text_ui.rgba;

        if (text_ui.wrap && char_index == ((int)' ' - 32)) {
            int j = i + 1;
            float tmp_pos_x = pos_x;
            float tmp_pos_y = pos_y;

            while (text_ui.text[j] != '\0') {
                uint32 char_idx = (int)text_ui.text[j] - 32;
                if (char_idx == ((int)' ' - 32)) {
                    break;
                }
                stbtt_aligned_quad test_quad;

                stbtt_GetPackedQuad(i_font_data->char_data,
                    i_font_data->texture_dimensions.x,
                    i_font_data->texture_dimensions.y,
                    char_idx,
                    &tmp_pos_x,
                    &tmp_pos_y,
                    &test_quad,
                    0
                );

                if ((text_start_pos.x + (test_quad.x1 * text_ui.font_scale / i_pixels_per_unit)) > text_ui.max_x) {
                    // NOTE: We divide by the font_scale here because stbtt_GetPackedQuad is expecting everything to be the baked font size
                    pos_y += i_font_data->baked_pixel_size + i_font_data->line_gap + 4;
                    pos_x = 0;
                    break;
                }
                j++;
            }
        }

        i++;
    }

    return (DrawTextResult) {
        .text_bottom_y = (text_start_pos.y - ((pos_y * text_ui.font_scale) / i_pixels_per_unit)),
            .text_right_x = (text_start_pos.x + ((pos_x * text_ui.font_scale) / i_pixels_per_unit)),
    };
}

struct UI_ProgressBar {
    Vec2 position;
    Vec2 dimensions;
    float progress_percentage;
    Vec4 background_color;
    Vec4 progress_color;
};

void draw_progress_bar(UI_ProgressBar progress_bar, float pixels_per_unit, RenderGroup* render_group) {
    RenderQuad* bar_background = get_next_quad(render_group);

	bar_background->world_position = progress_bar.position;
	bar_background->world_size = progress_bar.dimensions;
    bar_background->draw_colored_rect = 1.0;
    bar_background->rgba = progress_bar.background_color;

    RenderQuad* bar_progress = get_next_quad(render_group);

    float progress_width = progress_bar.dimensions.x * progress_bar.progress_percentage;
    float bar_left_position = progress_bar.position.x - (progress_bar.dimensions.x / 2);

    bar_progress->world_position = {
        bar_left_position + (progress_width / 2),
        progress_bar.position.y
	};

	bar_progress->world_size = {
        progress_width,
        progress_bar.dimensions.y
    };
    bar_progress->draw_colored_rect = 1.0;
    bar_progress->rgba = progress_bar.progress_color;
}

enum UIContainerDirection {
	UI_CONTAINER_DIRECTION_ROW,
	UI_CONTAINER_DIRECTION_COL
};

enum UIElementType {
    UI_ELEMENT_CONTAINER,
    UI_ELEMENT_SPRITE,
    UI_ELEMENT_TEXT,
    UI_ELEMENT_SPACER,
    UI_ELEMENT_TYPE_COUNT,
};

struct UIElementSpriteData {
    Vec2 sprite_pos;
    Vec2 sprite_size;
};

struct UIElementTextData {
    char text[256];
    Vec4 rgba;
    uint32 scale;
};

typedef struct UIElement UIElement;

struct UIElementArray {
    UIElement* elements;
    uint32 capacity;
    uint32 count;
};

struct UIElementContainerData {
    Vec2 position;
    UIContainerDirection direction;
    float margin;
    float child_gap;
    UIElementArray child_array;
};

struct UIElement {
    UIElementType type;
    Vec2 size;
    union {
        UIElementSpriteData sprite_data;
        UIElementTextData text_data;
        UIElementContainerData container_data;
    };
};

struct UIContainer {
    Vec2 position;
    UIContainerDirection direction;
    float margin;
    float item_gap;
    UIElement items[10];
    uint32 item_count;
};

// TODO: could be worth having methods for getting element specific data. Each one would assert that the element you are passing in matches the type you expect

UIElement* ui_create_container(float margin, float child_gap, UIContainerDirection direction, uint32 num_children, Arena* arena) {
    UIElement* container = (UIElement*)w_arena_alloc(arena, sizeof(UIElement));

    container->type = UI_ELEMENT_CONTAINER;

    UIElementContainerData* container_data = &container->container_data;
    container_data->margin = margin;
    container_data->child_gap = child_gap;
    container_data->direction = direction;

    UIElementArray* child_array = &container_data->child_array;

    child_array->elements = (UIElement*)w_arena_alloc(arena, num_children * sizeof(UIElement));
    child_array->capacity = num_children;
    child_array->count = 0;

    return container;
}

void ui_add_sprite_element(UIElement* container, Vec2 sprite_pos, Vec2 sprite_size, Vec2 size) {
    ASSERT(container->type == UI_ELEMENT_CONTAINER, "add_sprite_element must only operate on container elements");

    UIElementContainerData* container_data = &container->container_data;

    UIElementArray* child_array = &container_data->child_array;
    UIElement* child = &child_array->elements[child_array->count++];

    ASSERT(child_array->count <= child_array->capacity, "UIElement container max child count overflow!");

    child->sprite_data.sprite_pos = sprite_pos;
    child->sprite_data.sprite_size = sprite_size;
    child->size = size;
    child->type = UI_ELEMENT_SPRITE;
}

void ui_add_text_element(UIElement* container, const char* text, uint32 scale, Vec4 rgba) {
    ASSERT(container->type == UI_ELEMENT_CONTAINER, "add_text_element must only operate on container elements");

    UIElementContainerData* container_data = &container->container_data;
    UIElementArray* child_array = &container_data->child_array;
    UIElement* child = &child_array->elements[child_array->count++];

    ASSERT(child_array->count <= child_array->capacity, "UIElement container max child count overflow!");

    child->type = UI_ELEMENT_TEXT;
    child->size = get_text_size(text, scale);
    w_str_copy(child->text_data.text, text);
    child->text_data.scale = scale;
    child->text_data.rgba = rgba;
};

void ui_add_spacer_element(UIElement* container, float spacing) {
    ASSERT(container->type == UI_ELEMENT_CONTAINER, "add_spacer_element must only operate on container elements");

    UIElementContainerData* container_data = &container->container_data;
    UIElementArray* child_array = &container_data->child_array;
    UIElement* child = &child_array->elements[child_array->count++];

    ASSERT(child_array->count <= child_array->capacity, "UIElement container max child count overflow!");

    child->type = UI_ELEMENT_SPACER;

    if (container_data->direction == UI_CONTAINER_DIRECTION_ROW) {
        child->size.x = spacing;
    }
    else {
        child->size.y = spacing;
    }
}

// TODO: Maybe this should be a generic get_element_size function?
Vec2 ui_get_container_size(UIElement* container) {
    ASSERT(container->type == UI_ELEMENT_CONTAINER, "get_container_size must only operate on container elements");

    UIElementContainerData* container_data = &container->container_data;
    UIElementArray* child_array = &container_data->child_array;

    // TODO: Add support for column container
    Vec2 size = {};
    if (container_data->direction == UI_CONTAINER_DIRECTION_ROW) {
        size.x += (container_data->margin * 2) + (container_data->child_gap * (container_data->child_array.count - 1));

        float item_max_height = 0;
        for (int i = 0; i < child_array->count; i++) {
            UIElement* child = &child_array->elements[i];

            item_max_height = w_max(item_max_height, child->size.y);
            size.x += child->size.x;
        }

        size.y = (2 * container_data->margin) + item_max_height;
    }
    else {
        size.y += (container_data->margin * 2) + (container_data->child_gap * (container_data->child_array.count - 1));

        float item_max_width = 0;
        for (int i = 0; i < child_array->count; i++) {
            UIElement* child = &child_array->elements[i];

            item_max_width = w_max(item_max_width, child->size.x);
            size.y += child->size.y;
        }

        size.x = (2 * container_data->margin) + item_max_width;
    }

    return size;
}

#define UI_CONTAINER_DRAW_F_LEFT_ALIGN (1 << 0)

// TODO: Maybe this should be a generic draw element function?
void ui_draw_container(UIElement* container, RenderGroup* render_group, uint32 opts) {
    ASSERT(container->type == UI_ELEMENT_CONTAINER, "get_container_size must only operate on container elements");
    UIElementContainerData* container_data = &container->container_data;

    Vec2 container_size = ui_get_container_size(container);

    float current_x;
    float current_y;
    if (container_data->direction == UI_CONTAINER_DIRECTION_ROW) {
        current_x = container_data->position.x - (container_size.x / 2) + container_data->margin;
        current_y = container_data->position.y;
    }
    else {
        current_x = container_data->position.x;
        current_y = container_data->position.y + (container_size.y / 2) - container_data->margin;
    }

    UIElementArray* child_array = &container_data->child_array;

    for (int i = 0; i < child_array->count; i++) {
        UIElement* child = &child_array->elements[i];

        Vec2 child_position = {};
        if (container_data->direction == UI_CONTAINER_DIRECTION_ROW) {
            child_position.x = current_x + (child->size.x / 2);
            child_position.y = current_y;
            current_x += child->size.x + container_data->child_gap;
        }
        else {
			if(is_set(opts, UI_CONTAINER_DRAW_F_LEFT_ALIGN)) {
				child_position.x = container_data->position.x - (container_size.x / 2) + (child->size.x / 2) + container_data->margin;
			}
			else {
            	child_position.x = container_data->position.x;
			}
            child_position.y = current_y - (child->size.y / 2);
            current_y -= child->size.y + container_data->child_gap;
        }

        if (child->type == UI_ELEMENT_SPRITE) {
            RenderQuad* quad = get_next_quad(render_group);
            UIElementSpriteData* sprite_data = &child->sprite_data;

			quad->sprite_position = sprite_data->sprite_pos;
			quad->sprite_size = sprite_data->sprite_size;
			quad->world_position = child_position;
			quad->world_size = child->size;
        }
        else if (child->type == UI_ELEMENT_TEXT) {
            UIElementTextData* text_data = &child->text_data;
            UIText ui_text = {
                .text = text_data->text,
                .position = {
                    child_position.x - (child->size.x / 2),
                    child_position.y + (child->size.y / 2)
                },
                .font_scale = text_data->scale,
                .rgba = text_data->rgba
            };

            draw_text(ui_text, render_group);
        }
    }
}

// static Sprite panel_slice_sprites[UI_PANEL_SLICE_COUNT] = {
//     [UI_PANEL_SLICE_TOP_LEFT] = {
//         .position = { 0, 128 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_TOP_EDGE] = {
//         .position = { 16, 128 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_TOP_RIGHT] = {
//         .position = { 32, 128 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_LEFT_EDGE] = {
//         .position = { 0, 144 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_CENTER] = {
//         .position = { 16, 144 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_RIGHT_EDGE] = {
//         .position = { 32, 144 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_BOTTOM_LEFT] = {
//         .position = { 0, 160 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_BOTTOM_EDGE] = {
//         .position = { 16, 160 },
//         .size = { 16, 16 }
//     },
//     [UI_PANEL_SLICE_BOTTOM_RIGHT] = {
//         .position = { 32, 160 },
//         .size = { 16, 16 }
//     }
// };

// void ui_draw_panel(Vec2 position, Vec2 size, RenderGroup* render_group) {
//     float middle_y = position.y;
//     float top_y = middle_y + (size.y / 2) - 0.5;
//     float bottom_y = middle_y - (size.y / 2) + 0.5;
//
//     float left_x = position.x - (size.x / 2) + 0.5;
//     float right_x = position.x + (size.x / 2) - 0.5;
//     float middle_x = position.x;
//
//     float stretch_x = size.x - 2; // Subtracting 2 for both corners	
//     float stretch_y = size.y - 2; // Subtracting 2 for both corners
//
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_TOP_LEFT], { left_x, top_y }, { 1, 1 }, render_group);
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_TOP_EDGE], { middle_x, top_y }, { stretch_x, 1 }, render_group);
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_TOP_RIGHT], { right_x, top_y }, { 1, 1 }, render_group);
//
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_LEFT_EDGE], { left_x, middle_y }, { 1, stretch_y }, render_group);
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_CENTER], position, { stretch_x, stretch_y }, render_group);
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_RIGHT_EDGE], { right_x, middle_y }, { 1, stretch_y }, render_group);
//
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_BOTTOM_LEFT], { left_x, bottom_y }, { 1, 1 }, render_group);
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_BOTTOM_EDGE], { middle_x, bottom_y }, { stretch_x, 1 }, render_group);
//     render_sprite(panel_slice_sprites[UI_PANEL_SLICE_BOTTOM_RIGHT], { right_x, bottom_y }, { 1, 1 }, render_group);
// }



