#include <io/import.h>

#include <game_manager.h>
#include <world.h>
#include <game/model.h>
#include <io/log.h>
#include <io/filesystem.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

static bool
save_primitive(cgltf_primitive* primitive, const char* path_to_file) {
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
    te_model_vertex* vertices = malloc(sizeof(te_model_vertex) * vertex_count);
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
                glm_vec3_copy(*(vec3*)data, vertices[i].pos);
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
                glm_vec3_copy(*(vec3*)data, vertices[i].normal);
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
                glm_vec2_copy(*(vec2*)data, vertices[i].uv);
                data += stride;
            }
        }
    }

    FILE* fp = fopen(path_to_file, "w");
    if (fp == NULL) {
        log_error_fmt("failed to open file for writing %s", path_to_file);
        free(vertices);
        free(indices);
        return false;
    }

    unsigned char id = 0;
    fwrite(&id, sizeof(id), 1, fp);

    fwrite(&vertex_count, sizeof(vertex_count), 1, fp);
    fwrite(vertices, sizeof(te_model_vertex), vertex_count, fp);

    fwrite(&index_count, sizeof(index_count), 1, fp);
    fwrite(indices, sizeof(unsigned short), index_count, fp);

    fclose(fp);
    free(vertices);
    free(indices);

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

    filesystem_create_directory(res_out_dir);

    // Prepare path for newly imported geometry files.
    unsigned int geo_dir_len;
    char* geo_dir =
        filesystem_append_path(res_out_dir, res_out_dir_len, "geo", 3, &geo_dir_len);
    filesystem_create_directory(geo_dir);

    te_world* world = game_manager_create_world(game_manager, "import_gltf_world");
    if (world == NULL) {
        log_error_fmt("failed to create a new world to import %s", path_to_file);
        free(res_out_dir);
        return false;
    }

    bool failed = false;
    for (size_t node_idx = 0; node_idx < data->scene->nodes_count; node_idx++) {
        cgltf_node* node = data->scene->nodes[node_idx];

        if (node->mesh == NULL) {
            continue;
        }

        if (node->mesh->primitives_count == 0) {
            continue;
        }
        if (node->mesh->primitives_count > 1) {
            // For now this way is simpler.
            log_warn_fmt(
                "GLTF node %s has more that 1 primitive, currently only a single primitive "
                "per node is imported",
                node->name);
        }
        cgltf_primitive* primitive = &node->mesh->primitives[0];

        char* prim_path =
            filesystem_append_path_ext(geo_dir, geo_dir_len, node->name, 0, ".bin", 4, NULL);
        failed = !save_primitive(primitive, prim_path);
        if (failed) {
            free(prim_path);
            break;
        }

        char* geo_relative = filesystem_convert_path_to_relative(prim_path);

        te_model* model = model_create();
        model_set_geometry(model, geo_relative);

        world_spawn_model(world, model);

        free(prim_path);
        free(geo_relative);
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

    return true;
}
