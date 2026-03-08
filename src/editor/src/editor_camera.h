#pragma once

#include <stdbool.h>
#include <input/gamepad_button.h>
#include <input/keyboard_button.h>

/** AKA viewport camera (holds an actual camera inside of it). */
typedef struct te_editor_camera te_editor_camera;

struct te_camera;
struct te_world;

te_editor_camera* editor_camera_create();
void editor_camera_destroy(te_editor_camera* editor_camera);

// Spawns the viewport camera in the specified world and makes the camera active.
// @ref editor_camera_despawn must be called before the world is destroyed.
void editor_camera_spawn(te_editor_camera* editor_camera, struct te_world* world);
void editor_camera_despawn(te_editor_camera* editor_camera, struct te_world* world);

// Sets whether the camera should react to the input or not.
void editor_camera_enable_input(te_editor_camera* editor_camera, bool enable);

// Do not free/destroy returned pointer, always valid pointer while the editor camera exists.
struct te_camera* editor_camera_get_camera(te_editor_camera* editor_camera);

// callbacks -------------------------------------------------------------------------------
void
editor_camera_on_mouse_moved(te_editor_camera* editor_camera, float x_offset, float y_offset);
void editor_camera_on_gamepad_axis_moved(
    te_editor_camera* editor_camera, enum te_gamepad_axis axis, float new_pos);
void editor_camera_on_keyboard_button_pressed(
    te_editor_camera* editor_camera, enum te_keyboard_button button);
void editor_camera_on_keyboard_button_released(
    te_editor_camera* editor_camera, enum te_keyboard_button button);
void editor_camera_on_game_tick(te_editor_camera* editor_camera, float delta_time_sec);
void editor_camera_on_gamepad_connected(te_editor_camera* editor_camera);
void editor_camera_on_gamepad_disconnected(te_editor_camera* editor_camera);
// ------------------------------------------------------------------------------------------------
