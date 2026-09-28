#pragma once

enum te_mouse_button {
    TE_MB_LEFT,
    TE_MB_RIGHT,
    TE_MB_MIDDLE,
    TE_MB_X1,
    TE_MB_X2,
};

// Converts mouse button enum value to a string.
// Do not free/destroy returned pointer.
static inline const char*
mouse_button_get_name(enum te_mouse_button button) {
    switch (button) {
        case (TE_MB_LEFT): return "mouse left";
        case (TE_MB_RIGHT): return "mouse right";
        case (TE_MB_MIDDLE): return "mouse middle";
        case (TE_MB_X1): return "mouse X1";
        case (TE_MB_X2): return "mouse X2";
    }
}
