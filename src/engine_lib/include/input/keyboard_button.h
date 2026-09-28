#pragma once

#include <stdbool.h>

typedef struct te_keyboard_modifiers {
    // bit 0 - alt
    // bit 1 - ctrl
    // bit 2 - shift
    unsigned char bitmask;
} te_keyboard_modifiers;

static inline bool
keyboard_modifiers_is_shift_pressed(te_keyboard_modifiers* mods) {
    return mods->bitmask & 0b1;
}

static inline bool
keyboard_modifiers_is_ctrl_pressed(te_keyboard_modifiers* mods) {
    return mods->bitmask & 0b10;
}

static inline bool
keyboard_modifiers_is_alt_pressed(te_keyboard_modifiers* mods) {
    return mods->bitmask & 0b100;
}

#if defined(WIN32)

// windows scancodes
enum te_keyboard_button {
    TE_KB_SPACE = 0x39,
    TE_KB_COMMA = 0x33,
    TE_KB_MINUS = 0x0C,
    TE_KB_PERIOD = 0x34,
    TE_KB_SLASH = 0x35,
    TE_KB_TILDE = 0x29,
    TE_KB_0 = 0x0B,
    TE_KB_1 = 0x02,
    TE_KB_2 = 0x03,
    TE_KB_3 = 0x04,
    TE_KB_4 = 0x05,
    TE_KB_5 = 0x06,
    TE_KB_6 = 0x07,
    TE_KB_7 = 0x08,
    TE_KB_8 = 0x09,
    TE_KB_9 = 0x0A,
    TE_KB_SEMICOLON = 0x27,
    TE_KB_EQUALS = 0x0D,
    TE_KB_A = 0x1E,
    TE_KB_B = 0x30,
    TE_KB_C = 0x2E,
    TE_KB_D = 0x20,
    TE_KB_E = 0x12,
    TE_KB_F = 0x21,
    TE_KB_G = 0x22,
    TE_KB_H = 0x23,
    TE_KB_I = 0x17,
    TE_KB_J = 0x24,
    TE_KB_K = 0x25,
    TE_KB_L = 0x26,
    TE_KB_M = 0x32,
    TE_KB_N = 0x31,
    TE_KB_O = 0x18,
    TE_KB_P = 0x19,
    TE_KB_Q = 0x10,
    TE_KB_R = 0x13,
    TE_KB_S = 0x1F,
    TE_KB_T = 0x14,
    TE_KB_U = 0x16,
    TE_KB_V = 0x2F,
    TE_KB_W = 0x11,
    TE_KB_X = 0x2D,
    TE_KB_Y = 0x15,
    TE_KB_Z = 0x2C,
    TE_KB_BACKSLASH = 0x2B,
    TE_KB_ESCAPE = 0x01,
    TE_KB_ENTER = 0x1C,
    TE_KB_TAB = 0x0F,
    TE_KB_BACKSPACE = 0x0E,
    TE_KB_INSERT = 0xE052,
    TE_KB_DELETE = 0xE053,
    TE_KB_RIGHT = 0xE04D,
    TE_KB_LEFT = 0xE04B,
    TE_KB_DOWN = 0xE050,
    TE_KB_UP = 0xE048,
    TE_KB_HOME = 0xE047,
    TE_KB_END = 0xE04F,
    TE_KB_CAPS_LOCK = 0x3A,
    TE_KB_PRINT_SCREEN = 0xE037,
    TE_KB_F1 = 0x3B,
    TE_KB_F2 = 0x3C,
    TE_KB_F3 = 0x3D,
    TE_KB_F4 = 0x3E,
    TE_KB_F5 = 0x3F,
    TE_KB_F6 = 0x40,
    TE_KB_F7 = 0x41,
    TE_KB_F8 = 0x42,
    TE_KB_F9 = 0x43,
    TE_KB_F10 = 0x44,
    TE_KB_F11 = 0x57,
    TE_KB_F12 = 0x58,
    TE_KB_LEFT_SHIFT = 0x2A,
    TE_KB_LEFT_CONTROL = 0x1D,
    TE_KB_LEFT_ALT = 0x38,
    TE_KB_RIGHT_SHIFT = 0x36,
    TE_KB_RIGHT_CONTROL = 0xE01D,
    TE_KB_RIGHT_ALT = 0xE038,
};

#else

NOT_IMPLEMENTED;

#endif
