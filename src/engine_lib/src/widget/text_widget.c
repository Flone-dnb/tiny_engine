#include <widget/text_widget.h>

#include <string.h>
#include <game_manager.h>
#include <io/log.h>
#include <misc/wchar_funcs.h>
#include <render/font_manager.h>
#include <render/renderer.h>
#include <render/widget_renderer.h>
#include <type_database.h>
#include <widget/widget.h>
#include <window.h>
#include <world.h>

#define INVALID_RENDER_DATA_HANDLE 0xffffffff

struct te_text_widget {
    te_widget* widget;

    // Always non-NULL.
    wchar_t* text;

    // RGBA color of the text.
    vec4 color;

    // strlen of @ref text.
    unsigned int text_len;

    // Height of the text in range [0.0; 1.0] relative to window height.
    float text_height;

    // Vertical space between lines of text, in range [0.0f; +inf] relative to the height of the text.
    float line_spacing;

    // Stores invalid value if not being rendered.
    unsigned int render_data_handle;

    // `true` if entered the "destroy" function.
    bool is_text_widget_destroy;

    bool is_multiline;
};

// Widget callbacks:
static void prv_text_widget_on_pos_changed(void* this);
static void prv_text_widget_on_size_changed(void* this);
static void prv_text_widget_on_after_spawned(void* this);
static void prv_text_widget_on_before_despawned(void* this);
static void prv_text_widget_on_before_base_destroyed(void* this);
static void prv_text_widget_on_window_size_changed(void* this);

static void prv_text_widget_register_for_rendering(te_text_widget* text_widget);
static void prv_text_widget_unregister_from_rendering(te_text_widget* text_widget);
static void prv_text_widget_update_all_render_data(te_text_widget* text_widget);

te_text_widget*
text_widget_create(void) {
    te_text_widget* text_widget = malloc(sizeof(te_text_widget));

    text_widget->text_height = 0.03f;
    text_widget->line_spacing = 0.1f;
    text_widget->is_multiline = false;
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, text_widget->color);

    text_widget->widget = widget_create(
        text_widget, text_widget_get_type_id, prv_text_widget_on_pos_changed,
        prv_text_widget_on_size_changed, prv_text_widget_on_before_base_destroyed, NULL, NULL,
        prv_text_widget_on_after_spawned, prv_text_widget_on_before_despawned,
        prv_text_widget_on_window_size_changed);
    text_widget->is_text_widget_destroy = false;
    text_widget->render_data_handle = INVALID_RENDER_DATA_HANDLE;

    // Setup some placeholder text.
    text_widget->text = wchar_from_char("hello", &text_widget->text_len);
    text_widget->text[text_widget->text_len] = 0;

    return text_widget;
}

void
text_widget_destroy(te_text_widget* text_widget) {
    text_widget->is_text_widget_destroy = true;

    if (text_widget->widget != NULL) { // may be null if we got here from base destroy
        widget_destroy(text_widget->widget);
    }

    free(text_widget->text);
    free(text_widget);
}

static void
prv_text_widget_on_before_base_destroyed(void* this) {
    te_text_widget* text_widget = this;
    if (text_widget->is_text_widget_destroy) {
        return;
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    text_widget->widget = NULL;
    text_widget_destroy(text_widget);
}

te_widget*
text_widget_get_widget(te_text_widget* text_widget) {
    return text_widget->widget;
}

void
text_widget_set_text_own(te_text_widget* text_widget, wchar_t* text, unsigned int strlen) {
    if (text == NULL) {
        log_error("text pointer must not be NULL");
        abort();
    }

    free(text_widget->text);

    text_widget->text = text;
    text_widget->text_len = strlen;

    if (text_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        prv_text_widget_update_all_render_data(text_widget);
    }
}

void
text_widget_set_text(te_text_widget* text_widget, const wchar_t* text) {
    if (text == NULL) {
        log_error("text pointer must not be NULL");
        abort();
    }

    free(text_widget->text);

    const unsigned int text_len = (unsigned int)wcslen(text);
    text_widget->text = malloc(sizeof(wchar_t) * (text_len + 1));
    memcpy(text_widget->text, text, sizeof(wchar_t) * text_len);
    text_widget->text[text_len] = 0;
#if defined(DEBUG)
    if (text_len > 0xffffffff) {
        log_error("text too long");
        abort();
    }
#endif
    text_widget->text_len = (unsigned int)text_len;

    if (text_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        prv_text_widget_update_all_render_data(text_widget);
    }
}

wchar_t*
text_widget_get_text(te_text_widget* text_widget, unsigned int* text_len) {
    (*text_len) = text_widget->text_len;
    return text_widget->text;
}

void
text_widget_set_color(te_text_widget* text_widget, vec4 color) {
    glm_vec4_copy(color, text_widget->color);

    if (text_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        return;
    }

    te_world* world = widget_get_world(text_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }

    te_text_widget_render_data* data = widget_renderer_get_text_widget_render_data_tmp(
        world_get_widget_renderer(world), text_widget->render_data_handle);

    glm_vec4_copy(text_widget->color, data->color);
}

void
text_widget_get_color(te_text_widget* text_widget, vec4 out) {
    glm_vec4_copy(text_widget->color, out);
}

void
text_widget_set_text_height(te_text_widget* text_widget, float height) {
    text_widget->text_height = glm_clamp(height, 0.001f, 1.0f);

    if (text_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        prv_text_widget_update_all_render_data(text_widget);
    }
}

void
text_widget_set_is_multiline(te_text_widget* text_widget, bool is_multiline) {
    if (text_widget->is_multiline == is_multiline) {
        return;
    }

    text_widget->is_multiline = is_multiline;

    if (text_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        prv_text_widget_update_all_render_data(text_widget);
    }
}

bool
text_widget_is_multiline(te_text_widget* text_widget) {
    return text_widget->is_multiline;
}

unsigned int
prv_text_widget_get_render_data_handle(te_text_widget* text_widget) {
    return text_widget->render_data_handle;
}

float
text_widget_get_text_height(te_text_widget* text_widget) {
    return text_widget->text_height;
}

void
text_widget_set_line_spacing(te_text_widget* text_widget, float spacing) {
    text_widget->line_spacing = spacing;

    if (text_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        prv_text_widget_update_all_render_data(text_widget);
    }
}

float
text_widget_get_line_spacing(te_text_widget* text_widget) {
    return text_widget->line_spacing;
}

static void
prv_text_widget_register_for_rendering(te_text_widget* text_widget) {
#if defined(DEBUG)
    if (text_widget->render_data_handle != INVALID_RENDER_DATA_HANDLE) {
        log_error("expected the render data handle to be invalid");
        abort();
    }
#endif

    te_world* world = widget_get_world(text_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }

    text_widget->render_data_handle =
        widget_renderer_add_text_widget(world_get_widget_renderer(world));
    prv_text_widget_update_all_render_data(text_widget);
}

static void
prv_text_widget_unregister_from_rendering(te_text_widget* text_widget) {
#if defined(DEBUG)
    if (text_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        log_error("expected the render data handle to be valid");
        abort();
    }
#endif

    te_world* world = widget_get_world(text_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }
    te_widget_renderer* widget_renderer = world_get_widget_renderer(world);

    widget_renderer_remove_text_widget(widget_renderer, text_widget->render_data_handle);
    text_widget->render_data_handle = INVALID_RENDER_DATA_HANDLE;
}

static void
prv_text_widget_on_pos_changed(void* this) {
    te_text_widget* text_widget = this;

    if (text_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        return;
    }

    te_world* world = widget_get_world(text_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(
        game_manager_get_window(world_get_game_manager(world)), &window_width, &window_height);

    te_text_widget_render_data* data = widget_renderer_get_text_widget_render_data_tmp(
        world_get_widget_renderer(world), text_widget->render_data_handle);

    widget_get_screen_position(text_widget->widget, data->pos_pix);
    glm_vec2_mul(
        data->pos_pix, (vec2){(float)window_width, (float)window_height}, data->pos_pix);
}

static void
prv_text_widget_on_size_changed(void* this) {
    te_text_widget* text_widget = this;

    if (text_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        return;
    }

    prv_text_widget_update_all_render_data(text_widget);
}

static void
prv_text_widget_on_after_spawned(void* this) {
    te_text_widget* text_widget = this;
    prv_text_widget_register_for_rendering(text_widget);
}

static void
prv_text_widget_on_before_despawned(void* this) {
    te_text_widget* text_widget = this;
    prv_text_widget_unregister_from_rendering(text_widget);
}

static void
prv_text_widget_on_window_size_changed(void* this) {
    te_text_widget* text_widget = this;

    if (text_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        return;
    }

    prv_text_widget_update_all_render_data(text_widget);
}

static void
prv_text_widget_update_all_render_data(te_text_widget* text_widget) {
#if defined(DEBUG)
    if (text_widget->render_data_handle == INVALID_RENDER_DATA_HANDLE) {
        log_error("expected the render data handle to be valid");
        abort();
    }
#endif

    te_world* world = widget_get_world(text_widget->widget);
    if (world == NULL) {
        log_error("expected the widget to be spawned");
        abort();
    }
    te_game_manager* game_manager = world_get_game_manager(world);
    te_font_manager* font_manager =
        renderer_get_font_manager(game_manager_get_renderer(game_manager));

    te_text_widget_render_data* data = widget_renderer_get_text_widget_render_data_tmp(
        world_get_widget_renderer(world), text_widget->render_data_handle);

    glm_vec4_copy(text_widget->color, data->color);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(game_manager_get_window(game_manager), &window_width, &window_height);

    widget_get_screen_position(text_widget->widget, data->pos_pix);
    glm_vec2_mul(
        data->pos_pix, (vec2){(float)window_width, (float)window_height}, data->pos_pix);

    // Update glyphs (calculating in pixels).
    const float glyph_scale =
        text_widget->text_height / prv_font_manager_get_font_height_to_load();
    const float glyph_height = text_widget->text_height * (float)window_height;
    const float line_spacing = text_widget->line_spacing * glyph_height;

    vec2 size;
    widget_get_screen_size(text_widget->widget, size);
    glm_vec2_mul(size, (vec2){(float)window_width, (float)window_height}, size);

    // Count how much glyphs with non 0 width there are (i.e. displayable glyphs).
    data->glyph_count = 0;
    for (unsigned int i = 0; i < text_widget->text_len; i++) {
        te_font_glyph glyph =
            font_manager_get_glyph(font_manager, (unsigned long)text_widget->text[i]);
        data->glyph_count += glyph.width > 0;
    }
    free(data->glyphs);
    data->glyphs = NULL;

    if (data->glyph_count > 0) {
        data->glyphs = malloc(sizeof(te_text_widget_glyph) * data->glyph_count);

        // Offset from the widget's pivot.
        vec2 offset;
        glm_vec2_copy((vec2){0.0f, 0.0f}, offset);

        // Switch to the first row of the text.
        offset[1] += glyph_height;

        for (unsigned int char_idx = 0, glyph_idx = 0; char_idx < text_widget->text_len;
             char_idx++) {
            te_font_glyph src_glyph = font_manager_get_glyph(
                font_manager, (unsigned long)text_widget->text[char_idx]);
            glyph_idx += src_glyph.width > 0;

            if (text_widget->text[char_idx] == '\n' && text_widget->is_multiline) {
                offset[1] += glyph_height + line_spacing;
                offset[0] = 0.0f;

                if (offset[1] > size[1]) {
                    // Reached vertical limit.
                    break;
                }

                continue; // don't render \n
            }

            const float distance_to_next_glyph = (float)(src_glyph.advance >> 6) * glyph_scale;

            if (offset[0] + distance_to_next_glyph > size[0]) {
                if (text_widget->is_multiline) {
                    // Handle word wrap.
                    offset[1] += glyph_height + line_spacing;
                    offset[0] = 0.0f;

                    if (offset[1] > size[1]) {
                        // Reached vertical limit.
                        break;
                    }
                } else {
                    // Reached horizontal limit.
                    break;
                }
            }

            if (src_glyph.width != 0) {
                te_text_widget_glyph* dst_glyph = &data->glyphs[glyph_idx - 1];

                dst_glyph->tex_id = src_glyph.tex_id;

                glm_vec2_copy(
                    (vec2){(float)src_glyph.width * glyph_scale,
                           (float)src_glyph.height * glyph_scale},
                    dst_glyph->size_pix);

                glm_vec2_copy(offset, dst_glyph->offset_pix);
                glm_vec2_add(
                    dst_glyph->offset_pix,
                    (vec2){(float)src_glyph.bearing_x * glyph_scale,
                           -(float)src_glyph.bearing_y * glyph_scale},
                    dst_glyph->offset_pix);
            }

            // Switch to the next glyph.
            offset[0] += distance_to_next_glyph;
        }
    }
}

static inline void
prv_text_widget_set_position(te_text_widget* text_widget, vec2 pos) {
    widget_set_relative_position(text_widget->widget, pos);
}

static inline void
prv_text_widget_get_position(te_text_widget* text_widget, vec2 out) {
    widget_get_relative_position(text_widget->widget, out);
}

static inline void
prv_text_widget_set_size(te_text_widget* text_widget, vec2 size) {
    widget_set_relative_size(text_widget->widget, size);
}

static inline void
prv_text_widget_get_size(te_text_widget* text_widget, vec2 out) {
    widget_get_relative_size(text_widget->widget, out);
}

static inline const wchar_t*
prv_text_widget_get_text(te_text_widget* text_widget) {
    unsigned int text_len;
    return text_widget_get_text(text_widget, &text_len);
}

const char*
text_widget_get_type_id(void) {
    return "text_widget";
}

static te_widget*
prv_text_widget_get_base(te_text_widget* text_widget) {
    return text_widget->widget;
}

static void
prv_text_widget_spawn(te_world* world, te_text_widget* text_widget) {
    world_spawn_widget(world, prv_text_widget_get_base(text_widget));
}

static void
set_name(te_text_widget* widget, const char* name) {
    widget_set_name(widget->widget, name);
}

static const char*
get_name(te_text_widget* widget) {
    return widget_get_name(widget->widget);
}

void
text_widget_register_type(void) {
    te_type_info* info = type_info_create(
        text_widget_get_type_id(), text_widget_create, prv_text_widget_spawn,
        prv_text_widget_get_base);
    type_info_add_vec2_variable(
        info, "position", prv_text_widget_set_position, prv_text_widget_get_position);
    type_info_add_vec2_variable(
        info, "size", prv_text_widget_set_size, prv_text_widget_get_size);
    type_info_add_wstring_variable(
        info, "text", text_widget_set_text, prv_text_widget_get_text);
    type_info_add_bool_variable(
        info, "is_multiline", text_widget_set_is_multiline, text_widget_is_multiline);
    type_info_add_float_variable(
        info, "text_height", text_widget_set_text_height, text_widget_get_text_height);
    type_info_add_float_variable(
        info, "line_spacing", text_widget_set_line_spacing, text_widget_get_line_spacing);
    type_info_add_vec4_variable(info, "color", text_widget_set_color, text_widget_get_color);
    type_info_add_string_variable(info, "name", set_name, get_name);

    type_database_register_type(info);
}
