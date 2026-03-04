#include <widget/progress_widget.h>

#include <io/log.h>
#include <type_database.h>
#include <widget/rect_widget.h>
#include <widget/widget.h>
#include <world.h>

struct te_progress_widget {
    te_widget* widget;

    // Always valid.
    te_rect_widget* background_rect;
    te_rect_widget* foreground_rect;

    vec4 background_color;
    vec4 foreground_color;

    // NULL if was not set.
    char* background_tex_relative_path;
    char* foreground_tex_relative_path;

    // Current state of the progress widget in range [0.0; 1.0].
    float value;

    bool is_progress_widget_destroy;
};

// Widget callbacks:
static void prv_progress_widget_on_before_base_destroyed(void* this);
static void prv_progress_widget_on_after_spawned(void* this);
static void prv_progress_widget_on_before_despawned(void* this);

te_progress_widget*
progress_widget_create(void) {
    te_progress_widget* progress_widget = malloc(sizeof(te_progress_widget));

    progress_widget->widget = widget_create(
        progress_widget, progress_widget_get_type_id, NULL, NULL,
        prv_progress_widget_on_before_base_destroyed, NULL, NULL,
        prv_progress_widget_on_after_spawned, prv_progress_widget_on_before_despawned, NULL);

    progress_widget->background_tex_relative_path = NULL;
    progress_widget->foreground_tex_relative_path = NULL;
    glm_vec4_copy((vec4){0.5f, 0.5f, 0.5f, 1.0f}, progress_widget->background_color);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, progress_widget->foreground_color);
    progress_widget->value = 0.5f;
    progress_widget->is_progress_widget_destroy = false;

    {
        progress_widget->background_rect = rect_widget_create();
        te_widget* rect = rect_widget_get_widget(progress_widget->background_rect);

        widget_set_is_serialization_allowed(rect, false);
        widget_set_parent(rect, progress_widget->widget);
        widget_set_relative_position(rect, (vec2){0.0f, 0.0f});
        widget_set_relative_size(rect, (vec2){1.0f, 1.0f});
    }

    {
        progress_widget->foreground_rect = rect_widget_create();
        te_widget* rect = rect_widget_get_widget(progress_widget->foreground_rect);

        widget_set_is_serialization_allowed(rect, false);
        widget_set_parent(rect, progress_widget->widget);
        widget_set_relative_position(rect, (vec2){0.0f, 0.0f});
        widget_set_relative_size(rect, (vec2){1.0f, 1.0f});
    }

    return progress_widget;
}

void
progress_widget_destroy(te_progress_widget* progress_widget) {
    progress_widget->is_progress_widget_destroy = true;

    if (progress_widget->widget != NULL) { // may be null if we got here from base destroy
        widget_destroy(progress_widget->widget);
    }

    free(progress_widget->foreground_tex_relative_path);
    free(progress_widget->background_tex_relative_path);
    free(progress_widget);
}

static void
prv_progress_widget_on_before_base_destroyed(void* this) {
    te_progress_widget* progress_widget = this;
    if (progress_widget->is_progress_widget_destroy) {
        return;
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    progress_widget->widget = NULL;
    progress_widget_destroy(progress_widget);
}

te_widget*
progress_widget_get_widget(te_progress_widget* progress_widget) {
    return progress_widget->widget;
}

void
progress_widget_set_value(te_progress_widget* progress_widget, float value) {
    progress_widget->value = value;
    rect_widget_set_clip_rect(
        progress_widget->foreground_rect, (vec4){0.0f, 0.0f, progress_widget->value, 1.0f});
}

float
progress_widget_get_value(te_progress_widget* progress_widget) {
    return progress_widget->value;
}

void
progress_widget_set_background_color(te_progress_widget* progress_widget, vec4 color) {
    glm_vec4_copy(color, progress_widget->background_color);
    rect_widget_set_color(progress_widget->background_rect, color);
}

void
progress_widget_set_foreground_color(te_progress_widget* progress_widget, vec4 color) {
    glm_vec4_copy(color, progress_widget->foreground_color);
    rect_widget_set_color(progress_widget->foreground_rect, color);
}

void
progress_widget_get_background_color(te_progress_widget* progress_widget, vec4 out) {
    glm_vec4_copy(progress_widget->background_color, out);
}

void
progress_widget_get_foreground_color(te_progress_widget* progress_widget, vec4 out) {
    glm_vec4_copy(progress_widget->foreground_color, out);
}

void
progress_widget_set_background_texture(
    te_progress_widget* progress_widget, const char* relative_path) {
    free(progress_widget->background_tex_relative_path);
    progress_widget->background_tex_relative_path = NULL;

    if (relative_path == NULL) {
        rect_widget_set_texture(progress_widget->background_rect, NULL);
        return;
    }

    const size_t path_len = strlen(relative_path);
    progress_widget->background_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
    memcpy(
        progress_widget->background_tex_relative_path, relative_path, sizeof(char) * path_len);
    progress_widget->background_tex_relative_path[path_len] = 0;

    rect_widget_set_texture(
        progress_widget->background_rect, progress_widget->background_tex_relative_path);
}

void
progress_widget_set_foreground_texture(
    te_progress_widget* progress_widget, const char* relative_path) {
    free(progress_widget->foreground_tex_relative_path);
    progress_widget->foreground_tex_relative_path = NULL;

    if (relative_path == NULL) {
        rect_widget_set_texture(progress_widget->foreground_rect, NULL);
        return;
    }

    const size_t path_len = strlen(relative_path);
    progress_widget->foreground_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
    memcpy(
        progress_widget->foreground_tex_relative_path, relative_path, sizeof(char) * path_len);
    progress_widget->foreground_tex_relative_path[path_len] = 0;

    rect_widget_set_texture(
        progress_widget->foreground_rect, progress_widget->foreground_tex_relative_path);
}

char*
progress_widget_get_background_texture(te_progress_widget* progress_widget) {
    return progress_widget->background_tex_relative_path;
}

char*
progress_widget_get_foreground_texture(te_progress_widget* progress_widget) {
    return progress_widget->foreground_tex_relative_path;
}

static void
prv_progress_widget_on_after_spawned(void* this) {
    te_progress_widget* progress_widget = this;

    // Self check:
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(progress_widget->widget, &child_count);
    if (child_count != 2) {
        log_error("unexpected child widget count on a widget");
        abort();
    }

    rect_widget_set_color(progress_widget->background_rect, progress_widget->background_color);
    rect_widget_set_color(progress_widget->foreground_rect, progress_widget->foreground_color);

    if (progress_widget->background_tex_relative_path != NULL) {
        rect_widget_set_texture(
            progress_widget->background_rect, progress_widget->background_tex_relative_path);
    }
    if (progress_widget->foreground_tex_relative_path != NULL) {
        rect_widget_set_texture(
            progress_widget->foreground_rect, progress_widget->foreground_tex_relative_path);
    }

    rect_widget_set_clip_rect(
        progress_widget->foreground_rect, (vec4){0.0f, 0.0f, progress_widget->value, 1.0f});
}

static void
prv_progress_widget_on_before_despawned(void* this) {
    te_progress_widget* progress_widget = this;

    // Self check:
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(progress_widget->widget, &child_count);
    if (child_count != 2) {
        log_error("unexpected child widget count on a widget");
        abort();
    }
}

static inline void
prv_progress_widget_set_position(te_progress_widget* progress_widget, vec2 pos) {
    widget_set_relative_position(progress_widget->widget, pos);
}

static inline void
prv_progress_widget_get_position(te_progress_widget* progress_widget, vec2 out) {
    widget_get_relative_position(progress_widget->widget, out);
}

static inline void
prv_progress_widget_set_size(te_progress_widget* progress_widget, vec2 size) {
    widget_set_relative_size(progress_widget->widget, size);
}

static inline void
prv_progress_widget_get_size(te_progress_widget* progress_widget, vec2 out) {
    widget_get_relative_size(progress_widget->widget, out);
}

const char*
progress_widget_get_type_id(void) {
    return "progress_widget";
}

static te_widget*
prv_progress_widget_get_base(te_progress_widget* progress_widget) {
    return progress_widget->widget;
}

static void
prv_progress_widget_spawn(te_world* world, te_progress_widget* progress_widget) {
    world_spawn_widget(world, prv_progress_widget_get_base(progress_widget));
}

void
progress_widget_register_type(void) {
    te_type_info* info = type_info_create(
        progress_widget_get_type_id(), progress_widget_create, prv_progress_widget_spawn,
        prv_progress_widget_get_base);
    type_info_add_vec2_variable(
        info, "position", prv_progress_widget_set_position, prv_progress_widget_get_position);
    type_info_add_vec2_variable(
        info, "size", prv_progress_widget_set_size, prv_progress_widget_get_size);
    type_info_add_float_variable(
        info, "value", progress_widget_set_value, progress_widget_get_value);
    type_info_add_vec4_variable(
        info, "background_color", progress_widget_set_background_color,
        progress_widget_get_background_color);
    type_info_add_vec4_variable(
        info, "foreground_color", progress_widget_set_foreground_color,
        progress_widget_get_foreground_color);
    type_info_add_string_variable(
        info, "background_texture", progress_widget_set_background_texture,
        progress_widget_get_background_texture);
    type_info_add_string_variable(
        info, "foreground_texture", progress_widget_set_foreground_texture,
        progress_widget_get_foreground_texture);

    type_database_register_type(info);
}
