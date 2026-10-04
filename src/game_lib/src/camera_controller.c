#include <camera_controller.h>

#include <stdbool.h>
#include <math/vec2.h>
#include <game/camera.h>
#include <math/math_funcs.h>
#include <misc/globals.h>
#include <world.h>

struct te_camera_controller {
    /* NULL if not controlling a camera, otherwise pointer to a spawned camera */
    te_camera* camera;

    /* stores the current state of the input
     * X stores "forward" movement in [-1.0f; 1.0], Y stores "right" movement and Z stores up */
    te_vec3 movement_input;

    /* the current state of the right thumbstick */
    te_vec2 gamepad_look;

    float rotation_sensitivity;
    float speed;

    /* `true` if should react to the input */
    bool is_input_enabled;
};

te_camera_controller*
camera_controller_create(void) {
    te_camera_controller* camera_controller = malloc(sizeof(te_camera_controller));

    camera_controller->camera = NULL;
    camera_controller->is_input_enabled = true;
    camera_controller->rotation_sensitivity = 0.1f;
    camera_controller->speed = 4.0f;
    vec2_zero(camera_controller->gamepad_look);
    vec3_zero(camera_controller->movement_input);

    return camera_controller;
}

void
camera_controller_destroy(te_camera_controller* camera_controller) {
    free(camera_controller);
}

static void
reset_input_state(te_camera_controller* controller) {
    vec3_zero(controller->movement_input);
    vec2_zero(controller->gamepad_look);
}

static void
on_before_camera_destroyed(te_camera* camera) {
    te_camera_controller* controller = camera_get_custom_ptr(camera);
    controller->camera = NULL;
}

void
camera_controller_set_camera(te_camera_controller* controller, te_camera* camera) {
    if (controller->camera != NULL) {
        camera_set_custom_ptr(camera, NULL);
        camera_set_custom_on_before_destroyed(camera, NULL);
    }

    controller->camera = camera;

    if (controller->camera != NULL) {
        camera_set_custom_ptr(camera, controller);
        camera_set_custom_on_before_destroyed(camera, on_before_camera_destroyed);
    }

    reset_input_state(controller);
}

void
camera_controller_enable_input(te_camera_controller* controller, bool enable) {
    controller->is_input_enabled = enable;

    reset_input_state(controller);
}

void
camera_controller_apply_look_input(
    te_camera_controller* controller, float x_offset, float y_offset) {
    te_vec3 rotation;

    if (controller->camera == NULL) {
        return;
    }

    camera_get_rotation(controller->camera, rotation);

    rotation[1] -= x_offset * controller->rotation_sensitivity;
    rotation[0] -= y_offset * controller->rotation_sensitivity;

    camera_set_rotation(controller->camera, rotation);
}

void
camera_controller_on_mouse_moved(
    te_camera_controller* controller, float x_offset, float y_offset) {
    if (controller->camera == NULL) {
        return;
    }
    if (!controller->is_input_enabled) {
        return;
    }

    camera_controller_apply_look_input(controller, x_offset, y_offset);
}

void
camera_controller_on_gamepad_axis_moved(
    te_camera_controller* controller, enum te_gamepad_axis axis, float new_pos) {
    if (controller->camera == NULL) {
        return;
    }

    if (axis == TE_GA_RIGHT_STICK_X) {
        controller->gamepad_look[0] = new_pos;
    } else if (axis == TE_GA_RIGHT_STICK_Y) {
        controller->gamepad_look[1] = new_pos;
    } else if (axis == TE_GA_LEFT_STICK_Y) {
        controller->movement_input[0] = -new_pos;
    } else if (axis == TE_GA_LEFT_STICK_X) {
        controller->movement_input[1] = new_pos;
    }
}

void
camera_controller_on_keyboard_button_pressed(
    te_camera_controller* controller, enum te_keyboard_button button) {
    if (controller->camera == NULL) {
        return;
    }
    if (!controller->is_input_enabled) {
        return;
    }

    if (button == TE_KB_W) {
        controller->movement_input[0] = 1.0f;
    } else if (button == TE_KB_S) {
        controller->movement_input[0] = -1.0f;
    } else if (button == TE_KB_D) {
        controller->movement_input[1] = 1.0f;
    } else if (button == TE_KB_A) {
        controller->movement_input[1] = -1.0f;
    }
}

void
camera_controller_on_keyboard_button_released(
    te_camera_controller* controller, enum te_keyboard_button button) {
    if (controller->camera == NULL) {
        return;
    }
    if (!controller->is_input_enabled) {
        return;
    }

    if (button == TE_KB_W && controller->movement_input[0] > 0.0f) {
        controller->movement_input[0] = 0.0f;
    } else if (button == TE_KB_S && controller->movement_input[0] < 0.0f) {
        controller->movement_input[0] = 0.0f;
    } else if (button == TE_KB_D && controller->movement_input[1] > 0.0f) {
        controller->movement_input[1] = 0.0f;
    } else if (button == TE_KB_A && controller->movement_input[1] < 0.0f) {
        controller->movement_input[1] = 0.0f;
    }
}

void
camera_controller_on_gamepad_connected(te_camera_controller* controller) {
    reset_input_state(controller);
}

void
camera_controller_on_gamepad_disconnected(te_camera_controller* controller) {
    reset_input_state(controller);
}

void
camera_controller_on_game_tick(te_camera_controller* controller, float delta_time_sec) {
    te_vec3 movement;
    te_vec3 forward;
    te_vec3 right;
    te_vec3 up;
    te_vec3 position;

    if (controller->camera == NULL) {
        return;
    }

    if (controller->gamepad_look[0] != 0.0f || controller->gamepad_look[1] != 0.0f) {
        const float mult = 2000.0f * delta_time_sec;
        camera_controller_apply_look_input(
            controller, controller->gamepad_look[0] * mult,
            controller->gamepad_look[1] * mult);
    }

    if (controller->movement_input[0] == 0.0f && controller->movement_input[1] == 0.0f
        && controller->movement_input[2] == 0.0f) {
        return;
    }

    vec3_set(controller->movement_input, movement);
    math_fix_diagonal_movement_speedup(movement);

    vec3_muls(movement, controller->speed * delta_time_sec, movement);

    camera_get_forward(controller->camera, forward);
    camera_get_right(controller->camera, right);
    globals_get_world_up(up);

    vec3_muls(forward, movement[0], forward);
    vec3_muls(right, movement[1], right);
    vec3_muls(up, movement[2], up);

    camera_get_position(controller->camera, position);

    vec3_add(position, forward, position);
    vec3_add(position, right, position);
    vec3_add(position, up, position);

    camera_set_position(controller->camera, position);
}
