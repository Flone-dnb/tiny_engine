#include "widget/slider_widget.h"

#include "misc/error.h"
#include "widget/rect_widget.h"
#include "widget/widget.h"
#include "world.h"

#define TE_SLIDER_HANDLE_WIDTH 0.1f

struct te_slider_widget {
    te_widget* widget;

    // Slider elements.
    te_rect_widget* background_rect;
    te_rect_widget* handle_rect;

    // May be NULL if not set.
    void (*on_value_changed)(float new_value);

    vec4 background_color;
    vec4 handle_color;

    char* background_tex_relative_path;
    char* handle_tex_relative_path;

    // Slider value in range [0.0; 1.0].
    float value;
    float step_size;

    // `true` if entered the "destroy" function.
    bool is_slider_widget_destroy;
    bool is_handle_grabbed;
};

// Widget callbacks:
static void prv_slider_widget_on_pos_changed(void* this);
static void prv_slider_widget_on_size_changed(void* this);
static void prv_slider_widget_on_window_size_changed(void* this);
static void prv_slider_widget_on_before_base_destroyed(void* this);
static void prv_slider_widget_on_after_spawned(void* this);
static void prv_slider_widget_on_before_despawned(void* this);

// Interactable callbacks:
static void prv_slider_widget_on_mouse_button_pressed(void* this, enum te_mouse_button button, vec2 cursor_pos);
static void prv_slider_widget_on_mouse_button_released(void* this, enum te_mouse_button button, vec2 cursor_pos);
static void prv_slider_widget_on_hovered_cursor_moved(void* this, vec2 cursor_pos);
static void prv_slider_widget_on_cursor_left(void* this, vec2 cursor_pos);

te_slider_widget*
slider_widget_create(void) {
    te_slider_widget* slider_widget = malloc(sizeof(te_slider_widget));

    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, slider_widget->background_color);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, slider_widget->handle_color);
    slider_widget->background_tex_relative_path = NULL;
    slider_widget->handle_tex_relative_path = NULL;
    slider_widget->value = 0.5f;
    slider_widget->step_size = 0.1f;
    slider_widget->on_value_changed = NULL;
    slider_widget->is_slider_widget_destroy = false;
    slider_widget->is_handle_grabbed = false;

    slider_widget->widget = widget_create(
        slider_widget, prv_slider_widget_on_pos_changed, prv_slider_widget_on_size_changed,
        prv_slider_widget_on_before_base_destroyed, prv_slider_widget_on_after_spawned, prv_slider_widget_on_before_despawned,
        prv_slider_widget_on_window_size_changed);

    prv_widget_set_input_callbacks(
        slider_widget->widget, NULL, prv_slider_widget_on_cursor_left, prv_slider_widget_on_mouse_button_pressed,
        prv_slider_widget_on_mouse_button_released, prv_slider_widget_on_hovered_cursor_moved, NULL, NULL);

    {
        slider_widget->background_rect = rect_widget_create();
        te_widget* rect = rect_widget_get_widget(slider_widget->background_rect);
        widget_set_is_serialization_allowed(rect, false);

        const float background_height = 0.2f;

        widget_set_parent(rect, slider_widget->widget);
        widget_set_relative_position(rect, (vec2){0.0f, 0.5f - background_height / 2.0f});
        widget_set_relative_size(rect, (vec2){1.0f, background_height});
    }

    {
        slider_widget->handle_rect = rect_widget_create();
        te_widget* rect = rect_widget_get_widget(slider_widget->handle_rect);
        widget_set_is_serialization_allowed(rect, false);

        widget_set_parent(rect, slider_widget->widget);
        widget_set_relative_size(rect, (vec2){TE_SLIDER_HANDLE_WIDTH, 1.0f});
        widget_set_relative_position(rect, (vec2){slider_widget->value - TE_SLIDER_HANDLE_WIDTH / 2.0f, 0.0f});
    }

    return slider_widget;
}

void
slider_widget_destroy(te_slider_widget* slider_widget) {
    slider_widget->is_slider_widget_destroy = true;

    if (slider_widget->widget != NULL) { // may be null if we got here from base destroy
        widget_destroy(slider_widget->widget);
    }

    free(slider_widget->background_tex_relative_path);
    free(slider_widget->handle_tex_relative_path);
    free(slider_widget);
}

static void
prv_slider_widget_on_before_base_destroyed(void* this) {
    te_slider_widget* slider_widget = this;
    if (slider_widget->is_slider_widget_destroy) {
        return;
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    slider_widget->widget = NULL;
    slider_widget_destroy(slider_widget);
}

te_widget*
slider_widget_get_widget(te_slider_widget* slider_widget) {
    return slider_widget->widget;
}

static float
prv_slider_widget_snap_to_nearest(float value, float step_size) {
    return roundf(value / step_size) * step_size;
}

void
slider_widget_set_on_value_changed(te_slider_widget* slider_widget, void (*on_value_changed)(float new_value)) {
    slider_widget->on_value_changed = on_value_changed;
}

void
slider_widget_set_value(te_slider_widget* slider_widget, float value) {
    slider_widget->value = value; // ignore step size here

    if (widget_get_world(slider_widget->widget) != NULL) {
        widget_set_relative_position(
            rect_widget_get_widget(slider_widget->handle_rect), (vec2){value - TE_SLIDER_HANDLE_WIDTH / 2.0f, 0.0f});
    }
}

float
slider_widget_get_value(te_slider_widget* slider_widget) {
    return slider_widget->value;
}

void
slider_widget_set_step_size(te_slider_widget* slider_widget, float step_size) {
    slider_widget->step_size = step_size;
    // don't update value here
}

float
slider_widget_get_step_size(te_slider_widget* slider_widget) {
    return slider_widget->step_size;
}

void
slider_widget_set_background_color(te_slider_widget* slider_widget, vec4 color) {
    glm_vec4_copy(color, slider_widget->background_color);
    rect_widget_set_color(slider_widget->background_rect, color);
}

void
slider_widget_set_handle_color(te_slider_widget* slider_widget, vec4 color) {
    glm_vec4_copy(color, slider_widget->handle_color);
    rect_widget_set_color(slider_widget->handle_rect, color);
}

void
slider_widget_set_background_texture(te_slider_widget* slider_widget, const char* relative_path) {
    free(slider_widget->background_tex_relative_path);
    slider_widget->background_tex_relative_path = NULL;

    if (relative_path == NULL) {
        rect_widget_set_texture(slider_widget->background_rect, NULL);
        return;
    }

    const size_t path_len = strlen(relative_path);
    slider_widget->background_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
    memcpy(slider_widget->background_tex_relative_path, relative_path, sizeof(char) * path_len);
    slider_widget->background_tex_relative_path[path_len] = 0;

    rect_widget_set_texture(slider_widget->background_rect, slider_widget->background_tex_relative_path);
}

void
slider_widget_set_handle_texture(te_slider_widget* slider_widget, const char* relative_path) {
    free(slider_widget->handle_tex_relative_path);
    slider_widget->handle_tex_relative_path = NULL;

    if (relative_path == NULL) {
        rect_widget_set_texture(slider_widget->handle_rect, NULL);
        return;
    }

    const size_t path_len = strlen(relative_path);
    slider_widget->handle_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
    memcpy(slider_widget->handle_tex_relative_path, relative_path, sizeof(char) * path_len);
    slider_widget->handle_tex_relative_path[path_len] = 0;

    rect_widget_set_texture(slider_widget->handle_rect, slider_widget->handle_tex_relative_path);
}

void
slider_widget_get_background_color(te_slider_widget* slider_widget, vec4 out) {
    glm_vec4_copy(slider_widget->background_color, out);
}

void
slider_widget_get_handle_color(te_slider_widget* slider_widget, vec4 out) {
    glm_vec4_copy(slider_widget->handle_color, out);
}

char*
slider_widget_get_background_texture(te_slider_widget* slider_widget) {
    return slider_widget->background_tex_relative_path;
}

char*
slider_widget_get_handle_texture(te_slider_widget* slider_widget) {
    return slider_widget->handle_tex_relative_path;
}

static void
prv_slider_widget_on_pos_changed(void* this) {
    (void)this;
}

static void
prv_slider_widget_on_size_changed(void* this) {
    (void)this;
}

static void
prv_slider_widget_on_window_size_changed(void* this) {
    (void)this;
}

static void
prv_slider_widget_on_after_spawned(void* this) {
    te_slider_widget* slider_widget = this;

    // Self check:
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(slider_widget->widget, &child_count);
    if (child_count != 2) {
        show_error_and_abort("unexpected child widget count on a widget");
    }

    rect_widget_set_color(slider_widget->background_rect, slider_widget->background_color);
    rect_widget_set_color(slider_widget->handle_rect, slider_widget->handle_color);

    if (slider_widget->background_tex_relative_path != NULL) {
        rect_widget_set_texture(slider_widget->background_rect, slider_widget->background_tex_relative_path);
    }
    if (slider_widget->handle_tex_relative_path != NULL) {
        rect_widget_set_texture(slider_widget->handle_rect, slider_widget->handle_tex_relative_path);
    }

    widget_set_relative_position(
        rect_widget_get_widget(slider_widget->handle_rect), (vec2){slider_widget->value - TE_SLIDER_HANDLE_WIDTH / 2.0f, 0.0f});

    te_world* world = widget_get_world(slider_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    prv_world_add_interactable_widget(world, slider_widget->widget);
}

static void
prv_slider_widget_on_before_despawned(void* this) {
    te_slider_widget* slider_widget = this;

    // Self check:
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(slider_widget->widget, &child_count);
    if (child_count != 2) {
        show_error_and_abort("unexpected child widget count on a widget");
    }

    te_world* world = widget_get_world(slider_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    prv_world_remove_interactable_widget(world, slider_widget->widget);
}

static void
prv_slider_widget_update_value(te_slider_widget* slider_widget, vec2 cursor_pos) {
    vec2 screen_pos;
    vec2 screen_size;
    widget_get_screen_position(slider_widget->widget, screen_pos);
    widget_get_screen_size(slider_widget->widget, screen_size);

    float new_value = (cursor_pos[0] - screen_pos[0]) / screen_size[0];
    if (new_value < 0.0f) {
        new_value = 0.0f;
    } else if (new_value > 1.0f) {
        new_value = 1.0f;
    }

    slider_widget->is_handle_grabbed = true;
    slider_widget->value = prv_slider_widget_snap_to_nearest(new_value, slider_widget->step_size);
    widget_set_relative_position(
        rect_widget_get_widget(slider_widget->handle_rect), (vec2){slider_widget->value - TE_SLIDER_HANDLE_WIDTH / 2.0f, 0.0f});

    if (slider_widget->on_value_changed != NULL) {
        slider_widget->on_value_changed(slider_widget->value);
    }
}

static void
prv_slider_widget_on_mouse_button_pressed(void* this, enum te_mouse_button button, vec2 cursor_pos) {
    if (button != TE_MB_LEFT) {
        return;
    }

    te_slider_widget* slider_widget = this;
    prv_slider_widget_update_value(slider_widget, cursor_pos);
}

static void
prv_slider_widget_on_mouse_button_released(void* this, enum te_mouse_button button, vec2 cursor_pos) {
    te_slider_widget* slider_widget = this;
    if (!slider_widget->is_handle_grabbed) {
        return;
    }

    if (button != TE_MB_LEFT) {
        return;
    }

    prv_slider_widget_update_value(slider_widget, cursor_pos);
    slider_widget->is_handle_grabbed = false;
}

static void
prv_slider_widget_on_hovered_cursor_moved(void* this, vec2 cursor_pos) {
    te_slider_widget* slider_widget = this;

    if (!slider_widget->is_handle_grabbed) {
        return;
    }

    prv_slider_widget_update_value(slider_widget, cursor_pos);
}

static void
prv_slider_widget_on_cursor_left(void* this, vec2 cursor_pos) {
    te_slider_widget* slider_widget = this;
    if (!slider_widget->is_handle_grabbed) {
        return;
    }

    prv_slider_widget_update_value(slider_widget, cursor_pos);
    slider_widget->is_handle_grabbed = false;
}
