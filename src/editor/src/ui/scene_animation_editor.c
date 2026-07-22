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
#include <game/game_object_info.h>
#include <ui/theme.h>
#include <game/scene_animation.h>

#define TE_SCENE_ANIM_EDITOR_X_POS 0.05f
#define TE_SCENE_ANIM_MENU_WIDTH 0.09f
#define TE_SCENE_ANIM_TRACK_NAME_WIDTH 0.14f
#define BUTTON_HIDDEN_X_POS 10.0f

struct te_scene_animation_editor {
    te_world* world;
    te_widget* root_widget;

    te_text_widget* left_border_time_text;
    te_text_widget* right_border_time_text;

    te_scene_animation* animation;

    te_button_widget** track_buttons;

    // Not NULL if showing available tracks from an object.
    void* selected_obj;
    const te_type_info* selected_obj_type_info;

    float current_time;

    float timeline_x_pos;
    float button_height;
    float track_buttons_x;

    unsigned int track_button_count;
    unsigned int track_scroll_count;

    unsigned int left_border_time;
    unsigned int right_border_time;
};

static void redraw_timeline(te_scene_animation_editor* editor);

te_scene_animation_editor*
scene_animation_editor_create(te_world* world) {
    te_scene_animation_editor* editor = malloc(sizeof(te_scene_animation_editor));
    editor->world = world;
    editor->left_border_time = 0;
    editor->right_border_time = 4;
    editor->current_time = 0.0f;
    editor->track_scroll_count = 0;
    editor->animation = scene_animation_create();
    editor->selected_obj = NULL;
    editor->selected_obj_type_info = NULL;

    vec2 pos;
    glm_vec2_copy((vec2){TE_SCENE_ANIM_EDITOR_X_POS, 0.73f}, pos);

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
                wchar_t* wtext = wchar_from_char("Stop", &text_len);
                text_widget_set_text_own(text, wtext, text_len);
            }
        }
        y_pos += button_height + vspacing;

        // Time.
        {
            te_text_widget* text = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text);
                widget_set_relative_position(widget, (vec2){padding_x, y_pos});
                widget_set_relative_size(widget, (vec2){button_width, button_height});
                widget_set_parent(widget, editor->root_widget);
            }

            text_widget_set_text_height(text, theme_get_text_height());

            unsigned int text_len;
            wchar_t* wtext = wchar_from_char("00:00 / 00:00", &text_len);
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
                y += button_height + vspacing;
            }

            y = timeline_top_height;
            editor->track_buttons =
                malloc(sizeof(te_button_widget*) * editor->track_button_count);
            for (unsigned int i = 0; i < editor->track_button_count; i++) {
                te_button_widget* button = button_widget_create();
                editor->track_buttons[i] = button;
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, (vec2){x, y});
                    widget_set_relative_size(
                        widget, (vec2){TE_SCENE_ANIM_TRACK_NAME_WIDTH, button_height});
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
                    wchar_t* wtext = wchar_from_char("<track name>", &text_len);
                    text_widget_set_text_own(text, wtext, text_len);
                }

                if (i % 2 != 0) {
                    // Also draw special background for non-even items (for better visibility).
                    te_rect_widget* background = rect_widget_create();
                    {
                        te_widget* widget = rect_widget_get_widget(background);
                        widget_set_relative_position(widget, (vec2){timeline_x, y});
                        widget_set_relative_size(
                            widget, (vec2){1.0f - timeline_x, button_height});
                        widget_set_parent(widget, editor->root_widget);
                    }
                    rect_widget_set_color(background, separator_color);
                }

                y += button_height + vspacing;
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
        const unsigned int section_count = 20;
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
    }

    redraw_timeline(editor);

    world_spawn_widget(world, editor->root_widget);

    return editor;
}

void
scene_animation_editor_destroy(te_scene_animation_editor* editor) {
    world_despawn_widget(editor->world, editor->root_widget);
    widget_destroy(editor->root_widget);

    free(editor->track_buttons);

    if (editor->animation != NULL) {
        scene_animation_destroy(editor->animation);
    }

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

static void
redraw_timeline(te_scene_animation_editor* editor) {
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

    // Current time separator.
    {
        const float current_portion =
            editor->current_time
            / (float)(editor->right_border_time - editor->left_border_time);

        te_rect_widget* separator = rect_widget_create();
        {
            te_widget* widget = rect_widget_get_widget(separator);
            widget_set_relative_position(
                widget, (vec2){editor->timeline_x_pos
                                   + (1.0f - editor->timeline_x_pos) * current_portion,
                               editor->button_height});
            widget_set_relative_size(widget, (vec2){0.004f, 1.0f - editor->button_height});
            widget_set_parent(widget, editor->root_widget);
        }

        vec4 color;
        theme_get_accent_color(color);
        rect_widget_set_color(separator, color);
    }

    // First hide all tracks.
    for (unsigned int i = 0; i < editor->track_button_count; i++) {
        te_widget* widget = button_widget_get_widget(editor->track_buttons[i]);

        vec2 pos;
        widget_get_relative_position(widget, pos);
        pos[0] = BUTTON_HIDDEN_X_POS;

        widget_set_relative_position(button_widget_get_widget(editor->track_buttons[i]), pos);
    }

#define DISPLAY_ANIMATION_TRACKS(type)                                                        \
    {                                                                                         \
        unsigned int var_count;                                                               \
        char** var_names = scene_animation_get_##type##_variable_names(                       \
            editor->animation, obj_names[obj_idx], &var_count);                               \
        for (unsigned int var_idx = 0; var_idx < var_count; var_idx++) {                      \
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
            vec2 pos;                                                                         \
            widget_get_relative_position(                                                     \
                button_widget_get_widget(editor->track_buttons[track_button_idx]), pos);      \
            pos[0] = editor->track_buttons_x;                                                 \
            widget_set_relative_position(                                                     \
                button_widget_get_widget(editor->track_buttons[track_button_idx]), pos);      \
                                                                                              \
            total_track_count += 1;                                                           \
            track_button_idx += 1;                                                            \
            if (track_button_idx == editor->track_button_count) {                             \
                break;                                                                        \
            }                                                                                 \
        }                                                                                     \
        free(var_names);                                                                      \
        if (track_button_idx == editor->track_button_count) {                                 \
            break;                                                                            \
        }                                                                                     \
    }

    // Show only available tracks.
    unsigned int total_track_count = 0;
    unsigned int track_button_idx = 0;
    unsigned int obj_count;
    char** obj_names = scene_animation_get_object_names(editor->animation, &obj_count);
    for (unsigned int obj_idx = 0; obj_idx < obj_count; obj_idx++) {
        DISPLAY_ANIMATION_TRACKS(vec4)
        DISPLAY_ANIMATION_TRACKS(vec3)
        DISPLAY_ANIMATION_TRACKS(vec2)
        DISPLAY_ANIMATION_TRACKS(float)
        DISPLAY_ANIMATION_TRACKS(uint)
        DISPLAY_ANIMATION_TRACKS(bool)
    }
    free(obj_names);

    if (editor->selected_obj != NULL && editor->selected_obj_type_info != NULL
        && track_button_idx < editor->track_button_count) {
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
        vec2 pos;                                                                             \
        widget_get_relative_position(                                                         \
            button_widget_get_widget(editor->track_buttons[track_button_idx]), pos);          \
        pos[0] = editor->track_buttons_x;                                                     \
        widget_set_relative_position(                                                         \
            button_widget_get_widget(editor->track_buttons[track_button_idx]), pos);          \
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

    redraw_timeline(editor);
}
