#pragma once

struct te_window;

/** Draws on the window. */
typedef struct te_renderer {
    /** Always valid pointer, window that owns the renderer. This pointer should not be freed. */
    struct te_window* window;
} te_renderer;

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
