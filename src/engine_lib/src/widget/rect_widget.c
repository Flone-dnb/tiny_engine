#include "widget/rect_widget.h"

#include <string.h>
#include "game_manager.h"
#include "misc/error.h"
#include "render/renderer.h"
#include "render/texture_manager.h"
#include "render/widget_renderer.h"
#include "widget/widget.h"
#include "window.h"
#include "world.h"
#include "type_database.h"

#define INVALID_RENDER_DATA_HANDLE  0xffffffff
#define RECT_WIDGET_TEX_LOAD_OPTION TE_TLO_NO_MIPMAPS

struct te_rect_widget {
    te_widget* widget;

    // NULL if texture is not set, otherwise path (relative to the `res` directory) to the texture file.
    char* tex_relative_path;

    // Allows "cutting" part of the rectangle during the rendering.
    // XY stores clip start in range [0.0; 1.0] and ZW stores clip size in the same range.
    vec4 clip_rect;

    vec4 color;

    // Stores invalid value if not being rendered.
    unsigned int render_data_handle;

    // `true` if entered the "destroy" function.
    bool is_rect_widget_destroy;
};

// Widget callbacks:
static void prv_rect_widget_on_pos_changed(void* this);
static void prv_rect_widget_on_size_changed(void* this);
static void prv_rect_widget_on_after_spawned(void* this);
static void prv_rect_widget_on_before_despawned(void* this);
static void prv_rect_widget_on_before_base_destroyed(void* this);
static void prv_rect_widget_on_window_size_changed(void* this);

static void prv_rect_widget_register_for_rendering(te_rect_widget* rect_widget);
static void prv_rect_widget_unregister_from_rendering(te_rect_widget* rect_widget);
static void prv_rect_widget_update_non_tex_render_data(te_rect_widget* rect_widget);

te_rect_widget*
rect_widget_create(void) {
    te_rect_widget* rect_widget = malloc(sizeof(te_rect_widget));

    rect_widget->widget = widget_create(
        rect_widget, prv_rect_widget_on_pos_changed, prv_rect_widget_on_size_changed, prv_rect_widget_on_before_base_destroyed,
        NULL, NULL, prv_rect_widget_on_after_spawned, prv_rect_widget_on_before_despawned,
        prv_rect_widget_on_window_size_changed);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, rect_widget->color);
    rect_widget->tex_relative_path = NULL;
    rect_widget->render_data_handle = INVALID_RENDER_DATA_HANDLE;
    rect_widget->is_rect_widget_destroy = false;

    glm_vec4_copy((vec4){0.0f, 0.0f, 1.0f, 1.0f}, rect_widget->clip_rect);

    return rect_widget;
}

void
rect_widget_destroy(te_rect_widget* rect_widget) {
    rect_widget->is_rect_widget_destroy = true;

    if (rect_widget->widget != NULL) { // may be null if we got here from base destroy
        if (rect_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
            prv_rect_widget_unregister_from_rendering(rect_widget);
        }
        widget_destroy(rect_widget->widget);
    }

    free(rect_widget->tex_relative_path);
    free(rect_widget);
}

static void
prv_rect_widget_on_before_base_destroyed(void* this) {
    te_rect_widget* rect_widget = this;
    if (rect_widget->is_rect_widget_destroy) {
        return;
    }

    if (rect_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        prv_rect_widget_unregister_from_rendering(rect_widget);
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    rect_widget->widget = NULL;
    rect_widget_destroy(rect_widget);
}

te_widget*
rect_widget_get_widget(te_rect_widget* rect_widget) {
    return rect_widget->widget;
}

void
rect_widget_set_color(te_rect_widget* rect_widget, vec4 color) {
    glm_vec4_copy(color, rect_widget->color);

    if (rect_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        te_world* world = widget_get_world(rect_widget->widget);
        if (world == NULL) {
            show_error_and_abort("expected the widget to be spawned");
        }

        te_rect_widget_render_data* data =
            widget_renderer_get_rect_widget_render_data_tmp(world_get_widget_renderer(world), rect_widget->render_data_handle);
        glm_vec4_copy(rect_widget->color, data->color);
    }
}

void
rect_widget_get_color(te_rect_widget* rect_widget, vec4 out) {
    glm_vec4_copy(rect_widget->color, out);
}

void
rect_widget_set_texture(te_rect_widget* rect_widget, const char* relative_path) {
    if (rect_widget->tex_relative_path == NULL && relative_path == NULL) {
        return;
    }

    free(rect_widget->tex_relative_path);

    if (relative_path == NULL) {
        // Remove current texture.
        rect_widget->tex_relative_path = NULL;
        if (rect_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
            te_world* world = widget_get_world(rect_widget->widget);
            if (world == NULL) {
                show_error_and_abort("expected the widget to be spawned");
            }

            te_rect_widget_render_data* data = widget_renderer_get_rect_widget_render_data_tmp(
                world_get_widget_renderer(world), rect_widget->render_data_handle);

            if (data->tex_id > 0) {
                // Mark texture as unused.
                te_texture_manager* texture_manager =
                    renderer_get_texture_manager(game_manager_get_renderer(world_get_game_manager(world)));
                texture_manager_mark_unused_texture(texture_manager, data->tex_id);
            }

            data->tex_id = 0;
        }
    } else {
        // Set new texture.
        const size_t len = strlen(relative_path);
        rect_widget->tex_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(rect_widget->tex_relative_path, relative_path, sizeof(char) * len);
        rect_widget->tex_relative_path[len] = 0;

        if (rect_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
            te_world* world = widget_get_world(rect_widget->widget);
            if (world == NULL) {
                show_error_and_abort("expected the widget to be spawned");
            }

            te_rect_widget_render_data* data = widget_renderer_get_rect_widget_render_data_tmp(
                world_get_widget_renderer(world), rect_widget->render_data_handle);

            te_texture_manager* texture_manager =
                renderer_get_texture_manager(game_manager_get_renderer(world_get_game_manager(world)));
            if (data->tex_id > 0) {
                texture_manager_mark_unused_texture(texture_manager, data->tex_id);
            }

            // Load new texture.
            data->tex_id = texture_manager_request_texture(texture_manager, relative_path, RECT_WIDGET_TEX_LOAD_OPTION);
        }
    }
}

const char*
rect_widget_get_texture(te_rect_widget* rect_widget) {
    return rect_widget->tex_relative_path;
}

void
rect_widget_set_clip_rect(te_rect_widget* rect_widget, vec4 clip_rect) {
    glm_vec4_copy(clip_rect, rect_widget->clip_rect);

    if (rect_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        te_world* world = widget_get_world(rect_widget->widget);
        if (world == NULL) {
            show_error_and_abort("expected the widget to be spawned");
        }

        te_rect_widget_render_data* data =
            widget_renderer_get_rect_widget_render_data_tmp(world_get_widget_renderer(world), rect_widget->render_data_handle);
        glm_vec4_copy(clip_rect, data->clip_rect);
    }
}

void
rect_widget_get_clip_rect(te_rect_widget* rect_widget, vec4 out) {
    glm_vec4_copy(rect_widget->clip_rect, out);
}

static void
prv_rect_widget_register_for_rendering(te_rect_widget* rect_widget) {
#if defined(DEBUG)
    if (rect_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        show_error_and_abort("expected the render data handle to be invalid");
    }
#endif

    te_world* world = widget_get_world(rect_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    te_game_manager* game_manager = world_get_game_manager(world);
    te_widget_renderer* widget_renderer = world_get_widget_renderer(world);

    rect_widget->render_data_handle = widget_renderer_add_rect_widget(widget_renderer);

    te_rect_widget_render_data* data =
        widget_renderer_get_rect_widget_render_data_tmp(widget_renderer, rect_widget->render_data_handle);

    // Setup texture.
    data->tex_id = 0;
    if (rect_widget->tex_relative_path != NULL) {
        te_texture_manager* texture_manager = renderer_get_texture_manager(game_manager_get_renderer(game_manager));
        data->tex_id =
            texture_manager_request_texture(texture_manager, rect_widget->tex_relative_path, RECT_WIDGET_TEX_LOAD_OPTION);
    }

    prv_rect_widget_update_non_tex_render_data(rect_widget);
}

static void
prv_rect_widget_unregister_from_rendering(te_rect_widget* rect_widget) {
#if defined(DEBUG)
    if (rect_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        show_error_and_abort("expected the render data handle to be valid");
    }
#endif

    te_world* world = widget_get_world(rect_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    te_widget_renderer* widget_renderer = world_get_widget_renderer(world);

    // Cleanup data.
    te_rect_widget_render_data* data =
        widget_renderer_get_rect_widget_render_data_tmp(widget_renderer, rect_widget->render_data_handle);
    if (data->tex_id > 0) {
        te_texture_manager* texture_manager =
            renderer_get_texture_manager(game_manager_get_renderer(world_get_game_manager(world)));
        texture_manager_mark_unused_texture(texture_manager, data->tex_id);
    }

    widget_renderer_remove_rect_widget(widget_renderer, rect_widget->render_data_handle);
    rect_widget->render_data_handle = INVALID_RENDER_DATA_HANDLE;
}

static void
prv_rect_widget_update_non_tex_render_data(te_rect_widget* rect_widget) {
#if defined(DEBUG)
    if (rect_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        show_error_and_abort("expected the render data handle to be valid");
    }
#endif

    te_world* world = widget_get_world(rect_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    te_game_manager* game_manager = world_get_game_manager(world);

    te_rect_widget_render_data* data =
        widget_renderer_get_rect_widget_render_data_tmp(world_get_widget_renderer(world), rect_widget->render_data_handle);

    glm_vec4_copy(rect_widget->color, data->color);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(game_manager_get_window(game_manager), &window_width, &window_height);

    widget_get_screen_position(rect_widget->widget, data->pos_pix);
    widget_get_screen_size(rect_widget->widget, data->size_pix);
    glm_vec2_mul(data->pos_pix, (vec2){(float)window_width, (float)window_height}, data->pos_pix);
    glm_vec2_mul(data->size_pix, (vec2){(float)window_width, (float)window_height}, data->size_pix);

    glm_vec4_copy(rect_widget->clip_rect, data->clip_rect);
}

static void
prv_rect_widget_on_pos_changed(void* this) {
    te_rect_widget* rect_widget = this;

    if (rect_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        return;
    }

    prv_rect_widget_update_non_tex_render_data(rect_widget);
}

static void
prv_rect_widget_on_size_changed(void* this) {
    te_rect_widget* rect_widget = this;

    if (rect_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        return;
    }

    prv_rect_widget_update_non_tex_render_data(rect_widget);
}

static void
prv_rect_widget_on_after_spawned(void* this) {
    te_rect_widget* rect_widget = this;
    prv_rect_widget_register_for_rendering(rect_widget);
}

static void
prv_rect_widget_on_before_despawned(void* this) {
    te_rect_widget* rect_widget = this;
    prv_rect_widget_unregister_from_rendering(rect_widget);
}

static void
prv_rect_widget_on_window_size_changed(void* this) {
    te_rect_widget* rect_widget = this;

    if (rect_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        return;
    }

    prv_rect_widget_update_non_tex_render_data(rect_widget);
}

static inline void
prv_rect_widget_set_position(te_rect_widget* rect_widget, vec2 pos) {
    widget_set_relative_position(rect_widget->widget, pos);
}

static inline void
prv_rect_widget_get_position(te_rect_widget* rect_widget, vec2 out) {
    widget_get_relative_position(rect_widget->widget, out);
}

static inline void
prv_rect_widget_set_size(te_rect_widget* rect_widget, vec2 size) {
    widget_set_relative_size(rect_widget->widget, size);
}

static inline void
prv_rect_widget_get_size(te_rect_widget* rect_widget, vec2 out) {
    widget_get_relative_size(rect_widget->widget, out);
}

const char*
rect_widget_get_type_id(void) {
    return "rect_widget";
}

void
rect_widget_register_type(void) {
    te_type_info* info = type_info_create(rect_widget_get_type_id());
    type_info_add_vec2_variable(info, "position", prv_rect_widget_set_position, prv_rect_widget_get_position);
    type_info_add_vec2_variable(info, "size", prv_rect_widget_set_size, prv_rect_widget_get_size);
    type_info_add_vec4_variable(info, "color", rect_widget_set_color, rect_widget_get_color);
    type_info_add_string_variable(info, "texture", rect_widget_set_texture, rect_widget_get_texture);

    type_database_register_type(info);
}