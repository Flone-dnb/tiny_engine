#include "widget/checkbox_widget.h"

#include "game_manager.h"
#include "misc/error.h"
#include "widget/rect_widget.h"
#include "widget/widget.h"
#include "window.h"
#include "world.h"

struct te_checkbox_widget {
    te_widget* widget;

    // Always valid.
    te_rect_widget* background_rect;

    // NULL if the checkbox is not enabled (not checked).
    te_rect_widget* checked_rect;

    // May be NULL if not set.
    void (*on_changed)(te_checkbox_widget* checkbox_widget, bool is_checked);

    char* background_tex_relative_path;
    char* checked_tex_relative_path;

    vec4 background_color;
    vec4 checked_color;

    // `true` if entered the "destroy" function.
    bool is_checkbox_widget_destroy;

    bool is_checked;
    bool is_fixing_height;
};

// Widget callbacks:
static void prv_checkbox_widget_on_pos_changed(void* this);
static void prv_checkbox_widget_on_size_changed(void* this);
static void prv_checkbox_widget_on_window_size_changed(void* this);
static void prv_checkbox_widget_on_before_base_destroyed(void* this);
static void prv_checkbox_widget_on_after_spawned(void* this);
static void prv_checkbox_widget_on_before_despawned(void* this);

// Interactable callbacks:
static void prv_checkbox_widget_on_mouse_button_released(void* this, enum te_mouse_button button, vec2 cursor_pos);

te_checkbox_widget*
checkbox_widget_create(void) {
    te_checkbox_widget* checkbox_widget = malloc(sizeof(te_checkbox_widget));

    checkbox_widget->widget = widget_create(
        checkbox_widget, prv_checkbox_widget_on_pos_changed, prv_checkbox_widget_on_size_changed,
        prv_checkbox_widget_on_before_base_destroyed, prv_checkbox_widget_on_after_spawned,
        prv_checkbox_widget_on_before_despawned, prv_checkbox_widget_on_window_size_changed);

    prv_widget_set_input_callbacks(
        checkbox_widget->widget, NULL, NULL, NULL, prv_checkbox_widget_on_mouse_button_released, NULL, NULL, NULL);

    glm_vec4_copy((vec4){0.5f, 0.5f, 0.5f, 1.0f}, checkbox_widget->background_color);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, checkbox_widget->checked_color);

    checkbox_widget->background_tex_relative_path = NULL;
    checkbox_widget->checked_tex_relative_path = NULL;

    checkbox_widget->checked_rect = NULL;
    checkbox_widget->is_checked = false;
    checkbox_widget->is_fixing_height = false;
    checkbox_widget->on_changed = NULL;

    {
        checkbox_widget->background_rect = rect_widget_create();
        te_widget* rect = rect_widget_get_widget(checkbox_widget->background_rect);

        widget_set_is_serialization_allowed(rect, false);
        widget_set_parent(rect, checkbox_widget->widget);
        widget_set_relative_position(rect, (vec2){0.0f, 0.0f});
        widget_set_relative_size(rect, (vec2){1.0f, 1.0f});
    }

    return checkbox_widget;
}

void
checkbox_widget_destroy(te_checkbox_widget* checkbox_widget) {
    checkbox_widget->is_checkbox_widget_destroy = true;

    if (checkbox_widget->widget != NULL) { // may be null if we got here from base destroy
        widget_destroy(checkbox_widget->widget);
    }

    free(checkbox_widget->background_tex_relative_path);
    free(checkbox_widget->checked_tex_relative_path);
    free(checkbox_widget);
}

static void
prv_checkbox_widget_on_before_base_destroyed(void* this) {
    te_checkbox_widget* checkbox_widget = this;
    if (checkbox_widget->is_checkbox_widget_destroy) {
        return;
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    checkbox_widget->widget = NULL;
    checkbox_widget_destroy(checkbox_widget);
}

te_widget*
checkbox_widget_get_widget(te_checkbox_widget* checkbox_widget) {
    return checkbox_widget->widget;
}

void
checkbox_widget_set_on_changed(
    te_checkbox_widget* checkbox_widget, void (*on_changed)(te_checkbox_widget* checkbox_widget, bool is_checked)) {
    checkbox_widget->on_changed = on_changed;
}

static void
prv_checkbox_widget_create_checked_rect(te_checkbox_widget* checkbox_widget) {
#if defined(DEBUG)
    if (checkbox_widget->checked_rect != NULL) {
        show_error_and_abort("expected checked rect widget to be invalid");
    }
#endif

    checkbox_widget->checked_rect = rect_widget_create();
    te_widget* rect = rect_widget_get_widget(checkbox_widget->checked_rect);

    widget_set_is_serialization_allowed(rect, false);
    widget_set_parent(rect, checkbox_widget->widget);
    widget_set_relative_position(rect, (vec2){0.1f, 0.1f});
    widget_set_relative_size(rect, (vec2){0.8f, 0.8f});

    rect_widget_set_color(checkbox_widget->checked_rect, checkbox_widget->checked_color);
    if (checkbox_widget->checked_tex_relative_path != NULL) {
        rect_widget_set_texture(checkbox_widget->checked_rect, checkbox_widget->checked_tex_relative_path);
    }
}

void
checkbox_widget_set_is_checked(te_checkbox_widget* checkbox_widget, bool is_checked) {
    if (checkbox_widget->is_checked == is_checked) {
        return;
    }
    checkbox_widget->is_checked = is_checked;

    if (widget_get_world(checkbox_widget->widget) != NULL) {
        if (is_checked) {
            prv_checkbox_widget_create_checked_rect(checkbox_widget);
        } else {
            widget_set_parent(rect_widget_get_widget(checkbox_widget->checked_rect), NULL);
            rect_widget_destroy(checkbox_widget->checked_rect);
            checkbox_widget->checked_rect = NULL;
        }
    }
}

bool
checkbox_widget_is_checked(te_checkbox_widget* checkbox_widget) {
    return checkbox_widget->is_checked;
}

void
checkbox_widget_set_background_color(te_checkbox_widget* checkbox_widget, vec4 color) {
    glm_vec4_copy(color, checkbox_widget->background_color);
    rect_widget_set_color(checkbox_widget->background_rect, color);
}

void
checkbox_widget_set_checked_color(te_checkbox_widget* checkbox_widget, vec4 color) {
    glm_vec4_copy(color, checkbox_widget->checked_color);

    if (checkbox_widget->checked_rect != NULL) {
        rect_widget_set_color(checkbox_widget->checked_rect, color);
    }
}

void
checkbox_widget_get_background_color(te_checkbox_widget* checkbox_widget, vec4 out) {
    glm_vec4_copy(checkbox_widget->background_color, out);
}

void
checkbox_widget_get_checked_color(te_checkbox_widget* checkbox_widget, vec4 out) {
    glm_vec4_copy(checkbox_widget->checked_color, out);
}

void
checkbox_widget_set_background_texture(te_checkbox_widget* checkbox_widget, const char* relative_path) {
    free(checkbox_widget->background_tex_relative_path);

    if (relative_path == NULL) {
        checkbox_widget->background_tex_relative_path = NULL;
    } else {
        const size_t path_len = strlen(relative_path);
        checkbox_widget->background_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
        memcpy(checkbox_widget->background_tex_relative_path, relative_path, sizeof(char) * path_len);
        checkbox_widget->background_tex_relative_path[path_len] = 0;
    }

    rect_widget_set_texture(checkbox_widget->background_rect, checkbox_widget->background_tex_relative_path);
}

void
checkbox_widget_set_checked_texture(te_checkbox_widget* checkbox_widget, const char* relative_path) {
    free(checkbox_widget->checked_tex_relative_path);

    if (relative_path == NULL) {
        checkbox_widget->checked_tex_relative_path = NULL;
    } else {
        const size_t path_len = strlen(relative_path);
        checkbox_widget->checked_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
        memcpy(checkbox_widget->checked_tex_relative_path, relative_path, sizeof(char) * path_len);
        checkbox_widget->checked_tex_relative_path[path_len] = 0;
    }

    if (checkbox_widget->checked_rect != NULL) {
        rect_widget_set_texture(checkbox_widget->checked_rect, checkbox_widget->checked_tex_relative_path);
    }
}

char*
checkbox_widget_get_background_texture(te_checkbox_widget* checkbox_widget) {
    return checkbox_widget->background_tex_relative_path;
}

char*
checkbox_widget_get_checked_texture(te_checkbox_widget* checkbox_widget) {
    return checkbox_widget->checked_tex_relative_path;
}

static void
prv_checkbox_widget_on_pos_changed(void* this) {
    (void)this;
}

static void
prv_checkbox_fix_height(te_checkbox_widget* checkbox_widget) {
    te_world* world = widget_get_world(checkbox_widget->widget);
    if (world == NULL) {
        return;
    }

    // To avoid recursion.
    checkbox_widget->is_fixing_height = true;

    // Recalculate height to be a square.
    te_window* window = game_manager_get_window(world_get_game_manager(world));

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);

    vec2 old_screen_size;
    widget_get_screen_size(checkbox_widget->widget, old_screen_size);

    const float target_pixel_count = (float)window_width * old_screen_size[0];
    float height = target_pixel_count / (float)window_height;
    if (height > 1.0f) {
        height = 1.0f;
    }

    const float multiplier = height / old_screen_size[1];
    vec2 relative_size;
    widget_get_relative_size(checkbox_widget->widget, relative_size);

    relative_size[1] *= multiplier;
    widget_set_relative_size(checkbox_widget->widget, relative_size);

    checkbox_widget->is_fixing_height = false;
}

static void
prv_checkbox_widget_on_size_changed(void* this) {
    te_checkbox_widget* checkbox_widget = this;
    if (checkbox_widget->is_fixing_height) {
        return;
    }
    prv_checkbox_fix_height(checkbox_widget);
}

static void
prv_checkbox_widget_on_window_size_changed(void* this) {
    te_checkbox_widget* checkbox_widget = this;
    prv_checkbox_fix_height(checkbox_widget);
}

static void
prv_checkbox_widget_on_after_spawned(void* this) {
    te_checkbox_widget* checkbox_widget = this;

    // Self check:
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(checkbox_widget->widget, &child_count);
    if (child_count != 1) {
        show_error_and_abort("unexpected child widget count on a widget");
    }

    rect_widget_set_color(checkbox_widget->background_rect, checkbox_widget->background_color);
    if (checkbox_widget->background_tex_relative_path != NULL) {
        rect_widget_set_texture(checkbox_widget->background_rect, checkbox_widget->background_tex_relative_path);
    }

    if (checkbox_widget->is_checked) {
        prv_checkbox_widget_create_checked_rect(checkbox_widget);
    }

    prv_checkbox_fix_height(checkbox_widget);

    te_world* world = widget_get_world(checkbox_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    prv_world_add_interactable_widget(world, checkbox_widget->widget);
}

static void
prv_checkbox_widget_on_before_despawned(void* this) {
    te_checkbox_widget* checkbox_widget = this;

    // Self check:
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(checkbox_widget->widget, &child_count);
    if (child_count > 2) {
        show_error_and_abort("unexpected child widget count on a widget");
    }

    te_world* world = widget_get_world(checkbox_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    prv_world_remove_interactable_widget(world, checkbox_widget->widget);
}

static void
prv_checkbox_widget_on_mouse_button_released(void* this, enum te_mouse_button button, vec2 cursor_pos) {
    (void)cursor_pos;

    if (button != TE_MB_LEFT) {
        return;
    }

    te_checkbox_widget* checkbox_widget = this;

    checkbox_widget_set_is_checked(checkbox_widget, !checkbox_widget->is_checked);
    if (checkbox_widget->on_changed) {
        checkbox_widget->on_changed(checkbox_widget, checkbox_widget->is_checked);
    }
}
