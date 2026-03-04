#pragma once

#include <input/keyboard_button.h>
#include <input/mouse_button.h>

typedef struct te_world te_world;

struct te_game_manager;
struct te_model_renderer;
struct te_widget_renderer;
struct te_camera;
struct te_model;
struct te_widget;

// Returns world's name.
// Do not free/destroy returned pointer.
const char* world_get_name(te_world* world);

// The model will be automatically despawned and destroyed when the world is being destroyed
// but you can despawn the model earlier to manually manage its destruction.
void world_spawn_model(te_world* world, struct te_model* model);
void world_despawn_model(te_world* world, struct te_model* model);

// The camera will be automatically despawned and destroyed when the world is being destroyed
// but you can despawn the camera earlier to manually manage its destruction.
void world_spawn_camera(te_world* world, struct te_camera* camera);
void world_despawn_camera(te_world* world, struct te_camera* camera);

// The widget will be automatically despawned and destroyed when the world is being destroyed
// but you can despawn the widget earlier to manually manage its destruction.
// Also spawns/despawns all child widgets of the specified widget.
void world_spawn_widget(te_world* world, struct te_widget* widget);
void world_despawn_widget(te_world* world, struct te_widget* widget);

// Sets the camera to view the world.
// Specify NULL to remove active camera.
//
// The camera must be previously spawned in this world.
void world_set_active_camera(te_world* world, struct te_camera* camera);

// Serializes all spawned world entities into the specified file
// (path relative to the `res` directory).
void world_save_to_file(te_world* world, const char* relative_path);

// Deserializes game entities from the specified file
// (path relative to the `res` directory) and spawns them in the world.
void world_add_from_file(te_world* world, const char* relative_path);

// Returns NULL if the world has no active camera.
// Do not free/destroy returned pointer, valid until the camera is not destroyed.
struct te_camera* world_get_active_camera(te_world* world);

// Returns NULL if no camera is spawned, otherwise all spawned cameras.
// Do not save/store returned pointer as it might become invalid after a camera is spawned/despawned.
struct te_camera** world_get_cameras_tmp(te_world* world, unsigned int* count);
struct te_model** world_get_models_tmp(te_world* world, unsigned int* count);

// Do not free/destroy returned pointer, valid while the world exists.
struct te_model_renderer* world_get_opaque_model_renderer(te_world* world);
struct te_model_renderer* world_get_transparent_model_renderer(te_world* world);

// Do not free/destroy returned pointer, valid while the world exists.
struct te_widget_renderer* world_get_widget_renderer(te_world* world);

// Returns game manager.
// Always valid pointer. Do not free/destroy returned pointer.
struct te_game_manager* world_get_game_manager(te_world* world);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Creates a new world. Game manager is expected to call this function because it manages game worlds.
te_world* prv_world_create(struct te_game_manager* game_manager, const char* name);
void prv_world_destroy(te_world* world);

// Returns `true` if currently in @ref prv_world_destroy.
bool prv_world_is_being_destroyed(te_world* world);

// Called to possibly notify widgets.
void prv_world_on_window_size_changed(te_world* world);

// Adds/removes the specified item to/from the array of spawned root item
// (does nothing if already added/removed). Does not notify the item.
void prv_world_add_root_model_no_notify(te_world* world, struct te_model* model, bool check_if_already_added);
void prv_world_remove_root_model_no_notify(te_world* world, struct te_model* model, bool must_exist_in_array);
void prv_world_add_root_widget_no_notify(te_world* world, struct te_widget* widget, bool check_if_already_added);
void prv_world_remove_root_widget_no_notify(te_world* world, struct te_widget* widget, bool must_exist_in_array);
void prv_world_add_root_camera_no_notify(te_world* world, struct te_camera* camera, bool check_if_already_added);
void prv_world_remove_root_camera_no_notify(te_world* world, struct te_camera* camera, bool must_exist_in_array);

// Called by spawned widgets that receive input (for example buttons).
// Note: these functions are not called from the base te_widget type (base type does not implement such functionality).
void prv_world_add_interactable_widget(te_world* world, struct te_widget* widget);
void prv_world_remove_interactable_widget(te_world* world, struct te_widget* widget);
void prv_world_interactable_widget_pos_size_changed(te_world* world);

// Cursor pos in range [0.0; 1.0] relative to the window.
// Returns `true` if was handled by some widget.
void prv_world_on_mouse_moved(te_world* world, float cursor_pos[2]);
bool prv_world_on_mouse_button_pressed(te_world* world, enum te_mouse_button button, float cursor_pos[2]);
bool prv_world_on_mouse_button_released(te_world* world, enum te_mouse_button button, float cursor_pos[2]);
void prv_world_on_keyboard_input_text(te_world* world, const char* text);
void prv_world_on_keyboard_input(te_world* world, enum te_keyboard_button button, bool is_repeat);

// Called by game manager after user input device was changed (keyboard+mouse/gamepad).
void prv_world_on_input_source_changed(te_world* world);

// Looks if the specified widget is spawned as one of the root widgets.
bool prv_world_find_root_widget(te_world* world, struct te_widget* widget);

#if defined(ENGINE_DEBUG_TOOLS)
unsigned int prv_world_get_gl_query_draw_models(te_world* world);
unsigned int prv_world_get_gl_query_draw_widgets(te_world* world);
#endif
