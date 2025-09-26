#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include "string.h"
#include "renderer.h"
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION

// this surpressed the unused function warning in the lib
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma clang diagnostic pop

#define MAX_VBO_INSTANCE_COUNT 5000

void get_path_with_base(const char* file_path, char* result);

struct OpenGLTexture {
    uint32 id;
    uint32 width;
    uint32 height;
	uint32 num_channels;
	bool initialized;
};

struct OpenGLRenderData {
    uint32 shader_program_id;

    OpenGLTexture textures[MAX_LOADED_TEXTURES];
	uint32 texture_count;

    uint32 quad_vao_id;
    uint32 quad_local_vbo_id;
    uint32 quad_instance_vbo_id;
};

static OpenGLRenderData render_data = {};

uint32 load_and_compile_shader(const char* shader_file_path, GLenum shader_type, Arena* arena)
{
	char* arena_marker = arena->next;
	FileContents file_contents = {};	
	if(w_read_file(shader_file_path, &file_contents, arena) != 0) {
		fprintf(stderr, "Failed to read shader file: %s\n", shader_file_path);
		return 0;
	}

    unsigned int shader_id;
    shader_id = glCreateShader(shader_type);
    glShaderSource(shader_id, 1, &file_contents.data, NULL);

    glCompileShader(shader_id);

    int success;
    char infoLog[512];
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(shader_id, 512, NULL, infoLog);
        fprintf(stderr, "Error compiling shader file: %s\n", shader_file_path);
        fprintf(stderr, "Error info: %s\n", infoLog);
        return 0;
    }

	w_arena_restore(arena, arena_marker);

    return shader_id;
}

unsigned int create_and_link_gl_program(unsigned int vertex_shader, unsigned int frag_shader)
{
    uint32 shader_program;
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, frag_shader);
    glLinkProgram(shader_program);

    int success;
    char infoLog[512];
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shader_program, 512, NULL, infoLog);
        fprintf(stderr, "Error linking shader program: %s\n", infoLog);
        return 0;
    }

    return shader_program;
}

void set_projection_matrix(float width, float height) {
    // This matrix translates from world coordinates to uv coords from -1 to 1
    float projection_matrix[] = {
        2.0f / width, 0.0f, 0.0f, 0.0f,          // row 1
        0.0f, (2.0f / height), 0.0f, 0.0f, // row 2
        0.0f, 0.0f, 1.0f, 0.0f,                          // row 3
        0.0f, 0.0f, 0.0f, 1.0f                          // row 4
    };

    glUniformMatrix4fv(glGetUniformLocation(render_data.shader_program_id, "projection_matrix"), 1, GL_FALSE, projection_matrix);
}

SET_VIEWPORT(set_viewport) {
	Vec2 size_diff = w_sub_vec(screen_size, viewport);
	Vec2 viewport_position = {
		size_diff.x / 2,
		size_diff.y / 2
	};

    glViewport(viewport_position.x, viewport_position.y, viewport.x, viewport.y);
}

INITIALIZE_RENDERER(initialize_renderer) {
	set_viewport(viewport, screen_size);

    // This enables transparency in our texture
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int vertex_shader = load_and_compile_shader("resources/shaders/vertex_shader.glsl", GL_VERTEX_SHADER, arena);
    if (vertex_shader == 0)
    {
        return 1;
    }

    unsigned int frag_shader = load_and_compile_shader("resources/shaders/frag_shader.glsl", GL_FRAGMENT_SHADER, arena);
    if (vertex_shader == 0)
    {
        return 1;
    }

    unsigned int shader_program = create_and_link_gl_program(vertex_shader, frag_shader);

    render_data.shader_program_id = shader_program;

    glDeleteShader(vertex_shader);
    glDeleteShader(frag_shader);

    // Since we only ever use one shader program, we can just set it and forget it
    glUseProgram(shader_program);

    // this might already happen by default but doing it anyways to be explicit
    glUniform1i(glGetUniformLocation(shader_program, "our_texture"), 0); // this maps our_texture to texture unit 0
	
	int texture_units[MAX_LOADED_TEXTURES] = { 0, 1 };
    glUniform1iv(glGetUniformLocation(shader_program, "texture_units"), MAX_LOADED_TEXTURES, texture_units);

    // Maps from our world space to opengl clip space (NDC)
    set_projection_matrix(camera_size.x, camera_size.y);

    glGenVertexArrays(1, &render_data.quad_vao_id);
    glGenBuffers(1, &render_data.quad_local_vbo_id);
    glGenBuffers(1, &render_data.quad_instance_vbo_id);

    glBindVertexArray(render_data.quad_vao_id);

    // defines local space used for both geometry and texture
    float vertices[] = {
        -0.5f, 0.5f, 0.0f, // top left
        0.5f, 0.5f, 0.0f, // top right
        -0.5f, -0.5f, 0.0f, // bottom left
        0.5f, 0.5f, 0.0f, // top right
        -0.5f, -0.5f, 0.0f, // bottom left
        0.5f, -0.5f, 0.0f  // bottom right
    };

    // Setup local space vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, render_data.quad_local_vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // have to do glVertexAttribPointer before unbinding the VBO, because they refer to the currently bound GL_ARRAY_BUFFER
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Setup instance vbo: Will store each entity rect for use in model matrix
    glBindBuffer(GL_ARRAY_BUFFER, render_data.quad_instance_vbo_id);

    // Initialize vbo instance buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(RenderQuad) * MAX_VBO_INSTANCE_COUNT, NULL, GL_DYNAMIC_DRAW);

    /// world_position
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(RenderQuad), (void*)offsetof(RenderQuad, world_position));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1); // This ensures that the instance data changes for each instance

    // world_size
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderQuad), (void*)offsetof(RenderQuad, world_size));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    // sprite_position
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(RenderQuad), (void*)offsetof(RenderQuad, sprite_position));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    // sprite_size
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(RenderQuad), (void*)offsetof(RenderQuad, sprite_size));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);

	// rotation_rads
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(RenderQuad), (void*)offsetof(RenderQuad, rotation_rads));
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);

    // rgba
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(RenderQuad), (void*)offsetof(RenderQuad, rgba));
    glEnableVertexAttribArray(6);
    glVertexAttribDivisor(6, 1);

    // texture_unit
    glVertexAttribIPointer(7, 1, GL_INT, sizeof(RenderQuad), (void*)offsetof(RenderQuad, texture_unit));
    glEnableVertexAttribArray(7);
    glVertexAttribDivisor(7, 1);

    // draw_colored_rect
    glVertexAttribIPointer(8, 1, GL_INT, sizeof(RenderQuad), (void*)offsetof(RenderQuad, draw_colored_rect));
    glEnableVertexAttribArray(8);
    glVertexAttribDivisor(8, 1);
	
    // tint
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(RenderQuad), (void*)offsetof(RenderQuad, tint));
    glEnableVertexAttribArray(9);
    glVertexAttribDivisor(9, 1);

	// flip_x
    glVertexAttribIPointer(10, 1, GL_INT, sizeof(RenderQuad), (void*)offsetof(RenderQuad, flip_x));
    glEnableVertexAttribArray(10);
    glVertexAttribDivisor(10, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    return 0;
}

void render_begin_frame() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

LOAD_TEXTURE(load_texture) {
	ASSERT(texture_id >= 0 && texture_id < MAX_LOADED_TEXTURES, "texture_id provided to load_texture is out of bounds");

    OpenGLTexture* texture = &render_data.textures[texture_id];

	ASSERT(texture->initialized == false, "texture_id provided to load_texture already refers to a texture");

    glGenTextures(1, &texture->id);

    glBindTexture(GL_TEXTURE_2D, texture->id);

    // TODO: Parameterize these when we need these to be configurable
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    //The y-axis is flipped in opengl where the bottom is y = 0 and top is y = 1
    stbi_set_flip_vertically_on_load(true);

    int texture_width, texture_height, nrChannels;
    unsigned char* texture_data = stbi_load(file_path, &texture_width, &texture_height, &nrChannels, 0);
    if (!texture_data)
    {
        fprintf(stderr, "Failed to load texture asset: %s\n", file_path);
        return -1;
    }

    GLenum source_format;
    if (nrChannels == 4) {
        source_format = GL_RGBA;
    }
    else if (nrChannels == 3) {
        source_format = GL_RGB;
    }
    else if (nrChannels == 1) {
        source_format = GL_RED;
    }
    else {
        ASSERT(false, "Unsupported number of channels in texture image file\n");
    }


    if (nrChannels == 1) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, texture_width, texture_height, 0, GL_RED, GL_UNSIGNED_BYTE, texture_data);
    }
    else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width, texture_height, 0, source_format, GL_UNSIGNED_BYTE, texture_data);
    }
    // glGenerateMipmap(GL_TEXTURE_2D);

    free(texture_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture->width = texture_width;
    texture->height = texture_height;
	texture->num_channels = (uint32)nrChannels;
	texture->initialized = true;
	render_data.texture_count++;

    return 0;
}

PUSH_RENDER_GROUP(push_render_group) {
    if (opts.flags & RENDER_OPTION_SCISSOR_F) {
        glScissor(opts.scissor_rect.x, opts.scissor_rect.y, opts.scissor_rect.w, opts.scissor_rect.h);
        glEnable(GL_SCISSOR_TEST);
    }

    glBindVertexArray(render_data.quad_vao_id);
    glBindBuffer(GL_ARRAY_BUFFER, render_data.quad_instance_vbo_id);

	float texture_matrices[MAX_LOADED_TEXTURES][16];
	int texture_num_channels[MAX_LOADED_TEXTURES];

	for(int i = 0; i < MAX_LOADED_TEXTURES; i++) {
    	OpenGLTexture* texture = &render_data.textures[i];

		// texture_matrices[i] = { 
		// 	1.0f / texture->width, 0.0f, 0.0f, 0.0f,   // row 1
		// 	0.0f, -1.0f / texture->height, 0.0f, 0.0f, // row 2
		// 	0.0f, 0.0f, 1.0f, 0.0f,                   // row 3
		// 	0.0f, 1.0f, 0.0f, 1.0f                    // row 4
		// };	

		texture_matrices[i][0]  = 1.0f / texture->width;   // row 1, col 1
		texture_matrices[i][1]  = 0.0f;                    // row 2, col 1
		texture_matrices[i][2]  = 0.0f;                    // row 3, col 1
		texture_matrices[i][3]  = 0.0f;                    // row 4, col 1

		texture_matrices[i][4]  = 0.0f;                    // row 1, col 2
		texture_matrices[i][5]  = -1.0f / texture->height; // row 2, col 2
		texture_matrices[i][6]  = 0.0f;                    // row 3, col 2
		texture_matrices[i][7]  = 0.0f;                    // row 4, col 2

		texture_matrices[i][8]  = 0.0f;                    // row 1, col 3
		texture_matrices[i][9]  = 0.0f;                    // row 2, col 3
		texture_matrices[i][10] = 1.0f;                    // row 3, col 3
		texture_matrices[i][11] = 0.0f;                    // row 4, col 3

		texture_matrices[i][12] = 0.0f;                    // row 1, col 4
		texture_matrices[i][13] = 1.0f;                    // row 2, col 4
		texture_matrices[i][14] = 0.0f;                    // row 3, col 4
		texture_matrices[i][15] = 1.0f;                    // row 4, col 4
		
		texture_num_channels[i] = texture->num_channels;

		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, texture->id);
	}

    glUniformMatrix4fv(glGetUniformLocation(render_data.shader_program_id, "texture_matrices"), MAX_LOADED_TEXTURES, GL_FALSE, (float *)texture_matrices);
    glUniform3f(glGetUniformLocation(render_data.shader_program_id, "camera_position"), camera_position.x, camera_position.y, 0.0f);
	glUniform1iv(glGetUniformLocation(render_data.shader_program_id, "texture_num_channels"), MAX_LOADED_TEXTURES, texture_num_channels);

    glBufferData(GL_ARRAY_BUFFER, sizeof(RenderQuad) * instance_count, instance_data, GL_DYNAMIC_DRAW);

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instance_count);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_SCISSOR_TEST);
}
