#include "render/texture_manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad/glad.h"
#include "io/log.h"
#include "io/paths.h"
#include "misc/error.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

/** Groups information about a texture. */
typedef struct te_texture {
    /** Non-NULL path (relative to the `res` directory) to the texture. */
    char* relative_path;

    /** strlen of @ref relative_path. */
    unsigned int relative_path_len;

    /** OpenGL ID of the texture. */
    unsigned int id;

    /** The number of places this texture is currently used in. */
    unsigned int usage_count;
} te_texture;

/** Loads textures from disk. */
struct te_texture_manager {
    /** Loaded textures. Size of this array is @ref texture_count. */
    te_texture* textures;

    /** Size of @ref textures. */
    unsigned int texture_count;
};

te_texture_manager*
prv_texture_manager_create() {
    te_texture_manager* manager = malloc(sizeof(te_texture_manager));
    manager->texture_count = 0;
    manager->textures = NULL;

    return manager;
}

void
prv_texture_manager_destroy(te_texture_manager* manager) {
    if (manager->texture_count > 0) {
        show_error_and_abort("texture manager is being destroyed but there are still some textures in use");
    }

    free(manager);
}

unsigned int
texture_manager_request_texture(te_texture_manager* manager, const char* relative_path,
                                enum te_texture_load_opt opt) {
    const size_t relative_path_len = strlen(relative_path);

    // Check cache.
    unsigned int index = 0;
    bool found = false;
    for (unsigned int i = 0; i < manager->texture_count; i++) {
        if (manager->textures[i].relative_path_len != relative_path_len) {
            continue;
        }
        if (strcmp(manager->textures[i].relative_path, relative_path) != 0) {
            continue;
        }
        found = true;
        index = i;
        break;
    }
    if (!found) {
        // Load new texture.
        char* tex_path = paths_prepend_res_to_path(relative_path);
        unsigned int tex_id = 0;

        if (opt != TE_TLO_CUBEMAP_NO_MIPMAPS) {
            int width = 0;
            int height = 0;
            int channel_count = 0;
            stbi_uc* pixels = stbi_load(tex_path, &width, &height, &channel_count, 0);
            if (pixels == NULL) {
                show_error_and_abort("failed to load a texture");
            }

            const int gl_format = channel_count == 4 ? GL_RGBA : GL_RGB;
            const int gl_internal_format = gl_format;

            glGenTextures(1, &tex_id);

            glBindTexture(GL_TEXTURE_2D, tex_id);
            {
                glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, width, height, 0, gl_format,
                             GL_UNSIGNED_BYTE, pixels);
                if (opt == TE_TLO_GENERATE_MIPMAPS) {
                    glGenerateMipmap(GL_TEXTURE_2D);

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                }

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(pixels);
        } else {
            show_error_and_abort("not implemented yet");
        }

        // Cache results.
        te_texture* new_textures = malloc(sizeof(te_texture) * (manager->texture_count + 1));
        memcpy(new_textures, manager->textures, sizeof(te_texture) * manager->texture_count);

        free(manager->textures);
        manager->textures = new_textures;

        index = manager->texture_count;
        manager->texture_count += 1;

        // Init data.
        te_texture* tex = &manager->textures[index];

        tex->usage_count = 0; // will increment below
        tex->id = tex_id;

        tex->relative_path = malloc(sizeof(char) * (relative_path_len + 1));
        memcpy(tex->relative_path, relative_path, sizeof(char) * relative_path_len);
        tex->relative_path[relative_path_len] = 0;
        tex->relative_path_len = (unsigned int)relative_path_len;

        free(tex_path);
    }

    manager->textures[index].usage_count += 1;
    return manager->textures[index].id;
}

void
texture_manager_mark_unused_texture(te_texture_manager* manager, unsigned int tex_id) {
    unsigned int index = 0;
    bool found = false;
    for (unsigned int i = 0; i < manager->texture_count; i++) {
        if (manager->textures[i].id != tex_id) {
            continue;
        }
        found = true;
        index = i;
        break;
    }
    if (!found) {
        show_error_and_abort("unable to find the specified texture");
    }
    if (manager->textures[index].usage_count == 0) {
        show_error_and_abort(
            "the specified texture id already has usage count of 0 (this is a texture manager bug)");
    }

    manager->textures[index].usage_count -= 1;
    if (manager->textures[index].usage_count > 0) {
        return;
    }

    // Cleanup texture data.
    glDeleteTextures(1, &manager->textures[index].id);
    free(manager->textures[index].relative_path);

    // Remove from cache.
    if (manager->texture_count == 1) {
        free(manager->textures);
        manager->textures = NULL;
        manager->texture_count = 0;
    } else {
        te_texture* new_textures = malloc(sizeof(te_texture) * (manager->texture_count - 1));
        memcpy(new_textures, manager->textures, sizeof(te_texture) * index);
        memcpy(new_textures + index, manager->textures + (index + 1),
               sizeof(te_texture) * (manager->texture_count - index - 1));

        free(manager->textures);
        manager->textures = new_textures;
        manager->texture_count -= 1;
    }
}
