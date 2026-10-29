#include <render/texture_manager.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#include <stb/stb_image.h>

/* Groups information about a texture. */
typedef struct te_texture {
    /* Non-NULL path (relative to the `res` directory) to the texture. */
    char* relative_path;

    /* strlen of @ref relative_path. */
    unsigned int relative_path_len;

    /* OpenGL ID of the texture. */
    unsigned int id;

    /* The number of places this texture is currently used in. */
    unsigned int usage_count;
} te_texture;

/* Loads textures from disk. */
struct te_texture_manager {
    /* Loaded textures. Size of this array is @ref texture_count. */
    te_texture* textures;

    /* Size of @ref textures. */
    unsigned int texture_count;
};

te_texture_manager*
prv_texture_manager_create(void) {
    te_texture_manager* manager = malloc(sizeof(te_texture_manager));
    manager->texture_count = 0;
    manager->textures = NULL;

    return manager;
}

void
prv_texture_manager_destroy(te_texture_manager* manager) {
    if (manager->texture_count > 0) {
        log_error(
            __FILE__, __LINE__,
            "texture manager is being destroyed but there are still some textures in use");
        abort();
    }

    free(manager);
}

static bool
is_power_of_2(unsigned int value) {
    unsigned int i;
    bool found_1 = false;
    for (i = 0; i < 32; i++) {
        if ((value & 1) == 1) {
            if (found_1) {
                return false;
            }
            found_1 = true;
        }
        value >>= 1;
    }

    return true;
}

unsigned int
texture_manager_request_texture(
    te_texture_manager* manager, const char* relative_path, bool generate_mipmaps) {
    size_t relative_path_len;
    unsigned int i;
    unsigned int index;
    bool found;

    relative_path_len = strlen(relative_path);

    /* check cache */
    index = 0;
    found = false;
    for (i = 0; i < manager->texture_count; i++) {
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
        te_texture* new_textures;
        te_texture* tex;

        /* load new texture */
        char* tex_path = filesystem_prepend_res_to_path(relative_path, NULL);
        unsigned int tex_id = 0;

        int width = 0;
        int height = 0;
        int channel_count = 0;
        int gl_format;
        int gl_internal_format;

        stbi_uc* pixels = stbi_load(tex_path, &width, &height, &channel_count, 0);
        if (pixels == NULL) {
            return 0;
        }
        if (width <= 0 || height <= 0) {
            log_error_fmt(
                __FILE__, __LINE__, "failed to read texture size from %s, width %i, height %i",
                tex_path, width, height);
            return 0;
        }

        if (!is_power_of_2((unsigned int)width) || !is_power_of_2((unsigned int)height)) {
            log_error_fmt(
                __FILE__, __LINE__, "texture %s has a non power of 2 size of %ix%i", tex_path,
                width, height);
            stbi_image_free(pixels);
            return 0;
        }

        gl_format = channel_count == 4 ? GL_RGBA : GL_RGB;
        gl_internal_format = gl_format;

        glGenTextures(1, &tex_id);

        glBindTexture(GL_TEXTURE_2D, tex_id);
        {
            texture_manager_upload_tex_with_mipmaps(
                (unsigned int)width, (unsigned int)height, (unsigned int)channel_count, pixels,
                generate_mipmaps ? 0 : 1, 0);

            glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                generate_mipmaps ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels);

        /* cache results */
        new_textures = malloc(sizeof(te_texture) * (manager->texture_count + 1));
        memcpy(new_textures, manager->textures, sizeof(te_texture) * manager->texture_count);

        free(manager->textures);
        manager->textures = new_textures;

        index = manager->texture_count;
        manager->texture_count += 1;

        /* init data */
        tex = &manager->textures[index];

        tex->usage_count = 0; /* will increment below */
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
    unsigned int i;

    for (i = 0; i < manager->texture_count; i++) {
        if (manager->textures[i].id != tex_id) {
            continue;
        }
        found = true;
        index = i;
        break;
    }
    if (!found) {
        log_error(__FILE__, __LINE__, "unable to find the specified texture");
        abort();
    }
    if (manager->textures[index].usage_count == 0) {
        log_error(
            __FILE__, __LINE__,
            "the specified texture id already has usage count of 0 (this is a texture "
            "manager bug)");
        abort();
    }

    manager->textures[index].usage_count -= 1;
    if (manager->textures[index].usage_count > 0) {
        return;
    }

    /* cleanup texture data */
    glDeleteTextures(1, &manager->textures[index].id);
    free(manager->textures[index].relative_path);

    /* remove from cache */
    if (manager->texture_count == 1) {
        free(manager->textures);
        manager->textures = NULL;
        manager->texture_count = 0;
    } else {
        te_texture* new_textures = malloc(sizeof(te_texture) * (manager->texture_count - 1));
        memcpy(new_textures, manager->textures, sizeof(te_texture) * index);
        memcpy(
            new_textures + index, manager->textures + (index + 1),
            sizeof(te_texture) * (manager->texture_count - index - 1));

        free(manager->textures);
        manager->textures = new_textures;
        manager->texture_count -= 1;
    }
}

void
texture_manager_upload_tex_with_mipmaps(
    unsigned int tex_width, unsigned int tex_height, unsigned int channel_count,
    unsigned char* tex_data, unsigned int levels_to_write, unsigned int level_skip_count) {
    unsigned char* curr_data;
    unsigned char* next_data;
    unsigned int y, x, curr_w, curr_h, next_w, next_h;
    int gl_format;

    unsigned int level = 0;

    if (!is_power_of_2(tex_width) || !is_power_of_2(tex_height)) {
        log_error(__FILE__, __LINE__, "texture width and height must be power of 2");
        abort();
    }

    if (channel_count == 4) {
        gl_format = GL_RGBA;
    } else if (channel_count == 3) {
        gl_format = GL_RGB;
    } else {
        gl_format = GL_LUMINANCE;
    }

    curr_data = malloc(tex_width * tex_height * channel_count);
    memcpy(curr_data, tex_data, tex_width * tex_height * channel_count);

    curr_w = tex_width;
    curr_h = tex_height;

    while (1) {
        if (level >= level_skip_count) {
            glTexImage2D(
                GL_TEXTURE_2D, (int)(level - level_skip_count), (GLint)gl_format, curr_w,
                curr_h, 0, (GLenum)gl_format, GL_UNSIGNED_BYTE, curr_data);
        }

        if (curr_w == 1 && curr_h == 1) {
            break;
        }
        if (levels_to_write != 0 && (level + 1) > level_skip_count
            && ((level + 1) - level_skip_count == levels_to_write)) {
            break;
        }

        next_w = (curr_w > 1) ? (curr_w / 2) : 1;
        next_h = (curr_h > 1) ? (curr_h / 2) : 1;
        next_data = malloc(next_w * next_h * channel_count);

        for (y = 0; y < next_h; y++) {
            for (x = 0; x < next_w; x++) {
                unsigned int src_x0 = x * 2;
                unsigned int src_y0 = y * 2;
                unsigned int src_x1 = (curr_w > 1) ? src_x0 + 1 : src_x0;
                unsigned int src_y1 = (curr_h > 1) ? src_y0 + 1 : src_y0;

                /* get 4 neighbors */
                unsigned int p00 = (src_y0 * curr_w + src_x0) * channel_count;
                unsigned int p10 = (src_y0 * curr_w + src_x1) * channel_count;
                unsigned int p01 = (src_y1 * curr_w + src_x0) * channel_count;
                unsigned int p11 = (src_y1 * curr_w + src_x1) * channel_count;

                /* average channels */
                unsigned int dst = (y * next_w + x) * channel_count;
                next_data[dst + 0] = (curr_data[p00 + 0] + curr_data[p10 + 0]
                                      + curr_data[p01 + 0] + curr_data[p11 + 0])
                                     / 4;
                next_data[dst + 1] = (curr_data[p00 + 1] + curr_data[p10 + 1]
                                      + curr_data[p01 + 1] + curr_data[p11 + 1])
                                     / 4;
                next_data[dst + 2] = (curr_data[p00 + 2] + curr_data[p10 + 2]
                                      + curr_data[p01 + 2] + curr_data[p11 + 2])
                                     / 4;
                next_data[dst + 3] = (curr_data[p00 + 3] + curr_data[p10 + 3]
                                      + curr_data[p01 + 3] + curr_data[p11 + 3])
                                     / 4;
            }
        }

        free(curr_data);
        curr_data = next_data;
        curr_w = next_w;
        curr_h = next_h;
        level += 1;

        if (levels_to_write != 0 && level >= level_skip_count
            && (level - level_skip_count) == levels_to_write) {
            break;
        }
    }

    free(curr_data);
}
