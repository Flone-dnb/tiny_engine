#pragma once

#include "SDL3/SDL_gamepad.h"

enum te_gamepad_button {
    TE_GB_BUTTON_LEFT =
        SDL_GAMEPAD_BUTTON_WEST, //< one of the 4 buttons on the right side of a gamepad, X button
                                 //< on xbox, square on sony gamepad and so on
    TE_GB_BUTTON_UP = SDL_GAMEPAD_BUTTON_NORTH,
    TE_GB_BUTTON_RIGHT = SDL_GAMEPAD_BUTTON_EAST,
    TE_GB_BUTTON_DOWN = SDL_GAMEPAD_BUTTON_SOUTH,
    TE_GB_START = SDL_GAMEPAD_BUTTON_START,
    TE_GB_BACK = SDL_GAMEPAD_BUTTON_BACK,
    TE_GB_DPAD_LEFT = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    TE_GB_DPAD_UP = SDL_GAMEPAD_BUTTON_DPAD_UP,
    TE_GB_DPAD_RIGHT = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    TE_GB_DPAD_DOWN = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    TE_GB_LEFT_STICK = SDL_GAMEPAD_BUTTON_LEFT_STICK,
    TE_GB_RIGHT_STICK = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    TE_GB_LEFT_SHOULDER = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    TE_GB_RIGHT_SHOULDER = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
};

enum te_gamepad_axis {
    TE_GA_RIGHT_TRIGGER = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    TE_GA_LEFT_TRIGGER = SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    TE_GA_RIGHT_STICK_X = SDL_GAMEPAD_AXIS_RIGHTX,
    TE_GA_RIGHT_STICK_Y = SDL_GAMEPAD_AXIS_RIGHTY,
    TE_GA_LEFT_STICK_X = SDL_GAMEPAD_AXIS_LEFTX,
    TE_GA_LEFT_STICK_Y = SDL_GAMEPAD_AXIS_LEFTY,
};

// Converts gamepad button enum value to a string.
// Do not free/destroy returned pointer.
static inline const char*
gamepad_button_get_name(enum te_gamepad_button button) {
    return SDL_GetGamepadStringForButton((SDL_GamepadButton)button);
}

// Converts gamepad axis enum value to a string.
// Do not free/destroy returned pointer.
static inline const char*
gamepad_axis_get_name(enum te_gamepad_axis axis) {
    return SDL_GetGamepadStringForAxis((SDL_GamepadAxis)axis);
}
