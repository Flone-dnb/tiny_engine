#include "editor_camera.h"

#include <stdbool.h>
#include "game/camera.h"
#include "math_funcs.h"
#include "misc/globals.h"
#include "world.h"

#define DEFAULT_CAMERA_SPEED 4.0f

struct te_editor_camera {
    te_camera* camera;

    // Stores the current state of the input.
    // X stores "forward" movement in [-1.0f; 1.0], Y stores "right" movement and Z stores up.
    vec3 movement_input;

    // The current state of the right thumbstick.
    vec2 gamepad_look;

    float rotation_sensitivity;
    float speed;

    // `true` if should react to the input.
    bool is_input_enabled;
};

te_editor_camera*
editor_camera_create() {
    te_editor_camera* editor_camera = malloc(sizeof(te_editor_camera));

    editor_camera->camera = camera_create();
    editor_camera->is_input_enabled = false;
    editor_camera->rotation_sensitivity = 0.1f;
    editor_camera->speed = DEFAULT_CAMERA_SPEED;
    glm_vec2_zero(editor_camera->gamepad_look);
    glm_vec3_zero(editor_camera->movement_input);

    return editor_camera;
}

void
editor_camera_destroy(te_editor_camera* editor_camera) {
    // if you crashed here then you forgot to despawn the camera before world destruction
    camera_destroy(editor_camera->camera);

    free(editor_camera);
}

void
editor_camera_spawn(te_editor_camera* editor_camera, struct te_world* world) {
    // Set initial location/rotation.
    camera_set_location(editor_camera->camera, (vec3){0.0f, 2.0f, 4.0f});
    camera_set_rotation(editor_camera->camera, (vec3){0.0f, 0.0f, 0.0f});

    world_spawn_camera(world, editor_camera->camera);
    world_set_active_camera(world, editor_camera->camera);
}

void
editor_camera_despawn(te_editor_camera* editor_camera, struct te_world* world) {
    world_despawn_camera(world, editor_camera->camera);
}

void
editor_camera_enable_input(te_editor_camera* editor_camera, bool enable) {
    editor_camera->is_input_enabled = enable;

    glm_vec3_zero(editor_camera->movement_input);
    glm_vec2_zero(editor_camera->gamepad_look);
}

void
editor_camera_apply_look_input(te_editor_camera* editor_camera, float x_offset, float y_offset) {
    vec3 rotation;
    camera_get_rotation(editor_camera->camera, rotation);

    rotation[1] -= x_offset * editor_camera->rotation_sensitivity;
    rotation[0] -= y_offset * editor_camera->rotation_sensitivity;

    camera_set_rotation(editor_camera->camera, rotation);
}

void
editor_camera_on_mouse_moved(te_editor_camera* editor_camera, float x_offset, float y_offset) {
    if (!editor_camera->is_input_enabled) {
        return;
    }

    editor_camera_apply_look_input(editor_camera, x_offset, y_offset);
}

void
editor_camera_on_gamepad_axis_moved(te_editor_camera* editor_camera, enum te_gamepad_axis axis,
                                    float new_pos) {
    if (axis == TE_GA_RIGHT_STICK_X) {
        editor_camera->gamepad_look[0] = new_pos;
    } else if (axis == TE_GA_RIGHT_STICK_Y) {
        editor_camera->gamepad_look[1] = new_pos;
    } else if (axis == TE_GA_LEFT_STICK_Y) {
        editor_camera->movement_input[0] = -new_pos;
    } else if (axis == TE_GA_LEFT_STICK_X) {
        editor_camera->movement_input[1] = new_pos;
    } else if (axis == TE_GA_LEFT_TRIGGER) {
        editor_camera->movement_input[2] = -new_pos;
    } else if (axis == TE_GA_RIGHT_TRIGGER) {
        editor_camera->movement_input[2] = new_pos;
    }
}

void
editor_camera_on_keyboard_button_pressed(te_editor_camera* editor_camera, enum te_keyboard_button button) {
    if (!editor_camera->is_input_enabled) {
        return;
    }

    if (button == TE_KB_W) {
        editor_camera->movement_input[0] = 1.0f;
    } else if (button == TE_KB_S) {
        editor_camera->movement_input[0] = -1.0f;
    } else if (button == TE_KB_D) {
        editor_camera->movement_input[1] = 1.0f;
    } else if (button == TE_KB_A) {
        editor_camera->movement_input[1] = -1.0f;
    } else if (button == TE_KB_E) {
        editor_camera->movement_input[2] = 1.0f;
    } else if (button == TE_KB_Q) {
        editor_camera->movement_input[2] = -1.0f;
    }

    if (button == TE_KB_LEFT_CONTROL) {
        editor_camera->speed = DEFAULT_CAMERA_SPEED / 3.0f;
    } else if (button == TE_KB_LEFT_SHIFT) {
        editor_camera->speed = DEFAULT_CAMERA_SPEED * 3.0f;
    }
}

void
editor_camera_on_keyboard_button_released(te_editor_camera* editor_camera, enum te_keyboard_button button) {
    if (!editor_camera->is_input_enabled) {
        return;
    }

    if (button == TE_KB_W && editor_camera->movement_input[0] > 0.0f) {
        editor_camera->movement_input[0] = 0.0f;
    } else if (button == TE_KB_S && editor_camera->movement_input[0] < 0.0f) {
        editor_camera->movement_input[0] = 0.0f;
    } else if (button == TE_KB_D && editor_camera->movement_input[1] > 0.0f) {
        editor_camera->movement_input[1] = 0.0f;
    } else if (button == TE_KB_A && editor_camera->movement_input[1] < 0.0f) {
        editor_camera->movement_input[1] = 0.0f;
    } else if (button == TE_KB_E && editor_camera->movement_input[2] > 0.0f) {
        editor_camera->movement_input[2] = 0.0f;
    } else if (button == TE_KB_Q && editor_camera->movement_input[2] < 0.0f) {
        editor_camera->movement_input[2] = 0.0f;
    }

    if (button == TE_KB_LEFT_CONTROL && editor_camera->speed < DEFAULT_CAMERA_SPEED) {
        editor_camera->speed = DEFAULT_CAMERA_SPEED;
    } else if (button == TE_KB_LEFT_SHIFT && editor_camera->speed > DEFAULT_CAMERA_SPEED) {
        editor_camera->speed = DEFAULT_CAMERA_SPEED;
    }
}

void
editor_camera_on_gamepad_connected(te_editor_camera* editor_camera) {
    glm_vec3_zero(editor_camera->movement_input);
    glm_vec2_zero(editor_camera->gamepad_look);
}

void
editor_camera_on_gamepad_disconnected(te_editor_camera* editor_camera) {
    glm_vec3_zero(editor_camera->movement_input);
    glm_vec2_zero(editor_camera->gamepad_look);
}

void
editor_camera_on_game_tick(te_editor_camera* editor_camera, float delta_time_sec) {
    if (editor_camera->gamepad_look[0] != 0.0f || editor_camera->gamepad_look[1] != 0.0f) {
        const float mult = 0.1f;
        editor_camera_apply_look_input(editor_camera, editor_camera->gamepad_look[0] * mult,
                                       editor_camera->gamepad_look[1] * mult);
    }

    if (editor_camera->movement_input[0] == 0.0f && editor_camera->movement_input[1] == 0.0f
        && editor_camera->movement_input[2] == 0.0f) {
        return;
    }

    vec3 movement;
    glm_vec3_make(editor_camera->movement_input, movement);
    math_fix_diagonal_movement_speedup(movement);

    glm_vec3_scale(movement, editor_camera->speed * delta_time_sec, movement);

    vec3 forward;
    vec3 right;
    vec3 up;
    camera_get_forward(editor_camera->camera, forward);
    camera_get_right(editor_camera->camera, right);
    globals_get_world_up(up);

    glm_vec3_scale(forward, movement[0], forward);
    glm_vec3_scale(right, movement[1], right);
    glm_vec3_scale(up, movement[2], up);

    vec3 location;
    camera_get_location(editor_camera->camera, location);

    glm_vec3_add(location, forward, location);
    glm_vec3_add(location, right, location);
    glm_vec3_add(location, up, location);

    camera_set_location(editor_camera->camera, location);
}
