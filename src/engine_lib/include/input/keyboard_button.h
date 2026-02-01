#pragma once

#include <stdbool.h>
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_keycode.h"

typedef struct te_keyboard_modifiers {
    // SDL keyboard modifiers value.
    uint16_t mod;
} te_keyboard_modifiers;

static inline bool
keyboard_modifiers_is_shift_pressed(te_keyboard_modifiers* mods) {
    return mods->mod & SDL_KMOD_LSHIFT;
}

static inline bool
keyboard_modifiers_is_ctrl_pressed(te_keyboard_modifiers* mods) {
    return mods->mod & SDL_KMOD_LCTRL;
}

static inline bool
keyboard_modifiers_is_alt_pressed(te_keyboard_modifiers* mods) {
    return mods->mod & SDL_KMOD_LALT;
}

static inline bool
keyboard_modifiers_is_caps_lock_pressed(te_keyboard_modifiers* mods) {
    return mods->mod & SDL_KMOD_CAPS;
}

enum te_keyboard_button {
    TE_KB_SPACE = SDL_SCANCODE_SPACE,
    TE_KB_COMMA = SDL_SCANCODE_COMMA,
    TE_KB_MINUS = SDL_SCANCODE_MINUS,
    TE_KB_PERIOD = SDL_SCANCODE_PERIOD,
    TE_KB_SLASH = SDL_SCANCODE_SLASH,
    TE_KB_TILDE = SDL_SCANCODE_GRAVE,
    TE_KB_0 = SDL_SCANCODE_0,
    TE_KB_1 = SDL_SCANCODE_1,
    TE_KB_2 = SDL_SCANCODE_2,
    TE_KB_3 = SDL_SCANCODE_3,
    TE_KB_4 = SDL_SCANCODE_4,
    TE_KB_5 = SDL_SCANCODE_5,
    TE_KB_6 = SDL_SCANCODE_6,
    TE_KB_7 = SDL_SCANCODE_7,
    TE_KB_8 = SDL_SCANCODE_8,
    TE_KB_9 = SDL_SCANCODE_9,
    TE_KB_SEMICOLON = SDL_SCANCODE_SEMICOLON,
    TE_KB_EQUALS = SDL_SCANCODE_EQUALS,
    TE_KB_A = SDL_SCANCODE_A,
    TE_KB_B = SDL_SCANCODE_B,
    TE_KB_C = SDL_SCANCODE_C,
    TE_KB_D = SDL_SCANCODE_D,
    TE_KB_E = SDL_SCANCODE_E,
    TE_KB_F = SDL_SCANCODE_F,
    TE_KB_G = SDL_SCANCODE_G,
    TE_KB_H = SDL_SCANCODE_H,
    TE_KB_I = SDL_SCANCODE_I,
    TE_KB_J = SDL_SCANCODE_J,
    TE_KB_K = SDL_SCANCODE_K,
    TE_KB_L = SDL_SCANCODE_L,
    TE_KB_M = SDL_SCANCODE_M,
    TE_KB_N = SDL_SCANCODE_N,
    TE_KB_O = SDL_SCANCODE_O,
    TE_KB_P = SDL_SCANCODE_P,
    TE_KB_Q = SDL_SCANCODE_Q,
    TE_KB_R = SDL_SCANCODE_R,
    TE_KB_S = SDL_SCANCODE_S,
    TE_KB_T = SDL_SCANCODE_T,
    TE_KB_U = SDL_SCANCODE_U,
    TE_KB_V = SDL_SCANCODE_V,
    TE_KB_W = SDL_SCANCODE_W,
    TE_KB_X = SDL_SCANCODE_X,
    TE_KB_Y = SDL_SCANCODE_Y,
    TE_KB_Z = SDL_SCANCODE_Z,
    TE_KB_BACKSLASH = SDL_SCANCODE_BACKSLASH,
    TE_KB_ESCAPE = SDL_SCANCODE_ESCAPE,
    TE_KB_ENTER = SDL_SCANCODE_RETURN,
    TE_KB_TAB = SDL_SCANCODE_TAB,
    TE_KB_BACKSPACE = SDL_SCANCODE_BACKSPACE,
    TE_KB_INSERT = SDL_SCANCODE_INSERT,
    TE_KB_DELETE = SDL_SCANCODE_DELETE,
    TE_KB_RIGHT = SDL_SCANCODE_RIGHT,
    TE_KB_LEFT = SDL_SCANCODE_LEFT,
    TE_KB_DOWN = SDL_SCANCODE_DOWN,
    TE_KB_UP = SDL_SCANCODE_UP,
    TE_KB_HOME = SDL_SCANCODE_HOME,
    TE_KB_END = SDL_SCANCODE_END,
    TE_KB_CAPS_LOCK = SDL_SCANCODE_CAPSLOCK,
    TE_KB_PRINT_SCREEN = SDL_SCANCODE_PRINTSCREEN,
    TE_KB_PAUSE = SDL_SCANCODE_PAUSE,
    TE_KB_F1 = SDL_SCANCODE_F1,
    TE_KB_F2 = SDL_SCANCODE_F2,
    TE_KB_F3 = SDL_SCANCODE_F3,
    TE_KB_F4 = SDL_SCANCODE_F4,
    TE_KB_F5 = SDL_SCANCODE_F5,
    TE_KB_F6 = SDL_SCANCODE_F6,
    TE_KB_F7 = SDL_SCANCODE_F7,
    TE_KB_F8 = SDL_SCANCODE_F8,
    TE_KB_F9 = SDL_SCANCODE_F9,
    TE_KB_F10 = SDL_SCANCODE_F10,
    TE_KB_F11 = SDL_SCANCODE_F11,
    TE_KB_F12 = SDL_SCANCODE_F12,
    TE_KB_LEFT_SHIFT = SDL_SCANCODE_LSHIFT,
    TE_KB_LEFT_CONTROL = SDL_SCANCODE_LCTRL,
    TE_KB_LEFT_ALT = SDL_SCANCODE_LALT,
    TE_KB_RIGHT_SHIFT = SDL_SCANCODE_RSHIFT,
    TE_KB_RIGHT_CONTROL = SDL_SCANCODE_RCTRL,
    TE_KB_RIGHT_ALT = SDL_SCANCODE_RALT,
};

// Converts keyboard button enum value to a string.
// Do not free/destroy returned pointer.
static inline const char*
keyboard_button_get_name(enum te_keyboard_button button) {
    return SDL_GetKeyName(SDL_SCANCODE_TO_KEYCODE((SDL_Scancode)button));
}
