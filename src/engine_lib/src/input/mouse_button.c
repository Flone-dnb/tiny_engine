#include <input/mouse_button.h>

#include <stdlib.h>
#include <io/log.h>

const char*
mouse_button_get_name(enum te_mouse_button button) {
    switch (button) {
        case (TE_MB_LEFT): return "mouse left";
        case (TE_MB_RIGHT): return "mouse right";
        case (TE_MB_MIDDLE): return "mouse middle";
        case (TE_MB_X1): return "mouse X1";
        case (TE_MB_X2): return "mouse X2";
    }
    log_error(__FILE__, __LINE__, "unhandled case");
    abort();
}
