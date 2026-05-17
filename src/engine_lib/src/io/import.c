#include <io/import.h>

#include <game_manager.h>
#include <world.h>
#include <game/model.h>
#include <io/log.h>
#include <io/filesystem.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stb/stb_image.h>

// Returns absolute path to the destination texture (located in the "res" directory).
// You must free returned pointer.
static char*
save_texture(
    size_t node_idx, size_t prim_idx, const char* path_to_gltf_dir,
    unsigned int path_to_gltf_dir_len, cgltf_image* image, const char* path_to_tex_dir,
    unsigned int path_to_tex_dir_len) {
    // Check image URI.
    if (image->uri != NULL && image->uri[0] != '\0') {
        // Don't handle slashes in URI (for now).
        const unsigned int uri_len = (unsigned int)strlen(image->uri);
        for (unsigned int i = 0; i < uri_len; i++) {
            if (image->uri[i] == '/' || image->uri[i] == '\\') {
                log_error_fmt(
                    "found path in the image path (paths in images not supported): %s",
                    image->uri);
                abort();
            }
        }

        char* path_to_src_image = filesystem_append_path(
            path_to_gltf_dir, path_to_gltf_dir_len, image->uri, uri_len, NULL);
        if (filesystem_does_path_exists(path_to_src_image)) {
            char* path_to_dst_image = filesystem_append_path(
                path_to_tex_dir, path_to_tex_dir_len, image->uri, uri_len, NULL);

            if (filesystem_does_path_exists(path_to_dst_image)) {
                // Multiple primitives using the same texture that we already saved.
                free(path_to_src_image);
                return path_to_dst_image;
            }

            filesystem_copy_file(path_to_src_image, path_to_dst_image);

            free(path_to_src_image);
            return path_to_dst_image;
        }

        free(path_to_src_image);
    }

    // Prepare a new texture name.
    const int tex_name_len = snprintf(NULL, 0, "tex_%zu_%zu", node_idx, prim_idx);
    if (tex_name_len < 0) {
        log_error("snprintf error");
        abort();
    }
    char* tex_name = malloc(sizeof(char) * ((unsigned int)tex_name_len + 1));
    snprintf(tex_name, (unsigned int)tex_name_len + 1, "tex_%zu_%zu", node_idx, prim_idx);

    // Load texture.
    int width;
    int height;
    int channel_count;
    stbi_uc* image_data = stbi_load_from_memory(
        (const stbi_uc*)image->buffer_view->buffer->data + image->buffer_view->offset,
        (int)image->buffer_view->size, &width, &height, &channel_count, 0);
    if (image_data == NULL) {
        log_error("failed to load image data");
        abort();
    }

    // Write texture to disk.
    char* absolute_tex_path = NULL;
    if (strcmp(image->mime_type, "image/jpeg") == 0) {
        absolute_tex_path = filesystem_append_path_ext(
            path_to_tex_dir, path_to_tex_dir_len, tex_name, (unsigned int)tex_name_len, ".jpg",
            4, NULL);
        if (stbi_write_jpg(absolute_tex_path, width, height, channel_count, image_data, 100)
            != 1) {
            log_error("failed to write image");
            abort();
        }
    } else if (strcmp(image->mime_type, "image/png") == 0) {
        absolute_tex_path = filesystem_append_path_ext(
            path_to_tex_dir, path_to_tex_dir_len, tex_name, (unsigned int)tex_name_len, ".png",
            4, NULL);
        if (stbi_write_png(
                absolute_tex_path, width, height, channel_count, image_data,
                width * channel_count)
            != 1) {
            log_error("failed to write image");
            abort();
        }
    } else {
        stbi_image_free(image_data);
        free(tex_name);
        log_error_fmt("unknown image type %s", image->mime_type);
        abort();
    }

    stbi_image_free(image_data);

    free(tex_name);
    return absolute_tex_path;
}

static bool
save_primitive(
    const char* path_to_gltf_dir, unsigned int path_to_gltf_dir_len,
    cgltf_primitive* primitive, size_t node_idx, size_t prim_idx, const char* path_to_file,
    te_model* model, const char* path_to_tex_dir, unsigned int path_to_tex_dir_len) {
    // Do a few checks.

    // Check index type.
    if (primitive->indices->component_type != cgltf_component_type_r_16u) {
        log_error_fmt(
            "found GLTF mesh with unsupported indices type %d, indices type must be "
            "UNSIGNED_SHORT",
            primitive->indices->component_type);
        return false;
    }

    // Check index count.
    if (primitive->indices->count
        > 0xFFFFFFFFu) { // because we store index count as unsigned int
        log_error_fmt("GLTF mesh index count exceeds limit of %u", 0xFFFFFFFFu);
        return false;
    }

    // Check attribute types.
    size_t pos_attribute_idx = 0xFFFFFFFF;
    size_t normal_attribute_idx = 0xFFFFFFFF;
    size_t texcoord_attribute_idx = 0xFFFFFFFF;
    size_t joints_attribute_idx = 0xFFFFFFFF;
    size_t weights_attribute_idx = 0xFFFFFFFF;
    for (size_t i = 0; i < primitive->attributes_count; i++) {
        if (primitive->attributes[i].type == cgltf_attribute_type_position) {
            pos_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec3) {
                log_error_fmt(
                    "found GLTF mesh with unsupported position type %d, expected vec3",
                    primitive->attributes[i].data->type);
                return false;
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_32f) {
                log_error_fmt(
                    "found GLTF mesh with unsupported position component type %d, expected "
                    "float",
                    primitive->attributes[i].data->type);
                return false;
            }
        } else if (primitive->attributes[i].type == cgltf_attribute_type_normal) {
            normal_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec3) {
                log_error_fmt(
                    "found GLTF mesh with unsupported normal type %d, expected vec3",
                    primitive->attributes[i].data->type);
                return false;
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_32f) {
                log_error_fmt(
                    "found GLTF mesh with unsupported normal component type %d, expected "
                    "float",
                    primitive->attributes[i].data->type);
                return false;
            }
        } else if (primitive->attributes[i].type == cgltf_attribute_type_texcoord) {
            texcoord_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec2) {
                log_error_fmt(
                    "found GLTF mesh with unsupported texcoord type %d, expected vec2",
                    primitive->attributes[i].data->type);
                return false;
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_32f) {
                log_error_fmt(
                    "found GLTF mesh with unsupported texcoord component type %d, expected "
                    "float",
                    primitive->attributes[i].data->type);
                return false;
            }
        } else if (primitive->attributes[i].type == cgltf_attribute_type_joints) {
            joints_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec4) {
                log_error_fmt(
                    "found GLTF mesh with unsupported joints type %d, expected vec4",
                    primitive->attributes[i].data->type);
                return false;
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_8u) {
                log_error_fmt(
                    "found GLTF mesh with unsupported texcoord component type %d, expected "
                    "unsigned byte",
                    primitive->attributes[i].data->type);
                return false;
            }
        }
    }
    if (pos_attribute_idx == 0xFFFFFFFF) {
        log_error("found GLTF mesh without a POSITION attribute");
        return false;
    } else if (normal_attribute_idx == 0xFFFFFFFF) {
        log_error("found GLTF mesh without a NORMAL attribute");
        return false;
    } else if (texcoord_attribute_idx == 0xFFFFFFFF) {
        log_error("found GLTF mesh without a TEXCOORD attribute");
        return false;
    }

    // Check vertex count.
    if (primitive->attributes[pos_attribute_idx].data->count
        > 0xFFFFFFFFu) { // because we store vertex count as unsigned int
        log_error_fmt("GLTF mesh vertex count exceeds limit of %u", 0xFFFFFFFFu);
        return false;
    }
    if (primitive->attributes[pos_attribute_idx].data->count == 0) {
        log_error("GLTF mesh vertex count is zero");
        return false;
    }

    // Load indices.
    unsigned int index_count = (unsigned int)primitive->indices->count;
    unsigned short* indices = malloc(sizeof(unsigned short) * index_count);
    {
        cgltf_buffer_view* index_buffer_view = primitive->indices->buffer_view;
        const size_t index_stride = index_buffer_view->stride == 0 ? sizeof(unsigned short)
                                                                   : index_buffer_view->stride;

        char* index_data = (char*)index_buffer_view->buffer->data
                           + (index_buffer_view->offset + primitive->indices->offset);

        for (size_t i = 0; i < primitive->indices->count; i++) {
            indices[i] = *(unsigned short*)index_data;
            index_data += index_stride;
        }
    }

    // Load vertices.
    unsigned int vertex_count =
        (unsigned int)primitive->attributes[pos_attribute_idx].data->count;
    const bool is_skinned = joints_attribute_idx != 0xFFFFFFFF;
    te_vertex_pack* vertices = vertex_pack_create(vertex_count, is_skinned);
    {
        // Load position.
        {
            cgltf_accessor* accessor = primitive->attributes[pos_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(vec3) : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (size_t i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                 [vertices->vertex_sizeof * i
                                  + vertices->attribute_offsets[TE_VA_POSITION]];
                glm_vec3_copy(*(vec3*)data, (float*)dst);
                data += stride;
            }
        }

        // Load normal.
        {
            cgltf_accessor* accessor = primitive->attributes[normal_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(vec3) : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (size_t i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                [vertices->vertex_sizeof * i
                                 + vertices->attribute_offsets[TE_VA_NORMAL]];
                glm_vec3_copy(*(vec3*)data, (float*)dst);
                data += stride;
            }
        }

        // Load UV.
        {
            cgltf_accessor* accessor = primitive->attributes[texcoord_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(vec2) : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (size_t i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                [vertices->vertex_sizeof * i
                                 + vertices->attribute_offsets[TE_VA_NORMAL]];
                glm_vec2_copy(*(vec2*)data, (float*)dst);
                data += stride;
            }
        }

        // Load bone indices.
        if (joints_attribute_idx != 0xFFFFFFFF) {
            cgltf_accessor* accessor = primitive->attributes[joints_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            assert(sizeof(unsigned char) == sizeof(te_bone_index_t));

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(unsigned char) * 4 : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (size_t i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                [vertices->vertex_sizeof * i
                                 + vertices->attribute_offsets[TE_VA_BONE_INDICES]];
                memcpy(dst, data, sizeof(unsigned char) * 4);
                data += stride;
            }
        }

        // Load bone weights.
        if (weights_attribute_idx != 0xFFFFFFFF) {
            cgltf_accessor* accessor = primitive->attributes[weights_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(vec4) : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (size_t i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                [vertices->vertex_sizeof * i
                                 + vertices->attribute_offsets[TE_VA_BONE_WEIGHTS]];
                glm_vec4_copy(*(vec4*)data, (float*)dst);
                data += stride;
            }
        }
    }

    FILE* fp = fopen(path_to_file, "wb");
    if (fp == NULL) {
        log_error_fmt("failed to open file for writing %s", path_to_file);
        free(vertices);
        free(indices);
        return false;
    }

    unsigned char id = is_skinned ? 100 : 0;
    fwrite(&id, sizeof(id), 1, fp);

    fwrite(&vertex_count, sizeof(vertex_count), 1, fp);
    fwrite(vertices->data, vertices->vertex_sizeof, vertex_count, fp);

    fwrite(&index_count, sizeof(index_count), 1, fp);
    fwrite(indices, sizeof(unsigned short), index_count, fp);

    fclose(fp);
    free(vertices);
    free(indices);

    // Save material.
    if (primitive->material != NULL) {
        // Color.
        vec4 rgba;
        glm_vec3_copy(primitive->material->pbr_metallic_roughness.base_color_factor, rgba);
        rgba[3] = 1.0f;
        model_set_color(model, rgba);

        // Texture.
        if (primitive->material->pbr_metallic_roughness.base_color_texture.texture != NULL) {
            cgltf_texture* tex =
                primitive->material->pbr_metallic_roughness.base_color_texture.texture;

            if (!filesystem_does_path_exists(path_to_tex_dir)) {
                filesystem_create_directory(path_to_tex_dir);
            }

            char* absolute_tex_path = save_texture(
                node_idx, prim_idx, path_to_gltf_dir, path_to_gltf_dir_len, tex->image,
                path_to_tex_dir, path_to_tex_dir_len);
            char* relative_path = filesystem_convert_path_to_relative(absolute_tex_path);

            model_set_texture(model, relative_path);

            free(absolute_tex_path);
            free(relative_path);
        }
    }

    return true;
}

bool
import_file_as_world(
    te_game_manager* game_manager, const char* path_to_file,
    const char* relative_path_to_dir) {
    unsigned int res_path_len;
    char* res_path = filesystem_prepend_res_to_path(relative_path_to_dir, &res_path_len);
    if (!filesystem_does_path_exists(res_path)) {
        free(res_path);
        log_error_fmt("the specified path %s does not exist", res_path);
        return false;
    }

    unsigned int filename_len;
    const char* filename = filesystem_find_filename(path_to_file, false, &filename_len);
    if (filename == NULL) {
        log_error_fmt("unable to parse filename from %s", path_to_file);
        free(res_path);
        return false;
    }

    // Because we will create a bunch of directories for imported objects (named after the GLTF file)
    // we need to make sure directory names don't have special characters that some OSes forbid.
    for (unsigned int name_idx = 0; name_idx < filename_len; name_idx++) {
        if (!((filename[name_idx] >= 'A' && filename[name_idx] <= 'Z')
              || (filename[name_idx] >= 'a' && filename[name_idx] <= 'z')
              || (filename[name_idx] >= '0' && filename[name_idx] <= '9')
              || filename[name_idx] == '-' || filename[name_idx] == '_')) {
            log_error_fmt(
                "file %s contains forbidden characters in the name, allowed characters: A-z, "
                "0-9, _, -",
                path_to_file);
            free(res_path);
            return false;
        }
    }

    // Prepare output directory path.
    unsigned int res_out_dir_len;
    char* res_out_dir = filesystem_append_path(
        res_path, res_path_len, filename, filename_len, &res_out_dir_len);

    free(res_path);
    res_path = NULL;

    // Make sure the output directory does not exist yet.
    if (filesystem_does_path_exists(res_out_dir)) {
        log_error_fmt(
            "attempted to create a new directory to import GLTF data but that directory "
            "already exists %s",
            res_out_dir);
        free(res_out_dir);
        return false;
    }

    // Parse GLTF.
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path_to_file, &data);
    if (result != cgltf_result_success) {
        log_error_fmt("failed to parse GLTF file %s", path_to_file);
        free(res_out_dir);
        return false;
    }
    if (data->scene == NULL) {
        log_error_fmt("parsed GLTF file has invalid scene %s", path_to_file);
        free(res_out_dir);
        return false;
    }
    result = cgltf_load_buffers(&options, data, path_to_file);
    if (result != cgltf_result_success) {
        log_error_fmt("failed to load buffers of parsed GLTF file %s", path_to_file);
        free(res_out_dir);
        return false;
    }

    te_world* world = game_manager_create_world(game_manager, "import_gltf_world");
    if (world == NULL) {
        log_error_fmt("failed to create a new world to import %s", path_to_file);
        free(res_out_dir);
        return false;
    }

    // Get GLTF parent dir for later use.
    unsigned int path_to_gltf_dir_len;
    char* path_to_gltf_dir =
        filesystem_get_parent_path(path_to_file, 0, &path_to_gltf_dir_len);
    if (path_to_gltf_dir == NULL) {
        log_error_fmt("failed to get parent directory of %s", path_to_file);
        free(res_out_dir);
        return false;
    }

    filesystem_create_directory(res_out_dir);

    // Prepare path for newly imported geometry files.
    unsigned int geo_dir_len;
    char* geo_dir =
        filesystem_append_path(res_out_dir, res_out_dir_len, "geo", 3, &geo_dir_len);
    filesystem_create_directory(geo_dir);

    // Prepare path for newly imported texture files.
    unsigned int tex_dir_len;
    char* tex_dir =
        filesystem_append_path(res_out_dir, res_out_dir_len, "tex", 3, &tex_dir_len);
    // Note: don't create tex directory, we will create it if textures found.

    bool failed = false;
    for (size_t node_idx = 0; node_idx < data->scene->nodes_count; node_idx++) {
        cgltf_node* node = data->scene->nodes[node_idx];

        if (node->mesh == NULL) {
            continue;
        }

        for (size_t prim_idx = 0; prim_idx < node->mesh->primitives_count; prim_idx++) {
            cgltf_primitive* primitive = &node->mesh->primitives[prim_idx];
            te_model* model = model_create();

            // Prepare geometry bin file name.
            int len = snprintf(NULL, 0, "%zu_%zu", node_idx, prim_idx);
            if (len < 0) {
                log_error("snprintf error");
                abort();
            }
            unsigned int geo_name_len = (unsigned int)len;
            char* geo_name = malloc(sizeof(char) * (geo_name_len + 1));
            snprintf(geo_name, geo_name_len + 1, "%zu_%zu", node_idx, prim_idx);

            char* geo_path = filesystem_append_path_ext(
                geo_dir, geo_dir_len, geo_name, geo_name_len, ".bin", 4, NULL);
            failed = !save_primitive(
                path_to_gltf_dir, path_to_gltf_dir_len, primitive, node_idx, prim_idx,
                geo_path, model, tex_dir, tex_dir_len);
            if (failed) {
                free(geo_path);
                break;
            }

            char* geo_relative = filesystem_convert_path_to_relative(geo_path);

            model_set_geometry(model, geo_relative);

            world_spawn_game_object(world, model_get_game_object_info(model));

            free(geo_path);
            free(geo_relative);
            free(geo_name);
        }
    }

    if (!failed) {
        char* world_path = filesystem_append_path_ext(
            res_out_dir, res_out_dir_len, filename, filename_len, ".txt", 4, NULL);

        char* relative_path = filesystem_convert_path_to_relative(world_path);
        if (relative_path == NULL) {
            log_error_fmt(
                "failed to convert path %s to be relative to the \"res\" directory",
                world_path);
            abort();
        }

        world_save_to_file(world, relative_path);

        free(relative_path);
        free(world_path);
    }
    game_manager_destroy_world(game_manager, world);

    cgltf_free(data);
    free(res_out_dir);
    free(geo_dir);
    free(tex_dir);
    free(path_to_gltf_dir);

    return true;
}
