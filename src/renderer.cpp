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

// TODO: are we going to use this?
RenderQuad* render_sprite(Vec2 sprite_position, Vec2 sprite_size, Vec2 position, Vec2 size, RenderGroup* render_group) {
    RenderQuad* quad = get_next_quad(render_group);

    quad->world_position = position;
    quad->world_size = size;
    quad->sprite_position = sprite_position;
    quad->sprite_size = sprite_size;

    return quad;
}
