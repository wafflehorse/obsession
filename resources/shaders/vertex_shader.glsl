#version 330 core

layout (location = 0) in vec3 local_position;
layout (location = 1) in vec2 world_position; 
layout (location = 2) in vec2 world_size;
layout (location = 3) in vec2 sprite_position;
layout (location = 4) in vec2 sprite_size;
layout (location = 5) in float rotation_rads;
layout (location = 6) in vec4 rgba_;
layout (location = 7) in int texture_unit_;
layout (location = 8) in int draw_colored_rect_;
layout (location = 9) in vec4 tint_;
layout (location = 10) in int flip_x;

flat out vec4 rgba;
flat out float draw_colored_rect;
flat out vec4 tint;
flat out int texture_unit;
out vec2 texel_coords;

uniform mat4 projection_matrix;
uniform vec3 camera_position;
uniform mat4 texture_matrix; // maps local text coords to opengl coords
uniform mat4 texture_matrices[2];

void main()
{
    float cos_theta = cos(rotation_rads);
    float sin_theta = sin(rotation_rads);
    mat4 model_matrix = mat4(1.0);
    model_matrix[0][0] = world_size.x * cos_theta;
    model_matrix[0][1] = world_size.x * sin_theta;
    model_matrix[1][0] = -world_size.y * sin_theta;
    model_matrix[1][1] = world_size.y * cos_theta;
    model_matrix[3][0] = world_position.x;
    model_matrix[3][1] = world_position.y;

    vec4 world_vertex_position = model_matrix * vec4(local_position, 1.0f);
    vec4 camera_rel_position = world_vertex_position - vec4(camera_position, 0.0f);

    // The order of this matters
    gl_Position = projection_matrix * camera_rel_position;

	mat4 texture_model_matrix = mat4(1.0);
	texture_model_matrix[0][0] = sprite_size.x; // width
	texture_model_matrix[1][1] = sprite_size.y; // height
	texture_model_matrix[3][0] = sprite_position.x;
	texture_model_matrix[3][1] = sprite_position.y;

	vec4 sprite_local_position = vec4(local_position.x + 0.5, 0.5 - local_position.y, local_position.z, 1.0f);

	if(flip_x == 1) {
		sprite_local_position.x = 1.0 - sprite_local_position.x;
	}

    vec4 sprite_uv_position = texture_model_matrix * sprite_local_position;

    vec4 texel_vec4 = texture_matrices[texture_unit_] * sprite_uv_position;

	texture_unit = texture_unit_;
	texel_coords = vec2(texel_vec4.x, texel_vec4.y);
    draw_colored_rect = draw_colored_rect_;
    rgba = rgba_;
    tint = tint_;
}
