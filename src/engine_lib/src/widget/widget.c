#include <widget/widget.h>

#include <string.h>
#include <io/log.h>
#include <world.h>

struct te_widget {
    // Actual widget that owns this component.
    void* owner;

    // Not NULL if was set.
    char* name;

    // May be NULL. Do not free/destroy this pointer.
    te_widget* parent;

    // Non-NULL if spawned. Do not free/destroy this pointer.
    te_world* world;

    // Size of this array is @ref child_widget_count.
    te_widget** child_widgets;

    // Always valid.
    const char* (*get_type_id)(void);

    // May be NULL.
    void (*on_pos_changed)(void* owner);
    void (*on_size_changed)(void* owner);
    void (*on_parent_changed)(void* owner);
    void (*on_children_changed)(void* owner);
    void (*on_before_base_destroyed)(void* owner);
    void (*on_after_spawned)(void* owner);
    void (*on_before_despawned)(void* owner);
    void (*on_window_size_changed)(void* owner);

    // May be NULL, used by interactable widgets.
    void (*on_cursor_entered)(void* owner, vec2 cursor_pos);
    void (*on_cursor_left)(void* owner, vec2 cursor_pos);
    void (*on_mouse_button_pressed)(void* owner, enum te_mouse_button button, vec2 cursor_pos);
    void (*on_mouse_button_released)(
        void* owner, enum te_mouse_button button, vec2 cursor_pos);
    void (*on_hovered_cursor_moved)(void* owner, vec2 cursor_pos);
    void (*on_keyboard_input_text)(void* owner, const char* text);
    void (*on_keyboard_input)(void* owner, enum te_keyboard_button button);

    // If @ref parent is NULL equal to screen pos/size, otherwise
    // stores pos/size relative to the parent.
    vec2 relative_pos;
    vec2 relative_size;

    // Stores the final screen pos/size (includes parents if have parents)
    // in range [0.0; 1.0] relative to the window size.
    vec2 screen_pos;
    vec2 screen_size;

    size_t custom_value;
    void* custom_ptr;

    unsigned int child_widget_count;

    // `false` if this widget (and its child widgets) should not be serialized.
    bool allow_serialization;
};

te_widget*
widget_create(
    void* owner, const char* (*get_type_id)(void), void (*on_pos_changed)(void* owner),
    void (*on_size_changed)(void* owner), void (*on_before_base_destroyed)(void* owner),
    void (*on_parent_changed)(void* owner), void (*on_children_changed)(void* owner),
    void (*on_after_spawned)(void* owner), void (*on_before_despawned)(void* owner),
    void (*on_window_size_changed)(void* owner)) {
    te_widget* widget = malloc(sizeof(te_widget));

    if (owner == NULL) {
        log_error("owner widget must be specified");
        abort();
    }
    if (get_type_id == NULL) {
        log_error("get_type_id function must be specified");
        abort();
    }

    widget->owner = owner;
    widget->parent = NULL;
    widget->name = NULL;
    widget->world = NULL;
    widget->child_widgets = NULL;
    widget->get_type_id = get_type_id;
    widget->on_pos_changed = on_pos_changed;
    widget->on_size_changed = on_size_changed;
    widget->on_parent_changed = on_parent_changed;
    widget->on_children_changed = on_children_changed;
    widget->on_before_base_destroyed = on_before_base_destroyed;
    widget->on_after_spawned = on_after_spawned;
    widget->on_before_despawned = on_before_despawned;
    widget->on_window_size_changed = on_window_size_changed;
    widget->custom_value = 0;
    widget->custom_ptr = NULL;
    widget->child_widget_count = 0;
    widget->allow_serialization = true;

    widget->on_cursor_entered = NULL;
    widget->on_cursor_left = NULL;
    widget->on_mouse_button_pressed = NULL;
    widget->on_mouse_button_released = NULL;
    widget->on_hovered_cursor_moved = NULL;
    widget->on_keyboard_input_text = NULL;
    widget->on_keyboard_input = NULL;

    glm_vec2_copy((vec2){0.1f, 0.1f}, widget->relative_pos);
    glm_vec2_copy((vec2){0.1f, 0.05f}, widget->relative_size);

    glm_vec2_copy(widget->relative_pos, widget->screen_pos);
    glm_vec2_copy(widget->relative_size, widget->screen_size);

    return widget;
}

void
widget_destroy(te_widget* widget) {
    if (widget->world != NULL) {
        log_error("can't destroy a spawned widget, despawn it first");
        abort();
    }

    if (widget->on_before_base_destroyed != NULL) {
        widget->on_before_base_destroyed(widget->owner);
    }

    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        widget_destroy(widget->child_widgets[i]);
    }
    free(widget->child_widgets);
    widget->child_widget_count = 0;

    free(widget->name);

    free(widget);
}

void*
widget_get_owner(te_widget* widget) {
    return widget->owner;
}

const char*
widget_get_owner_type_id(te_widget* widget) {
    return widget->get_type_id();
}

void
widget_set_custom_value(te_widget* widget, size_t value) {
    widget->custom_value = value;
}

void
widget_set_custom_ptr(te_widget* widget, void* ptr) {
    widget->custom_ptr = ptr;
}

size_t
widget_get_custom_value(te_widget* widget) {
    return widget->custom_value;
}

void*
widget_get_custom_ptr(te_widget* widget) {
    return widget->custom_ptr;
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
        prv_widget_calc_screen_pos_size_recursive(
            widget->parent, widget->screen_pos, widget->screen_size);
    }
}

void
widget_set_parent(te_widget* widget, te_widget* new_parent) {
    if (new_parent == widget->parent) {
        return;
    }
    if (widget == new_parent) {
        log_error("can't attach a widget to itself");
        abort();
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
            log_error("unable to find widget in parent's array of child widgets");
            abort();
        }

        if (widget->parent->child_widget_count == 1) {
            widget->parent->child_widget_count = 0;
            free(widget->parent->child_widgets);
            widget->parent->child_widgets = NULL;
        } else {
            te_widget** new_children =
                malloc(sizeof(te_widget*) * (widget->parent->child_widget_count - 1));
            memcpy(new_children, widget->parent->child_widgets, sizeof(te_widget*) * index);
            memcpy(
                new_children + index, widget->parent->child_widgets + (index + 1),
                sizeof(te_widget) * (widget->parent->child_widget_count - index - 1));

            free(widget->parent->child_widgets);
            widget->parent->child_widgets = new_children;

            widget->parent->child_widget_count -= 1;
        }
    } else if (widget->world != NULL) {
        if (prv_world_find_root_widget(widget->world, widget)) {
            log_error("attaching a spawned root widgets to another widget is not allowed");
            abort();
        }
    }

    // Add self to new parent's array of child widgets.
    if (new_parent != NULL) {
        te_widget** new_children =
            malloc(sizeof(te_widget*) * (new_parent->child_widget_count + 1));
        memcpy(
            new_children, new_parent->child_widgets,
            sizeof(te_widget*) * new_parent->child_widget_count);

        free(new_parent->child_widgets);
        new_parent->child_widgets = new_children;

        new_parent->child_widgets[new_parent->child_widget_count] = widget;
        new_parent->child_widget_count += 1;
    }

    widget->parent = new_parent;
    prv_widget_recalc_screen_pos_size(widget);

    if (widget->world != NULL && widget->on_pos_changed != NULL) {
        widget->on_pos_changed(widget->owner);
    }
    if (widget->world != NULL && widget->on_size_changed != NULL) {
        widget->on_size_changed(widget->owner);
    }

    if (widget->world == NULL) {
        if (new_parent != NULL && new_parent->world != NULL) {
            world_spawn_widget(new_parent->world, widget);
            prv_world_remove_root_widget_no_notify(new_parent->world, widget, true);
        }
    } else {
        if (new_parent == NULL) {
            prv_world_add_root_widget_no_notify(widget->world, widget, true);
        } else {
            if (new_parent->world != NULL) {
                if (widget->world != new_parent->world) {
                    log_error("can't attach a widget to another widget because they are "
                              "spawned in different worlds");
                    abort();
                } else {
                    prv_world_remove_root_widget_no_notify(widget->world, widget, false);
                }
            } else {
                // This is a child widget and we also don't need to notify the world
                // (root widgets don't have parents).
                prv_widget_on_despawned(widget);
            }
        }
    }

    if (widget->world != NULL && widget->on_parent_changed != NULL) {
        widget->on_parent_changed(widget->owner);
    }

    if (new_parent != NULL && new_parent->world != NULL
        && new_parent->on_children_changed != NULL) {
        new_parent->on_children_changed(new_parent->owner);
    }
}

te_widget*
widget_get_parent(te_widget* widget) {
    return widget->parent;
}

te_widget**
widget_get_child_widgets_tmp(te_widget* widget, unsigned int* count) {
    (*count) = widget->child_widget_count;
    return widget->child_widgets;
}

void
widget_set_name(te_widget* widget, const char* name) {
    free(widget->name);
    widget->name = NULL;

    if (name != NULL) {
        const size_t len = strlen(name);
        widget->name = malloc(sizeof(char) * (len + 1));
        memcpy(widget->name, name, sizeof(char) * len);
        widget->name[len] = 0;
    }
}

const char*
widget_get_name(te_widget* widget) {
    return widget->name;
}

static void
prv_widget_on_parent_pos_changed(te_widget* widget) {
    prv_widget_recalc_screen_pos_size(widget);

    // Notify child widgets.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_parent_pos_changed(widget->child_widgets[i]);
    }

    if (widget->world != NULL && widget->on_pos_changed != NULL) {
        widget->on_pos_changed(widget->owner);
    }
}

static void
prv_widget_on_parent_size_changed(te_widget* widget) {
    prv_widget_recalc_screen_pos_size(widget);

    // Notify child widgets.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_parent_size_changed(widget->child_widgets[i]);
    }

    if (widget->world != NULL && widget->on_size_changed != NULL) {
        widget->on_size_changed(widget->owner);
    }
}

void
widget_set_relative_position(te_widget* widget, vec2 position) {
    glm_vec2_copy(position, widget->relative_pos); // don't check for [0.0; 1.0]
    prv_widget_recalc_screen_pos_size(widget);

    // Notify child widgets.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_parent_pos_changed(widget->child_widgets[i]);
    }

    if (widget->world != NULL && widget->on_pos_changed != NULL) {
        widget->on_pos_changed(widget->owner);
    }
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

    if (widget->world != NULL && widget->on_size_changed != NULL) {
        widget->on_size_changed(widget->owner);
    }
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

te_world*
widget_get_world(te_widget* widget) {
#if defined(DEBUG)
    if (widget == NULL) {
        log_error("invalid widget specified");
        abort();
    }
#endif
    return widget->world;
}

void
widget_set_is_serialization_allowed(te_widget* widget, bool allow) {
    widget->allow_serialization = allow;
}

bool
widget_is_serialization_allowed(te_widget* widget) {
    return widget->allow_serialization;
}

void
prv_widget_on_spawned(te_widget* widget, te_world* world) {
    // Spawn from top to bottom (in the hierarchy) so that widgets will be placed in the renderer
    // in the order from top to bottom (in the hierarchy) for top widgets to be rendered first and bottom widgets last.
    widget->world = world;
    if (widget->on_after_spawned != NULL) {
        widget->on_after_spawned(widget->owner);
    }

    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_spawned(widget->child_widgets[i], world);
    }
}

void
prv_widget_on_despawned(te_widget* widget) {
    // Despawn children first.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_despawned(widget->child_widgets[i]);
    }

    if (widget->on_before_despawned != NULL) {
        widget->on_before_despawned(widget->owner);
    }
    widget->world = NULL;
}

void
prv_widget_on_window_size_changed(te_widget* widget) {
    if (widget->world == NULL) {
        // No need to notify widgets because they will query window size when spawned.
        return;
    }

    // Start from deepest child widget so that when parent widgets are notified
    // they will have child widgets having already updated data.
    for (unsigned int i = 0; i < widget->child_widget_count; i++) {
        prv_widget_on_window_size_changed(widget->child_widgets[i]);
    }

    if (widget->on_window_size_changed != NULL) {
        widget->on_window_size_changed(widget->owner);
    }
}

void
prv_widget_set_input_callbacks(
    te_widget* widget, void (*on_cursor_entered)(void* owner, vec2 cursor_pos),
    void (*on_cursor_left)(void* owner, vec2 cursor_pos),
    void (*on_mouse_button_pressed)(void* owner, enum te_mouse_button button, vec2 cursor_pos),
    void (*on_mouse_button_released)(
        void* owner, enum te_mouse_button button, vec2 cursor_pos),
    void (*on_hovered_cursor_moved)(void* owner, vec2 cursor_pos),
    void (*on_keyboard_input_text)(void* owner, const char* input_text),
    void (*on_keyboard_input)(void* owner, enum te_keyboard_button button)) {
    widget->on_cursor_entered = on_cursor_entered;
    widget->on_cursor_left = on_cursor_left;
    widget->on_mouse_button_pressed = on_mouse_button_pressed;
    widget->on_mouse_button_released = on_mouse_button_released;
    widget->on_hovered_cursor_moved = on_hovered_cursor_moved;
    widget->on_keyboard_input_text = on_keyboard_input_text;
    widget->on_keyboard_input = on_keyboard_input;
}

void
prv_widget_on_mouse_button_pressed(
    te_widget* widget, enum te_mouse_button button, vec2 cursor_pos) {
    if (widget->on_mouse_button_pressed == NULL) {
        return;
    }

    widget->on_mouse_button_pressed(widget->owner, button, cursor_pos);
}

void
prv_widget_on_mouse_button_released(
    te_widget* widget, enum te_mouse_button button, vec2 cursor_pos) {
    if (widget->on_mouse_button_released == NULL) {
        return;
    }

    widget->on_mouse_button_released(widget->owner, button, cursor_pos);
}

void
prv_widget_on_cursor_entered(te_widget* widget, vec2 cursor_pos) {
    if (widget->on_cursor_entered == NULL) {
        return;
    }

    widget->on_cursor_entered(widget->owner, cursor_pos);
}

void
prv_widget_on_cursor_left(te_widget* widget, vec2 cursor_pos) {
    if (widget->on_cursor_left == NULL) {
        return;
    }

    widget->on_cursor_left(widget->owner, cursor_pos);
}

void
prv_widget_on_hovered_cursor_moved(te_widget* widget, vec2 cursor_pos) {
    if (widget->on_hovered_cursor_moved == NULL) {
        return;
    }

    widget->on_hovered_cursor_moved(widget->owner, cursor_pos);
}

void
prv_widget_on_keyboard_input(te_widget* widget, enum te_keyboard_button button) {
    if (widget->on_keyboard_input == NULL) {
        return;
    }

    widget->on_keyboard_input(widget->owner, button);
    return;
}

void
prv_widget_on_keyboard_input_text(te_widget* widget, const char* text) {
    if (widget->on_keyboard_input_text == NULL) {
        return;
    }

    widget->on_keyboard_input_text(widget->owner, text);
    return;
}
