#include <widget/checkbox_widget.h>

#include <game_manager.h>
#include <io/log.h>
#include <type_database.h>
#include <widget/rect_widget.h>
#include <widget/widget.h>
#include <window.h>
#include <world.h>

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
static void prv_checkbox_widget_on_size_changed(void* this);
static void prv_checkbox_widget_on_window_size_changed(void* this);
static void prv_checkbox_widget_on_before_base_destroyed(void* this);
static void prv_checkbox_widget_on_after_spawned(void* this);
static void prv_checkbox_widget_on_before_despawned(void* this);

// Interactable callbacks:
static void prv_checkbox_widget_on_mouse_button_released(
    void* this, enum te_mouse_button button, vec2 cursor_pos);

te_checkbox_widget*
checkbox_widget_create(void) {
    te_checkbox_widget* checkbox_widget = malloc(sizeof(te_checkbox_widget));

    checkbox_widget->widget = widget_create(
        checkbox_widget, checkbox_widget_get_type_id, NULL,
        prv_checkbox_widget_on_size_changed, prv_checkbox_widget_on_before_base_destroyed,
        NULL, NULL, prv_checkbox_widget_on_after_spawned,
        prv_checkbox_widget_on_before_despawned, prv_checkbox_widget_on_window_size_changed);

    prv_widget_set_input_callbacks(
        checkbox_widget->widget, NULL, NULL, NULL,
        prv_checkbox_widget_on_mouse_button_released, NULL, NULL, NULL);

    glm_vec4_copy((vec4){0.5f, 0.5f, 0.5f, 1.0f}, checkbox_widget->background_color);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, checkbox_widget->checked_color);

    checkbox_widget->background_tex_relative_path = NULL;
    checkbox_widget->checked_tex_relative_path = NULL;

    checkbox_widget->checked_rect = NULL;
    checkbox_widget->is_checked = false;
    checkbox_widget->is_fixing_height = false;
    checkbox_widget->on_changed = NULL;

    checkbox_widget->is_checkbox_widget_destroy = false;

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
    te_checkbox_widget* checkbox_widget,
    void (*on_changed)(te_checkbox_widget* checkbox_widget, bool is_checked)) {
    checkbox_widget->on_changed = on_changed;
}

static void
prv_checkbox_widget_create_checked_rect(te_checkbox_widget* checkbox_widget) {
#if defined(DEBUG)
    if (checkbox_widget->checked_rect != NULL) {
        log_error("expected checked rect widget to be invalid");
        abort();
    }
#endif

    checkbox_widget->checked_rect = rect_widget_create();
    te_widget* rect = rect_widget_get_widget(checkbox_widget->checked_rect);

    widget_set_is_serialization_allowed(rect, false);
    widget_set_relative_position(rect, (vec2){0.2f, 0.2f});
    widget_set_relative_size(rect, (vec2){0.6f, 0.6f});

    rect_widget_set_color(checkbox_widget->checked_rect, checkbox_widget->checked_color);
    if (checkbox_widget->checked_tex_relative_path != NULL) {
        rect_widget_set_texture(
            checkbox_widget->checked_rect, checkbox_widget->checked_tex_relative_path);
    }

    widget_set_parent(rect, checkbox_widget->widget);
}

void
checkbox_widget_set_is_checked(te_checkbox_widget* checkbox_widget, bool is_checked) {
    if (checkbox_widget->is_checked == is_checked) {
        return;
    }
    checkbox_widget->is_checked = is_checked;

    te_world* world = widget_get_world(checkbox_widget->widget);
    if (world != NULL) {
        if (is_checked) {
            prv_checkbox_widget_create_checked_rect(checkbox_widget);
        } else {
            // Despawn.
            widget_set_parent(rect_widget_get_widget(checkbox_widget->checked_rect), NULL);
            world_despawn_widget(world, rect_widget_get_widget(checkbox_widget->checked_rect));

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
checkbox_widget_set_background_texture(
    te_checkbox_widget* checkbox_widget, const char* relative_path) {
    free(checkbox_widget->background_tex_relative_path);

    if (relative_path == NULL) {
        checkbox_widget->background_tex_relative_path = NULL;
    } else {
        const size_t path_len = strlen(relative_path);
        checkbox_widget->background_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
        memcpy(
            checkbox_widget->background_tex_relative_path, relative_path,
            sizeof(char) * path_len);
        checkbox_widget->background_tex_relative_path[path_len] = 0;
    }

    rect_widget_set_texture(
        checkbox_widget->background_rect, checkbox_widget->background_tex_relative_path);
}

void
checkbox_widget_set_checked_texture(
    te_checkbox_widget* checkbox_widget, const char* relative_path) {
    free(checkbox_widget->checked_tex_relative_path);

    if (relative_path == NULL) {
        checkbox_widget->checked_tex_relative_path = NULL;
    } else {
        const size_t path_len = strlen(relative_path);
        checkbox_widget->checked_tex_relative_path = malloc(sizeof(char) * (path_len + 1));
        memcpy(
            checkbox_widget->checked_tex_relative_path, relative_path,
            sizeof(char) * path_len);
        checkbox_widget->checked_tex_relative_path[path_len] = 0;
    }

    if (checkbox_widget->checked_rect != NULL) {
        rect_widget_set_texture(
            checkbox_widget->checked_rect, checkbox_widget->checked_tex_relative_path);
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

static inline void
prv_checkbox_widget_set_position(te_checkbox_widget* checkbox_widget, vec2 pos) {
    widget_set_relative_position(checkbox_widget->widget, pos);
}

static inline void
prv_checkbox_widget_get_position(te_checkbox_widget* checkbox_widget, vec2 out) {
    widget_get_relative_position(checkbox_widget->widget, out);
}

static inline void
prv_checkbox_widget_set_size(te_checkbox_widget* checkbox_widget, vec2 size) {
    widget_set_relative_size(checkbox_widget->widget, size);
}

static inline void
prv_checkbox_widget_get_size(te_checkbox_widget* checkbox_widget, vec2 out) {
    widget_get_relative_size(checkbox_widget->widget, out);
}

const char*
checkbox_widget_get_type_id(void) {
    return "checkbox_widget";
}

static te_widget*
prv_checkbox_widget_get_base(te_checkbox_widget* checkbox_widget) {
    return checkbox_widget->widget;
}

static void
widget_spawn(te_world* world, te_checkbox_widget* checkbox_widget) {
    world_spawn_widget(world, prv_checkbox_widget_get_base(checkbox_widget));
}

static void
widget_despawn(te_world* world, te_checkbox_widget* checkbox_widget) {
    world_despawn_widget(world, prv_checkbox_widget_get_base(checkbox_widget));
}

static void
set_name(te_checkbox_widget* widget, const char* name) {
    widget_set_name(widget->widget, name);
}

static const char*
get_name(te_checkbox_widget* widget) {
    return widget_get_name(widget->widget);
}

void
checkbox_widget_register_type(void) {
    te_type_info* info = type_info_create(
        checkbox_widget_get_type_id(), checkbox_widget_create, checkbox_widget_destroy,
        widget_spawn, widget_despawn, prv_checkbox_widget_get_base);
    type_info_add_vec2_variable(
        info, "position", prv_checkbox_widget_set_position, prv_checkbox_widget_get_position);
    type_info_add_vec2_variable(
        info, "size", prv_checkbox_widget_set_size, prv_checkbox_widget_get_size);
    type_info_add_bool_variable(
        info, "is_checked", checkbox_widget_set_is_checked, checkbox_widget_is_checked);
    type_info_add_vec4_variable(
        info, "background_color", checkbox_widget_set_background_color,
        checkbox_widget_get_background_color);
    type_info_add_vec4_variable(
        info, "checked_color", checkbox_widget_set_checked_color,
        checkbox_widget_get_checked_color);
    type_info_add_string_variable(
        info, "background_texture", checkbox_widget_set_background_texture,
        checkbox_widget_get_background_texture);
    type_info_add_string_variable(
        info, "checked_texture", checkbox_widget_set_checked_texture,
        checkbox_widget_get_checked_texture);
    type_info_add_string_variable(info, "name", set_name, get_name);

    type_database_register_type(info);
}

static void
prv_checkbox_fix_height(te_checkbox_widget* checkbox_widget) {
    te_world* world = widget_get_world(checkbox_widget->widget);
    if (world == NULL) {
        return;
    }

    // To avoid recursion from on size changed callback.
    checkbox_widget->is_fixing_height = true;

    // Recalculate height to be a square.
    te_window* window = game_manager_get_window(world_get_game_manager(world));

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);

    vec2 old_screen_size;
    widget_get_screen_size(checkbox_widget->widget, old_screen_size);

    const float screen_height = (float)window_height * old_screen_size[1];
    const float screen_width = (float)window_width * old_screen_size[0];
    const float target_pixel_count =
        screen_height < screen_width ? screen_height : screen_width;

    vec2 new_screen_size;
    glm_vec2_copy(
        (vec2){target_pixel_count / (float)window_width,
               target_pixel_count / (float)window_height},
        new_screen_size);

    vec2 multiplier;
    glm_vec2_div(new_screen_size, old_screen_size, multiplier);

    vec2 relative_size;
    widget_get_relative_size(checkbox_widget->widget, relative_size);

    glm_vec2_mul(relative_size, multiplier, relative_size);
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
        log_error("unexpected child widget count on a widget");
        abort();
    }

    rect_widget_set_color(checkbox_widget->background_rect, checkbox_widget->background_color);
    if (checkbox_widget->background_tex_relative_path != NULL) {
        rect_widget_set_texture(
            checkbox_widget->background_rect, checkbox_widget->background_tex_relative_path);
    }

    if (checkbox_widget->is_checked) {
        prv_checkbox_widget_create_checked_rect(checkbox_widget);
    }

    prv_checkbox_fix_height(checkbox_widget);

    te_world* world = widget_get_world(checkbox_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
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
        log_error("unexpected child widget count on a widget");
        abort();
    }

    te_world* world = widget_get_world(checkbox_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }
    prv_world_remove_interactable_widget(world, checkbox_widget->widget);
}

static void
prv_checkbox_widget_on_mouse_button_released(
    void* this, enum te_mouse_button button, vec2 cursor_pos) {
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
