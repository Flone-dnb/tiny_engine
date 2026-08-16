#pragma once

#include <input/keyboard_button.h>
#include <input/mouse_button.h>
#include <cglm/vec3.h>

typedef struct te_world te_world;

struct te_game_manager;
struct te_model_renderer;
struct te_widget_renderer;
struct te_particle_renderer;
struct te_game_object_info;
struct te_widget;
struct te_camera;
struct te_sound;
struct te_model;
struct te_scene_animation;

// Returns world's name.
// Do not free/destroy returned pointer.
const char* world_get_name(te_world* world);

// The game object will be automatically despawned and destroyed when the world is being destroyed
// but you can despawn the game object earlier to manually manage its destruction.
void
world_spawn_game_object(te_world* world, void* game_object, struct te_game_object_info* info);
void world_despawn_game_object(
    te_world* world, void* game_object, struct te_game_object_info* info);

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

// Worlds takes the ownership of the sound and plays it.
// The world will destroy the sound once the sound is finished or paused/stopped (or when the world is destroyed).
// Generally you are NOT expected to use the sound pointer after calling this function.
// In case you want to attach a 3D sound to a model use model's functionality.
void world_play_fire_and_forget_sound_2d(te_world* world, struct te_sound* sound);
void world_play_fire_and_forget_sound_3d(
    te_world* world, struct te_sound* sound, vec3 world_position);

// Creates (or loads if path is not NULL) a new scene animation that will be saved next to the world file
// (separately) when @ref world_save_to_file is called.
// Do not destroy returned pointer, it will be automatically destroyed by world during world
// destruction or when another scene animation will replace it.
struct te_scene_animation*
world_create_scene_animation(te_world* world, const char* relative_path_to_load);

// Returns NULL if no scene animation was created/loaded previously,
// see @ref world_create_scene_animation.
struct te_scene_animation* world_get_scene_animation(te_world* world);

// Serializes all spawned world entities into the specified file
// (path relative to the `res` directory).
// Specify `write_light_params` to also save world lighting parameters.
void world_save_to_file(te_world* world, const char* relative_path, bool write_light_params);

// Deserializes game entities from the specified file (path relative to the `res` directory)
// and spawns them in the world.
// - If `load_light_params` is `true` will also load lighting parameters such as ambient light color,
// fog color, directional light color/direction and etc. that were used while the world was saved.
// After the world finished loaded it will adjust renderer's lighting parameters that were saved.
// - Additionally can add a location offset to 3D game objects.
void world_add_from_file(te_world* world, const char* relative_path, bool load_light_params);
void world_add_from_file_with_offset(
    te_world* world, const char* relative_path, bool load_light_params, vec3 location_offset);

// Returns NULL if the world has no active camera.
// Do not free/destroy returned pointer, valid until the camera is not destroyed.
struct te_camera* world_get_active_camera(te_world* world);

// Returns `false` if the cursor is outside of the world's viewport (or if there's no active camera).
// Otherwise returns cursor pos relative to the world camera's viewport.
bool world_get_cursor_relative_pos(te_world* world, vec2 cursor_pos);

// Returns NULL if no objects are spawned, otherwise all spawned objects.
// You must free returned array (but not the items in the array).
// Note: returned array only contains "root" game objects (does not include attached/child game objects).
struct te_game_object_data* world_get_root_game_objects(te_world* world, unsigned int* count);
struct te_widget** world_get_widgets(te_world* world, unsigned int* count);

// Do not free/destroy returned pointer, valid while the world exists.
struct te_model_renderer* world_get_opaque_model_renderer(te_world* world);
struct te_model_renderer* world_get_transparent_model_renderer(te_world* world);

// Do not free/destroy returned pointer, valid while the world exists.
struct te_widget_renderer* world_get_widget_renderer(te_world* world);

// Do not free/destroy returned pointer, valid while the world exists.
struct te_particle_renderer* world_get_particle_renderer(te_world* world);

// Returns game manager.
// Always valid pointer. Do not free/destroy returned pointer.
struct te_game_manager* world_get_game_manager(te_world* world);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Creates a new world. Game manager is expected to call this function because it manages game worlds.
te_world* prv_world_create(struct te_game_manager* game_manager, const char* name);
void prv_world_destroy(te_world* world);

// Called every frame.
void prv_world_tick(te_world* world);

// Returns `true` if currently in @ref prv_world_destroy.
bool prv_world_is_being_destroyed(te_world* world);

// Called to possibly notify widgets.
void prv_world_on_window_size_changed(te_world* world);

// Adds/removes the specified item to/from the array of spawned root game objects
// (does nothing if already added/removed). Does not notify the item being removed.
void prv_world_add_root_game_object_no_notify(
    te_world* world, void* game_object, struct te_game_object_info* info,
    bool ignore_if_already_added);
void prv_world_remove_root_game_object_no_notify(
    te_world* world, void* game_object, bool must_exist_in_array);
void prv_world_add_root_widget_no_notify(
    te_world* world, struct te_widget* widget, bool check_if_already_added);
void prv_world_remove_root_widget_no_notify(
    te_world* world, struct te_widget* widget, bool must_exist_in_array);

// Called by spawned widgets that receive input (for example buttons).
// Note: these functions are not called from the base te_widget type (base type does not implement such functionality).
void prv_world_add_interactable_widget(te_world* world, struct te_widget* widget);
void prv_world_remove_interactable_widget(te_world* world, struct te_widget* widget);
void prv_world_interactable_widget_pos_size_changed(te_world* world);

// Cursor pos in range [0.0; 1.0] relative to the window.
// Returns `true` if was handled by some widget.
void prv_world_on_mouse_cursor_captured(te_world* world, bool captured, float cursor_pos[2]);
void prv_world_on_mouse_moved(te_world* world, float cursor_pos[2]);
bool prv_world_on_mouse_button_pressed(
    te_world* world, enum te_mouse_button button, float cursor_pos[2]);
bool prv_world_on_mouse_button_released(
    te_world* world, enum te_mouse_button button, float cursor_pos[2]);
void prv_world_on_keyboard_input_text(te_world* world, const char* text);
void
prv_world_on_keyboard_input(te_world* world, enum te_keyboard_button button, bool is_repeat);

// Called by game manager after user input device was changed (keyboard+mouse/gamepad).
void prv_world_on_input_source_changed(te_world* world);

#if defined(ENGINE_DEBUG_TOOLS)
unsigned int prv_world_get_gl_query_draw_models(te_world* world);
unsigned int prv_world_get_gl_query_draw_particles(te_world* world);
unsigned int prv_world_get_gl_query_draw_widgets(te_world* world);
#endif
