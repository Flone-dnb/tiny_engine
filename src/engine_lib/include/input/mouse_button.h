#pragma once

enum te_mouse_button { TE_MB_LEFT, TE_MB_RIGHT, TE_MB_MIDDLE, TE_MB_X1, TE_MB_X2 };

/* converts mouse button enum value to a string
 * do not free/destroy returned pointer. */
const char* mouse_button_get_name(enum te_mouse_button button);
