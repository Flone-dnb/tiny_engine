#include "widget/widget.h"

#include <stdbool.h>
#include <string.h>
#include "misc/error.h"

struct te_widget {
    // Actual widget that owns this component.
    void* owner;

    // May be NULL. Do not free/destroy this pointer.
    te_widget* parent;

    // Non-NULL if set to a currently active camera. Do not free/destroy this pointer.
    struct te_camera* active_camera;

    // Size of this array is @ref child_widget_count.
    te_widget** child_widgets;

    void (*on_pos_changed)(void* owner);
    void (*on_size_changed)(void* owner);
    void (*on_parent_changed)(void* owner);
    void (*on_before_base_destroyed)(void* owner);
    void (*on_after_activated)(void* owner);
    void (*on_before_deactivated)(void* owner);
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
    void (*on_parent_changed)(void* owner), void (*on_before_base_destroyed)(void* owner),
    void (*on_after_activated)(void* owner), void (*on_before_deactivated)(void* owner),
    void (*on_window_size_changed)(void* owner)) {
    te_widget* widget = malloc(sizeof(te_widget));

    widget->owner = owner;
    widget->parent = NULL;
    widget->child_widgets = NULL;
    widget->on_pos_changed = on_pos_changed;
    widget->on_size_changed = on_size_changed;
    widget->on_parent_changed = on_parent_changed;
    widget->on_before_base_destroyed = on_before_base_destroyed;
    widget->on_after_activated = on_after_activated;
    widget->on_before_deactivated = on_before_deactivated;
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
widget_set_parent(te_widget* widget, te_widget* parent) {
    if (widget == parent) {
        show_error_and_abort("can't attach self to self");
    }

    if (parent == widget->parent) {
        return;
    }

    if (parent == NULL) {
        // Remove self from parent's array of child widgets.
        unsigned int index = 0;
        bool found = false;
        for (unsigned int i = 0; i < parent->child_widget_count; i++) {
            if (parent->child_widgets[i] != widget) {
                continue;
            }

            index = i;
            found = true;
            break;
        }
        if (!found) {
            show_error_and_abort("unable to find widget in parent's array of child widgets");
        }

        if (parent->child_widget_count == 1) {
            parent->child_widget_count = 0;
            free(parent->child_widgets);
            parent->child_widgets = NULL;
        } else {
            te_widget** new_children = malloc(sizeof(te_widget*) * (parent->child_widget_count - 1));
            memcpy(new_children, parent->child_widgets, sizeof(te_widget*) * index);
            memcpy(
                new_children + index, parent->child_widgets + (index + 1),
                sizeof(te_widget) * (parent->child_widget_count - index - 1));

            free(parent->child_widgets);
            parent->child_widgets = new_children;
        }
    } else {
        // Add self to parent's array of child widgets.
        te_widget** new_children = malloc(sizeof(te_widget*) * (parent->child_widget_count + 1));
        memcpy(new_children, parent->child_widgets, sizeof(te_widget*) * parent->child_widget_count);

        free(parent->child_widgets);
        parent->child_widgets = new_children;

        parent->child_widgets[parent->child_widget_count] = widget;
        parent->child_widget_count += 1;
    }

    widget->parent = parent;
    prv_widget_recalc_screen_pos_size(widget);
    widget->on_parent_changed(widget->owner);

    if (widget->active_camera == NULL && widget->parent->active_camera != NULL) {
        prv_widget_on_camera_activated(widget, widget->parent->active_camera);
    } else if (widget->active_camera != NULL && (widget->parent->active_camera != widget->active_camera)) {
        prv_widget_on_camera_deactivated(widget);
        if (widget->parent->active_camera != NULL) {
            prv_widget_on_camera_activated(widget, widget->parent->active_camera);
        }
    }
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

struct te_camera*
widget_get_active_camera(te_widget* widget) {
    return widget->active_camera;
}

void
prv_widget_on_camera_activated(te_widget* widget, struct te_camera* active_camera) {
    widget->active_camera = active_camera;
    widget->on_after_activated(widget->owner);

    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_camera_activated(widget->child_widgets[i], active_camera);
    }
}

void
prv_widget_on_camera_deactivated(te_widget* widget) {
    widget->on_before_deactivated(widget->owner);
    widget->active_camera = NULL;

    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_camera_deactivated(widget->child_widgets[i]);
    }
}

void
prv_widget_on_window_size_changed(te_widget* widget) {
    if (widget->active_camera == NULL) {
        // No need to notify widgets because they will query window size
        // when there will be an active camera (i.e. when activated).
        return;
    }

    // Start from deepest child widget.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_window_size_changed(widget->child_widgets[i]);
    }

    widget->on_window_size_changed(widget->owner);
}
