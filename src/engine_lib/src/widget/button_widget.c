#include <widget/button_widget.h>

#include <string.h>
#include <game_manager.h>
#include <io/log.h>
#include <render/renderer.h>
#include <render/texture_manager.h>
#include <type_database.h>
#include <widget/rect_widget.h>
#include <widget/widget.h>
#include <world.h>

#define BUTTON_WIDGET_TEX_LOAD_OPTION TE_TLO_NO_MIPMAPS

struct te_button_widget {
    vec4 color;
    vec4 color_hovered;
    vec4 color_pressed;

    te_widget* widget;

    // Do not free/destroy. Child widget.
    te_rect_widget* rect_widget;

    // NULL if not set. Must be freed.
    char* tex_relative_path;
    char* tex_hovered_relative_path;
    char* tex_pressed_relative_path;

    // May be NULL if not set.
    void (*on_clicked)(te_button_widget*);
    void (*on_right_clicked)(te_button_widget*);

    // Cached textures.
    unsigned int tex_id;
    unsigned int tex_hovered_id;
    unsigned int tex_pressed_id;

    // `true` if entered the "destroy" function.
    bool is_button_widget_destroy;
    bool is_cursor_inside_widget;
};

// Widget callbacks:
static void prv_button_widget_on_pos_changed(void* this);
static void prv_button_widget_on_size_changed(void* this);
static void prv_button_widget_on_before_base_destroyed(void* this);
static void prv_button_widget_on_after_spawned(void* this);
static void prv_button_widget_on_before_despawned(void* this);

// Interactable callbacks:
static void prv_button_widget_on_mouse_button_pressed(
    void* this, enum te_mouse_button button, vec2 cursor_pos);
static void prv_button_widget_on_mouse_button_released(
    void* this, enum te_mouse_button button, vec2 cursor_pos);
static void prv_button_widget_on_cursor_entered(void* this, vec2 cursor_pos);
static void prv_button_widget_on_cursor_left(void* this, vec2 cursor_pos);

static void prv_button_widget_register_render_data(te_button_widget* button_widget);
static void prv_button_widget_unregister_render_data(te_button_widget* button_widget);

te_button_widget*
button_widget_create(void) {
    te_button_widget* button_widget = malloc(sizeof(te_button_widget));

    button_widget->widget = widget_create(
        button_widget, button_widget_get_type_id, prv_button_widget_on_pos_changed,
        prv_button_widget_on_size_changed, prv_button_widget_on_before_base_destroyed, NULL,
        NULL, prv_button_widget_on_after_spawned, prv_button_widget_on_before_despawned, NULL);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, button_widget->color);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, button_widget->color_hovered);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, button_widget->color_pressed);

    button_widget->on_clicked = NULL;
    button_widget->on_right_clicked = NULL;

    button_widget->tex_relative_path = NULL;
    button_widget->tex_hovered_relative_path = NULL;
    button_widget->tex_pressed_relative_path = NULL;

    button_widget->tex_id = 0;
    button_widget->tex_hovered_id = 0;
    button_widget->tex_pressed_id = 0;

    button_widget->is_cursor_inside_widget = false;
    button_widget->is_button_widget_destroy = false;

    button_widget->rect_widget = rect_widget_create();
    widget_set_parent(
        rect_widget_get_widget(button_widget->rect_widget), button_widget->widget);
    widget_set_is_serialization_allowed(
        rect_widget_get_widget(button_widget->rect_widget), false);

    widget_set_relative_position(
        rect_widget_get_widget(button_widget->rect_widget), (vec2){0.0f, 0.0f});
    widget_set_relative_size(
        rect_widget_get_widget(button_widget->rect_widget), (vec2){1.0f, 1.0f});

    prv_widget_set_input_callbacks(
        button_widget->widget, prv_button_widget_on_cursor_entered,
        prv_button_widget_on_cursor_left, prv_button_widget_on_mouse_button_pressed,
        prv_button_widget_on_mouse_button_released, NULL, NULL, NULL);

    return button_widget;
}

void
button_widget_destroy(te_button_widget* button_widget) {
    button_widget->is_button_widget_destroy = true;

    if (button_widget->widget != NULL) { // may be null if we got here from base destroy
        widget_destroy(button_widget->widget);
    }

    free(button_widget->tex_relative_path);
    free(button_widget->tex_hovered_relative_path);
    free(button_widget->tex_pressed_relative_path);

    free(button_widget);
}

static void
prv_button_widget_on_before_base_destroyed(void* this) {
    te_button_widget* button_widget = this;
    if (button_widget->is_button_widget_destroy) {
        return;
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    button_widget->widget = NULL;
    button_widget_destroy(button_widget);
}

static void
prv_button_widget_on_after_spawned(void* this) {
    te_button_widget* button_widget = this;
    prv_button_widget_register_render_data(button_widget);

    button_widget->is_cursor_inside_widget = false;
    rect_widget_set_color(button_widget->rect_widget, button_widget->color);
    if (button_widget->tex_relative_path != NULL) {
        rect_widget_set_texture(button_widget->rect_widget, button_widget->tex_relative_path);
    }
}

static void
prv_button_widget_on_before_despawned(void* this) {
    te_button_widget* button_widget = this;
    prv_button_widget_unregister_render_data(button_widget);
}

static void
prv_button_widget_register_render_data(te_button_widget* button_widget) {
    te_world* world = widget_get_world(button_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }

    te_texture_manager* texture_manager =
        renderer_get_texture_manager(game_manager_get_renderer(world_get_game_manager(world)));
    if (button_widget->tex_relative_path != NULL) {
        button_widget->tex_id = texture_manager_request_texture(
            texture_manager, button_widget->tex_relative_path, BUTTON_WIDGET_TEX_LOAD_OPTION);
    }
    if (button_widget->tex_hovered_relative_path != NULL) {
        button_widget->tex_hovered_id = texture_manager_request_texture(
            texture_manager, button_widget->tex_hovered_relative_path,
            BUTTON_WIDGET_TEX_LOAD_OPTION);
    }
    if (button_widget->tex_pressed_relative_path != NULL) {
        button_widget->tex_pressed_id = texture_manager_request_texture(
            texture_manager, button_widget->tex_pressed_relative_path,
            BUTTON_WIDGET_TEX_LOAD_OPTION);
    }

    prv_world_add_interactable_widget(world, button_widget->widget);
}

static void
prv_button_widget_unregister_render_data(te_button_widget* button_widget) {
    te_world* world = widget_get_world(button_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }

    te_texture_manager* texture_manager =
        renderer_get_texture_manager(game_manager_get_renderer(world_get_game_manager(world)));
    if (button_widget->tex_id > 0) {
        texture_manager_mark_unused_texture(texture_manager, button_widget->tex_id);
        button_widget->tex_id = 0;
    }
    if (button_widget->tex_hovered_id > 0) {
        texture_manager_mark_unused_texture(texture_manager, button_widget->tex_hovered_id);
        button_widget->tex_hovered_id = 0;
    }
    if (button_widget->tex_pressed_id > 0) {
        texture_manager_mark_unused_texture(texture_manager, button_widget->tex_pressed_id);
        button_widget->tex_pressed_id = 0;
    }

    prv_world_remove_interactable_widget(world, button_widget->widget);
}

te_widget*
button_widget_get_widget(te_button_widget* button_widget) {
    return button_widget->widget;
}

static inline void
prv_button_widget_set_position(te_button_widget* button_widget, vec2 pos) {
    widget_set_relative_position(button_widget->widget, pos);
}

static inline void
prv_button_widget_get_position(te_button_widget* button_widget, vec2 out) {
    widget_get_relative_position(button_widget->widget, out);
}

static inline void
prv_button_widget_set_size(te_button_widget* button_widget, vec2 size) {
    widget_set_relative_size(button_widget->widget, size);
}

static inline void
prv_button_widget_get_size(te_button_widget* button_widget, vec2 out) {
    widget_get_relative_size(button_widget->widget, out);
}

const char*
button_widget_get_type_id(void) {
    return "button_widget";
}

static te_widget*
prv_button_widget_get_base(te_button_widget* button_widget) {
    return button_widget->widget;
}

static void
widget_spawn(te_world* world, te_button_widget* button_widget) {
    world_spawn_widget(world, prv_button_widget_get_base(button_widget));
}

static void
widget_despawn(te_world* world, te_button_widget* button_widget) {
    if (widget_get_parent(button_widget->widget) != NULL) {
        widget_set_parent(button_widget->widget, NULL);
    }

    world_despawn_widget(world, button_widget->widget);
}

static void
set_name(te_button_widget* widget, const char* name) {
    widget_set_name(widget->widget, name);
}

static const char*
get_name(te_button_widget* widget) {
    return widget_get_name(widget->widget);
}

static bool is_serialization_allowed(te_button_widget* widget) {
    return widget_is_serialization_allowed(widget->widget);
}

void
button_widget_register_type(void) {
    te_type_info* info = type_info_create(
        button_widget_get_type_id(), button_widget_create, button_widget_destroy, widget_spawn,
        widget_despawn, prv_button_widget_get_base, is_serialization_allowed);
    type_info_add_vec2_variable(
        info, "position", prv_button_widget_set_position, prv_button_widget_get_position);
    type_info_add_vec2_variable(
        info, "size", prv_button_widget_set_size, prv_button_widget_get_size);
    type_info_add_vec4_variable(
        info, "color", button_widget_set_color, button_widget_get_color);
    type_info_add_vec4_variable(
        info, "color_hovered", button_widget_set_color_hovered,
        button_widget_get_color_hovered);
    type_info_add_vec4_variable(
        info, "color_pressed", button_widget_set_color_pressed,
        button_widget_get_color_pressed);
    type_info_add_string_variable(
        info, "texture", button_widget_set_texture, button_widget_get_texture);
    type_info_add_string_variable(
        info, "texture_hovered", button_widget_set_texture_hovered,
        button_widget_get_texture_hovered);
    type_info_add_string_variable(
        info, "texture_pressed", button_widget_set_texture_pressed,
        button_widget_get_texture_pressed);
    type_info_add_string_variable(info, "name", set_name, get_name);

    type_database_register_type(info);
}

void
button_widget_set_on_clicked(
    te_button_widget* button_widget, void (*on_clicked)(te_button_widget*)) {
    button_widget->on_clicked = on_clicked;
}

void
button_widget_set_on_right_clicked(
    te_button_widget* button_widget, void (*on_right_clicked)(te_button_widget*)) {
    button_widget->on_right_clicked = on_right_clicked;
}

void
button_widget_set_color(te_button_widget* button_widget, vec4 color) {
    glm_vec4_copy(color, button_widget->color);

    if (widget_get_world(button_widget->widget) != NULL
        && !button_widget->is_cursor_inside_widget) {
        rect_widget_set_color(button_widget->rect_widget, color);
    }
}

void
button_widget_set_color_hovered(te_button_widget* button_widget, vec4 color) {
    glm_vec4_copy(color, button_widget->color_hovered);

    if (widget_get_world(button_widget->widget) != NULL
        && button_widget->is_cursor_inside_widget) {
        rect_widget_set_color(button_widget->rect_widget, color);
    }
}

void
button_widget_set_color_pressed(te_button_widget* button_widget, vec4 color) {
    glm_vec4_copy(color, button_widget->color_pressed);
}

void
button_widget_get_color(te_button_widget* button_widget, vec4 out) {
    glm_vec4_copy(button_widget->color, out);
}

void
button_widget_get_color_hovered(te_button_widget* button_widget, vec4 out) {
    glm_vec4_copy(button_widget->color_hovered, out);
}

void
button_widget_get_color_pressed(te_button_widget* button_widget, vec4 out) {
    glm_vec4_copy(button_widget->color_pressed, out);
}

void
button_widget_set_texture(te_button_widget* button_widget, const char* relative_path) {
    free(button_widget->tex_relative_path);
    button_widget->tex_relative_path = NULL;

    if (relative_path != NULL) {
        const size_t len = strlen(relative_path);
        button_widget->tex_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(button_widget->tex_relative_path, relative_path, sizeof(char) * len);
        button_widget->tex_relative_path[len] = 0;
    }

    te_world* world = widget_get_world(button_widget->widget);
    if (world != NULL) {
        te_texture_manager* texture_manager = renderer_get_texture_manager(
            game_manager_get_renderer(world_get_game_manager(world)));
        if (button_widget->tex_id > 0) {
            texture_manager_mark_unused_texture(texture_manager, button_widget->tex_id);
            button_widget->tex_id = 0;
        }

        if (relative_path != NULL) {
            button_widget->tex_id = texture_manager_request_texture(
                texture_manager, relative_path, BUTTON_WIDGET_TEX_LOAD_OPTION);
        }

        if (!button_widget->is_cursor_inside_widget) {
            rect_widget_set_texture(button_widget->rect_widget, relative_path);
        }
    }
}

void
button_widget_set_texture_hovered(te_button_widget* button_widget, const char* relative_path) {
    free(button_widget->tex_hovered_relative_path);
    button_widget->tex_hovered_relative_path = NULL;

    if (relative_path != NULL) {
        const size_t len = strlen(relative_path);
        button_widget->tex_hovered_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(button_widget->tex_hovered_relative_path, relative_path, sizeof(char) * len);
        button_widget->tex_hovered_relative_path[len] = 0;
    }

    te_world* world = widget_get_world(button_widget->widget);
    if (world != NULL) {
        te_texture_manager* texture_manager = renderer_get_texture_manager(
            game_manager_get_renderer(world_get_game_manager(world)));
        if (button_widget->tex_hovered_id > 0) {
            texture_manager_mark_unused_texture(
                texture_manager, button_widget->tex_hovered_id);
            button_widget->tex_hovered_id = 0;
        }

        if (relative_path != NULL) {
            button_widget->tex_hovered_id = texture_manager_request_texture(
                texture_manager, relative_path, BUTTON_WIDGET_TEX_LOAD_OPTION);
        }

        if (button_widget->is_cursor_inside_widget) {
            rect_widget_set_texture(button_widget->rect_widget, relative_path);
        }
    }
}

void
button_widget_set_texture_pressed(te_button_widget* button_widget, const char* relative_path) {
    free(button_widget->tex_pressed_relative_path);
    button_widget->tex_pressed_relative_path = NULL;

    if (relative_path != NULL) {
        const size_t len = strlen(relative_path);
        button_widget->tex_pressed_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(button_widget->tex_pressed_relative_path, relative_path, sizeof(char) * len);
        button_widget->tex_pressed_relative_path[len] = 0;
    }

    te_world* world = widget_get_world(button_widget->widget);
    if (world != NULL) {
        te_texture_manager* texture_manager = renderer_get_texture_manager(
            game_manager_get_renderer(world_get_game_manager(world)));
        if (button_widget->tex_pressed_id > 0) {
            texture_manager_mark_unused_texture(
                texture_manager, button_widget->tex_pressed_id);
            button_widget->tex_pressed_id = 0;
        }

        if (relative_path != NULL) {
            button_widget->tex_pressed_id = texture_manager_request_texture(
                texture_manager, relative_path, BUTTON_WIDGET_TEX_LOAD_OPTION);
        }
    }
}

char*
button_widget_get_texture(te_button_widget* button_widget) {
    return button_widget->tex_relative_path;
}

char*
button_widget_get_texture_hovered(te_button_widget* button_widget) {
    return button_widget->tex_hovered_relative_path;
}

char*
button_widget_get_texture_pressed(te_button_widget* button_widget) {
    return button_widget->tex_pressed_relative_path;
}

static void
prv_button_widget_on_pos_changed(void* this) {
    te_button_widget* button_widget = this;
    te_world* world = widget_get_world(button_widget->widget);
    if (world != NULL) {
        prv_world_interactable_widget_pos_size_changed(world);
    }
}

static void
prv_button_widget_on_size_changed(void* this) {
    te_button_widget* button_widget = this;
    te_world* world = widget_get_world(button_widget->widget);
    if (world != NULL) {
        prv_world_interactable_widget_pos_size_changed(world);
    }
}

void
button_widget_enter_normal_state(te_button_widget* button_widget) {
    rect_widget_set_color(button_widget->rect_widget, button_widget->color);
    if (button_widget->tex_relative_path != NULL) {
        rect_widget_set_texture(button_widget->rect_widget, button_widget->tex_relative_path);
    }
}

void
button_widget_enter_hovered_state(te_button_widget* button_widget) {
    rect_widget_set_color(button_widget->rect_widget, button_widget->color_hovered);
    if (button_widget->tex_hovered_relative_path != NULL) {
        rect_widget_set_texture(
            button_widget->rect_widget, button_widget->tex_hovered_relative_path);
    }
}

void
button_widget_enter_pressed_state(te_button_widget* button_widget) {
    rect_widget_set_color(button_widget->rect_widget, button_widget->color_pressed);
    if (button_widget->tex_pressed_relative_path != NULL) {
        rect_widget_set_texture(
            button_widget->rect_widget, button_widget->tex_pressed_relative_path);
    }
}

static void
prv_button_widget_on_mouse_button_pressed(
    void* this, enum te_mouse_button button, vec2 cursor_pos) {
    (void)cursor_pos;
    te_button_widget* button_widget = this;

    if ((button_widget->on_clicked != NULL && button == TE_MB_LEFT)
        || (button_widget->on_right_clicked != NULL && button == TE_MB_RIGHT)) {
        button_widget_enter_pressed_state(this);
    }
}

static void
prv_button_widget_on_mouse_button_released(
    void* this, enum te_mouse_button button, vec2 cursor_pos) {
    (void)cursor_pos;
    te_button_widget* button_widget = this;

    if ((button_widget->on_clicked != NULL && button == TE_MB_LEFT)
        || (button_widget->on_right_clicked != NULL && button == TE_MB_RIGHT)) {
        if (button_widget->is_cursor_inside_widget) {
            button_widget_enter_hovered_state(button_widget);
        } else {
            button_widget_enter_normal_state(button_widget);
        }

        if (button == TE_MB_LEFT && button_widget->on_clicked != NULL) {
            button_widget->on_clicked(button_widget);
        } else if (button == TE_MB_RIGHT && button_widget->on_right_clicked != NULL) {
            button_widget->on_right_clicked(button_widget);
        }
    }
}

static void
prv_button_widget_on_cursor_entered(void* this, vec2 cursor_pos) {
    (void)cursor_pos;
    te_button_widget* button_widget = this;

    button_widget_enter_hovered_state(button_widget);
    button_widget->is_cursor_inside_widget = true;
}

static void
prv_button_widget_on_cursor_left(void* this, vec2 cursor_pos) {
    (void)cursor_pos;
    te_button_widget* button_widget = this;

    button_widget_enter_normal_state(button_widget);
    button_widget->is_cursor_inside_widget = false;
}
