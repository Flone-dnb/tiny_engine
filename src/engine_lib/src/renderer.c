#include "renderer.h"

#include <stdlib.h>

te_renderer*
renderer_create(struct te_window* window) {
    te_renderer* renderer = malloc(sizeof(te_renderer));
    renderer->window = window;

    return renderer;
}

void
renderer_destroy(te_renderer* renderer) {
    free(renderer);
}

void
prv_renderer_draw_frame(te_renderer* renderer) {
    (void)renderer; // <- unused for now
    // TODO
}

void
prv_renderer_on_window_size_changed(te_renderer* renderer) {
    (void)renderer; // <- unused for now
    // TODO: update font size
}
