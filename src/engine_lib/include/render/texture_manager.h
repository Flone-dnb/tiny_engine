#pragma once

#include <stdbool.h>

enum te_texture_format { TE_TF_RED, TE_TF_ALPHA, TE_TF_RGB, TE_TF_RGBA };

typedef struct te_texture_manager te_texture_manager;

/* Looks if the specified texture is already used (loaded) and if so returns OpenGL ID
 * of the texture (otherwise loads it).
 * 
 * You must use @ref texture_manager_mark_unused_texture when you no longer need the shader.
 * 
 * Specify a non-NULL path to the texture file relative to the `res` directory. */
unsigned int texture_manager_request_texture(
    te_texture_manager* manager, const char* relative_path, bool generate_mipmaps);

/* Decrements texture usage counter. */
void texture_manager_mark_unused_texture(te_texture_manager* manager, unsigned int tex_id);

/* load texture data (and generates mipmap(s)) for the currently bound GL_TEXTURE_2D
 * width and height MUST BE power of 2
 * specify levels_to_write as 0 to fill until 1x1 mip size otherwise limit how much mips to write
 * specify level_skip_count as non-zero to skip a specific level count, for example
 * if 1 then texture's mip #0 will store mip #1, then texture's mip #1 will store mip #2 and so on,
 * if 2 then texture's mip #0 will store mip #2, then texture's mip #1 will store mip #3 and so on,
 * if this value is non-zero then levels_to_write specifies levels to write after we skipped level_skip_count */
void texture_manager_upload_tex_with_mipmaps(
    unsigned int tex_width, unsigned int tex_height, enum te_texture_format format,
    unsigned char* tex_data, unsigned int levels_to_write, unsigned int level_skip_count);

/* ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------ */

/* Creates texture manager. Renderer is expected to create this manager. */
te_texture_manager* prv_texture_manager_create(void);

/* Destroys texture manager. */
void prv_texture_manager_destroy(te_texture_manager* manager);
