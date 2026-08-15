#pragma once

#include <stdbool.h>
#include <input/keyboard_button.h>
#include <input/mouse_button.h>

typedef struct te_game_manager te_game_manager;

struct te_renderer;
struct te_window;
struct te_world;
struct te_sound_manager;

// Creates a new world.
// Specify a non-NULL world name. The name will be copied to the world's object.
//
// All created worlds will be automatically destroyed when the game manager is being destroyed
// (when the window is closing) but you can manually destroy them earlier using @ref game_manager_destroy_world.
struct te_world* game_manager_create_world(te_game_manager* game_manager, const char* name);

// Destroys a world previously created using @ref game_manager_create_world.
void game_manager_destroy_world(te_game_manager* game_manager, struct te_world* world);

// Registers a custom function to be called every frame.
// Returns a unique ID of the callback that you need to use to unregister the callback
// in @ref game_manager_remove_tick_callback.
unsigned int game_manager_add_tick_callback(
    te_game_manager* game_manager, void* custom,
    void (*on_tick)(void* custom, float delta_time_sec));

// Unregisters a callback that was previously registered using @ref game_manager_add_tick_callback.
void
game_manager_remove_tick_callback(te_game_manager* game_manager, unsigned int callback_id);

// Returns window that owns game manager.
// Always valid pointer to the window. You should not free/destroy returned pointer.
struct te_window* game_manager_get_window(te_game_manager* game_manager);

// Returns renderer.
// Always valid pointer to the renderer. You should not free/destroy returned pointer.
struct te_renderer* game_manager_get_renderer(te_game_manager* game_manager);

// Returns array of currently existing worlds.
// Do not free/destroy returned pointer.
struct te_world**
game_manager_get_worlds(te_game_manager* game_manager, unsigned int* world_count);

// Returns user's main game system that was specified after window creation.
void* game_manager_get_game_instance(te_game_manager* game_manager);

// Returns sound manager.
// Do not destroy/free returned pointer, valid while the game manager exists.
struct te_sound_manager* game_manager_get_sound_manager(te_game_manager* game_manager);

// Returns the total number of currently registered tick callbacks, see @ref game_manager_add_tick_callback.
unsigned int game_manager_get_tick_callback_count(te_game_manager* game_manager);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

te_game_manager* prv_game_manager_create(struct te_window* window);
void prv_game_manager_destroy(te_game_manager* game_manager);

// Called by window before a new frame is rendered.
void prv_game_manager_tick(te_game_manager* game_manager, float delta_time_sec);

// Called by window to render a new frame.
void prv_game_manager_draw_frame(te_game_manager* game_manager, float delta_time_sec);

// Called by window after its size was changed.
void prv_game_manager_on_window_size_changed(te_game_manager* game_manager);

// Transfers the event to UI.
// Returns `true` if the event was handled by some widget.
bool prv_game_manager_on_mouse_button_pressed(
    te_game_manager* game_manager, enum te_mouse_button button);
bool prv_game_manager_on_mouse_button_released(
    te_game_manager* game_manager, enum te_mouse_button button);
void prv_game_manager_on_mouse_moved(te_game_manager* game_manager);
void prv_game_manager_on_mouse_cursor_captured(te_game_manager* game_manager, bool captured);
void prv_game_manager_on_keyboard_input_text(te_game_manager* game_manager, const char* text);
void prv_game_manager_on_keyboard_input(
    te_game_manager* game_manager, enum te_keyboard_button button, bool is_repeat);

// Called by window after user input device was changed (keyboard+mouse/gamepad).
void prv_game_manager_on_input_source_changed(te_game_manager* game_manager);
