#include "renderer.h"

RenderQuad* get_next_quad(RenderGroup* render_group) {
    RenderQuad* quad = &render_group->quads[render_group->count++];

    char assert_message[100];
    snprintf(assert_message, 100, "Render group reached capacity. Render group id: %i\n", render_group->id);
    ASSERT(render_group->count <= render_group->size, assert_message);

    quad->tint = {1, 1, 1, 1};
    quad->z_index = 1.0;

    return quad;
}

int render_group_cmp(const void* a, const void* b) {
    const RenderQuad* quad_a = (const RenderQuad*)a;
    const RenderQuad* quad_b = (const RenderQuad*)b;

    if (quad_a->z_index < quad_b->z_index)
        return -1;
    if (quad_a->z_index > quad_b->z_index)
        return 1;
    return 0;
}

void sort_render_group(RenderGroup* render_group) {
    qsort(render_group->quads, render_group->count, sizeof(RenderQuad), render_group_cmp);
}

void render_borders(float border_width, Vec4 border_color, Vec2 world_subject_position, Vec2 world_subject_size,
                    RenderGroup* render_group) {
    RenderQuad* top = get_next_quad(render_group);
    top->world_size = {world_subject_size.x, border_width};
    top->world_position = {world_subject_position.x,
                           world_subject_position.y + (world_subject_size.y * 0.5f) - (border_width * 0.5f)};
    top->draw_colored_rect = 1;
    top->rgba = border_color;

    RenderQuad* bottom = get_next_quad(render_group);
    bottom->world_size = {world_subject_size.x, border_width};
    bottom->world_position = {world_subject_position.x,
                              world_subject_position.y - (world_subject_size.y * 0.5f) + (border_width * 0.5f)};
    bottom->draw_colored_rect = 1;
    bottom->rgba = border_color;

    RenderQuad* left = get_next_quad(render_group);
    left->world_size = {border_width, world_subject_size.y};
    left->world_position = {world_subject_position.x - (world_subject_size.x * 0.5f) + (border_width * 0.5f),
                            world_subject_position.y};
    left->draw_colored_rect = 1;
    left->rgba = border_color;

    RenderQuad* right = get_next_quad(render_group);
    right->world_size = {border_width, world_subject_size.y};
    right->world_position = {world_subject_position.x + (world_subject_size.x * 0.5f) - (border_width * 0.5f),
                             world_subject_position.y};
    right->draw_colored_rect = 1;
    right->rgba = border_color;
}

// TODO: are we going to use this?
RenderQuad* render_sprite(Vec2 sprite_position, Vec2 sprite_size, Vec2 position, Vec2 size, RenderGroup* render_group) {
    RenderQuad* quad = get_next_quad(render_group);

    quad->world_position = position;
    quad->world_size = size;
    quad->sprite_position = sprite_position;
    quad->sprite_size = sprite_size;

    return quad;
}
