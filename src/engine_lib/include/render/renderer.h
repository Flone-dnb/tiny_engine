#pragma once

typedef struct te_renderer te_renderer;

struct te_window;
struct te_shader_manager;
struct te_texture_manager;
struct te_font_manager;

te_renderer* renderer_create(struct te_window* window);
void renderer_destroy(te_renderer* renderer);

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
