#pragma once

#include <stdbool.h>

typedef struct te_window te_window;

struct te_game_manager;

/** Initial callbacks that the user must specify. */
typedef struct te_game_window_callbacks {
    /** Called when engine initialized everything and the game is ready to start. */
    void (*on_game_started)(struct te_game_manager* game_manager);

    /** Called before a new frame is rendered. */
    void (*on_game_tick)(struct te_game_manager* game_manager, float delta_time_sec);

    /** Called before the window is closed (before the game is closed). */
    void (*on_window_close)(struct te_game_manager* game_manager);
} te_game_window_callbacks;

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

/**
 * Runs the window's event loop.
 *
 * @param window Window.
 * @param game_callbacks Essential game callbacks that should be specified.
 */
void window_process_events(te_window* window, te_game_window_callbacks* game_callbacks);

/**
 * Returns game manager.
 *
 * @param window Window.
 *
 * @return Always valid pointer to the game manager. Do not free/destroy returned pointer.
 * Valid while @ref window_process_events (i.e. the game) is running.
 */
struct te_game_manager* window_get_game_manager(te_window* window);

/**
 * Sets a flag that stops the window from processing window events
 * that were initiated by calling @ref window_process_events.
 *
 * @param window Window to close.
 */
void window_close(te_window* window);

/**
 * Returns the current size of the window.
 *
 * @param window Window.
 * @param width  The current width of the window.
 * @param height The current height of the window.
 */
void window_get_size(te_window* window, unsigned int* width, unsigned int* height);

/**
 * Returns refresh rate of the used display.
 *
 * @param window Window.
 *
 * @return Display's refresh rate.
 */
unsigned int window_get_display_refresh_rate(te_window* window);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

union SDL_Event;

/**
 * Processes the specified window event.
 *
 * @param window Window.
 * @param event Event to process.
 *
 * @return `true` if received "quit" event.
 */
bool prv_window_process_event(te_window* window, union SDL_Event event);

/**
 * Returns internal window object.
 *
 * @param window Window.
 *
 * @return Internal window object. Do not free/destroy returned pointer.
 */
struct SDL_Window* prv_window_get_sdl_window(te_window* window);
