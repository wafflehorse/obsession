#pragma once
#include <limits.h>

#define MAX_RENDER_GROUPS 10
#define MAX_LOADED_TEXTURES 2

#define RENDER_OPTION_SCISSOR_F (1 << 0) 

struct RenderQuad
{
	Vec2 world_position;
	Vec2 world_size;
	Vec2 sprite_position;
	Vec2 sprite_size;
    float rotation_rads;
    Vec4 rgba;
	uint32 texture_unit;
    uint32 draw_colored_rect;
    Vec4 tint;
	uint32 flip_x;
	float z_index;
};

struct RenderGroupOptions {
    flags flags;
    Rect scissor_rect; // x,y centered
};

struct RenderGroup {
	uint32 id;
    RenderQuad* quads;
    uint32 count;
    uint32 size;
    RenderGroupOptions opts;
};

#define PUSH_RENDER_GROUP(name) void name(RenderQuad *instance_data, uint32 instance_count, Vec2 camera_position, RenderGroupOptions opts)
typedef PUSH_RENDER_GROUP(PushRenderGroup);

#define LOAD_TEXTURE(name) int name(uint32 texture_id, const char* file_path)
typedef LOAD_TEXTURE(LoadTexture);

#define INITIALIZE_RENDERER(name) int name(Vec2 viewport, Vec2 screen_size, Vec2 camera_size, Arena* arena)
typedef INITIALIZE_RENDERER(InitializeRenderer);

#define SET_VIEWPORT(name) void name(Vec2 viewport, Vec2 screen_size)
typedef SET_VIEWPORT(SetViewport);
