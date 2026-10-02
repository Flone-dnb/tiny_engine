#include <input/keyboard_button.h>

bool
keyboard_modifiers_is_shift_pressed(te_keyboard_modifiers* mods) {
    return mods->bitmask & 1;
}

bool
keyboard_modifiers_is_ctrl_pressed(te_keyboard_modifiers* mods) {
    return mods->bitmask & 2;
}

bool
keyboard_modifiers_is_alt_pressed(te_keyboard_modifiers* mods) {
    return mods->bitmask & 4;
}
