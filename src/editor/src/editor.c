#include "editor.h"

#include "window.h"

void
editor_run(void) {
    te_window* window = window_create("tiny engine editor");

    window_destroy(window);
}
