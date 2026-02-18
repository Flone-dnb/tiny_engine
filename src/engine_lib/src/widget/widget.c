#include "widget/widget.h"

#include <stdbool.h>
#include <string.h>
#include "misc/error.h"

struct te_widget {
    // Actual widget that owns this component.
    void* owner;

    // May be NULL. Do not free/destroy this pointer.
    te_widget* parent;

    // Non-NULL if spawned. Do not free/destroy this pointer.
    struct te_world* world;

    // Size of this array is @ref child_widget_count.
    te_widget** child_widgets;

    void (*on_pos_changed)(void* owner);
    void (*on_size_changed)(void* owner);
    void (*on_parent_changed)(void* owner);
    void (*on_before_base_destroyed)(void* owner);
    void (*on_after_spawned)(void* owner);
    void (*on_before_despawned)(void* owner);
    void (*on_window_size_changed)(void* owner);

    // If @ref parent is NULL equal to screen pos/size, otherwise
    // stores pos/size relative to the parent.
    vec2 relative_pos;
    vec2 relative_size;

    // Stores the final screen pos/size (includes parents if have parents)
    // in range [0.0; 1.0] relative to the window size.
    vec2 screen_pos;
    vec2 screen_size;

    unsigned int child_widget_count;
};

te_widget*
widget_create(
    void* owner, void (*on_pos_changed)(void* owner), void (*on_size_changed)(void* owner),
    void (*on_before_base_destroyed)(void* owner), void (*on_after_spawned)(void* owner),
    void (*on_before_despawned)(void* owner), void (*on_window_size_changed)(void* owner)) {
    te_widget* widget = malloc(sizeof(te_widget));

    widget->owner = owner;
    widget->parent = NULL;
    widget->world = NULL;
    widget->child_widgets = NULL;
    widget->on_pos_changed = on_pos_changed;
    widget->on_size_changed = on_size_changed;
    widget->on_before_base_destroyed = on_before_base_destroyed;
    widget->on_after_spawned = on_after_spawned;
    widget->on_before_despawned = on_before_despawned;
    widget->on_window_size_changed = on_window_size_changed;
    widget->child_widget_count = 0;

    glm_vec2_zero(widget->relative_pos);
    glm_vec2_copy((vec2){0.1f, 0.05f}, widget->relative_size);

    glm_vec2_copy(widget->relative_pos, widget->screen_pos);
    glm_vec2_copy(widget->relative_size, widget->screen_size);

    return widget;
}

void
widget_destroy(te_widget* widget) {
    if (widget->world != NULL){
        show_error_and_abort("can't destroy a spawned widget, despawn it first");
    }

    widget->on_before_base_destroyed(widget->owner);

    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        widget_destroy(widget->child_widgets[i]);
    }
    free(widget->child_widgets);
    widget->child_widget_count = 0;

    free(widget);
}

static void
prv_widget_calc_screen_pos_size_recursive(te_widget* base, vec2 pos, vec2 size) {
    pos[0] = base->relative_pos[0] + pos[0] * base->relative_size[0];
    pos[1] = base->relative_pos[1] + pos[1] * base->relative_size[1];

    size[0] *= base->relative_size[0];
    size[1] *= base->relative_size[1];

    if (base->parent != NULL) {
        prv_widget_calc_screen_pos_size_recursive(base->parent, pos, size);
    }
}

static void
prv_widget_recalc_screen_pos_size(te_widget* widget) {
    glm_vec2_copy(widget->relative_pos, widget->screen_pos);
    glm_vec2_copy(widget->relative_size, widget->screen_size);

    if (widget->parent != NULL) {
        prv_widget_calc_screen_pos_size_recursive(widget->parent, widget->screen_pos, widget->screen_size);
    }
}

void
widget_set_parent(te_widget* widget, te_widget* new_parent) {
    if (widget == new_parent) {
        show_error_and_abort("can't attach a widget to itself");
    }
    if (widget->world != new_parent->world) {
        // This is because world need to register/unregister widgets from its internal arrays.
        // This can be reworked later if needed.
        show_error_and_abort("widgets can only be attached to widgets from the same world, "
            "if parent widget is spawned then spawn the child widget first and then attach");
    }
    if (new_parent == widget->parent) {
        return;
    }

    if (widget->parent != NULL) {
        // Remove self from old parent's array of child widgets.
        unsigned int index = 0;
        bool found = false;
        for (unsigned int i = 0; i < widget->parent->child_widget_count; i++) {
            if (widget->parent->child_widgets[i] != widget) {
                continue;
            }

            index = i;
            found = true;
            break;
        }
        if (!found) {
            show_error_and_abort("unable to find widget in parent's array of child widgets");
        }

        if (widget->parent->child_widget_count == 1) {
            widget->parent->child_widget_count = 0;
            free(widget->parent->child_widgets);
            widget->parent->child_widgets = NULL;
        } else {
            te_widget** new_children = malloc(sizeof(te_widget*) * (widget->parent->child_widget_count - 1));
            memcpy(new_children, widget->parent->child_widgets, sizeof(te_widget*) * index);
            memcpy(
                new_children + index, widget->parent->child_widgets + (index + 1),
                sizeof(te_widget) * (widget->parent->child_widget_count - index - 1));

            free(widget->parent->child_widgets);
            widget->parent->child_widgets = new_children;
        }
    }

    // Add self to new parent's array of child widgets.
    {
        te_widget** new_children = malloc(sizeof(te_widget*) * (new_parent->child_widget_count + 1));
        memcpy(new_children, new_parent->child_widgets, sizeof(te_widget*) * new_parent->child_widget_count);

        free(new_parent->child_widgets);
        new_parent->child_widgets = new_children;

        new_parent->child_widgets[new_parent->child_widget_count] = widget;
        new_parent->child_widget_count += 1;
    }

    widget->parent = new_parent;
    prv_widget_recalc_screen_pos_size(widget);

    widget->on_pos_changed(widget->owner);
    widget->on_size_changed(widget->owner);
}

static void
prv_widget_on_parent_pos_changed(te_widget* widget) {
    prv_widget_recalc_screen_pos_size(widget);

    // Notify child widgets.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_parent_pos_changed(widget->child_widgets[i]);
    }

    widget->on_pos_changed(widget->owner);
}

static void
prv_widget_on_parent_size_changed(te_widget* widget) {
    prv_widget_recalc_screen_pos_size(widget);

    // Notify child widgets.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_parent_size_changed(widget->child_widgets[i]);
    }

    widget->on_size_changed(widget->owner);
}

void
widget_set_relative_position(te_widget* widget, vec2 position) {
    glm_vec2_copy(position, widget->relative_pos); // don't check for [0.0; 1.0]
    prv_widget_recalc_screen_pos_size(widget);

    // Notify child widgets.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_parent_pos_changed(widget->child_widgets[i]);
    }

    widget->on_pos_changed(widget->owner);
}

void
widget_get_relative_position(te_widget* widget, vec2 out) {
    glm_vec2_copy(widget->relative_pos, out);
}

void
widget_set_relative_size(te_widget* widget, vec2 size) {
    glm_vec2_copy(size, widget->relative_size);
    prv_widget_recalc_screen_pos_size(widget);

    // Notify child widgets.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_parent_size_changed(widget->child_widgets[i]);
    }

    widget->on_size_changed(widget->owner);
}

void
widget_get_relative_size(te_widget* widget, vec2 out) {
    glm_vec2_copy(widget->relative_size, out);
}

void
widget_get_screen_position(te_widget* widget, vec2 pos) {
    glm_vec2_copy(widget->screen_pos, pos);
}

void
widget_get_screen_size(te_widget* widget, vec2 size) {
    glm_vec2_copy(widget->screen_size, size);
}

struct te_world*
widget_get_world(te_widget* widget) {
    return widget->world;
}

void
prv_widget_on_spawned(te_widget* widget, struct te_world* world) {
    // Spawn from top to bottom (in the hierarchy) so that widgets will be placed in the renderer
    // in the order from top to bottom (in the hierarchy) for top widgets to be rendered first and bottom widgets last.
    widget->world = world;
    widget->on_after_spawned(widget->owner);

    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_spawned(widget->child_widgets[i], world);
    }
}

void
prv_widget_on_despawned(te_widget* widget) {
    widget->on_before_despawned(widget->owner);
    widget->world = NULL;

    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_despawned(widget->child_widgets[i]);
    }
}

void
prv_widget_on_window_size_changed(te_widget* widget) {
    if (widget->world == NULL) {
        // No need to notify widgets because they will query window size when spawned.
        return;
    }

    // Start from deepest child widget.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_window_size_changed(widget->child_widgets[i]);
    }

    widget->on_window_size_changed(widget->owner);
}
