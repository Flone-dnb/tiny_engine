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

struct te_text_edit_widget {
    te_widget* widget;

    // Text cursor child widget. May be NULL if cursor is not shown.
    te_rect_widget* rect_cursor_widget;

    // Always valid child widget.
    te_text_widget* text_widget;

    // Non-NULL callback.
    void (*on_text_changed)(wchar_t*, unsigned int);

    vec4 text_color;

    // Height of the text in range [0.0; 1.0] relative to window height.
    float text_height;

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

te_text_edit_widget*
text_edit_widget_create(void (*on_text_changed)(wchar_t*, unsigned int)) {
    if (on_text_changed == NULL) {
        show_error_and_abort("you must specify a non-NULL callback for \"on text changed\"");
    }

    te_text_edit_widget* text_edit_widget = malloc(sizeof(te_text_edit_widget));

    text_edit_widget->widget = widget_create(
        text_edit_widget, prv_text_edit_widget_on_pos_changed, prv_text_edit_widget_on_size_changed,
        prv_text_edit_widget_on_before_base_destroyed, prv_text_edit_widget_on_after_spawned,
        prv_text_edit_widget_on_before_despawned, prv_text_edit_widget_on_window_size_changed);

    text_edit_widget->rect_cursor_widget = NULL;

    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, text_edit_widget->text_color);
    text_edit_widget->text_height = 0.03f;

    text_edit_widget->text_widget = text_widget_create();
    widget_set_is_serialization_allowed(text_widget_get_widget(text_edit_widget->text_widget), false);
    widget_set_parent(text_widget_get_widget(text_edit_widget->text_widget), text_edit_widget->widget);
    widget_set_relative_position(text_widget_get_widget(text_edit_widget->text_widget), (vec2){0.0f, 0.0f});
    widget_set_relative_size(text_widget_get_widget(text_edit_widget->text_widget), (vec2){1.0f, 1.0f});

    prv_widget_set_input_callbacks(
        text_edit_widget->widget, NULL, prv_text_edit_widget_on_cursor_left, prv_text_edit_widget_on_mouse_button_pressed, NULL,
        prv_text_edit_widget_on_keyboard_input_text);

    text_edit_widget->on_text_changed = on_text_changed;

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
        // The child widget (text cursor) is already despawned (because child widgets despawn first) we just need to destroy it.
        te_world* world = widget_get_world(text_edit_widget->widget);
        if (world == NULL) {
            show_error_and_abort("expected world to be valid");
        }

        widget_set_parent(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), NULL);
        rect_widget_destroy(text_edit_widget->rect_cursor_widget);
        text_edit_widget->rect_cursor_widget = NULL;

        // Disable text input events.
        te_window* window = game_manager_get_window(world_get_game_manager(world));
        SDL_StopTextInput(prv_window_get_sdl_window(window));
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

    te_window* window = game_manager_get_window(world_get_game_manager(world));
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

    vec2 target_pos;
    glm_vec2_copy(data->pos_pix, target_pos);
    target_pos[0] += data->glyphs[data->glyph_count - 1].offset_pix[0] + data->glyphs[data->glyph_count - 1].size_pix[0];
    float x_start = data->pos_pix[0];
    for (unsigned int char_idx = 0, glyph_idx = 0; char_idx < text_len && glyph_idx < data->glyph_count; char_idx++) {
        te_font_glyph glyph = font_manager_get_glyph(font_manager, (unsigned long)text[char_idx]);
        if (glyph.width == 0) {
            continue;
        }

        const float x_end = data->pos_pix[0] + data->glyphs[glyph_idx].offset_pix[0] + data->glyphs[glyph_idx].size_pix[0];

        if (cursor_pos_pix[0] >= x_start && cursor_pos_pix[0] <= x_end) {
            target_pos[0] = x_start;
            break;
        }

        x_start = x_end;
        glyph_idx += 1;
    }
    glm_vec2_div(target_pos, (vec2){(float)window_width, (float)window_height}, target_pos);

    if (text_edit_widget->rect_cursor_widget == NULL) {
        // Create text cursor.
        text_edit_widget->rect_cursor_widget = rect_widget_create();
        widget_set_is_serialization_allowed(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), false);

        // Attach and spawn.
        widget_set_parent(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), text_edit_widget->widget);

        // Enable text input events.
        SDL_StartTextInput(prv_window_get_sdl_window(window));
    }

    const float text_height = text_widget_get_text_height(text_edit_widget->text_widget);

    // Update text cursor.
    // TODO; // calculate cursor size relative to window 2 pixels
    widget_set_relative_position(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), target_pos);
    widget_set_relative_size(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), (vec2){0.01f, text_height});
}

void
prv_text_edit_widget_on_cursor_left(void* this) {
    te_text_edit_widget* text_edit_widget = this;

    if (text_edit_widget->rect_cursor_widget != NULL) {
        te_world* world = widget_get_world(text_edit_widget->widget);
        if (world == NULL) {
            show_error_and_abort("expected world to be valid");
        }

        // Detach and despawn.
        widget_set_parent(rect_widget_get_widget(text_edit_widget->rect_cursor_widget), NULL);

        rect_widget_destroy(text_edit_widget->rect_cursor_widget);
        text_edit_widget->rect_cursor_widget = NULL;

        // Disable text input events.
        te_window* window = game_manager_get_window(world_get_game_manager(world));
        SDL_StopTextInput(prv_window_get_sdl_window(window));
    }
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

    wchar_t* new_text = malloc(sizeof(wchar_t) * (old_text_len + added_text_len + 1));
    memcpy(new_text, old_text, sizeof(wchar_t) * old_text_len);
    memcpy(new_text + old_text_len, added_text, sizeof(wchar_t) * added_text_len);
    new_text[old_text_len + added_text_len] = 0;

    free(added_text);
    const unsigned int new_text_len = old_text_len + added_text_len;

    text_widget_set_text_own(text_edit_widget->text_widget, new_text, new_text_len);

    text_edit_widget->on_text_changed(new_text, new_text_len);
}

te_widget*
text_edit_widget_get_widget(te_text_edit_widget* text_edit_widget) {
    return text_edit_widget->widget;
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
