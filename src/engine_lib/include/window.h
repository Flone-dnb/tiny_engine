#pragma once

#include <stdbool.h>

#include "input/gamepad_button.h"
#include "input/keyboard_button.h"
#include "input/mouse_button.h"

typedef struct te_window te_window;

struct te_game_manager;

/** Initial callbacks that the user must specify. All must be non-NULL. */
typedef struct te_window_callbacks {
    /** Called when engine initialized everything and the game is ready to start. */
    void (*on_game_started)(void* game_instance, struct te_game_manager* game_manager);

    /** Called before a new frame is rendered. */
    void (*on_game_tick)(void* game_instance, struct te_game_manager* game_manager, float delta_time_sec);

    /** Called when the window receives keyboard input. */
    void (*on_keyboard_button_pressed)(void* game_instance, struct te_game_manager* game_manager,
                                       enum te_keyboard_button button, te_keyboard_modifiers modifiers);

    /** Called when the window receives keyboard input. */
    void (*on_keyboard_button_released)(void* game_instance, struct te_game_manager* game_manager,
                                        enum te_keyboard_button button, te_keyboard_modifiers modifiers);

    /** Called when the window receives gamepad input. */
    void (*on_gamepad_button_pressed)(void* game_instance, struct te_game_manager* game_manager,
                                      enum te_gamepad_button button);

    /** Called when the window receives gamepad input. */
    void (*on_gamepad_button_released)(void* game_instance, struct te_game_manager* game_manager,
                                       enum te_gamepad_button button);

    /** Called when the window receives gamepad input. New pos in range [-1.0f; 1.0f]. */
    void (*on_gamepad_axis_moved)(void* game_instance, struct te_game_manager* game_manager,
                                  enum te_gamepad_axis axis, float new_pos);

    /** Called when the window receives mouse input. */
    void (*on_mouse_button_pressed)(void* game_instance, struct te_game_manager* game_manager,
                                    enum te_mouse_button button);

    /** Called when the window receives mouse input. */
    void (*on_mouse_button_released)(void* game_instance, struct te_game_manager* game_manager,
                                     enum te_mouse_button button);

    /** Called when the window receives mouse input. */
    void (*on_mouse_moved)(void* game_instance, struct te_game_manager* game_manager, float x_offset,
                           float y_offset);

    /** Called when the window receives mouse input. */
    void (*on_mouse_scroll_moved)(void* game_instance, struct te_game_manager* game_manager, float offset);

    /** Called when a gamepad connects. Gamepad name string should not be freed/destroyed. */
    void (*on_gamepad_connected)(void* game_instance, struct te_game_manager* game_manager,
                                 const char* gamepad_name);

    /** Called when a gamepad disconnects. */
    void (*on_gamepad_disconnected)(void* game_instance, struct te_game_manager* game_manager);

    /** Called after the input device changed. */
    void (*on_input_source_changed)(void* game_instance, struct te_game_manager* game_manager,
                                    bool is_gamepad_current);

    /** Called after the window received focus. */
    void (*on_window_received_focus)(void* game_instance, struct te_game_manager* game_manager);

    /** Called after the window lost focus. */
    void (*on_window_lost_focus)(void* game_instance, struct te_game_manager* game_manager);

    /** Called before the window is closed (before the game is closed). */
    void (*on_window_close)(void* game_instance, struct te_game_manager* game_manager);
} te_window_callbacks;

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
 * @param window_callbacks Essential game callbacks that should be specified.
 * @param game_instance    Pointer to the game's main system. This pointer will be passed to various callbacks.
 */
void window_process_events(te_window* window, te_window_callbacks* window_callbacks, void* game_instance);

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
 * Captures the mouse cursor to be inside of the window.
 *
 * @param window Window.
 * @param enable `true` to enable.
 */
void window_capture_mouse_cursor(te_window* window, bool enable);

/**
 * Tells if the mouse is currently captured (see @ref window_capture_mouse_cursor).
 *
 * @param window Window.
 *
 * @return `true` if captured.
 */
bool window_is_mouse_captured(te_window* window);

/**
 * Tell if a gamepad is currently connected or not.
 *
 * @param window Window.
 *
 * @return `true` if connected.
 */
bool window_is_gamepad_connected(te_window* window);

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
