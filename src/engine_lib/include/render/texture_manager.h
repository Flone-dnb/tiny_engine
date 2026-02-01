#pragma once

typedef struct te_texture_manager te_texture_manager;

// Describes options to load a texture.
enum te_texture_load_opt {
    TE_TLO_GENERATE_MIPMAPS,   //< Mipmaps will be auto-generated.
    TE_TLO_NO_MIPMAPS,         //< No mipmaps generated.
    TE_TLO_CUBEMAP_NO_MIPMAPS, //< Cubemap texture. No mipmaps generated.
};

// Looks if the specified texture is already used (loaded) and if so returns OpenGL ID
// of the texture (otherwise loads it).
//
// You must use @ref texture_manager_mark_unused_texture when you no longer need the shader.
//
// Specify a non-NULL path to the texture file relative to the `res` directory.
// If the opt argument is set to "cubemap" the path should point to a directory that stores textures
// with the names "right.png", "left.png", "top.png", "bottom.png", "front.png", "back.png".
unsigned int texture_manager_request_texture(te_texture_manager* manager, const char* relative_path,
                                             enum te_texture_load_opt opt);

// Decrements texture usage counter.
void texture_manager_mark_unused_texture(te_texture_manager* manager, unsigned int tex_id);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Creates texture manager. Renderer is expected to create this manager.
te_texture_manager* prv_texture_manager_create();

// Destroys texture manager.
void prv_texture_manager_destroy(te_texture_manager* manager);
