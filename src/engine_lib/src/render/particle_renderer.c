#include <render/particle_renderer.h>

#include <render/renderer.h>
#include <render/render_data_array.h>
#include <render/shader_manager.h>
#include <glad/glad.h>
#include <cglm/vec2.h>
#include <render/gpu_section.h>

#define PARTICLE_QUAD_GL_VERT_ATTRIB_PTR                                                      \
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), NULL);

typedef struct te_particle_shader_data {
    unsigned int prog_id;

    int uniform_view_mat;
    int uniform_proj_mat;
    int uniform_in_color;
    int uniform_in_world_pos_size;
    int uniform_is_using_tex;
} te_particle_shader_data;

struct te_particle_renderer {
    te_renderer* renderer;

    te_render_data_array* emitter_data_array;

    te_particle_shader_data particle_shader_data;

    // Quad geometry.
#if !defined(ENGINE_GLES)
    unsigned int vao;
#endif
    unsigned int vbo;
    unsigned int ebo;
};

te_particle_renderer*
particle_renderer_create(te_renderer* renderer) {
    te_particle_renderer* particle_renderer = malloc(sizeof(te_particle_renderer));

    particle_renderer->renderer = renderer;
    particle_renderer->emitter_data_array =
        render_data_array_create(sizeof(te_particle_emitter_render_data), 2, 4);

    // Load quad shader.
    {
        te_particle_shader_data* shader = &particle_renderer->particle_shader_data;

        shader->prog_id = shader_manager_request_shader(
            renderer_get_shader_manager(renderer), "engine/shader/particle.vert.glsl",
            "engine/shader/particle.frag.glsl");

        shader->uniform_view_mat = get_uniform_location(shader->prog_id, "view_mat");
        shader->uniform_proj_mat = get_uniform_location(shader->prog_id, "proj_mat");
        shader->uniform_in_color = get_uniform_location(shader->prog_id, "in_color");
        shader->uniform_in_world_pos_size =
            get_uniform_location(shader->prog_id, "in_world_pos_size");
        shader->uniform_is_using_tex = get_uniform_location(shader->prog_id, "is_using_tex");
    }

    // Create quad geometry.
    {
        vec2 vertices[4]; // XY pos, ZW uv
        glm_vec2_copy((vec2){0.0f, 0.0f}, &vertices[0][0]);
        glm_vec2_copy((vec2){0.0f, 1.0f}, &vertices[1][0]);
        glm_vec2_copy((vec2){1.0f, 1.0f}, &vertices[2][0]);
        glm_vec2_copy((vec2){1.0f, 0.0f}, &vertices[3][0]);
        const unsigned short indices[6] = {0, 1, 2, 0, 2, 3};

#if !defined(ENGINE_GLES)
        glGenVertexArrays(1, &particle_renderer->vao);
#endif
        glGenBuffers(1, &particle_renderer->vbo);
        glGenBuffers(1, &particle_renderer->ebo);

#if !defined(ENGINE_GLES)
        glBindVertexArray(particle_renderer->vao);
#endif
        {
            // Vertex buffer.
            glBindBuffer(GL_ARRAY_BUFFER, particle_renderer->vbo);
            glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vec2), &vertices[0][0], GL_STATIC_DRAW);

            // Vertex layout.
            glEnableVertexAttribArray(0);
            PARTICLE_QUAD_GL_VERT_ATTRIB_PTR

            // Index buffer.
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, particle_renderer->ebo);
            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned short), &indices[0],
                GL_STATIC_DRAW);
        }
#if !defined(ENGINE_GLES)
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    return particle_renderer;
}

void
particle_renderer_destroy(te_particle_renderer* renderer) {
    render_data_array_destroy(renderer->emitter_data_array);

    free(renderer);
}

unsigned int
particle_renderer_add_emitter(te_particle_renderer* renderer) {
    return render_data_array_add_item(renderer->emitter_data_array);
}

void
particle_renderer_remove_emitter(te_particle_renderer* renderer, unsigned int handle) {
    // Cleanup.
    te_particle_emitter_render_data* data =
        render_data_array_get_item_data_tmp(renderer->emitter_data_array, handle);
    free(data->particles);

    render_data_array_remove_item(renderer->emitter_data_array, handle);
}

te_particle_emitter_render_data*
particle_renderer_get_emitter_render_data_tmp(
    te_particle_renderer* renderer, unsigned int handle) {
    return render_data_array_get_item_data_tmp(renderer->emitter_data_array, handle);
}

void
particle_renderer_draw(te_particle_renderer* renderer, mat4* view_mat, mat4* proj_mat) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const unsigned int emitter_count =
        render_data_array_get_item_count(renderer->emitter_data_array);
    if (emitter_count > 0) {
        te_particle_shader_data* shader = &renderer->particle_shader_data;
        GPU_SECTION_BEGIN("particle");

        glUseProgram(shader->prog_id);

#if defined(ENGINE_GLES)
        glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);
        PARTICLE_QUAD_GL_VERT_ATTRIB_PTR
#else
        glBindVertexArray(renderer->vao);
#endif

        glActiveTexture(GL_TEXTURE0); // quad texture

        glUniformMatrix4fv(shader->uniform_view_mat, 1, GL_FALSE, (*view_mat)[0]);
        glUniformMatrix4fv(shader->uniform_proj_mat, 1, GL_FALSE, (*proj_mat)[0]);

        te_particle_emitter_render_data* data =
            render_data_array_get_internal_array(renderer->emitter_data_array);

        for (unsigned int emitter_idx = 0; emitter_idx < emitter_count; emitter_idx++) {
            if (data->particle_count > 0) {
                glUniform1i(shader->uniform_is_using_tex, data->tex_id > 0);
                glBindTexture(GL_TEXTURE_2D, data->tex_id);

                for (unsigned int particle_idx = 0; particle_idx < data->particle_count;
                     particle_idx++) {
                    glUniform4fv(
                        shader->uniform_in_color, 1, data->particles[particle_idx].color);
                    glUniform4fv(
                        shader->uniform_in_world_pos_size, 1,
                        data->particles[particle_idx].pos_and_size);

                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
                }
            }
            data += 1;
        }

        GPU_SECTION_END;
    }

    glDisable(GL_BLEND);
}
