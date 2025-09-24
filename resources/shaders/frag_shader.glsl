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
            float alpha = texture(texture_units[texture_unit], texel_coords).r;
            FragColor = vec4(rgba.x / 255.0, rgba.y / 255.0, rgba.z / 255.0, alpha * rgba.w);
        }
        else {
            FragColor = texture(texture_units[texture_unit], texel_coords) * tint;
        }

    }
}
