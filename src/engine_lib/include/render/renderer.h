#pragma once

typedef struct te_renderer te_renderer;

struct te_window;
struct te_shader_manager;

/**
 * Creates a new renderer.
 *
 * @param window Window.
 *
 * @return Created renderer.
 */
te_renderer* renderer_create(struct te_window* window);

/**
 * Destroys the renderer.
 *
 * @param renderer Renderer.
 */
void renderer_destroy(te_renderer* renderer);

/**
 * Sets the maximum number of frames per second that is allowed for the renderer.
 *
 * @param renderer Renderer.
 * @param limit    Maximum allowed FPS, specify 0 to disable.
*/
void renderer_set_fps_limit(te_renderer* renderer, unsigned int limit);

/**
 * Returns shader manager.
 *
 * @param renderer Renderer.
 *
 * @return Always valid pointer. Do not free/destroy the pointer. Valid while the renderer exists.
 */
struct te_shader_manager* renderer_get_shader_manager(te_renderer* renderer);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Draws a new frame.
 *
 * @param renderer Renderer.
 *
 */
void prv_renderer_draw_frame(te_renderer* renderer);

/**
 * Called after the window changed its size.
 *
 * @param renderer Renderer.
 */
void prv_renderer_on_window_size_changed(te_renderer* renderer);
