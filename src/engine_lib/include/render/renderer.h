#pragma once

#include <math/vec2.h>
#include <math/vec3.h>
#include <math/vec4.h>

typedef struct te_renderer te_renderer;

struct te_window;
struct te_shader_manager;
struct te_texture_manager;
struct te_font_manager;

/* groups data about all lighting used during the rendering */
typedef struct te_light_params {
    /* color in RGB and intensity in A */
    te_vec4 directional_light_color;
    te_vec4 point_light_color;

    /* position in XYZ and light radius in W */
    te_vec4 point_light_pos_and_dist;

    /* unit vector in the direction of the light source */
    te_vec3 directional_light_direction;

    /* note: if adding new variables add them to reflection
     * ------------------------------------------------------ */

    te_vec3 ambient_light_color;

    /* backbuffer (background) fill color */
    te_vec3 clear_color;

    /* color if distance fog (if enabled  @ref distance_fog_range) */
    te_vec3 distance_fog_color;

    /* stores (-1, -1) if disabled otherwise stores start (min fog) and end (max fog)
     * positions in range [0.0; +inf] as distance from camera */
    te_vec2 distance_fog_range;
} te_light_params;

te_renderer* renderer_create(struct te_window* window);
void renderer_destroy(te_renderer* renderer);

/* returns parameters to configure lighting
 * do not free returned pointer, valid while the renderer exists */
te_light_params* renderer_get_light_params(te_renderer* renderer);

/* sets the maximum number of frames per second that is allowed for the renderer,
 * specify 0 to disable the limit */
void renderer_set_fps_limit(te_renderer* renderer, unsigned int limit);

/* returns window, always valid pointer, do not free/destroy the pointer */
struct te_window* renderer_get_window(te_renderer* renderer);

/* returns shader manager, always valid pointer
 * do not free/destroy the pointer, valid while the renderer exists */
struct te_shader_manager* renderer_get_shader_manager(te_renderer* renderer);

/* returns texture manager, always valid pointer
 * do not free/destroy the pointer, valid while the renderer exists */
struct te_texture_manager* renderer_get_texture_manager(te_renderer* renderer);

/* returns font manager, always valid pointer
 * do not free/destroy the pointer, valid while the renderer exists */
struct te_font_manager* renderer_get_font_manager(te_renderer* renderer);

unsigned int renderer_get_fps(te_renderer* renderer);

/* returns 0 if not set */
unsigned int renderer_get_fps_limit(te_renderer* renderer);

/** ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------- */

/* submits a new frame */
void prv_renderer_draw_frame(te_renderer* renderer, float delta_time_sec);

/* called after the window changed its size */
void prv_renderer_on_window_size_changed(te_renderer* renderer);
