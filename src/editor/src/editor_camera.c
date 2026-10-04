#include <editor_camera.h>

#include <stdbool.h>
#include <math/vec2.h>
#include <game/camera.h>
#include <math/math_funcs.h>
#include <misc/globals.h>
#include <ui/editor_theme.h>
#include <io/log.h>
#include <world.h>

#define DEFAULT_CAMERA_SPEED 4.0f

struct te_editor_camera {
    /* always valid, camera being piloted
     * do not free this camera */
    te_camera* controlled_camera_ref;

    /* always valid, may be equal to @ref camera but is different when piloting some game camera.
     * see @ref is_piloting_custom_camera, must be freed/destroyed */
    te_camera* viewport_camera;

    /* stores the current state of the input
     * X stores "forward" movement in [-1.0f; 1.0], Y stores "right" movement and Z stores up */
    te_vec3 movement_input;

    /* current state of the right thumbstick */
    te_vec2 gamepad_look;

    float rotation_sensitivity;
    float speed;

    /* `true` if should react to the input */
    bool is_input_enabled;
    bool is_fullscreen;
    bool is_piloting_custom_camera;
};

te_editor_camera*
editor_camera_create(void) {
    te_editor_camera* editor_camera = malloc(sizeof(te_editor_camera));

    editor_camera->viewport_camera = camera_create();
    editor_camera->controlled_camera_ref = editor_camera->viewport_camera;
    editor_camera->is_input_enabled = false;
    editor_camera->is_piloting_custom_camera = false;
    editor_camera->rotation_sensitivity = 0.1f;
    editor_camera->speed = DEFAULT_CAMERA_SPEED;
    editor_camera->is_fullscreen = false;
    vec2_zero(editor_camera->gamepad_look);
    vec3_zero(editor_camera->movement_input);

    camera_set_is_serialization_allowed(editor_camera->viewport_camera, false);

    return editor_camera;
}

void
editor_camera_destroy(te_editor_camera* editor_camera) {
    camera_destroy(editor_camera->viewport_camera);

    free(editor_camera);
}

void
editor_camera_spawn(te_editor_camera* editor_camera, struct te_world* world) {
    te_vec3 tmp3;

    /* set initial position/rotation */
    vec3_set(0.0f, 2.0f, 4.0f, tmp3);
    camera_set_position(editor_camera->viewport_camera, tmp3);
    vec3_set(0.0f, 0.0f, 0.0f, tmp3);
    camera_set_rotation(editor_camera->viewport_camera, tmp3);

    /* spawn */
    world_spawn_game_object(
        world, editor_camera->viewport_camera, camera_get_game_object_info());
    world_set_active_camera(world, editor_camera->viewport_camera);

    /* set viewport */
    editor_camera_set_is_fullscreen(editor_camera, false);
}

void
editor_camera_despawn(te_editor_camera* editor_camera, struct te_world* world) {
    world_despawn_game_object(
        world, editor_camera->viewport_camera, camera_get_game_object_info());
}

void
editor_camera_set_is_fullscreen(te_editor_camera* editor_camera, bool is_fullscreen) {
    te_vec4 viewport;

    if (is_fullscreen) {
        vec4_set(.0f, 0.0f, 1.0f, 1.0f, viewport);
    } else {
        vec4_set(
            editor_theme_get_left_panel_width(), 0.0f,
            1.0f
                - (editor_theme_get_left_panel_width() + editor_theme_get_right_panel_width()),
            1.0f, viewport);
    }

    camera_set_viewport(editor_camera->controlled_camera_ref, viewport);
    editor_camera->is_fullscreen = is_fullscreen;
}

bool
editor_camera_is_fullscreen(te_editor_camera* editor_camera) {
    return editor_camera->is_fullscreen;
}

void
editor_camera_enable_input(te_editor_camera* editor_camera, bool enable) {
    editor_camera->is_input_enabled = enable;

    vec3_zero(editor_camera->movement_input);
    vec2_zero(editor_camera->gamepad_look);
}

void
editor_camera_pilot_custom_camera(te_editor_camera* editor_camera, te_camera* camera) {
    te_world* world;

    editor_camera->controlled_camera_ref =
        camera == NULL ? editor_camera->viewport_camera : camera;
    editor_camera->is_piloting_custom_camera =
        editor_camera->controlled_camera_ref != editor_camera->viewport_camera;

    world = camera_get_world(editor_camera->controlled_camera_ref);
    if (world == NULL) {
        log_error(__FILE__, __LINE__, "expected the camera to be spawned");
        abort();
    }
    world_set_active_camera(world, editor_camera->controlled_camera_ref);
}

te_camera*
editor_camera_get_camera(te_editor_camera* editor_camera) {
    return editor_camera->viewport_camera;
}

bool
editor_camera_is_piloting_custom_camera(te_editor_camera* editor_camera) {
    return editor_camera->is_piloting_custom_camera;
}

void
editor_camera_apply_look_input(
    te_editor_camera* editor_camera, float x_offset, float y_offset) {
    te_vec3 rotation;
    camera_get_rotation(editor_camera->controlled_camera_ref, rotation);

    rotation[1] -= x_offset * editor_camera->rotation_sensitivity;
    rotation[0] -= y_offset * editor_camera->rotation_sensitivity;

    camera_set_rotation(editor_camera->controlled_camera_ref, rotation);
}

void
editor_camera_on_mouse_moved(te_editor_camera* editor_camera, float x_offset, float y_offset) {
    if (!editor_camera->is_input_enabled) {
        return;
    }

    editor_camera_apply_look_input(editor_camera, x_offset, y_offset);
}

void
editor_camera_on_gamepad_axis_moved(
    te_editor_camera* editor_camera, enum te_gamepad_axis axis, float new_pos) {
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
editor_camera_on_keyboard_button_pressed(
    te_editor_camera* editor_camera, enum te_keyboard_button button) {
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
editor_camera_on_keyboard_button_released(
    te_editor_camera* editor_camera, enum te_keyboard_button button) {
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
    vec3_zero(editor_camera->movement_input);
    vec2_zero(editor_camera->gamepad_look);
}

void
editor_camera_on_gamepad_disconnected(te_editor_camera* editor_camera) {
    vec3_zero(editor_camera->movement_input);
    vec2_zero(editor_camera->gamepad_look);
}

void
editor_camera_on_game_tick(te_editor_camera* editor_camera, float delta_time_sec) {
    if (editor_camera->gamepad_look[0] != 0.0f || editor_camera->gamepad_look[1] != 0.0f) {
        const float mult = 2000.0f * delta_time_sec;
        editor_camera_apply_look_input(
            editor_camera, editor_camera->gamepad_look[0] * mult,
            editor_camera->gamepad_look[1] * mult);
    }

    if (editor_camera->movement_input[0] == 0.0f && editor_camera->movement_input[1] == 0.0f
        && editor_camera->movement_input[2] == 0.0f) {
        return;
    } else {
        te_vec3 movement;
        te_vec3 forward;
        te_vec3 right;
        te_vec3 up;
        te_vec3 position;

        vec3_copy(editor_camera->movement_input, movement);
        math_fix_diagonal_movement_speedup(movement);

        vec3_muls(movement, editor_camera->speed * delta_time_sec, movement);

        camera_get_forward(editor_camera->controlled_camera_ref, forward);
        camera_get_right(editor_camera->controlled_camera_ref, right);
        globals_get_world_up(up);

        vec3_muls(forward, movement[0], forward);
        vec3_muls(right, movement[1], right);
        vec3_muls(up, movement[2], up);

        camera_get_position(editor_camera->controlled_camera_ref, position);

        vec3_add(position, forward, position);
        vec3_add(position, right, position);
        vec3_add(position, up, position);

        camera_set_position(editor_camera->controlled_camera_ref, position);
    }
}
