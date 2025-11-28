#include "waffle_lib.h"

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
Vec2 get_text_size(const char* text, float scale) {
    int i = 0;
    float pos_x = 0;
    float pos_y = 0;
    float line_height = (i_font_data->baked_pixel_size + i_font_data->line_gap + 4) * scale;
    uint32 line_count = 1;

    stbtt_aligned_quad quad;
    float max_quad_x1 = 0;
    while (text[i] != '\0') {
        if (text[i] != '\n') {
            uint32 char_idx = (int)text[i] - 32;

            stbtt_GetPackedQuad(i_font_data->char_data, i_font_data->texture_dimensions.x,
                                i_font_data->texture_dimensions.y, char_idx, &pos_x, &pos_y, &quad, 0);

            max_quad_x1 = w_max(max_quad_x1, quad.x1);
        } else {
            pos_x = 0;
            line_count++;
        }

        i++;
    }

    return (Vec2){.x = (max_quad_x1 * scale) / i_pixels_per_unit, .y = (line_count * line_height) / i_pixels_per_unit};
}

struct UIText {
    const char* text;
    Vec2 position; // top left
    float font_scale;
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

    Vec2 text_start_pos = {text_ui.position.x, (text_ui.position.y - ascent)};
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

        stbtt_GetPackedQuad(i_font_data->char_data, i_font_data->texture_dimensions.x,
                            i_font_data->texture_dimensions.y, char_index, &pos_x, &pos_y, &quad, 0);

        RenderQuad* render_quad = get_next_quad(render_group);
        ;

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
        render_quad->world_size = {world_quad_width, world_quad_height};

        stbtt_packedchar packed_char = i_font_data->char_data[char_index];

        render_quad->texture_unit = TEXTURE_ID_FONT;
        render_quad->sprite_position = {(float)packed_char.x0, (float)packed_char.y0};
        render_quad->sprite_size = {(float)packed_char.x1 - (float)packed_char.x0,
                                    (float)packed_char.y1 - (float)packed_char.y0};

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

                stbtt_GetPackedQuad(i_font_data->char_data, i_font_data->texture_dimensions.x,
                                    i_font_data->texture_dimensions.y, char_idx, &tmp_pos_x, &tmp_pos_y, &test_quad, 0);

                if ((text_start_pos.x + (test_quad.x1 * text_ui.font_scale / i_pixels_per_unit)) > text_ui.max_x) {
                    // NOTE: We divide by the font_scale here because stbtt_GetPackedQuad is expecting everything to be
                    // the baked font size
                    pos_y += i_font_data->baked_pixel_size + i_font_data->line_gap + 4;
                    pos_x = 0;
                    break;
                }
                j++;
            }
        }

        i++;
    }

    return (DrawTextResult){
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

    bar_progress->world_position = {bar_left_position + (progress_width / 2), progress_bar.position.y};

    bar_progress->world_size = {progress_width, progress_bar.dimensions.y};
    bar_progress->draw_colored_rect = 1.0;
    bar_progress->rgba = progress_bar.progress_color;
}

#define UI_ELEMENT_F_CONTAINER_ROW (1 << 0)
#define UI_ELEMENT_F_CONTAINER_COL (1 << 1)
#define UI_ELEMENT_F_DRAW_TEXT (1 << 2)
#define UI_ELEMENT_F_DRAW_SPRITE (1 << 3)
#define UI_ELEMENT_F_DRAW_BACKGROUND (1 << 4)

struct UIElement {
    UIElement* parent;
    UIElement* child;
    UIElement* next;
    UIElement* last_child;

    flags flags;

    Vec2 position;
    Vec2 rel_position; // top-left
    Vec2 size;
    Vec2 min_size;
    Vec2 max_size;

    char text[256];
    Vec4 rgba;
    float font_scale;

    Sprite sprite;

    Vec4 background_rgba;

    float padding;
    float child_gap;
};

struct UIContainerCreateParams {
    float padding;
    float child_gap;
    Vec2 min_size;
    Vec2 max_size;
    Vec4 background_rgba;
    flags opts;
};

UIElement* ui_create_container(UIContainerCreateParams params, Arena* arena) {
    UIElement* container = (UIElement*)w_arena_alloc(arena, sizeof(UIElement));

    ASSERT(is_set(params.opts, UI_ELEMENT_F_CONTAINER_ROW) || is_set(params.opts, UI_ELEMENT_F_CONTAINER_COL),
           "Must set container direction");
    ASSERT(params.min_size.x <= params.max_size.x && params.min_size.y <= params.max_size.y,
           "Min size is greater than max size");

    container->padding = params.padding;
    container->child_gap = params.child_gap;
    container->flags = params.opts;
    container->min_size = params.min_size;
    container->size = {(float)w_max(params.min_size.x, params.padding * 2),
                       (float)w_max(params.min_size.y, params.padding * 2)};
    container->max_size = params.max_size;
    container->background_rgba = params.background_rgba;

    return container;
}

Vec2 ui_container_content_size(UIElement* container) {
    Vec2 size = {};
    uint32 num_children = 0;
    UIElement* child = container->child;
    while (child) {
        if (is_set(container->flags, UI_ELEMENT_F_CONTAINER_ROW)) {
            size.x += child->size.x;
            size.y = w_max(size.y, child->size.y);
        } else {
            size.y += child->size.y;
            size.x = w_max(size.x, child->size.x);
        }
        num_children++;
        child = child->next;
    }

    if (is_set(container->flags, UI_ELEMENT_F_CONTAINER_ROW)) {
        size.x += container->child_gap * (num_children - 1);
    } else {
        size.y += container->child_gap * (num_children - 1);
    }

    return size;
}

UIElement* ui_create_sprite(Sprite sprite, Arena* arena) {
    UIElement* sprite_element = (UIElement*)w_arena_alloc(arena, sizeof(UIElement));

    sprite_element->sprite = sprite;
    sprite_element->size = {sprite.w / BASE_PIXELS_PER_UNIT, sprite.h / BASE_PIXELS_PER_UNIT};
    sprite_element->flags = UI_ELEMENT_F_DRAW_SPRITE;

    return sprite_element;
}

UIElement* ui_create_text(const char* text, Vec4 rgba, float font_scale, Arena* arena) {
    UIElement* text_element = (UIElement*)w_arena_alloc(arena, sizeof(UIElement));

    w_str_copy(text_element->text, text);
    text_element->rgba = rgba;
    text_element->font_scale = font_scale;
    text_element->size = get_text_size(text, font_scale);
    text_element->flags = UI_ELEMENT_F_DRAW_TEXT;

    return text_element;
}

UIElement* ui_create_spacer(Vec2 size, Arena* arena) {
    UIElement* spacer_element = (UIElement*)w_arena_alloc(arena, sizeof(UIElement));

    spacer_element->size = size;

    return spacer_element;
}

// TODO: add support for max width handling
void ui_push(UIElement* parent, UIElement* child) {
    child->parent = parent;

    if (!parent->child) {
        child->rel_position = {parent->padding, -parent->padding};
        parent->child = child;
        parent->last_child = child;
    } else {
        UIElement* last_child = parent->last_child;
        if (is_set(parent->flags, UI_ELEMENT_F_CONTAINER_ROW)) {
            parent->size.x += parent->child_gap;
            child->rel_position.x = last_child->rel_position.x + last_child->size.x + parent->child_gap;
            child->rel_position.y = last_child->rel_position.y;
        } else {
            parent->size.y += parent->child_gap;
            child->rel_position.y = last_child->rel_position.y - last_child->size.y - parent->child_gap;
            child->rel_position.x = last_child->rel_position.x;
        }
        parent->last_child->next = child;
        parent->last_child = child;
    }

    Vec2 content_size = ui_container_content_size(parent);
    parent->size.x = w_max(parent->size.x, content_size.x + (parent->padding * 2));
    parent->size.y = w_max(parent->size.y, content_size.y + (parent->padding * 2));
    if (parent->max_size.x > 0 && parent->max_size.y > 0) {
        parent->size.x = w_min(parent->size.x, parent->max_size.x);
        parent->size.y = w_min(parent->size.y, parent->max_size.y);
    }
}

// TODO: without a z-index specified on the child elements, the absolutely positioned elements
// will render first and therefore behind the relatively positioned elements
void ui_push_abs_position(UIElement* parent, UIElement* child, Vec2 rel_position) {
    child->parent = parent;

    if (parent->child) {
        child->next = parent->child;
    }

    parent->child = child;
    child->rel_position = rel_position;
}

void ui_draw_element(UIElement* element, Vec2 position, RenderGroup* render_group) {
    element->position = position; // position is top_left

    if (is_set(element->flags, UI_ELEMENT_F_DRAW_BACKGROUND)) {
        RenderQuad* quad = get_next_quad(render_group);

        quad->draw_colored_rect = 1;
        quad->rgba = element->background_rgba;
        quad->world_size = element->size;
        quad->world_position = {position.x + (quad->world_size.x / 2), position.y - (quad->world_size.y / 2)};
    }

    if (is_set(element->flags, UI_ELEMENT_F_DRAW_TEXT)) {
        UIText ui_text = {.text = element->text,
                          .position = position,
                          .font_scale = (float)element->font_scale,
                          .rgba = element->rgba};

        draw_text(ui_text, render_group);
    }

    if (is_set(element->flags, UI_ELEMENT_F_DRAW_SPRITE)) {
        RenderQuad* quad = get_next_quad(render_group);

        // TODO: No supported scaling
        quad->world_size = element->size;
        quad->world_position = {position.x + (quad->world_size.x / 2), position.y - (quad->world_size.y / 2)};
        quad->sprite_position = {element->sprite.x, element->sprite.y};
        quad->sprite_size = {element->sprite.w, element->sprite.h};
        quad->texture_unit = TEXTURE_ID_SPRITE;
    }

    UIElement* child = element->child;
    while (child) {
        Vec2 child_position = w_vec_add(element->position, child->rel_position);
        ui_draw_element(child, child_position, render_group);
        child = child->next;
    }
}
