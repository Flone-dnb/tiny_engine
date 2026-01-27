#include "game/model.h"

#include <stdlib.h>
#include <string.h>
#include "cglm/mat4.h"
#include "game_manager.h"
#include "glad/glad.h"
#include "math_funcs.h"
#include "misc/error.h"
#include "render/model_renderer.h"
#include "render/renderer.h"
#include "render/shader_manager.h"
#include "world.h"

/** Vertex of a model. */
typedef struct te_model_vertex {
    // NOTE: if changing this struct also update gl vertex attribute description

    /** Position. */
    vec3 pos;

    /** Normal direction. */
    vec3 normal;

    /** UV. */
    vec2 uv;
} te_model_vertex;

/** 3D model. */
struct te_model {
    /**
     * Path (relative to the `res` directory) to the file that stores mesh geometry.
     * NULL if instead a default model should be used.
     */
    char* path_to_geo;

    /** NULL if default vertex shader is used. Path (relative to the `res` directory) to custom vertex shader. */
    char* custom_vert_relative_path;

    /** NULL if default fragment shader is used. Path (relative to the `res` directory) to custom fragment shader.  */
    char* custom_frag_relative_path;

    /** NULL if not spawned. Do not free/destroy this pointer. */
    te_world* world;

    /** Color of the model in the RGBA format in range [0.0; 1.0]. */
    vec4 color;

    /** Location. */
    vec3 location;

    /** Rotation in degrees. */
    vec3 rotation;

    /** Scale. */
    vec3 scale;

    /** Stores invalid value if not spawned (see @ref world). */
    unsigned int render_data_handle;

    /** Stores invalid value if not spawned (see @ref world). OpenGL ID of the shader program used. */
    unsigned int shader_prog_id;

    /** Vertex buffer object ID. Invalid if not spawned (see @ref world). */
    unsigned int vbo;

    /** Element buffer object ID. Invalid if not spawned (see @ref world). */
    unsigned int ebo;
};

te_model*
model_create(const char* path_to_geo) {
    te_model* model = malloc(sizeof(te_model));

    model->world = NULL;
    model->render_data_handle = 0xffffffff;
    model->shader_prog_id = 0xffffffff;
    model->vbo = 0xffffffff;
    model->ebo = 0xffffffff;
    model->custom_vert_relative_path = NULL;
    model->custom_frag_relative_path = NULL;

    if (path_to_geo == NULL) {
        model->path_to_geo = NULL;
    } else {
        const unsigned long path_len = strlen(path_to_geo);
        model->path_to_geo = malloc(sizeof(char) * (path_len + 1));
        memcpy(model->path_to_geo, path_to_geo, path_len);
        model->path_to_geo[path_len] = 0;
    }

    glm_vec4_one(model->color);

    glm_vec3_zero(model->location);
    glm_vec3_zero(model->rotation);
    glm_vec3_one(model->scale);

    free(model->custom_vert_relative_path);
    free(model->custom_frag_relative_path);

    return model;
}

void
model_destroy(te_model* model) {
    free(model->path_to_geo);
    free(model);
}

void
prv_model_calc_world_normal_matrices(te_model* model, mat4 world, mat3 normal) {
    mat4 translate_mat;
    glm_translate_make(translate_mat, model->location);

    mat4 rot_mat;
    math_make_rotation_mat(model->rotation, rot_mat);

    mat4 scale_mat;
    glm_scale_make(scale_mat, model->scale);

    // Scale first, then rotate, then translate.
    glm_mat4_mul(rot_mat, scale_mat, world);
    glm_mat4_mul(translate_mat, world, world);

    mat4 normal_mat;
    glm_mat4_inv(world, normal_mat);
    glm_mat4_transpose(normal_mat);

    glm_mat4_pick3(normal_mat, normal);
}

void
model_set_location(te_model* model, vec3 location) {
    glm_vec3_copy(location, model->location);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            world_get_model_renderer(model->world), model->render_data_handle);
        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
    }
}

void
model_set_rotation(te_model* model, vec3 rotation) {
    glm_vec3_copy(rotation, model->rotation);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            world_get_model_renderer(model->world), model->render_data_handle);
        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
    }
}

void
model_set_scale(te_model* model, vec3 scale) {
    glm_vec3_copy(scale, model->scale);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            world_get_model_renderer(model->world), model->render_data_handle);
        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
    }
}

void
model_set_color(te_model* model, vec4 color) {
    glm_vec4_copy(color, model->color);

    if (model->world != NULL) {
        // Update render data.
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            world_get_model_renderer(model->world), model->render_data_handle);
        glm_vec4_copy(model->color, data->color);
    }
}

void
model_set_custom_vert_shader(te_model* model, const char* vert_relative_path) {
    if (model->world != NULL) {
        show_error_and_abort("setting custom shader is not allowed while the model is spawned");
    }

    free(model->custom_vert_relative_path);

    const unsigned long len = strlen(vert_relative_path);
    model->custom_vert_relative_path = malloc(sizeof(char) * (len + 1));
    memcpy(model->custom_vert_relative_path, vert_relative_path, sizeof(char) * len);
    model->custom_vert_relative_path[len] = 0;
}

void
model_set_custom_frag_shader(te_model* model, const char* frag_relative_path) {
    if (model->world != NULL) {
        show_error_and_abort("setting custom shader is not allowed while the model is spawned");
    }

    free(model->custom_frag_relative_path);

    const unsigned long len = strlen(frag_relative_path);
    model->custom_frag_relative_path = malloc(sizeof(char) * (len + 1));
    memcpy(model->custom_frag_relative_path, frag_relative_path, sizeof(char) * len);
    model->custom_frag_relative_path[len] = 0;
}

void
model_get_location(te_model* model, vec3 out) {
    glm_vec3_copy(model->location, out);
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
model_get_color(te_model* model, vec4 out) {
    glm_vec4_copy(model->color, out);
}

te_world*
model_get_world(te_model* model) {
    return model->world;
}

void prv_model_generate_cube(te_model_vertex** vertices, unsigned short** indices, unsigned int* vertex_count,
                             unsigned int* index_count);

void
prv_model_on_spawned(te_model* model, te_world* world) {
    model->world = world;

    // Get shader program.
    {
        te_shader_manager* shader_manager =
            renderer_get_shader_manager(game_manager_get_renderer(world_get_game_manager(world)));
        model->shader_prog_id = shader_manager_request_shader(
            shader_manager,
            model->custom_vert_relative_path == NULL ? "engine/shader/model.vert.glsl"
                                                     : model->custom_vert_relative_path,
            model->custom_frag_relative_path == NULL ? "engine/shader/model.frag.glsl"
                                                     : model->custom_frag_relative_path);
    }

    // Load geometry.
    unsigned int index_count = 0;
    {
        te_model_vertex* vertices;
        unsigned int vertex_count;
        unsigned short* indices;
        prv_model_generate_cube(&vertices, &indices, &vertex_count, &index_count);

        glGenBuffers(1, &model->vbo);
        glGenBuffers(1, &model->ebo);

        glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(te_model_vertex) * vertex_count, vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * index_count, indices, GL_STATIC_DRAW);

        const unsigned long pos_offset = 0;
        const unsigned long normal_offset = sizeof(vec3);
        const unsigned long uv_offset = sizeof(vec3) * 2;

        // Position.
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(te_model_vertex), &pos_offset);
        glBindAttribLocation(model->shader_prog_id, 0, "pos");

        // Normal.
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(te_model_vertex), &normal_offset);
        glBindAttribLocation(model->shader_prog_id, 1, "normal");

        // UV.
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(te_model_vertex), &uv_offset);
        glBindAttribLocation(model->shader_prog_id, 2, "uv");

        free(vertices);
        free(indices);
    }

    // Add to rendering.
    {
        te_model_renderer* model_renderer = world_get_model_renderer(world);
        model->render_data_handle = model_renderer_add_model(model_renderer, model->shader_prog_id);
    }

    // Init render data.
    {
        te_model_render_data* data = model_renderer_get_render_data_tmp(
            world_get_model_renderer(model->world), model->render_data_handle);

        glm_vec4_copy(model->color, data->color);
        prv_model_calc_world_normal_matrices(model, data->world_mat, data->normal_mat);
        data->vbo = model->vbo;
        data->ebo = model->ebo;
        data->tex_id = 0;
        glm_vec2_make((vec2){-1.0f, -1.0f}, data->tiling);
        data->index_count = index_count;
    }
}

void
prv_model_on_despawned(te_model* model) {
    te_model_renderer* model_renderer = world_get_model_renderer(model->world);

    // Remove from rendering.
    model_renderer_remove_model(model_renderer, model->render_data_handle);

    // Mark shader unused.
    te_shader_manager* shader_manager =
        renderer_get_shader_manager(game_manager_get_renderer(world_get_game_manager(model->world)));
    shader_manager_mark_unused_shader(shader_manager, model->shader_prog_id);

    // Release geometry.
    glDeleteBuffers(1, &model->vbo);
    glDeleteBuffers(1, &model->ebo);

    model->world = NULL;
    model->render_data_handle = 0xffffffff;
    model->shader_prog_id = 0xffffffff;
    model->vbo = 0xffffffff;
    model->ebo = 0xffffffff;
}

void
prv_model_generate_cube(te_model_vertex** vertices, unsigned short** indices, unsigned int* vertex_count,
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
