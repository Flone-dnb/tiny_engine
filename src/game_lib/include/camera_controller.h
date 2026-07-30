#pragma once

#include <stdbool.h>
#include <input/gamepad_button.h>
#include <input/keyboard_button.h>

// Give it a camera and it will allow the player to control the camera through user input.
typedef struct te_camera_controller te_camera_controller;

struct te_camera;

te_camera_controller* camera_controller_create();
void camera_controller_destroy(te_camera_controller* controller);

// Sets a camera to control. The controller binds to camera's on_before_destroyed and will automatically stop
// using the camera before its destroyed.
void camera_controller_set_camera(te_camera_controller* controller, struct te_camera* camera);

// Sets whether the camera should react to the input or not.
void camera_controller_enable_input(te_camera_controller* controller, bool enable);

// callbacks -------------------------------------------------------------------------------
void camera_controller_on_mouse_moved(
    te_camera_controller* controller, float x_offset, float y_offset);
void camera_controller_on_gamepad_axis_moved(
    te_camera_controller* controller, enum te_gamepad_axis axis, float new_pos);
void camera_controller_on_keyboard_button_pressed(
    te_camera_controller* controller, enum te_keyboard_button button);
void camera_controller_on_keyboard_button_released(
    te_camera_controller* controller, enum te_keyboard_button button);
void camera_controller_on_game_tick(te_camera_controller* controller, float delta_time_sec);
void camera_controller_on_gamepad_connected(te_camera_controller* controller);
void camera_controller_on_gamepad_disconnected(te_camera_controller* controller);
// ------------------------------------------------------------------------------------------------
