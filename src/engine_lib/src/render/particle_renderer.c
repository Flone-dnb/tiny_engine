#include <render/particle_renderer.h>

#include <stdlib.h>
#include <render/renderer.h>
#include <render/render_data_array.h>
#include <glad/gl.h>
#include <game/camera.h>
#include <math/vec2.h>

struct te_particle_renderer {
    te_renderer* renderer;

    te_render_data_array* emitter_data_array;
};

te_particle_renderer*
particle_renderer_create(te_renderer* renderer) {
    te_particle_renderer* particle_renderer = malloc(sizeof(te_particle_renderer));

    particle_renderer->renderer = renderer;
    particle_renderer->emitter_data_array =
        render_data_array_create(sizeof(te_particle_emitter_render_data), 2, 4);

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
    render_data_array_remove_item(renderer->emitter_data_array, handle);
}

te_particle_emitter_render_data*
particle_renderer_get_emitter_render_data_tmp(
    te_particle_renderer* renderer, unsigned int handle) {
    return render_data_array_get_item_data_tmp(renderer->emitter_data_array, handle);
}

void
particle_renderer_draw(
    te_particle_renderer* renderer, te_camera* camera, te_light_params* light_params,
    te_mat4* view_proj_mat) {
    unsigned int emitter_count;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    emitter_count = render_data_array_get_item_count(renderer->emitter_data_array);
    if (emitter_count > 0) {
        te_particle_emitter_render_data* data;
        te_vec3 camera_world_right, camera_world_up;
        unsigned int emitter_idx;
        unsigned int particle_idx;

        camera_get_right(camera, camera_world_right);
        camera_get_up(camera, camera_world_up);

        /* note: lighting should be already setup from the model renderer
	 * here we just need to enable a few things */
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        if (light_params->distance_fog_range[0] >= 0.0f) {
            glEnable(GL_FOG);
        } else {
            glDisable(GL_FOG);
        }

        /* setup global matrices */
        /* we want to draw billboards in screen space */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnable(GL_TEXTURE_2D);

        data = render_data_array_get_internal_array(renderer->emitter_data_array);

        for (emitter_idx = 0; emitter_idx < emitter_count; emitter_idx++) {
            if (data->particle_count > 0) {
                te_particle_render_data* p;
                te_vec3 side, top;
                te_vec4 min, max;

                glBindTexture(GL_TEXTURE_2D, data->tex_id); /* binds 0 if not set */

                for (particle_idx = 0; particle_idx < data->particle_count; particle_idx++) {
                    p = &data->particles[particle_idx];
                    min[3] = 1.0f;
                    max[3] = 1.0f;

                    vec3_muls(camera_world_right, p->pos_and_size[3] / 2.0f, side);
                    vec3_muls(camera_world_up, p->pos_and_size[3] / 2.0f, top);

                    /* quad top-right pos */
                    vec3_add(p->pos_and_size, side, max);
                    vec3_add(max, top, max);

                    vec3_muls(side, -1.0f, side);
                    vec3_muls(top, -1.0f, top);

                    /* quad left-bottom pos */
                    vec3_add(p->pos_and_size, side, min);
                    vec3_add(min, top, min);

                    mat4_mulv((*view_proj_mat), min, min);
                    mat4_mulv((*view_proj_mat), max, max);
                    vec3_divs(min, min[3], min);
                    vec3_divs(max, max[3], max);

                    if (min[0] > 1.0f || min[1] > 1.0f || max[0] < 0.0f || max[1] < 0.0f) {
                        continue;
                    }

                    glBegin(GL_QUADS);

                    glColor4fv(p->color);

                    glTexCoord2f(0.0f, 0.0f);
                    glVertex2f(min[0], min[1]);

                    glTexCoord2f(0.0f, 1.0f);
                    glVertex2f(min[0], max[1]);

                    glTexCoord2f(1.0f, 1.0f);
                    glVertex2f(max[0], max[1]);

                    glTexCoord2f(1.0f, 0.0f);
                    glVertex2f(max[0], min[1]);

                    glEnd();
                }
            }
            data += 1;
        }
    }

    glDisable(GL_BLEND);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHT1);
    glDisable(GL_LIGHTING);
}
