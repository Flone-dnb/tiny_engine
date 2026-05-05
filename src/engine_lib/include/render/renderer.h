#pragma once

#include <cglm/vec3.h>

typedef struct te_renderer te_renderer;

struct te_window;
struct te_shader_manager;
struct te_texture_manager;
struct te_font_manager;

// Groups data about all lighting used during the rendering.
typedef struct te_lighting_data {
    // Color in RGB and intensity in A.
    vec4 directional_light_color;

    // Color in RGB and intensity in A.
    vec4 point_light_color;

    // Position in XYZ and light radius in W.
    vec4 point_light_pos_and_dist;

    // Unit vector in the direction of the light source.
    vec3 directional_light_direction;

    vec3 ambient_light_color;
} te_lighting_data;

te_renderer* renderer_create(struct te_window* window);
void renderer_destroy(te_renderer* renderer);

// Returns parameters to configure lighting.
// Do not free returned pointer, valid while the renderer exists.
te_lighting_data* renderer_get_lighting_data(te_renderer* renderer);

// Sets the maximum number of frames per second that is allowed for the renderer,
// specify 0 to disable the limit.
void renderer_set_fps_limit(te_renderer* renderer, unsigned int limit);

// Returns window.
// Always valid pointer. Do not free/destroy the pointer.
struct te_window* renderer_get_window(te_renderer* renderer);

// Returns shader manager.
// Always valid pointer. Do not free/destroy the pointer, valid while the renderer exists.
struct te_shader_manager* renderer_get_shader_manager(te_renderer* renderer);

// Returns texture manager.
// Always valid pointer. Do not free/destroy the pointer, valid while the renderer exists.
struct te_texture_manager* renderer_get_texture_manager(te_renderer* renderer);

// Returns font manager.
// Always valid pointer. Do not free/destroy the pointer, valid while the renderer exists.
struct te_font_manager* renderer_get_font_manager(te_renderer* renderer);

unsigned int renderer_get_fps(te_renderer* renderer);

// Returns 0 if not set.
unsigned int renderer_get_fps_limit(te_renderer* renderer);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Submits a new frame.
void prv_renderer_draw_frame(te_renderer* renderer, float delta_time_sec);

// Called after the window changed its size.
void prv_renderer_on_window_size_changed(te_renderer* renderer);
