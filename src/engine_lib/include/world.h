#pragma once

#include "input/mouse_button.h"

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

// Returns NULL if the world has no active camera.
// Do not free/destroy returned pointer, valid until the camera is not destroyed.
struct te_camera* world_get_active_camera(te_world* world);

// Do not free/destroy returned pointer, valid while the world exists.
struct te_model_renderer* world_get_model_renderer(te_world* world);

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

// Called to possibly notify widgets.
void prv_world_on_window_size_changed(te_world* world);

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

// Called by game manager after user input device was changed (keyboard+mouse/gamepad).
void prv_world_on_input_source_changed(te_world* world);

#if defined(ENGINE_DEBUG_TOOLS)
unsigned int prv_world_get_gl_query_draw_models(te_world* world);
unsigned int prv_world_get_gl_query_draw_widgets(te_world* world);
#endif
