#include <ui/scene_animation_editor.h>

#include <io/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <world.h>
#include <widget/widget.h>
#include <widget/button_widget.h>
#include <widget/rect_widget.h>
#include <widget/text_widget.h>
#include <widget/checkbox_widget.h>
#include <misc/wchar_funcs.h>
#include <type_database.h>
#include <ui/theme.h>
#include <game/game_object_info.h>
#include <game/scene_animation.h>

#define TE_SCENE_ANIM_EDITOR_X_POS 0.05f
#define TE_SCENE_ANIM_MENU_WIDTH 0.09f
#define TE_SCENE_ANIM_TRACK_NAME_WIDTH 0.14f
#define BUTTON_HIDDEN_X_POS 10.0f

typedef struct te_keyframe_button_array {
    te_button_widget** buttons;
    unsigned int count;
    unsigned int capacity;
} te_keyframe_button_array;

static te_keyframe_button_array*
keyframe_button_array_create(void) {
    te_keyframe_button_array* array = malloc(sizeof(te_keyframe_button_array));
    array->buttons = NULL;
    array->count = 0;
    array->capacity = 0;
    return array;
}

static void
keyframe_button_array_add(
    te_keyframe_button_array* array, te_widget* parent, float topleft_x, float topleft_y,
    float track_width, float track_height) {
    if (array->count == array->capacity) {
        array->capacity += 50;

        te_button_widget** new_buttons = malloc(sizeof(te_button_widget*) * array->capacity);
        memcpy(new_buttons, array->buttons, sizeof(te_button_widget*) * array->count);

        free(array->buttons);
        array->buttons = new_buttons;
    }

    te_button_widget* button = button_widget_create();
    {
        float aspect = track_width / track_height;
        vec2 size;
        size[1] = track_height / 2.0f;
        size[0] = size[1] / aspect;

        te_widget* widget = button_widget_get_widget(button);
        widget_set_parent(widget, parent);
        widget_set_relative_position(
            widget, (vec2){topleft_x - size[0] / 2.0f, topleft_y + size[1] / 2.0f});
        widget_set_relative_size(widget, size);
    }

    button_widget_set_color(button, (vec4){1.0f, 0.0f, 0.0f, 1.0f});
    button_widget_set_color_hovered(button, (vec4){1.0f, 0.1f, 0.1f, 1.0f});
    button_widget_set_color_pressed(button, (vec4){0.5f, 0.0f, 0.0f, 1.0f});

    array->buttons[array->count] = button;
    array->count += 1;
}

static void
keyframe_button_array_clear(te_keyframe_button_array* array) {
    for (unsigned int i = 0; i < array->count; i++) {
        te_widget* widget = button_widget_get_widget(array->buttons[i]);
        widget_set_parent(widget, NULL); // make root widget to despawn
        te_world* world = widget_get_world(widget);
        world_despawn_widget(world, widget);
        widget_destroy(widget);
    }
    array->count = 0;
    array->capacity = 0;
    free(array->buttons);
    array->buttons = NULL;
}

static void
keyframe_button_array_destroy(te_keyframe_button_array* array) {
    keyframe_button_array_clear(array);
    free(array);
}

struct te_scene_animation_editor {
    // Always valid.
    te_world* world;
    te_widget* root_widget;

    te_text_widget* left_border_time_text;
    te_text_widget* right_border_time_text;
    te_text_widget* time_widget;

    te_button_widget** track_buttons;

    te_rect_widget* current_time_separator;

    // Not NULL if showing available tracks from an object.
    void* selected_obj;
    const te_type_info* selected_obj_type_info;

    te_keyframe_button_array* keyframe_button_array;

    float timeline_x_pos;
    float button_height;
    float track_buttons_x;

    unsigned int track_button_count;
    unsigned int track_scroll_count;

    unsigned int left_border_time;
    unsigned int right_border_time;
};

static void redraw_timeline(te_scene_animation_editor* editor, bool update_keyframes);
static void on_play_pause_clicked(te_button_widget* button);
static void on_stop_clicked(te_button_widget* button);
static void on_track_right_clicked(te_button_widget* button);

te_scene_animation_editor*
scene_animation_editor_create(te_world* world) {
    te_scene_animation_editor* editor = malloc(sizeof(te_scene_animation_editor));
    editor->world = world;
    editor->left_border_time = 0;
    editor->right_border_time = 4;
    editor->track_scroll_count = 0;
    editor->selected_obj = NULL;
    editor->selected_obj_type_info = NULL;
    editor->keyframe_button_array = keyframe_button_array_create();

    vec2 pos;
    glm_vec2_copy((vec2){TE_SCENE_ANIM_EDITOR_X_POS, 0.76f}, pos);

    vec2 size;
    glm_vec2_copy((vec2){1.0f - pos[0] * 2.0f, 0.98f - pos[1]}, size);

    const float padding_x = theme_get_horizontal_padding() / size[0];
    const float padding_y = theme_get_vertical_padding() / size[1];
    const float text_padding_x = padding_x * 10.0f;
    const float vspacing = theme_get_vertical_spacing() / size[1];
    const float button_height = theme_get_button_height() / size[1];
    const float button_width = TE_SCENE_ANIM_MENU_WIDTH - padding_x * 2.0f;
    const float separator_width = 0.0028f;

    editor->button_height = button_height;

    vec4 separator_color;
    theme_get_background_panel_color(separator_color);
    glm_vec3_mul(separator_color, (vec3){1.5f, 1.5f, 1.5f}, separator_color);

    te_rect_widget* root_background = rect_widget_create();
    editor->root_widget = rect_widget_get_widget(root_background);
    {
        {
            te_widget* widget = editor->root_widget;
            widget_set_relative_position(widget, pos);
            widget_set_relative_size(widget, (vec2){1.0f - pos[0] * 2.0f, 0.98f - pos[1]});
            widget_set_is_serialization_allowed(widget, false);
        }

        vec4 color;
        theme_get_background_panel_color(color);
        rect_widget_set_color(root_background, color);

        float y_pos = padding_y;

        // Play/pause button.
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_relative_position(widget, (vec2){padding_x, y_pos});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);

                widget_set_custom_ptr(widget, editor);
            }

            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_play_pause_clicked);

            // Text.
            {
                te_text_widget* text = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text);
                    widget_set_relative_position(widget, (vec2){text_padding_x, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - text_padding_x, 1.0f});
                    widget_set_parent(widget, button_widget_get_widget(button));
                }

                text_widget_set_text_height(text, theme_get_text_height());

                unsigned int text_len;
                wchar_t* wtext = wchar_from_char("Play / pause", &text_len);
                text_widget_set_text_own(text, wtext, text_len);
            }
        }
        y_pos += button_height + vspacing;

        // Stop button.
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_relative_position(widget, (vec2){padding_x, y_pos});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);

                widget_set_custom_ptr(widget, editor);
            }

            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_stop_clicked);

            // Text.
            {
                te_text_widget* text = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text);
                    widget_set_relative_position(widget, (vec2){text_padding_x, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - text_padding_x, 1.0f});
                    widget_set_parent(widget, button_widget_get_widget(button));
                }

                text_widget_set_text_height(text, theme_get_text_height());

                unsigned int text_len;
                wchar_t* wtext = wchar_from_char("Stop", &text_len);
                text_widget_set_text_own(text, wtext, text_len);
            }
        }
        y_pos += button_height + vspacing;

        // Time.
        {
            te_text_widget* text = text_widget_create();
            editor->time_widget = text;
            {
                te_widget* widget = text_widget_get_widget(text);
                widget_set_relative_position(widget, (vec2){padding_x, y_pos});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);
            }

            text_widget_set_text_height(text, theme_get_text_height());

            unsigned int text_len;
            wchar_t* wtext = wchar_from_char("Time: 0.0 sec", &text_len);
            text_widget_set_text_own(text, wtext, text_len);
        }
        y_pos += button_height + vspacing;

        // Save button.
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_relative_position(widget, (vec2){padding_x, y_pos});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);
            }

            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            // Text.
            {
                te_text_widget* text = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text);
                    widget_set_relative_position(widget, (vec2){text_padding_x, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - text_padding_x, 1.0f});
                    widget_set_parent(widget, button_widget_get_widget(button));
                }

                text_widget_set_text_height(text, theme_get_text_height());

                unsigned int text_len;
                wchar_t* wtext = wchar_from_char("Save as", &text_len);
                text_widget_set_text_own(text, wtext, text_len);
            }
        }
        y_pos += button_height + vspacing;

        // Switch to bottom.
        y_pos = 1.0f - padding_y - button_height;

        // Load button.
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_relative_position(widget, (vec2){padding_x, y_pos});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);
            }

            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            // Text.
            {
                te_text_widget* text = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text);
                    widget_set_relative_position(widget, (vec2){text_padding_x, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - text_padding_x, 1.0f});
                    widget_set_parent(widget, button_widget_get_widget(button));
                }

                text_widget_set_text_height(text, theme_get_text_height());

                unsigned int text_len;
                wchar_t* wtext = wchar_from_char("Load", &text_len);
                text_widget_set_text_own(text, wtext, text_len);
            }
        }

        // Separator.
        {
            te_rect_widget* separator = rect_widget_create();
            {
                te_widget* widget = rect_widget_get_widget(separator);
                widget_set_relative_position(widget, (vec2){TE_SCENE_ANIM_MENU_WIDTH, 0.0f});
                widget_set_relative_size(widget, (vec2){separator_width, 1.0f});
                widget_set_parent(widget, editor->root_widget);
            }

            rect_widget_set_color(separator, separator_color);
        }

        float timeline_x =
            TE_SCENE_ANIM_MENU_WIDTH + padding_x + TE_SCENE_ANIM_TRACK_NAME_WIDTH + padding_x;
        editor->timeline_x_pos = timeline_x;
        float timeline_top_height = button_height;

        // Track names.
        {
            float x = TE_SCENE_ANIM_MENU_WIDTH + padding_x + padding_x / 2.0f;
            float y = 0.0f;

            // Title.
            {
                te_text_widget* text = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text);
                    widget_set_relative_position(widget, (vec2){x, y});
                    widget_set_relative_size(widget, (vec2){button_width, button_height});
                    widget_set_parent(widget, editor->root_widget);
                }

                text_widget_set_text_height(text, theme_get_text_height());

                unsigned int text_len;
                wchar_t* wtext = wchar_from_char("Animated tracks:", &text_len);
                text_widget_set_text_own(text, wtext, text_len);
            }

            editor->track_buttons_x = x;

            // Count how much track names will fit.
            y = timeline_top_height;
            editor->track_button_count = 0;
            while (y + button_height <= 1.0f) {
                editor->track_button_count += 1;
                y += button_height; // no vspacing for tracks
            }

            y = timeline_top_height;
            editor->track_buttons =
                malloc(sizeof(te_button_widget*) * editor->track_button_count);
            for (unsigned int i = 0; i < editor->track_button_count; i++) {
                te_button_widget* button = button_widget_create();
                editor->track_buttons[i] = button;
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, y});
                    widget_set_relative_size(
                        widget, (vec2){TE_SCENE_ANIM_TRACK_NAME_WIDTH, button_height});
                    widget_set_parent(widget, editor->root_widget);

                    widget_set_custom_ptr(widget, editor);
                }

                // Have different background for even/odd tracks for better visibility.
                vec4 back;
                glm_vec4_copy(separator_color, back);
                glm_vec3_mul(back, (vec3){0.5f, 0.5f, 0.5f}, back);
                if (i % 2 != 0) {
                    glm_vec3_mul(back, (vec3){1.75f, 1.75f, 1.75f}, back);
                }

                button_widget_set_color(button, back);

                theme_get_button_color_hovered(color);
                button_widget_set_color_hovered(button, color);

                theme_get_button_color_pressed(color);
                button_widget_set_color_pressed(button, color);

                button_widget_set_on_right_clicked(button, on_track_right_clicked);

                // Text.
                {
                    te_text_widget* text = text_widget_create();
                    {
                        te_widget* widget = text_widget_get_widget(text);
                        widget_set_relative_position(widget, (vec2){text_padding_x, 0.0f});
                        widget_set_relative_size(widget, (vec2){1.0f - text_padding_x, 1.0f});
                        widget_set_parent(widget, button_widget_get_widget(button));
                    }

                    text_widget_set_text_height(text, theme_get_text_height());

                    unsigned int text_len;
                    wchar_t* wtext = wchar_from_char("<track name>", &text_len);
                    text_widget_set_text_own(text, wtext, text_len);
                }

                // Track background.
                te_rect_widget* background = rect_widget_create();
                {
                    te_widget* widget = rect_widget_get_widget(background);
                    widget_set_relative_position(widget, (vec2){timeline_x, y});
                    widget_set_relative_size(widget, (vec2){1.0f - timeline_x, button_height});
                    widget_set_parent(widget, editor->root_widget);
                }
                rect_widget_set_color(background, back);

                y += button_height;
            }
        }

        // Timeline top values (left border time, right border time).
        {
            te_text_widget* text = text_widget_create();
            editor->left_border_time_text = text;
            {
                te_widget* widget = text_widget_get_widget(text);
                widget_set_relative_position(widget, (vec2){timeline_x, 0.0f});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);
            }

            text_widget_set_text_height(text, theme_get_text_height());

            text = text_widget_create();
            editor->right_border_time_text = text;
            {
                te_widget* widget = text_widget_get_widget(text);
                widget_set_relative_position(widget, (vec2){1.0f - button_width / 2.0f, 0.0f});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);
            }

            text_widget_set_text_height(text, theme_get_text_height());
        }

        // Split timeline into N sections.
        const unsigned int section_count = 16;
        const float section_size = (1.0f - timeline_x) / (float)section_count;

        for (unsigned int i = 0; i < section_count; i++) {
            float x = timeline_x + section_size * (float)i;
            te_rect_widget* separator = rect_widget_create();
            {
                te_widget* widget = rect_widget_get_widget(separator);
                widget_set_relative_position(widget, (vec2){x, timeline_top_height});
                widget_set_relative_size(
                    widget,
                    (vec2){separator_width / 2.0f + (i % 2 == 0 ? separator_width : 0.0f),
                           1.0f - timeline_top_height});
                widget_set_parent(widget, editor->root_widget);
            }

            rect_widget_set_color(separator, separator_color);
        }

        // Current time separator (vertical line).
        editor->current_time_separator = rect_widget_create();
        {
            te_widget* widget = rect_widget_get_widget(editor->current_time_separator);
            widget_set_relative_position(
                widget, (vec2){editor->timeline_x_pos, editor->button_height});
            widget_set_relative_size(widget, (vec2){0.004f, 1.0f - editor->button_height});
            widget_set_parent(widget, editor->root_widget);
        }

        theme_get_accent_color(color);
        rect_widget_set_color(editor->current_time_separator, color);
    }

    redraw_timeline(editor, false);

    world_spawn_widget(world, editor->root_widget);

    return editor;
}

void
scene_animation_editor_destroy(te_scene_animation_editor* editor) {
    keyframe_button_array_destroy(editor->keyframe_button_array);
    world_despawn_widget(editor->world, editor->root_widget);
    widget_destroy(editor->root_widget);

    free(editor->track_buttons);

    free(editor);
}

te_widget*
scene_animation_editor_get_root_widget(te_scene_animation_editor* editor) {
    return editor->root_widget;
}

void
scene_animation_editor_hide(te_scene_animation_editor* editor) {
    vec2 pos;
    widget_get_relative_position(editor->root_widget, pos);

    pos[0] = 1.0f;
    widget_set_relative_position(editor->root_widget, pos);
}

void
scene_animation_editor_show(te_scene_animation_editor* editor) {
    vec2 pos;
    widget_get_relative_position(editor->root_widget, pos);

    pos[0] = TE_SCENE_ANIM_EDITOR_X_POS;
    widget_set_relative_position(editor->root_widget, pos);
}

static char*
time_to_border_str(unsigned int src) {
    int len = snprintf(NULL, 0, "%u", src);
    if (len <= 0) {
        log_error("snprintf error");
        abort();
    }
    char* dst = malloc(sizeof(char) * (size_t)(len + 1 + 4));
    snprintf(dst, (size_t)(len + 1), "%u", src);
    memcpy(dst + len, " sec", 4);
    dst[len + 4] = 0;

    return dst;
}

static te_text_widget*
get_button_text(te_button_widget* button) {
    unsigned int child_count;
    te_widget** child_widgets =
        widget_get_child_widgets(button_widget_get_widget(button), &child_count);

    te_text_widget* button_text = NULL;
    for (unsigned int i = 0; i < child_count; i++) {
        if (!widget_is_serialization_allowed(child_widgets[i])) {
            // Internal widget (rect) of the button.
            continue;
        }
        button_text = widget_get_owner(child_widgets[i]);
        break;
    }

    free(child_widgets);
    return button_text;
}

static const char*
get_selected_obj_name(te_scene_animation_editor* editor) {
    if (editor->selected_obj == NULL || editor->selected_obj_type_info == NULL) {
        log_error("expected selected object info to be valid");
        abort();
    }

    te_game_object_info* go_info =
        editor->selected_obj_type_info->get_game_object_info(editor->selected_obj);
    if (go_info == NULL) {
        log_error("expected to have game object info to get object name");
        abort();
    }
    const char* obj_name = go_info->get_name(editor->selected_obj);
    if (obj_name == NULL) {
        log_error("expected object to have a name");
        abort();
    }

    return obj_name;
}

static void
redraw_timeline(te_scene_animation_editor* editor, bool update_keyframes) {
    // Update left border time.
    char* timestr = time_to_border_str(editor->left_border_time);
    unsigned int text_len;
    wchar_t* wtext = wchar_from_char(timestr, &text_len);
    text_widget_set_text_own(editor->left_border_time_text, wtext, text_len);
    free(timestr);

    // Update right border time.
    timestr = time_to_border_str(editor->right_border_time);
    wtext = wchar_from_char(timestr, &text_len);
    text_widget_set_text_own(editor->right_border_time_text, wtext, text_len);
    free(timestr);

    te_scene_animation* animation = world_get_scene_animation(editor->world);

    // Update current time separator and time text.
    {
        float current_time = 0.0f;
        if (animation != NULL) {
            current_time = glm_clamp(
                scene_animation_get_current_time(animation), (float)editor->left_border_time,
                (float)editor->right_border_time);
        }
        const float current_portion =
            current_time / (float)(editor->right_border_time - editor->left_border_time);

        te_widget* widget = rect_widget_get_widget(editor->current_time_separator);
        widget_set_relative_position(
            widget,
            (vec2){editor->timeline_x_pos + (1.0f - editor->timeline_x_pos) * current_portion,
                   editor->button_height});
        widget_set_relative_size(widget, (vec2){0.004f, 1.0f - editor->button_height});
        widget_set_parent(widget, editor->root_widget);

        // Current time.
        int len = snprintf(NULL, 0, "Time: %.1f sec", current_time);
        if (len < 0) {
            log_error("snprintf failed");
            abort();
        }
        char* time_text = malloc(sizeof(char) * (size_t)(len + 1));
        snprintf(time_text, (size_t)(len + 1), "Time: %.1f sec", current_time);

        unsigned int text_len;
        wchar_t* wtext = wchar_from_char(time_text, &text_len);
        text_widget_set_text_own(editor->time_widget, wtext, text_len);

        free(time_text);
    }

    if (!update_keyframes) {
        return;
    }

    // Clear all previously drawn keyframes.
    keyframe_button_array_clear(editor->keyframe_button_array);

    // First hide all tracks.
    for (unsigned int i = 0; i < editor->track_button_count; i++) {
        te_widget* widget = button_widget_get_widget(editor->track_buttons[i]);

        vec2 pos;
        widget_get_relative_position(widget, pos);
        pos[0] = BUTTON_HIDDEN_X_POS;

        widget_set_relative_position(button_widget_get_widget(editor->track_buttons[i]), pos);
    }

    if (editor->selected_obj == NULL || editor->selected_obj_type_info == NULL) {
        // Nothing selected - don't show any tracks.
        return;
    }

    const char* selected_obj_name = get_selected_obj_name(editor);

#define DISPLAY_ANIMATION_TRACKS(type)                                                        \
    {                                                                                         \
        unsigned int var_count;                                                               \
        char** var_names = scene_animation_get_##type##_variable_names(                       \
            animation, selected_obj_name, &var_count);                                        \
        for (unsigned int var_idx = 0;                                                        \
             var_idx < var_count && track_button_idx < editor->track_button_count;            \
             var_idx++) {                                                                     \
            if (editor->track_scroll_count > total_track_count) {                             \
                continue;                                                                     \
            }                                                                                 \
                                                                                              \
            te_text_widget* button_text =                                                     \
                get_button_text(editor->track_buttons[track_button_idx]);                     \
                                                                                              \
            unsigned int name_len;                                                            \
            wchar_t* wname = wchar_from_char(var_names[var_idx], &name_len);                  \
            text_widget_set_text_own(button_text, wname, name_len);                           \
                                                                                              \
            te_widget* widget =                                                               \
                button_widget_get_widget(editor->track_buttons[track_button_idx]);            \
            vec2 pos;                                                                         \
            widget_get_relative_position(widget, pos);                                        \
            pos[0] = editor->track_buttons_x;                                                 \
            widget_set_relative_position(widget, pos);                                        \
                                                                                              \
            widget_set_custom_value(widget, 0xFFFFFFFF); /* reflected type variable index */  \
                                                                                              \
            unsigned int keyframe_count;                                                      \
            te_scene_animation_keyframe_##type* keyframes =                                   \
                scene_animation_get_keyframes_##type(                                         \
                    animation, selected_obj_name, var_names[var_idx], &keyframe_count);       \
            for (unsigned int keyframe_idx = 0; keyframe_idx < keyframe_count;                \
                 keyframe_idx++) {                                                            \
                if (keyframes[keyframe_idx].time < (float)editor->left_border_time            \
                    || keyframes[keyframe_idx].time > (float)editor->right_border_time) {     \
                    continue;                                                                 \
                }                                                                             \
                float keyframe_x_portion =                                                    \
                    (keyframes[keyframe_idx].time - (float)editor->left_border_time)          \
                    / (float)(editor->right_border_time - editor->left_border_time);          \
                float keyframe_x = editor->timeline_x_pos                                     \
                                   + (1.0f - editor->timeline_x_pos) * keyframe_x_portion;    \
                float keyframe_y = pos[1];                                                    \
                keyframe_button_array_add(                                                    \
                    editor->keyframe_button_array, editor->root_widget, keyframe_x,           \
                    keyframe_y, (1.0f - editor->timeline_x_pos), editor->button_height);      \
            }                                                                                 \
                                                                                              \
            total_track_count += 1;                                                           \
            track_button_idx += 1;                                                            \
            if (track_button_idx == editor->track_button_count) {                             \
                break;                                                                        \
            }                                                                                 \
        }                                                                                     \
        free(var_names);                                                                      \
    }

    // Show already animated variables.
    unsigned int total_track_count = 0;
    unsigned int track_button_idx = 0;
    if (animation != NULL) {
        DISPLAY_ANIMATION_TRACKS(vec4)
        DISPLAY_ANIMATION_TRACKS(vec3)
        DISPLAY_ANIMATION_TRACKS(vec2)
        DISPLAY_ANIMATION_TRACKS(float)
        DISPLAY_ANIMATION_TRACKS(uint)
        DISPLAY_ANIMATION_TRACKS(bool)
    }

    // Skip 1 button as a separator between animated tracks and available tracks.
    track_button_idx += 1;

    // Show tracks for all reflected variables.
    if (track_button_idx < editor->track_button_count) {
#define DISPLAY_AVAILABLE_TRACKS(var_type)                                                    \
    for (unsigned int var_idx = 0; var_idx < editor->selected_obj_type_info->variable_count   \
                                   && track_button_idx < editor->track_button_count;          \
         var_idx++) {                                                                         \
        if (editor->selected_obj_type_info->variables[var_idx].type != TE_VT_##var_type) {    \
            continue;                                                                         \
        }                                                                                     \
        if (editor->track_scroll_count > total_track_count) {                                 \
            continue;                                                                         \
        }                                                                                     \
                                                                                              \
        te_text_widget* button_text =                                                         \
            get_button_text(editor->track_buttons[track_button_idx]);                         \
                                                                                              \
        unsigned int name_len;                                                                \
        wchar_t* wname = wchar_from_char(                                                     \
            editor->selected_obj_type_info->variables[var_idx].name, &name_len);              \
        text_widget_set_text_own(button_text, wname, name_len);                               \
                                                                                              \
        te_widget* widget =                                                                   \
            button_widget_get_widget(editor->track_buttons[track_button_idx]);                \
        vec2 pos;                                                                             \
        widget_get_relative_position(widget, pos);                                            \
        pos[0] = editor->track_buttons_x;                                                     \
        widget_set_relative_position(widget, pos);                                            \
                                                                                              \
        widget_set_custom_value(widget, var_idx); /* reflected type variable index */         \
                                                                                              \
        total_track_count += 1;                                                               \
        track_button_idx += 1;                                                                \
    }

        DISPLAY_AVAILABLE_TRACKS(VEC4)
        DISPLAY_AVAILABLE_TRACKS(VEC3)
        DISPLAY_AVAILABLE_TRACKS(VEC2)
        DISPLAY_AVAILABLE_TRACKS(FLOAT)
        DISPLAY_AVAILABLE_TRACKS(UINT)
        DISPLAY_AVAILABLE_TRACKS(BOOL)
    }
}

void
scene_animation_editor_show_tracks(
    te_scene_animation_editor* editor, void* obj, const te_type_info* type_info) {
    editor->selected_obj = obj;
    editor->selected_obj_type_info = type_info;

    redraw_timeline(editor, true);
}

static void
create_keyframe_for_selected_obj_variable(
    te_scene_animation_editor* editor, te_variable_info* var_info,
    float keyframe_time_portion) {
    if (editor->selected_obj == NULL || editor->selected_obj_type_info == NULL) {
        log_error("expected selected object info to be valid");
        abort();
    }

    te_scene_animation* animation = world_get_scene_animation(editor->world);
    if (animation == NULL) {
        animation = world_create_scene_animation(editor->world);
    }

    const char* obj_name = get_selected_obj_name(editor);

    float time_sec =
        (float)editor->left_border_time
        + keyframe_time_portion
              * ((float)editor->right_border_time - (float)editor->left_border_time);

    // If previously had 0 keyframes for the variable change the time to 0.
    unsigned int prev_keyframe_count;
#define CHECK_KEYFRAMES_COUNT(var_type)                                                       \
    scene_animation_get_keyframes_##var_type(                                                 \
        animation, obj_name, var_info->name, &prev_keyframe_count);                           \
    if (prev_keyframe_count == 0) {                                                           \
        time_sec = 0.0f;                                                                      \
    }

    switch (var_info->type) {
        case (TE_VT_BOOL): {
            CHECK_KEYFRAMES_COUNT(bool)
            scene_animation_add_keyframe_bool(
                animation, obj_name, var_info->name, time_sec,
                editor->selected_obj_type_info->bool_getters[var_info->set_get_index](
                    editor->selected_obj));
            break;
        }
        case (TE_VT_UINT): {
            CHECK_KEYFRAMES_COUNT(uint)
            scene_animation_add_keyframe_uint(
                animation, obj_name, var_info->name, time_sec,
                editor->selected_obj_type_info->uint_getters[var_info->set_get_index](
                    editor->selected_obj));
            break;
        }
        case (TE_VT_FLOAT): {
            CHECK_KEYFRAMES_COUNT(float)
            scene_animation_add_keyframe_float(
                animation, obj_name, var_info->name, time_sec,
                editor->selected_obj_type_info->float_getters[var_info->set_get_index](
                    editor->selected_obj));
            break;
        }
        case (TE_VT_VEC2): {
            CHECK_KEYFRAMES_COUNT(vec2)
            vec2 val;
            editor->selected_obj_type_info->vec2_getters[var_info->set_get_index](
                editor->selected_obj, val);
            scene_animation_add_keyframe_vec2(
                animation, obj_name, var_info->name, time_sec, val);
            break;
        }
        case (TE_VT_VEC3): {
            CHECK_KEYFRAMES_COUNT(vec3)
            vec3 val;
            editor->selected_obj_type_info->vec3_getters[var_info->set_get_index](
                editor->selected_obj, val);
            scene_animation_add_keyframe_vec3(
                animation, obj_name, var_info->name, time_sec, val);
            break;
        }
        case (TE_VT_VEC4): {
            CHECK_KEYFRAMES_COUNT(vec4)
            vec4 val;
            editor->selected_obj_type_info->vec4_getters[var_info->set_get_index](
                editor->selected_obj, val);
            scene_animation_add_keyframe_vec4(
                animation, obj_name, var_info->name, time_sec, val);
            break;
        }
        default: {
            break;
        }
    }

    redraw_timeline(editor, true);
}

void
prv_scene_animation_editor_on_mouse_click(
    te_scene_animation_editor* editor, enum te_mouse_button button, vec2 cursor_pos) {
    if (cursor_pos[0] < editor->timeline_x_pos) {
        return;
    }

    if (button == TE_MB_MIDDLE) {
        // Want to create keyframe. Find which track was clicked on.
        for (unsigned int button_idx = 0; button_idx < editor->track_button_count;
             button_idx++) {
            te_widget* widget = button_widget_get_widget(editor->track_buttons[button_idx]);

            vec2 pos;
            widget_get_relative_position(widget, pos);
            if (pos[0] >= BUTTON_HIDDEN_X_POS - 0.01f) {
                continue;
            }
            if (cursor_pos[1] < pos[1]) {
                continue;
            }

            vec2 size;
            widget_get_relative_size(widget, size);
            if (cursor_pos[1] > pos[1] + size[1]) {
                continue;
            }

            float keyframe_time_portion =
                (cursor_pos[0] - editor->timeline_x_pos) / (1.0f - editor->timeline_x_pos);

            size_t var_idx = widget_get_custom_value(widget);
            if (var_idx == 0xFFFFFFFF) {
                // Add a keyframe for already animated track.
                te_text_widget* text_widget =
                    get_button_text(editor->track_buttons[button_idx]);
                unsigned int len;
                wchar_t* wtext = text_widget_get_text(text_widget, &len);
                char* variable_name = wchar_to_char(wtext, NULL);

                te_variable_info* var_info = NULL;
                for (unsigned int i = 0; i < editor->selected_obj_type_info->variable_count;
                     i++) {
                    if (strcmp(
                            editor->selected_obj_type_info->variables[i].name, variable_name)
                        != 0) {
                        continue;
                    }
                    var_info = &editor->selected_obj_type_info->variables[i];
                    break;
                }
                if (var_info == NULL) {
                    log_error_fmt("failed to find variable info for %s", variable_name);
                    abort();
                }

                create_keyframe_for_selected_obj_variable(
                    editor, var_info, keyframe_time_portion);

                free(variable_name);
            } else {
                // Add a keyframe for a new track.
                create_keyframe_for_selected_obj_variable(
                    editor, &editor->selected_obj_type_info->variables[var_idx],
                    keyframe_time_portion);
            }

            return;
        }
    } else if (button == TE_MB_LEFT) {
        // Move current position or select a keyframe.
        // TODO
    }
}

void
prv_scene_animation_editor_tick(te_scene_animation_editor* editor) {
    redraw_timeline(editor, false);
}

static void
on_play_pause_clicked(te_button_widget* button) {
    te_widget* widget = button_widget_get_widget(button);
    te_scene_animation_editor* editor = widget_get_custom_ptr(widget);

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    if (scene_animation_is_playing(anim)) {
        scene_animation_pause(anim);
    } else {
        scene_animation_play(anim);
    }
}

static void
on_stop_clicked(te_button_widget* button) {
    te_widget* widget = button_widget_get_widget(button);
    te_scene_animation_editor* editor = widget_get_custom_ptr(widget);

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    scene_animation_stop(anim);
}

static void
on_track_right_clicked(te_button_widget* button) {
    te_widget* widget = button_widget_get_widget(button);
    te_scene_animation_editor* editor = widget_get_custom_ptr(widget);

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    if (scene_animation_is_playing(anim)) {
        scene_animation_stop(anim);
    }

    const char* obj_name = get_selected_obj_name(editor);
    te_text_widget* text_widget = get_button_text(button);

    unsigned int len;
    wchar_t* wtext = text_widget_get_text(text_widget, &len);

    char* variable_name = wchar_to_char(wtext, NULL);
    for (unsigned int i = 0; i < editor->selected_obj_type_info->variable_count; i++) {
        te_variable_info* var_info = &editor->selected_obj_type_info->variables[i];
        if (strcmp(var_info->name, variable_name) != 0) {
            continue;
        }

        switch (var_info->type) {
            case (TE_VT_BOOL): {
                scene_animation_remove_keyframes_bool(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_FLOAT): {
                scene_animation_remove_keyframes_float(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_UINT): {
                scene_animation_remove_keyframes_uint(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_VEC2): {
                scene_animation_remove_keyframes_vec2(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_VEC3): {
                scene_animation_remove_keyframes_vec3(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_VEC4): {
                scene_animation_remove_keyframes_vec4(anim, obj_name, variable_name);
                break;
            }
            default: {
                log_error("unhandled case");
                abort();
                break;
            }
        }

        break;
    }

    free(variable_name);

    redraw_timeline(editor, true);
}
