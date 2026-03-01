#include "widget/vbox_widget.h"

#include "cglm/vec2.h"
#include "widget/widget.h"
#include "type_database.h"

struct te_vbox_widget {
    te_widget* widget;

    // Spacing between child widgets in range [0.0; 1.0] relative to window's height.
    float child_spacing;

    bool is_vbox_widget_destroy;
};

// Widget callbacks:
static void prv_vbox_widget_on_size_changed(void* this);
static void prv_vbox_widget_on_children_changed(void* this);
static void prv_vbox_widget_on_window_size_changed(void* this);
static void prv_vbox_widget_on_before_base_destroyed(void* this);
static void prv_vbox_widget_on_after_spawned(void* this);

te_vbox_widget*
vbox_widget_create(void) {
    te_vbox_widget* vbox_widget = malloc(sizeof(te_vbox_widget));

    vbox_widget->widget = widget_create(
        vbox_widget, vbox_widget_get_type_id, NULL, prv_vbox_widget_on_size_changed, prv_vbox_widget_on_before_base_destroyed,
        NULL, prv_vbox_widget_on_children_changed, prv_vbox_widget_on_after_spawned, NULL,
        prv_vbox_widget_on_window_size_changed);

    vbox_widget->child_spacing = 0.01f;
    vbox_widget->is_vbox_widget_destroy = false;

    return vbox_widget;
}

void
vbox_widget_destroy(te_vbox_widget* vbox_widget) {
    vbox_widget->is_vbox_widget_destroy = true;

    if (vbox_widget->widget != NULL) { // may be null if we got here from base destroy
        widget_destroy(vbox_widget->widget);
    }

    free(vbox_widget);
}

static void
prv_vbox_widget_on_before_base_destroyed(void* this) {
    te_vbox_widget* vbox_widget = this;
    if (vbox_widget->is_vbox_widget_destroy) {
        return;
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    vbox_widget->widget = NULL;
    vbox_widget_destroy(vbox_widget);
}

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
te_widget*
vbox_widget_get_widget(te_vbox_widget* vbox_widget) {
    return vbox_widget->widget;
}

float
vbox_widget_get_child_spacing(te_vbox_widget* vbox_widget) {
    return vbox_widget->child_spacing;
}

static void
prv_vbox_widget_update_children(te_vbox_widget* vbox_widget) {
    unsigned int child_count;
    te_widget** child_widgets = widget_get_child_widgets_tmp(vbox_widget->widget, &child_count);
    if (child_count == 0) {
        return;
    }

    // Recalculate child spacing to relative spacing.
    vec2 screen_size;
    widget_get_screen_size(vbox_widget->widget, screen_size);
    const float relative_spacing = vbox_widget->child_spacing / screen_size[1];

    vec2 relative_pos;
    glm_vec2_copy((vec2){0.0f, 0.0f}, relative_pos);
    for (unsigned int i = 0; i < child_count; i++) {
        vec2 child_relative_size;
        widget_get_relative_size(child_widgets[i], child_relative_size);

        widget_set_relative_position(child_widgets[i], relative_pos);
        relative_pos[1] += child_relative_size[1] + relative_spacing;
    }
}

void
vbox_widget_set_child_spacing(te_vbox_widget* vbox_widget, float spacing) {
    vbox_widget->child_spacing = spacing;

    if (widget_get_world(vbox_widget->widget) != NULL) {
        prv_vbox_widget_update_children(vbox_widget);
    }
}

static void
prv_vbox_widget_on_size_changed(void* this) {
    prv_vbox_widget_update_children(this);
}

static void
prv_vbox_widget_on_children_changed(void* this) {
    prv_vbox_widget_update_children(this);
}

static void
prv_vbox_widget_on_window_size_changed(void* this) {
    prv_vbox_widget_update_children(this);
}

static void
prv_vbox_widget_on_after_spawned(void* this) {
    prv_vbox_widget_update_children(this);
}

static inline void
prv_vbox_widget_set_position(te_vbox_widget* vbox_widget, vec2 pos) {
    widget_set_relative_position(vbox_widget->widget, pos);
}

static inline void
prv_vbox_widget_get_position(te_vbox_widget* vbox_widget, vec2 out) {
    widget_get_relative_position(vbox_widget->widget, out);
}

static inline void
prv_vbox_widget_set_size(te_vbox_widget* vbox_widget, vec2 size) {
    widget_set_relative_size(vbox_widget->widget, size);
}

static inline void
prv_vbox_widget_get_size(te_vbox_widget* vbox_widget, vec2 out) {
    widget_get_relative_size(vbox_widget->widget, out);
}

const char*
vbox_widget_get_type_id(void) {
    return "vbox_widget";
}

void
vbox_widget_register_type(void) {
    te_type_info* info = type_info_create(vbox_widget_get_type_id());
    type_info_add_vec2_variable(info, "position", prv_vbox_widget_set_position, prv_vbox_widget_get_position);
    type_info_add_vec2_variable(info, "size", prv_vbox_widget_set_size, prv_vbox_widget_get_size);
    type_info_add_float_variable(info, "child_spacing", vbox_widget_set_child_spacing, vbox_widget_get_child_spacing);

    type_database_register_type(info);
}
