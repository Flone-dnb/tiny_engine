#include <render/model_renderer.h>

#include <stdlib.h>
#include <debug_console.h>
#include <game/model.h>
#include <io/log.h>
#include <render/renderer.h>
#include <render/render_data_array.h>
#include <shape/frustum_shape.h>
#include <glad/gl.h>

#define INVALID_DATA_INDEX 0xffffffff

struct te_model_renderer {
    te_render_data_array* non_skinned_data_array;
    te_render_data_array* skinned_data_array;
};

te_model_renderer*
model_renderer_create(unsigned int capacity, unsigned int expand_size) {
    te_model_renderer* renderer = malloc(sizeof(te_model_renderer));

    renderer->non_skinned_data_array =
        render_data_array_create(sizeof(te_model_render_data), capacity, expand_size);
    renderer->skinned_data_array =
        render_data_array_create(sizeof(te_model_render_data), 1, 2);

    return renderer;
}

void
model_renderer_destroy(te_model_renderer* renderer) {
    render_data_array_destroy(renderer->non_skinned_data_array);
    render_data_array_destroy(renderer->skinned_data_array);

    free(renderer);
}

bool
model_renderer_has_models(te_model_renderer* renderer) {
    return render_data_array_get_item_count(renderer->non_skinned_data_array) > 0
           || render_data_array_get_item_count(renderer->skinned_data_array) > 0;
}

unsigned int
model_renderer_add_model(te_model_renderer* renderer, bool uses_skinning) {
    if (uses_skinning) {
        return render_data_array_add_item(renderer->skinned_data_array);
    } else {
        return render_data_array_add_item(renderer->non_skinned_data_array);
    }
}

void
model_renderer_remove_model(
    te_model_renderer* renderer, unsigned int handle, bool uses_skinning) {
    if (uses_skinning) {
        render_data_array_remove_item(renderer->skinned_data_array, handle);
    } else {
        render_data_array_remove_item(renderer->non_skinned_data_array, handle);
    }
}

te_model_render_data*
model_renderer_get_render_data_tmp(
    te_model_renderer* renderer, unsigned int handle, bool uses_skinning) {
    if (uses_skinning) {
        return render_data_array_get_item_data_tmp(renderer->skinned_data_array, handle);
    } else {
        return render_data_array_get_item_data_tmp(renderer->non_skinned_data_array, handle);
    }
}

unsigned int
model_renderer_draw(
    te_model_renderer* renderer, te_light_params* light_params, te_mat4* view_mat,
    te_mat4* proj_mat, te_frustum_shape* camera_frustum) {
    te_model_render_data* data;
    te_vec4 zero, default_diffuse;
    unsigned int model_count;
    unsigned int model_idx;

    vec4_zero(zero);
    vec4_set(1.0f, 1.0f, 1.0f, 1.0f, default_diffuse);

    glEnable(GL_LIGHTING);

    /* setup global matrices */
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((*proj_mat)[0]);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf((*view_mat)[0]);

    /* setup lighting */
    {
        te_vec4 light_gl_pos;

        /* ambient light */
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, light_params->ambient_light_color);

        /* directional light */
        glEnable(GL_LIGHT0);
        vec3_copy(light_params->directional_light_direction, light_gl_pos);
        vec3_muls(light_gl_pos, -1.0f, light_gl_pos);
        light_gl_pos[3] = 0.0f; /* need 0 to mark a directional light */
        glLightfv(GL_LIGHT0, GL_POSITION, light_gl_pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_params->directional_light_color);
        glLightfv(GL_LIGHT0, GL_SPECULAR, zero);

        /* point light */
        glEnable(GL_LIGHT1);
        vec3_copy(light_params->point_light_pos_and_dist, light_gl_pos);
        light_gl_pos[3] = 1.0f; /* need 1 to mark a point light */
        glLightfv(GL_LIGHT1, GL_POSITION, light_gl_pos);
        glLightfv(GL_LIGHT1, GL_AMBIENT, zero);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light_params->point_light_color);
        glLightfv(GL_LIGHT1, GL_SPECULAR, zero);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.0f);
        glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, light_params->point_light_pos_and_dist[3]);
        glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.0f);
        glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 180.0f); /* to tell that this is not a spotlight */

        /* fog */
        if (light_params->distance_fog_range[0] >= 0.0f) {
            glEnable(GL_FOG);
            glFogi(GL_FOG_MODE, GL_LINEAR);
            glFogf(GL_FOG_START, light_params->distance_fog_range[0]);
            glFogf(GL_FOG_END, light_params->distance_fog_range[1]);
            glFogfv(GL_FOG_COLOR, light_params->distance_fog_color);
            glHint(
                GL_FOG_HINT, GL_NICEST); /* want per-pixel fog (just a hint to the driver) */
        } else {
            glDisable(GL_FOG);
        }
    }

    /* reset some material params (before drawing) */
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, default_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, zero);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    glEnable(GL_TEXTURE_2D);

    /* draw non-skinned models */
    data = render_data_array_get_internal_array(renderer->non_skinned_data_array);
    model_count = render_data_array_get_item_count(renderer->non_skinned_data_array);
    for (model_idx = 0; model_idx < model_count; model_idx++) {
        if (!frustum_shape_is_aabb_inside(camera_frustum, &data->aabb_world)) {
            continue;
        }

        /* copy view matrix and modify to be model view */
        glPushMatrix();
        glMultMatrixf(data->world_mat[0]);

        /* texture and color */
        glMaterialfv(data->gl_face_type, GL_DIFFUSE, data->color);
        glBindTexture(GL_TEXTURE_2D, data->tex_id); /* binds 0 if not set */

        glCallList(data->display_list_id);
        model_count += 1;

        data += 1;

        /* remove modified model view mat */
        glPopMatrix();
    }

    /* skinned models */
    data = render_data_array_get_internal_array(renderer->skinned_data_array);
    model_count = render_data_array_get_item_count(renderer->skinned_data_array);
    for (model_idx = 0; model_idx < model_count; model_idx++) {
        /* NOTE: don't do frustum culling on skinned models due to animations */

        /* copy view matrix and modify to be model view */
        glPushMatrix();
        glMultMatrixf(data->world_mat[0]);

        /* texture and color */
        glMaterialfv(data->gl_face_type, GL_DIFFUSE, data->color);
        glBindTexture(GL_TEXTURE_2D, data->tex_id); /* binds 0 if not set */

        TODO;
        /*glCallList(data->display_list_id);*/
        model_count += 1;

        data += 1;

        /* remove modified model view mat */
        glPopMatrix();
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHT1);
    glDisable(GL_LIGHTING);

    return model_count;
}
