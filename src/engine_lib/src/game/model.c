#include <game/model.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cglm/mat4.h>
#include <game/camera.h>
#include <game/game_object_info.h>
#include <game_manager.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <math_funcs.h>
#include <render/model_renderer.h>
#include <render/renderer.h>
#include <render/shader_manager.h>
#include <render/texture_manager.h>
#include <shape/aabb_shape.h>
#include <type_database.h>
#include <world.h>
#include <glad/glad.h>

#define MODEL_TEX_LOAD_OPTION TE_TLO_GENERATE_MIPMAPS

// 3D model.
struct te_model {
    // AABB in model space.
    te_aabb_shape aabb_local;

    // Path (relative to the `res` directory) to the file that stores mesh geometry.
    // NULL if instead a default model should be used.
    char* path_to_geo;

    // NULL if default vertex shader is used. Path (relative to the `res` directory) to custom vertex shader.
    char* custom_vert_relative_path;

    // NULL if default fragment shader is used. Path (relative to the `res` directory) to custom fragment shader.
    char* custom_frag_relative_path;

    // NULL if texture is not set, otherwise path (relative to the `res` directory) to the texture file.
    char* tex_relative_path;

    // NULL if not set.
    char* name;

    // Always valid pointer.
    te_game_object_info* game_object_info;

    // NULL if not spawned. Do not free/destroy this pointer.
    te_world* world;

    // NULL if not attached.
    te_model* child_model;
    te_model* parent_model;
    te_camera* attached_camera;

    // Custom user-specified data.
    void* custom_ptr;
    void (*custom_on_before_destroyed)(te_model*);
    void (*custom_get_geometry)(
        te_model* model, te_model_vertex** vertices, unsigned short** indices,
        unsigned int* vertex_count, unsigned int* index_count, bool* free_custom_geometry);
    size_t custom_value;

    // Color in RGBA format in range [0.0; 1.0].
    vec4 color;

    vec3 position;
    vec3 rotation; // in degrees
    vec3 scale;

    vec2 tex_tiling;
    vec2 uv_offset;

    // Stores invalid value if not spawned (see @ref world).
    unsigned int render_data_handle;

    // Stores invalid value if not spawned (see @ref world). OpenGL ID of the shader program used.
    unsigned int shader_prog_id;

    bool is_opaque;
    bool is_serialization_allowed;
};

static void on_spawned(te_model* model, te_world* world);
static void on_despawned(te_model* model);

te_model*
model_create() {
    te_model* model = malloc(sizeof(te_model));

    model->world = NULL;
    model->name = NULL;
    model->render_data_handle = 0xffffffff;
    model->shader_prog_id = 0xffffffff;
    model->child_model = NULL;
    model->attached_camera = NULL;
    model->parent_model = NULL;
    model->tex_relative_path = NULL;
    model->path_to_geo = NULL;
    model->custom_vert_relative_path = NULL;
    model->custom_frag_relative_path = NULL;
    model->custom_ptr = NULL;
    model->custom_value = 0;
    model->custom_on_before_destroyed = NULL;
    model->custom_get_geometry = NULL;
    model->is_opaque = true;
    model->is_serialization_allowed = true;
    glm_vec2_one(model->tex_tiling);
    glm_vec2_zero(model->uv_offset);

    glm_vec4_one(model->color);

    glm_vec3_zero(model->position);
    glm_vec3_zero(model->rotation);
    glm_vec3_one(model->scale);

    model->game_object_info = malloc(sizeof(te_game_object_info));
    model->game_object_info->type_id = model_get_type_id();
    model->game_object_info->type = TE_GOT_MODEL;
    model->game_object_info->game_object = model;
    model->game_object_info->get_world = model_get_world;
    model->game_object_info->get_name = model_get_name;
    model->game_object_info->on_spawned = on_spawned;
    model->game_object_info->on_despawned = on_despawned;
    model->game_object_info->destroy = model_destroy;

    return model;
}

void
model_destroy(te_model* model) {
    if (model->custom_on_before_destroyed != NULL) {
        model->custom_on_before_destroyed(model);
    }

    if (model->child_model != NULL) {
        if (model->child_model->world != NULL) {
            // We should have despawned it in our despawn callback.
            log_error("expected the child model to be despawned already");
            abort();
        }
        model_destroy(model->child_model);
    }

    if (model->attached_camera != NULL) {
        if (camera_get_world(model->attached_camera) != NULL) {
            log_error("expected the attached camera to be despawned already");
            abort();
        }
        camera_destroy(model->attached_camera);
    }

    free(model->name);
    free(model->tex_relative_path);
    free(model->custom_frag_relative_path);
    free(model->custom_vert_relative_path);
    free(model->path_to_geo);
    free(model->game_object_info);

    free(model);
}

te_game_object_info*
model_get_game_object_info(te_model* model) {
    return model->game_object_info;
}

void
model_set_name(te_model* model, const char* name) {
    free(model->name);
    model->name = NULL;

    if (name != NULL) {
        const size_t len = strlen(name);
        model->name = malloc(sizeof(char) * (len + 1));
        memcpy(model->name, name, sizeof(char) * len);
        model->name[len] = 0;
    }
}

const char*
model_get_name(te_model* model) {
    return model->name;
}

te_model_renderer*
prv_model_get_renderer(te_model* model) {
#if defined(DEBUG)
    if (model->world == NULL) {
        log_error("expected world to be valid");
        abort();
    }
#endif

    if (model->is_opaque) {
        return world_get_opaque_model_renderer(model->world);
    } else {
        return world_get_transparent_model_renderer(model->world);
    }
}

static void
prv_model_calc_world_normal_matrices(te_model* model, mat4 world, mat3 normal) {
    mat4 translate_mat;
    glm_translate_make(translate_mat, model->position);

    mat4 rot_mat;
    math_make_rotation_mat(model->rotation, rot_mat);

    mat4 scale_mat;
    glm_scale_make(scale_mat, model->scale);

    // Scale, rotate and then translate.
    glm_mat4_mul(rot_mat, scale_mat, world);
    glm_mat4_mul(translate_mat, world, world);

    mat4 normal_mat;
    glm_mat4_inv(world, normal_mat);
    glm_mat4_transpose(normal_mat);

    glm_mat4_pick3(normal_mat, normal);

    if (model->parent_model != NULL && model->parent_model->render_data_handle != 0xffffffff) {
        te_model_renderer* renderer = prv_model_get_renderer(model->parent_model);
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            renderer, model->parent_model->render_data_handle);

        // Ignore parent's scale.
        mat4 parent_world;
        glm_mat4_copy(data->world_mat, parent_world);
        math_normalize_safely(parent_world[0]);
        math_normalize_safely(parent_world[1]);
        math_normalize_safely(parent_world[2]);

        glm_mat4_mul(parent_world, world, world);

        glm_mat3_mul(data->normal_mat, normal, normal);
    }

    if (model->child_model != NULL && model->child_model->render_data_handle != 0xffffffff) {
        te_model_renderer* renderer = prv_model_get_renderer(model->child_model);
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            renderer, model->child_model->render_data_handle);

        prv_model_calc_world_normal_matrices(
            model->child_model, data->world_mat, data->normal_mat);
        data->aabb_world =
            aabb_shape_convert_to_world(&model->child_model->aabb_local, data->world_mat);
    }

    if (model->attached_camera != NULL) {
        prv_camera_on_parent_model_world_mat_changed(model->attached_camera, model);
    }
}

void
model_set_position(te_model* model, vec3 position) {
    glm_vec3_copy(position, model->position);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            prv_model_get_renderer(model), model->render_data_handle);

        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
        data->aabb_world = aabb_shape_convert_to_world(&model->aabb_local, data->world_mat);
    }
}

void
model_set_rotation(te_model* model, vec3 rotation) {
    glm_vec3_copy(rotation, model->rotation);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            prv_model_get_renderer(model), model->render_data_handle);

        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
        data->aabb_world = aabb_shape_convert_to_world(&model->aabb_local, data->world_mat);
    }
}

void
model_set_scale(te_model* model, vec3 scale) {
    glm_vec3_copy(scale, model->scale);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            prv_model_get_renderer(model), model->render_data_handle);

        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
        data->aabb_world = aabb_shape_convert_to_world(&model->aabb_local, data->world_mat);
    }
}

void
model_set_color(te_model* model, vec4 color) {
    glm_vec4_copy(color, model->color);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            prv_model_get_renderer(model), model->render_data_handle);
        glm_vec4_copy(model->color, data->color);
    }
}

void
model_set_texture(te_model* model, const char* relative_path) {
#if defined(ENGINE_EDITOR)
    // Check if path exists.
    char* res_path = filesystem_prepend_res_to_path(relative_path, NULL);
    if (!filesystem_does_path_exists(res_path)) {
        // Do nothing, probably user typing the path.
        free(res_path);
        return;
    }
    free(res_path);
#endif

    free(model->tex_relative_path);
    model->tex_relative_path = NULL;

    if (relative_path == NULL) {
        // Remove current texture.
        model->tex_relative_path = NULL;
        if (model->world != NULL) {
            // Update render data.
            te_model_render_data* data = model_renderer_get_render_data_tmp(
                prv_model_get_renderer(model), model->render_data_handle);

            if (data->tex_id > 0) {
                te_texture_manager* texture_manager = renderer_get_texture_manager(
                    game_manager_get_renderer(world_get_game_manager(model->world)));
                texture_manager_mark_unused_texture(texture_manager, data->tex_id);
            }

            data->tex_id = 0;
            glm_vec2_make((vec2){-1.0f, -1.0f}, data->tex_tiling);
        }
    } else {
        // Set new texture.
        const size_t len = strlen(relative_path);
        model->tex_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(model->tex_relative_path, relative_path, sizeof(char) * len);
        model->tex_relative_path[len] = 0;

        if (model->world != NULL) {
            // Update render data.
            te_model_render_data* data = model_renderer_get_render_data_tmp(
                prv_model_get_renderer(model), model->render_data_handle);

            te_texture_manager* texture_manager = renderer_get_texture_manager(
                game_manager_get_renderer(world_get_game_manager(model->world)));
            if (data->tex_id > 0) {
                texture_manager_mark_unused_texture(texture_manager, data->tex_id);
            }
            data->tex_id = texture_manager_request_texture(
                texture_manager, relative_path, MODEL_TEX_LOAD_OPTION);
            glm_vec2_copy(model->tex_tiling, data->tex_tiling);
        }
    }
}

void
model_set_texture_tiling(te_model* model, vec2 tex_tiling) {
    glm_vec2_copy(tex_tiling, model->tex_tiling);

    if (model->world != NULL && model->tex_relative_path != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            prv_model_get_renderer(model), model->render_data_handle);
        glm_vec2_copy(model->tex_tiling, data->tex_tiling);
    }
}

void
model_set_uv_offset(te_model* model, vec2 uv_offset) {
    glm_vec2_copy(uv_offset, model->uv_offset);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            prv_model_get_renderer(model), model->render_data_handle);
        glm_vec2_copy(model->uv_offset, data->uv_offset);
    }
}

bool
model_is_transparency_enabled(te_model* model) {
    return !model->is_opaque;
}

void
model_set_is_serialization_allowed(te_model* model, bool enable) {
    model->is_serialization_allowed = enable;
}

bool
model_is_serialization_allowed(te_model* model) {
    return model->is_serialization_allowed;
}

void
model_get_position(te_model* model, vec3 out) {
    glm_vec3_copy(model->position, out);
}

void
model_get_rotation(te_model* model, vec3 out) {
    glm_vec3_copy(model->rotation, out);
}

void
model_get_scale(te_model* model, vec3 out) {
    glm_vec3_copy(model->scale, out);
}

void
model_get_world_position(te_model* model, vec3 out) {
    if (model->render_data_handle == 0xFFFFFFFF) {
        model_get_position(model, out);
        return;
    }

    te_model_render_data* target_data = model_renderer_get_render_data_tmp(
        prv_model_get_model_renderer(model), prv_model_get_render_data_handle(model));

    glm_vec3_copy(target_data->world_mat[3], out);
}

void
model_get_color(te_model* model, vec4 out) {
    glm_vec4_copy(model->color, out);
}

const char*
model_get_texture(te_model* model) {
    return model->tex_relative_path;
}

void
model_get_texture_tiling(te_model* model, vec2 tex_tiling) {
    glm_vec2_copy(model->tex_tiling, tex_tiling);
}

void
model_get_uv_offset(te_model* model, vec2 uv_offset) {
    glm_vec2_copy(model->uv_offset, uv_offset);
}

void
model_set_parent(te_model* model, te_model* new_parent) {
    if (model->parent_model == new_parent) {
        return;
    }

    if (new_parent != NULL && new_parent->child_model != NULL) {
        log_error("only 1 model can be attached");
        abort();
    }

    if (model->parent_model != NULL) {
        model->parent_model->child_model = NULL;
    }
    model->parent_model = new_parent;

    if (new_parent != NULL) {
        new_parent->child_model = model;
    }

    if (model->world == NULL) {
        if (new_parent != NULL && new_parent->world != NULL) {
            world_spawn_game_object(new_parent->world, model_get_game_object_info(model));
            prv_world_remove_root_game_object_no_notify(
                new_parent->world, model_get_game_object_info(model), true);
        }
    } else {
        if (new_parent == NULL) {
            prv_world_add_root_game_object_no_notify(
                model->world, model_get_game_object_info(model), true);
        } else {
            if (new_parent->world != NULL) {
                if (new_parent->world != model->world) {
                    log_error("can't attach a model from a different world, despawn the child "
                              "model first");
                    abort();
                } else {
                    prv_world_remove_root_game_object_no_notify(
                        model->world, model_get_game_object_info(model), false);
                }
            } else {
                world_despawn_game_object(model->world, model_get_game_object_info(model));
            }
        }
    }

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            prv_model_get_renderer(model), model->render_data_handle);

        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
        data->aabb_world = aabb_shape_convert_to_world(&model->aabb_local, data->world_mat);
    }
}

te_model*
model_get_parent(te_model* model) {
    return model->parent_model;
}

te_model*
model_get_child_model(te_model* model) {
    return model->child_model;
}

void
model_attach_camera(te_model* model, te_camera* camera) {
    if (model->attached_camera == camera) {
        return;
    }
    if (camera != NULL) {
        if (model->attached_camera != NULL) {
            log_error("only 1 camera can be attached");
            abort();
        }
        if (model->parent_model != NULL) {
            // Also serialization does not support this.
            log_error("can't attach camera to a model which has a parent model");
            abort();
        }
    }

    if (model->attached_camera != NULL) {
        prv_camera_on_parent_model_world_mat_changed(model->attached_camera, NULL);
    }
    te_camera* old_camera = model->attached_camera;
    model->attached_camera = camera;
    if (camera != NULL) {
        prv_camera_on_parent_model_world_mat_changed(camera, model);
    }

    if (camera != NULL) {
        te_world* camera_world = camera_get_world(camera);
        if (model->world != NULL && camera_world != NULL && model->world != camera_world) {
            log_error(
                "can't attach a camera from a different world, despawn the camera first");
            abort();
        }

        if (model->world == NULL) {
            if (camera_world != NULL) {
                world_despawn_game_object(camera_world, camera_get_game_object_info(camera));
            }
        } else {
            if (camera_world == NULL) {
                world_spawn_game_object(model->world, camera_get_game_object_info(camera));
            }
            prv_world_remove_root_game_object_no_notify(
                model->world, camera_get_game_object_info(camera), true);
        }
    } else if (old_camera != NULL) {
        te_world* camera_world = camera_get_world(old_camera);
        if (camera_world != NULL) {
            prv_world_add_root_game_object_no_notify(
                camera_world, camera_get_game_object_info(old_camera), true);
        }
    }
}

struct te_camera*
model_get_attached_camera(te_model* model) {
    return model->attached_camera;
}

void
model_set_custom_ptr(te_model* model, void* ptr) {
    model->custom_ptr = ptr;
}

void*
model_get_custom_ptr(te_model* model) {
    return model->custom_ptr;
}

void
model_set_custom_value(te_model* model, size_t value) {
    model->custom_value = value;
}

size_t
model_get_custom_value(te_model* model) {
    return model->custom_value;
}

void
model_set_custom_on_before_destroyed(
    te_model* model, void (*custom_on_before_destroyed)(te_model*)) {
    model->custom_on_before_destroyed = custom_on_before_destroyed;
}

te_world*
model_get_world(te_model* model) {
    return model->world;
}

static void prv_model_generate_cube(
    te_model_vertex** vertices, unsigned short** indices, unsigned int* vertex_count,
    unsigned int* index_count);
static void prv_model_load_geo(
    const char* path_to_geo, te_model_vertex** vertices, unsigned short** indices,
    unsigned int* vertex_count, unsigned int* index_count);

te_aabb_shape prv_model_calc_aabb(te_model_vertex* vertices, unsigned int vertex_count);

static void
prv_model_add_to_model_renderer(te_model* model) {
#if defined(DEBUG)
    if (model->world == NULL) {
        log_error("expected world to be valid");
        abort();
    }
#endif

    // Get shader program.
    {
        te_shader_manager* shader_manager = renderer_get_shader_manager(
            game_manager_get_renderer(world_get_game_manager(model->world)));
        model->shader_prog_id = shader_manager_request_shader(
            shader_manager,
            model->custom_vert_relative_path == NULL ? "engine/shader/model.vert.glsl"
                                                     : model->custom_vert_relative_path,
            model->custom_frag_relative_path == NULL ? "engine/shader/model.frag.glsl"
                                                     : model->custom_frag_relative_path);
    }

    // Load geometry.
    unsigned int index_count = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    {
        te_model_vertex* vertices;
        unsigned int vertex_count;
        unsigned short* indices;
        bool free_custom_geometry = false;
        if (model->custom_get_geometry != NULL) {
            model->custom_get_geometry(
                model, &vertices, &indices, &vertex_count, &index_count,
                &free_custom_geometry);
        } else if (model->path_to_geo != NULL) {
            prv_model_load_geo(
                model->path_to_geo, &vertices, &indices, &vertex_count, &index_count);
            free_custom_geometry = true;
        } else {
            prv_model_generate_cube(&vertices, &indices, &vertex_count, &index_count);
        }

        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(
            GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(te_model_vertex) * vertex_count), vertices,
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(unsigned short) * index_count),
            indices, GL_STATIC_DRAW);

        // Position.
        glBindAttribLocation(model->shader_prog_id, 0, "pos");
        glEnableVertexAttribArray(0);

        // Normal.
        glBindAttribLocation(model->shader_prog_id, 1, "normal");
        glEnableVertexAttribArray(1);

        glBindAttribLocation(model->shader_prog_id, 2, "uv");
        glEnableVertexAttribArray(2);

        // Calculate local AABB.
        model->aabb_local = prv_model_calc_aabb(vertices, vertex_count);

        if (model->custom_get_geometry == NULL || free_custom_geometry) {
            free(vertices);
            free(indices);
        }
    }

    // Add to rendering.
    te_model_renderer* model_renderer = prv_model_get_renderer(model);
    model->render_data_handle =
        model_renderer_add_model(model_renderer, model->shader_prog_id);

    // Init render data.
    {
        te_model_render_data* data =
            model_renderer_get_render_data_tmp(model_renderer, model->render_data_handle);

        glm_vec4_copy(model->color, data->color);
        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
        data->vbo = vbo;
        data->ebo = ebo;
        data->tex_id = 0;
        glm_vec2_copy((vec2){-1.0f, -1.0f}, data->tex_tiling);
        glm_vec2_copy(model->uv_offset, data->uv_offset);
        data->index_count = (int)index_count;
        data->aabb_world = aabb_shape_convert_to_world(&model->aabb_local, data->world_mat);
        if (model->tex_relative_path != NULL) {
            te_texture_manager* texture_manager = renderer_get_texture_manager(
                game_manager_get_renderer(world_get_game_manager(model->world)));
            data->tex_id = texture_manager_request_texture(
                texture_manager, model->tex_relative_path, MODEL_TEX_LOAD_OPTION);
            glm_vec2_copy(model->tex_tiling, data->tex_tiling);
        }
    }
}

static void
prv_model_remove_from_model_renderer(te_model* model) {
#if defined(DEBUG)
    if (model->world == NULL) {
        log_error("expected world to be valid");
        abort();
    }
#endif

    te_renderer* renderer = game_manager_get_renderer(world_get_game_manager(model->world));
    te_texture_manager* texture_manager = renderer_get_texture_manager(renderer);
    te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
    te_model_renderer* model_renderer = prv_model_get_renderer(model);

    unsigned int tex_id = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    {
        te_model_render_data* data =
            model_renderer_get_render_data_tmp(model_renderer, model->render_data_handle);
        tex_id = data->tex_id;
        vbo = data->vbo;
        ebo = data->ebo;
    }

    // Remove from rendering.
    model_renderer_remove_model(model_renderer, model->render_data_handle);

    // Mark unused stuff.
    shader_manager_mark_unused_shader(shader_manager, model->shader_prog_id);
    if (model->tex_relative_path != NULL) {
        texture_manager_mark_unused_texture(texture_manager, tex_id);
    }

    // Release geometry.
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);

    model->render_data_handle = 0xffffffff;
    model->shader_prog_id = 0xffffffff;
}

mat4*
prv_model_get_world_mat_tmp(te_model* model) {
    if (model->render_data_handle == 0xffffffff) {
        log_error("expected the model to be spawned and visible");
        abort();
    }

    te_model_render_data* data = model_renderer_get_render_data_tmp(
        prv_model_get_renderer(model), model->render_data_handle);
    return &data->world_mat;
}

void
model_enable_transparency(te_model* model, bool enable) {
    if (model->world == NULL) {
        model->is_opaque = !enable;
    } else {
        prv_model_remove_from_model_renderer(model);
        model->is_opaque = !enable;
        prv_model_add_to_model_renderer(model);
    }
}

void
model_set_custom_vert_shader(te_model* model, const char* vert_relative_path) {
#if defined(ENGINE_EDITOR)
    // Check if path exists.
    char* res_path = filesystem_prepend_res_to_path(vert_relative_path, NULL);
    if (!filesystem_does_path_exists(res_path)) {
        // Do nothing, probably user typing the path.
        free(res_path);
        return;
    }
    free(res_path);
#endif

    if (model->world != NULL) {
        prv_model_remove_from_model_renderer(model);
    }

    free(model->custom_vert_relative_path);
    model->custom_vert_relative_path = NULL;

    if (vert_relative_path != NULL) {
        const size_t len = strlen(vert_relative_path);
        model->custom_vert_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(model->custom_vert_relative_path, vert_relative_path, sizeof(char) * len);
        model->custom_vert_relative_path[len] = 0;
    }

    if (model->world != NULL) {
        prv_model_add_to_model_renderer(model);
    }
}

const char*
model_get_custom_vert_shader(te_model* model) {
    return model->custom_vert_relative_path;
}

void
model_set_custom_frag_shader(te_model* model, const char* frag_relative_path) {
#if defined(ENGINE_EDITOR)
    // Check if path exists.
    char* res_path = filesystem_prepend_res_to_path(frag_relative_path, NULL);
    if (!filesystem_does_path_exists(res_path)) {
        // Do nothing, probably user typing the path.
        free(res_path);
        return;
    }
    free(res_path);
#endif

    if (model->world != NULL) {
        prv_model_remove_from_model_renderer(model);
    }

    free(model->custom_frag_relative_path);
    model->custom_frag_relative_path = NULL;

    if (frag_relative_path != NULL) {
        const size_t len = strlen(frag_relative_path);
        model->custom_frag_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(model->custom_frag_relative_path, frag_relative_path, sizeof(char) * len);
        model->custom_frag_relative_path[len] = 0;
    }

    if (model->world != NULL) {
        prv_model_add_to_model_renderer(model);
    }
}

void
model_set_geometry(te_model* model, const char* relative_path) {
#if defined(ENGINE_EDITOR)
    // Check if path exists.
    char* res_path = filesystem_prepend_res_to_path(relative_path, NULL);
    if (!filesystem_does_path_exists(res_path)) {
        // Do nothing, probably user typing the path.
        free(res_path);
        return;
    }
    free(res_path);
#endif

    if (model->world != NULL) {
        prv_model_remove_from_model_renderer(model);
    }

    free(model->path_to_geo);
    model->path_to_geo = NULL;

    if (relative_path != NULL) {
        const size_t path_len = strlen(relative_path);
        model->path_to_geo = malloc(sizeof(char) * (path_len + 1));
        memcpy(model->path_to_geo, relative_path, path_len);
        model->path_to_geo[path_len] = 0;
    }

    if (model->world != NULL) {
        prv_model_add_to_model_renderer(model);
    }
}

const char*
model_get_geometry(te_model* model) {
    return model->path_to_geo;
}

void
model_set_custom_geometry_provider(
    te_model* model,
    void (*custom_get_geometry)(
        te_model* model, te_model_vertex** vertices, unsigned short** indices,
        unsigned int* vertex_count, unsigned int* index_count, bool* free_custom_geometry)) {
    model->custom_get_geometry = custom_get_geometry;
}

const char*
model_get_custom_frag_shader(te_model* model) {
    return model->custom_frag_relative_path;
}

void
prv_model_set_vertex_attributes() {
    // Position.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(te_model_vertex), NULL);

    // Normal.
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, sizeof(te_model_vertex), (void*)sizeof(vec3));

    // UV.
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, sizeof(te_model_vertex), (void*)(sizeof(vec3) * 2));
}

unsigned int
prv_model_get_render_data_handle(te_model* model) {
    return model->render_data_handle;
}

te_model_renderer*
prv_model_get_model_renderer(te_model* model) {
    if (model->render_data_handle == 0xFFFFFFFF) {
        return NULL;
    }

    return prv_model_get_renderer(model);
}

te_aabb_shape
prv_model_calc_aabb(te_model_vertex* vertices, unsigned int vertex_count) {
    vec3 min;
    vec3 max;
    glm_vec3_copy((vec3){FLT_MAX, FLT_MAX, FLT_MAX}, min);
    glm_vec3_copy((vec3){FLT_MIN, FLT_MIN, FLT_MIN}, max);

    for (unsigned int i = 0; i < vertex_count; i++) {
        glm_vec3_minv(min, vertices[i].pos, min);
        glm_vec3_maxv(max, vertices[i].pos, max);
    }

    te_aabb_shape aabb;
    aabb.center[0] = (min[0] + max[0]) * 0.5f;
    aabb.center[1] = (min[1] + max[1]) * 0.5f;
    aabb.center[2] = (min[2] + max[2]) * 0.5f;

    aabb.extents[0] = max[0] - aabb.center[0];
    aabb.extents[1] = max[1] - aabb.center[1];
    aabb.extents[2] = max[2] - aabb.center[2];

    return aabb;
}

static void
on_spawned(te_model* model, te_world* world) {
    model->world = world;
    prv_model_add_to_model_renderer(model);

    if (model->child_model != NULL) {
        if (model->child_model->world != NULL) {
            log_error("expected the child model to not be spawned yet");
            abort();
        }
        on_spawned(model->child_model, world);
    }

    if (model->attached_camera != NULL) {
        if (camera_get_world(model->attached_camera) != NULL) {
            log_error("expected the attached camera to not be spawned yet");
            abort();
        }
        camera_get_game_object_info(model->attached_camera)
            ->on_spawned(model->attached_camera, world);
    }
}

static void
on_despawned(te_model* model) {
    if (model->child_model != NULL && model->child_model->world != NULL) {
        on_despawned(model->child_model);
    }
    if (model->attached_camera != NULL && camera_get_world(model->attached_camera) != NULL) {
        camera_get_game_object_info(model->attached_camera)
            ->on_despawned(model->attached_camera);
    }

    prv_model_remove_from_model_renderer(model);
    model->world = NULL;
}

const char*
model_get_type_id(void) {
    return "model";
}

static void
type_spawn(te_world* world, te_model* model) {
    if (model->world != NULL) {
        log_error("the model is already spawned in the different world");
        abort();
    }

    world_spawn_game_object(world, model->game_object_info);
}

static void
type_despawn(te_world* world, te_model* model) {
    if (model->world != world) {
        log_error("the model is spawned in the different world");
        abort();
    }

    if (model->parent_model != NULL) {
        model_set_parent(model, NULL); // make model to be in the array of root world objects
    }
    world_despawn_game_object(
        model->world, model->game_object_info); // despawn root world object
}

void
model_register_type(void) {
    te_type_info* info = type_info_create(
        model_get_type_id(), model_create, model_destroy, type_spawn, type_despawn, NULL,
        model_get_game_object_info, model_is_serialization_allowed);
    type_info_add_vec3_variable(info, "position", model_set_position, model_get_position);
    type_info_add_vec3_variable(info, "rotation", model_set_rotation, model_get_rotation);
    type_info_add_vec3_variable(info, "scale", model_set_scale, model_get_scale);
    type_info_add_vec4_variable(info, "color", model_set_color, model_get_color);
    type_info_add_string_variable(info, "texture", model_set_texture, model_get_texture);
    type_info_add_vec2_variable(
        info, "texture_tiling", model_set_texture_tiling, model_get_texture_tiling);
    type_info_add_vec2_variable(info, "uv_offset", model_set_uv_offset, model_get_uv_offset);
    type_info_add_bool_variable(
        info, "transparent", model_enable_transparency, model_is_transparency_enabled);
    type_info_add_string_variable(info, "geometry", model_set_geometry, model_get_geometry);
    type_info_add_string_variable(
        info, "custom_vert_shader", model_set_custom_vert_shader,
        model_get_custom_vert_shader);
    type_info_add_string_variable(
        info, "custom_frag_shader", model_set_custom_frag_shader,
        model_get_custom_frag_shader);
    type_info_add_string_variable(info, "name", model_set_name, model_get_name);

    type_database_register_type(info);
}

static void
prv_model_load_geo(
    const char* path_to_geo, te_model_vertex** vertices, unsigned short** indices,
    unsigned int* vertex_count, unsigned int* index_count) {
    char* res_path = filesystem_prepend_res_to_path(path_to_geo, NULL);

    FILE* fp = fopen(res_path, "rb");
    if (fp == NULL) {
        log_error_fmt(
            "failed to load model geometry from file %s: unable to open file", path_to_geo);
        abort();
    }

    unsigned char id = 0;
    fread(&id, sizeof(id), 1, fp);
    if (id != 0) {
        log_error_fmt(
            "failed to load model geometry from file %s: unexpected file type ID %u",
            path_to_geo, (unsigned int)id);
        abort();
    }

    fread(vertex_count, sizeof(*vertex_count), 1, fp);
    (*vertices) = malloc(sizeof(te_model_vertex) * (*vertex_count));
    fread(*vertices, sizeof(te_model_vertex), *vertex_count, fp);

    fread(index_count, sizeof(*index_count), 1, fp);
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    fread(*indices, sizeof(unsigned short), *index_count, fp);

    fclose(fp);
    free(res_path);
}

static void
prv_model_generate_cube(
    te_model_vertex** vertices, unsigned short** indices, unsigned int* vertex_count,
    unsigned int* index_count) {
    const float half = 0.5f;

    (*vertex_count) = 24;
    (*vertices) = malloc(sizeof(te_model_vertex) * (*vertex_count));

    // Init UVs.
    for (unsigned int i = 0; i < (*vertex_count); i += 4) {
        glm_vec2_make((vec2){1.0f, 1.0f}, (*vertices)[i].uv);
        glm_vec2_make((vec2){0.0f, 1.0f}, (*vertices)[i + 1].uv);
        glm_vec2_make((vec2){1.0f, 0.0f}, (*vertices)[i + 2].uv);
        glm_vec2_make((vec2){0.0f, 0.0f}, (*vertices)[i + 3].uv);
    }

    // Init normals.
    unsigned int normal_i = 0;
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){1.0f, 0.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){-1.0f, 0.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, 1.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, -1.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, 0.0f, 1.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){.0f, 0.0f, -1.0f}, (*vertices)[normal_i].normal);
    }

    // Init positions.

    // +X face.
    unsigned int i = 0;
    glm_vec3_make((vec3){half, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, half, half}, (*vertices)[i].pos);
    i += 1;

    // -X face.
    glm_vec3_make((vec3){-half, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, -half, half}, (*vertices)[i].pos);
    i += 1;

    // +Y face.
    glm_vec3_make((vec3){half, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, half, half}, (*vertices)[i].pos);
    i += 1;

    // -Y face.
    glm_vec3_make((vec3){-half, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, -half, half}, (*vertices)[i].pos);
    i += 1;

    // +Z face.
    glm_vec3_make((vec3){-half, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, half, half}, (*vertices)[i].pos);
    i += 1;

    // -Z face.
    glm_vec3_make((vec3){-half, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half, -half, -half}, (*vertices)[i].pos);
    i += 1;

    (*index_count) = 36;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    (*indices)[0] = 0; // +X face.
    (*indices)[1] = 1;
    (*indices)[2] = 2;
    (*indices)[3] = 3;
    (*indices)[4] = 2;
    (*indices)[5] = 1;
    (*indices)[6] = 4; // -X face.
    (*indices)[7] = 5;
    (*indices)[8] = 6;
    (*indices)[9] = 7;
    (*indices)[10] = 6;
    (*indices)[11] = 5;
    (*indices)[12] = 8; // +Y face.
    (*indices)[13] = 9;
    (*indices)[14] = 10;
    (*indices)[15] = 11;
    (*indices)[16] = 10;
    (*indices)[17] = 9;
    (*indices)[18] = 12; // -Y face.
    (*indices)[19] = 13;
    (*indices)[20] = 14;
    (*indices)[21] = 15;
    (*indices)[22] = 14;
    (*indices)[23] = 13;
    (*indices)[24] = 16; // +Z face.
    (*indices)[25] = 17;
    (*indices)[26] = 18;
    (*indices)[27] = 19;
    (*indices)[28] = 18;
    (*indices)[29] = 17;
    (*indices)[30] = 20; // -Z face.
    (*indices)[31] = 21;
    (*indices)[32] = 22;
    (*indices)[33] = 23;
    (*indices)[34] = 22;
    (*indices)[35] = 21;
}
