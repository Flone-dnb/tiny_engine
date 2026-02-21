#include "widget/text_edit_widget.h"

#include "game_manager.h"
#include "misc/error.h"
#include "misc/wchar_funcs.h"
#include "render/font_manager.h"
#include "render/renderer.h"
#include "render/widget_renderer.h"
#include "widget/rect_widget.h"
#include "widget/text_widget.h"
#include "widget/widget.h"
#include "window.h"
#include "world.h"

#define TE_INVALID_TEXT_CURSOR_INDEX 0xffffffff

struct te_text_edit_widget {
    te_widget* widget;

    // Text cursor child widget. May be NULL if cursor is not shown.
    te_rect_widget* rect_cursor_widget;

    // Always valid child widget.
    te_text_widget* text_widget;

    // May be NULL if not set.
    void (*on_text_changed)(wchar_t*, unsigned int);

    vec4 text_color;

    // Height of the text in range [0.0; 1.0] relative to window height.
    float text_height;

    // Index of wchar_t in @ref text_widget where the cursor is currently at, if the cursor
    // is at the end of text the index will be equal to text's strlen.
    // Stores invalid index value if cursor is not visible.
    unsigned int text_cursor_index;

    // `true` if entered the "destroy" function.
    bool is_text_edit_widget_destroy;
};

// Widget callbacks:
static void prv_text_edit_widget_on_pos_changed(void* this);
static void prv_text_edit_widget_on_size_changed(void* this);
static void prv_text_edit_widget_on_window_size_changed(void* this);
static void prv_text_edit_widget_on_before_base_destroyed(void* this);
static void prv_text_edit_widget_on_after_spawned(void* this);
static void prv_text_edit_widget_on_before_despawned(void* this);

// Interactable callbacks:
static void prv_text_edit_widget_on_mouse_button_pressed(void* this, enum te_mouse_button button, vec2 cursor_pos);
static void prv_text_edit_widget_on_cursor_left(void* this);
static void prv_text_edit_widget_on_keyboard_input_text(void* this, const char* input_text);
static void prv_text_edit_widget_on_keyboard_input(void* this, enum te_keyboard_button button);

te_text_edit_widget*
text_edit_widget_create(void) {
    te_text_edit_widget* text_edit_widget = malloc(sizeof(te_text_edit_widget));

    text_edit_widget->widget = widget_create(
        text_edit_widget, prv_text_edit_widget_on_pos_changed, prv_text_edit_widget_on_size_changed,
        prv_text_edit_widget_on_before_base_destroyed, prv_text_edit_widget_on_after_spawned,
        prv_text_edit_widget_on_before_despawned, prv_text_edit_widget_on_window_size_changed);

    text_edit_widget->rect_cursor_widget = NULL;

    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, text_edit_widget->text_color);
    text_edit_widget->text_height = 0.03f;

    text_edit_widget->text_cursor_index = TE_INVALID_TEXT_CURSOR_INDEX;

    text_edit_widget->text_widget = text_widget_create();
    widget_set_is_serialization_allowed(text_widget_get_widget(text_edit_widget->text_widget), false);
    widget_set_parent(text_widget_get_widget(text_edit_widget->text_widget), text_edit_widget->widget);
    widget_set_relative_position(text_widget_get_widget(text_edit_widget->text_widget), (vec2){0.0f, 0.0f});
    widget_set_relative_size(text_widget_get_widget(text_edit_widget->text_widget), (vec2){1.0f, 1.0f});

    prv_widget_set_input_callbacks(
        text_edit_widget->widget, NULL, prv_text_edit_widget_on_cursor_left, prv_text_edit_widget_on_mouse_button_pressed, NULL,
        prv_text_edit_widget_on_keyboard_input_text, prv_text_edit_widget_on_keyboard_input);

    text_edit_widget->on_text_changed = NULL;

    return text_edit_widget;
}

void
text_edit_widget_destroy(te_text_edit_widget* text_edit_widget) {
    text_edit_widget->is_text_edit_widget_destroy = true;

    if (text_edit_widget->widget != NULL) { // may be null if we got here from base destroy
        widget_destroy(text_edit_widget->widget);
    }

    free(text_edit_widget);
}

static void
prv_text_edit_widget_on_before_base_destroyed(void* this) {
    te_text_edit_widget* text_edit_widget = this;

    if (text_edit_widget->is_text_edit_widget_destroy) {
        return;
    }

    // Destroy was called on the base (widget) component, possibly due to
    // parent being destroyed, cleanup our data.
    text_edit_widget->widget = NULL;
    text_edit_widget_destroy(text_edit_widget);
}

void
text_edit_widget_set_on_text_changed(
    te_text_edit_widget* text_edit_widget, void (*on_text_changed)(wchar_t* new_text, unsigned int strlen)) {
    text_edit_widget->on_text_changed = on_text_changed;
}

static void
prv_text_edit_widget_on_pos_changed(void* this) {
    (void)this;
}

static void
prv_text_edit_widget_on_size_changed(void* this) {
    (void)this;
}

static void
prv_text_edit_widget_on_window_size_changed(void* this) {
    (void)this;
}

void
prv_text_edit_widget_on_after_spawned(void* this) {
    te_text_edit_widget* text_edit_widget = this;

    text_widget_set_color(text_edit_widget->text_widget, text_edit_widget->text_color);
    text_widget_set_text_height(text_edit_widget->text_widget, text_edit_widget->text_height);

    // Self check: make sure we only have 1 child widget.
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(text_edit_widget->widget, &child_count);
    if (child_count != 1) {
        show_error_and_abort("unexpected child widget count on a widget");
    }

    te_world* world = widget_get_world(text_edit_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    prv_world_add_interactable_widget(world, text_edit_widget->widget);
}

static void
prv_text_edit_widget_despawn_destroy_cursor(te_text_edit_widget* text_edit_widget) {
    te_world* world = widget_get_world(text_edit_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected world to be valid");
    }

    // Detach and despawn.
    widget_set_parent(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), NULL);

    rect_widget_destroy(text_edit_widget->rect_cursor_widget);
    text_edit_widget->rect_cursor_widget = NULL;

    text_edit_widget->text_cursor_index = TE_INVALID_TEXT_CURSOR_INDEX;

    // Disable text input events.
    te_window* window = game_manager_get_window(world_get_game_manager(world));
    SDL_StopTextInput(prv_window_get_sdl_window(window));
}

void
prv_text_edit_widget_on_before_despawned(void* this) {
    te_text_edit_widget* text_edit_widget = this;

    // Self check: make sure we only have up to 2 child widgets.
    unsigned int child_count = 0;
    (void)widget_get_child_widgets_tmp(text_edit_widget->widget, &child_count);
    if (child_count > 2) {
        show_error_and_abort("unexpected child widget count on a widget");
    }

    te_world* world = widget_get_world(text_edit_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected the widget to be spawned");
    }
    prv_world_remove_interactable_widget(world, text_edit_widget->widget);

    if (text_edit_widget->rect_cursor_widget != NULL) {
        prv_text_edit_widget_despawn_destroy_cursor(text_edit_widget);
    }
}

void
prv_text_edit_widget_on_mouse_button_pressed(void* this, enum te_mouse_button button, vec2 cursor_pos) {
    if (button != TE_MB_LEFT) {
        return;
    }

    te_text_edit_widget* text_edit_widget = this;

    // Determine where to put the text cursor (snap to closest glyph start).

    te_world* world = widget_get_world(text_edit_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected world to be valid");
    }
    te_game_manager* game_manager = world_get_game_manager(world);
    te_font_manager* font_manager = renderer_get_font_manager(game_manager_get_renderer(game_manager));

    te_window* window = game_manager_get_window(game_manager);
    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);

    const unsigned int text_render_data_handle = prv_text_widget_get_render_data_handle(text_edit_widget->text_widget);
    if (text_render_data_handle == 0xffffffff) {
        show_error_and_abort("expected text render data handle to be valid");
    }
    te_text_widget_render_data* data =
        widget_renderer_get_text_widget_render_data_tmp(world_get_widget_renderer(world), text_render_data_handle);

    vec2 cursor_pos_pix;
    glm_vec2_mul(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos_pix);
    if (data->pos_pix[0] > cursor_pos_pix[0]) {
        return;
    }

    unsigned int text_len = 0;
    wchar_t* text = text_widget_get_text(text_edit_widget->text_widget, &text_len);

    // Prepare cursor position.
    vec2 rect_pos;
    glm_vec2_copy(data->pos_pix, rect_pos);
    text_edit_widget->text_cursor_index = 0;

    if (text_len > 0) {
        // Put to end by default if have text.
        rect_pos[0] += data->glyphs[data->glyph_count - 1].offset_pix[0] + data->glyphs[data->glyph_count - 1].size_pix[0];

        text_edit_widget->text_cursor_index = text_len;

        float x_start = data->pos_pix[0];
        bool found = false;
        const float glyph_scale = text_edit_widget->text_height / prv_font_manager_get_font_height_to_load();
        for (unsigned int char_idx = 0, glyph_idx = 0; char_idx < text_len; char_idx++) {
            te_font_glyph glyph = font_manager_get_glyph(font_manager, (unsigned long)text[char_idx]);

            float x_end = 0.0f;
            if (glyph.width == 0) {
                const float distance_to_next_glyph = (float)(glyph.advance >> 6) * glyph_scale;
                x_end = x_start + distance_to_next_glyph;
            } else {
                x_end = data->pos_pix[0] + data->glyphs[glyph_idx].offset_pix[0] + data->glyphs[glyph_idx].size_pix[0];
            }

            if (cursor_pos_pix[0] >= x_start && cursor_pos_pix[0] <= x_end) {
                rect_pos[0] = x_start;
                text_edit_widget->text_cursor_index = char_idx;
                found = true;
                break;
            }

            x_start = x_end;
            glyph_idx += glyph.width > 0;
        }
        if (!found) {
            rect_pos[0] = x_start;
            text_edit_widget->text_cursor_index = text_len;
        }
    }
    glm_vec2_div(rect_pos, (vec2){(float)window_width, (float)window_height}, rect_pos);

    if (text_edit_widget->rect_cursor_widget == NULL) {
        // Create text cursor.
        text_edit_widget->rect_cursor_widget = rect_widget_create();
        widget_set_is_serialization_allowed(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), false);

        // Attach and spawn.
        widget_set_parent(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), text_edit_widget->widget);

        // Enable text input events.
        SDL_StartTextInput(prv_window_get_sdl_window(window));
    }

    // Calculate rect pos/size to be relative to parent.

    vec2 rect_size;
    rect_size[0] = 2.0f / (float)window_width;
    rect_size[1] = text_widget_get_text_height(text_edit_widget->text_widget);

    vec2 text_widget_pos;
    vec2 text_widget_size;
    widget_get_screen_position(text_widget_get_widget(text_edit_widget->text_widget), text_widget_pos);
    widget_get_screen_size(text_widget_get_widget(text_edit_widget->text_widget), text_widget_size);

    glm_vec2_div(rect_size, text_widget_size, rect_size);

    glm_vec2_sub(rect_pos, text_widget_pos, rect_pos);
    glm_vec2_div(rect_pos, text_widget_size, rect_pos);

    // Update text cursor.
    widget_set_relative_position(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), rect_pos);
    widget_set_relative_size(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), rect_size);
}

void
prv_text_edit_widget_on_cursor_left(void* this) {
    te_text_edit_widget* text_edit_widget = this;

    if (text_edit_widget->rect_cursor_widget != NULL) {
        prv_text_edit_widget_despawn_destroy_cursor(text_edit_widget);
    }
}

static void
prv_text_edit_widget_update_cursor(te_text_edit_widget* text_edit_widget) {
    unsigned int text_len;
    wchar_t* text = text_widget_get_text(text_edit_widget->text_widget, &text_len);

    te_world* world = widget_get_world(text_edit_widget->widget);
    if (world == NULL) {
        show_error_and_abort("expected world to be valid");
    }
    te_game_manager* game_manager = world_get_game_manager(world);
    te_font_manager* font_manager = renderer_get_font_manager(game_manager_get_renderer(game_manager));

    const unsigned int text_render_data_handle = prv_text_widget_get_render_data_handle(text_edit_widget->text_widget);
    if (text_render_data_handle == 0xffffffff) {
        show_error_and_abort("expected text render data handle to be valid");
    }
    te_text_widget_render_data* data =
        widget_renderer_get_text_widget_render_data_tmp(world_get_widget_renderer(world), text_render_data_handle);

    te_window* window = game_manager_get_window(game_manager);
    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);

    vec2 rect_pos;
    glm_vec2_copy(data->pos_pix, rect_pos);
    if (text_edit_widget->text_cursor_index > 0) {
        float x_start = data->pos_pix[0];
        const float glyph_scale = text_edit_widget->text_height / prv_font_manager_get_font_height_to_load();
        bool found = false;
        for (unsigned int char_idx = 0, glyph_idx = 0; char_idx < text_len; char_idx++) {
            te_font_glyph glyph = font_manager_get_glyph(font_manager, (unsigned long)text[char_idx]);

            float x_end = 0.0f;
            if (glyph.width == 0) {
                const float distance_to_next_glyph = (float)(glyph.advance >> 6) * glyph_scale;
                x_end = x_start + distance_to_next_glyph;
            } else {
                x_end = data->pos_pix[0] + data->glyphs[glyph_idx].offset_pix[0] + data->glyphs[glyph_idx].size_pix[0];
            }

            if (text_edit_widget->text_cursor_index == char_idx) {
                rect_pos[0] = x_start;
                found = false;
                break;
            }

            x_start = x_end;
            glyph_idx += glyph.width > 0;
        }
        if (!found) {
            rect_pos[0] = x_start;
        }
    }

    glm_vec2_div(rect_pos, (vec2){(float)window_width, (float)window_height}, rect_pos);

    // Calculate rect pos to be relative to parent.

    vec2 text_widget_pos;
    vec2 text_widget_size;
    widget_get_screen_position(text_widget_get_widget(text_edit_widget->text_widget), text_widget_pos);
    widget_get_screen_size(text_widget_get_widget(text_edit_widget->text_widget), text_widget_size);

    glm_vec2_sub(rect_pos, text_widget_pos, rect_pos);
    glm_vec2_div(rect_pos, text_widget_size, rect_pos);

    if (rect_pos[0] < 0.0f) {
        rect_pos[0] = 1.0f;
    }

    widget_set_relative_position(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), rect_pos);
}

static void
prv_text_edit_widget_on_keyboard_input_text(void* this, const char* input_text) {
    te_text_edit_widget* text_edit_widget = this;

    unsigned int added_text_len;
    wchar_t* added_text = wchar_from_char(input_text, &added_text_len);
    if (added_text_len == 0) {
        show_error_and_abort("unexpected added text len");
    }

    unsigned int old_text_len;
    wchar_t* old_text = text_widget_get_text(text_edit_widget->text_widget, &old_text_len);

    if (text_edit_widget->text_cursor_index > old_text_len) {
        show_error_and_abort("expected a valid text cursor index");
    }

    wchar_t* new_text = malloc(sizeof(wchar_t) * (old_text_len + added_text_len + 1));
    memcpy(new_text, old_text, sizeof(wchar_t) * text_edit_widget->text_cursor_index);
    memcpy(new_text + text_edit_widget->text_cursor_index, added_text, sizeof(wchar_t) * added_text_len);
    memcpy(
        new_text + (text_edit_widget->text_cursor_index + added_text_len), old_text + text_edit_widget->text_cursor_index,
        sizeof(wchar_t) * (old_text_len - text_edit_widget->text_cursor_index));
    new_text[old_text_len + added_text_len] = 0;

    free(added_text);
    const unsigned int new_text_len = old_text_len + added_text_len;

    text_widget_set_text_own(text_edit_widget->text_widget, new_text, new_text_len);

    text_edit_widget->text_cursor_index += added_text_len;
    prv_text_edit_widget_update_cursor(text_edit_widget);

    if (text_edit_widget->on_text_changed != NULL) {
        text_edit_widget->on_text_changed(new_text, new_text_len);
    }
}

static void
prv_text_edit_widget_on_keyboard_input(void* this, enum te_keyboard_button button) {
    if (button == TE_KB_BACKSPACE) {
        te_text_edit_widget* text_edit_widget = this;

        unsigned int old_text_len;
        wchar_t* old_text = text_widget_get_text(text_edit_widget->text_widget, &old_text_len);

        if (old_text_len == 0 || text_edit_widget->text_cursor_index > old_text_len || text_edit_widget->text_cursor_index == 0) {
            return;
        }

        wchar_t* new_text = malloc(sizeof(wchar_t) * old_text_len);
        memcpy(new_text, old_text, sizeof(wchar_t) * (text_edit_widget->text_cursor_index - 1));
        memcpy(
            new_text + (text_edit_widget->text_cursor_index - 1), old_text + text_edit_widget->text_cursor_index,
            sizeof(wchar_t) * (old_text_len - text_edit_widget->text_cursor_index));
        new_text[old_text_len - 1] = 0;

        const unsigned int new_text_len = old_text_len - 1;
        text_widget_set_text_own(text_edit_widget->text_widget, new_text, new_text_len);

        text_edit_widget->text_cursor_index -= 1;
        prv_text_edit_widget_update_cursor(text_edit_widget);

        if (text_edit_widget->on_text_changed != NULL) {
            text_edit_widget->on_text_changed(new_text, new_text_len);
        }
    } else if (button == TE_KB_RIGHT) {
        te_text_edit_widget* text_edit_widget = this;

        unsigned int text_len;
        (void)text_widget_get_text(text_edit_widget->text_widget, &text_len);

        if (text_len == 0 || text_edit_widget->text_cursor_index >= text_len) {
            return;
        }

        text_edit_widget->text_cursor_index += 1;
        prv_text_edit_widget_update_cursor(text_edit_widget);
    } else if (button == TE_KB_LEFT) {
        te_text_edit_widget* text_edit_widget = this;

        unsigned int text_len;
        (void)text_widget_get_text(text_edit_widget->text_widget, &text_len);

        if (text_len == 0 || text_edit_widget->text_cursor_index == 0) {
            return;
        }

        text_edit_widget->text_cursor_index -= 1;
        prv_text_edit_widget_update_cursor(text_edit_widget);
    }
}

te_widget*
text_edit_widget_get_widget(te_text_edit_widget* text_edit_widget) {
    return text_edit_widget->widget;
}

void
text_edit_widget_set_text(te_text_edit_widget* text_edit_widget, const wchar_t* text) {
    text_widget_set_text(text_edit_widget->text_widget, text);

    if (text_edit_widget->rect_cursor_widget != NULL) {
        prv_text_edit_widget_despawn_destroy_cursor(text_edit_widget);
    }
}

void
text_edit_widget_set_text_own(te_text_edit_widget* text_edit_widget, wchar_t* text, unsigned int strlen) {
    text_widget_set_text_own(text_edit_widget->text_widget, text, strlen);

    if (text_edit_widget->rect_cursor_widget != NULL) {
        prv_text_edit_widget_despawn_destroy_cursor(text_edit_widget);
    }
}

wchar_t*
text_edit_widget_get_text(te_text_edit_widget* text_edit_widget, unsigned int* text_len) {
    return text_widget_get_text(text_edit_widget->text_widget, text_len);
}

void
text_edit_widget_set_text_height(te_text_edit_widget* text_edit_widget, float height) {
    text_edit_widget->text_height = height;
    text_widget_set_text_height(text_edit_widget->text_widget, height);
}

float
text_edit_widget_get_text_height(te_text_edit_widget* text_edit_widget) {
    return text_edit_widget->text_height;
}

void
text_edit_widget_set_color(te_text_edit_widget* text_edit_widget, vec4 color) {
    glm_vec4_copy(color, text_edit_widget->text_color);
    text_widget_set_color(text_edit_widget->text_widget, color);
}

void
text_edit_widget_get_color(te_text_edit_widget* text_edit_widget, vec4 out) {
    glm_vec4_copy(text_edit_widget->text_color, out);
}
