#pragma once

/** Game window. */
typedef struct {
    /** SDL window. */
    struct SDL_Window* sdl_window;
} te_window;

/**
 * Creates a new window.
 *
 * @param window_title Title text.
 *
 * @return Created window.
 */
te_window* window_create(const char* window_title);

/**
 * Destroys the specified window.
 *
 * @param window Window.
 */
void window_destroy(te_window* window);
