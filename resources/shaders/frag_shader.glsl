#version 330 core

out vec4 FragColor;

flat in vec4 rgba;
flat in float draw_colored_rect;
flat in vec4 tint;
flat in int texture_unit;
in vec2 texel_coords;

uniform sampler2D texture_units[2];
uniform int texture_num_channels[2];

void main()
{
    if(draw_colored_rect != 0.0) {
        FragColor = vec4(rgba.x / 255.0, rgba.y / 255.0, rgba.z / 255.0, rgba.w) * tint;
    } 
    else {
        //For text rendering
        if(texture_num_channels[texture_unit] == 1) {
            float alpha;
			// This is a hack for now cause non-constant indexing into sampler2D not allowed (but mac allowed it)
			if (texture_unit == 0) {
				alpha = texture(texture_units[0], texel_coords).r;
			}
			if (texture_unit == 1) {
				alpha = texture(texture_units[1], texel_coords).r;
			}
            FragColor = vec4(rgba.x / 255.0, rgba.y / 255.0, rgba.z / 255.0, alpha * rgba.w);
        }
        else {
            vec4 texture_color;
			// This is a hack for now cause non-constant indexing into sampler2D not allowed (but mac allowed it)
			if (texture_unit == 0) {
				texture_color = texture(texture_units[0], texel_coords);
			}
			if (texture_unit == 1) {
				texture_color = texture(texture_units[1], texel_coords);
			}
            FragColor = texture_color * tint;
        }

    }
}
