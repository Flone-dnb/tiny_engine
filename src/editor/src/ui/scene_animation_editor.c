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
#include <widget/text_edit_widget.h>
#include <misc/wchar_funcs.h>
#include <type_database.h>
#include <misc/globals.h>
#include <ui/theme.h>
#include <io/filesystem.h>
#include <game/game_object_info.h>
#include <game/scene_animation.h>
#include <editor.h>

#define TE_SCENE_ANIM_EDITOR_X_POS 0.05f
#define TE_SCENE_ANIM_EDITOR_Y_POS 0.73f
#define TE_SCENE_ANIM_MENU_WIDTH 0.15f
#define TE_SCENE_ANIM_TRACK_NAME_WIDTH 0.14f
#define BUTTON_HIDDEN_X_POS 10.0f

enum te_scene_animation_editor_scroll_mode {
    TE_SAESM_VERTICAL_SCROLL,
    TE_SAESM_ZOOM,
    TE_SAESM_HORIZONTAL_SCROLL,
};

typedef struct te_keyframe_button_data {
    te_scene_animation_editor* editor;
    char* variable_name;
    void* keyframe;
    float time_sec;
    enum te_scene_animation_interpolation_type interpolation;
} te_keyframe_button_data;

typedef struct te_keyframe_button_array {
    te_button_widget** buttons;
    te_keyframe_button_data* datas;
    unsigned int count;
    unsigned int capacity;
    unsigned int selected_idx; // 0xFFFFFFFF if not selected
} te_keyframe_button_array;

static te_keyframe_button_array*
keyframe_button_array_create(void) {
    te_keyframe_button_array* array = malloc(sizeof(te_keyframe_button_array));
    array->buttons = NULL;
    array->datas = NULL;
    array->count = 0;
    array->capacity = 0;
    array->selected_idx = 0xFFFFFFFF;
    return array;
}

static void
keyframe_button_array_select_keyframe(
    te_keyframe_button_array* array, unsigned int button_idx) {
    if (array->selected_idx != 0xFFFFFFFF) {
        button_widget_set_color(
            array->buttons[array->selected_idx], (vec4){1.0f, 0.0f, 0.0f, 1.0f});
        button_widget_set_color_hovered(
            array->buttons[array->selected_idx], (vec4){1.0f, 0.1f, 0.1f, 1.0f});
    }

    array->selected_idx = button_idx;

    if (array->selected_idx != 0xFFFFFFFF) {
        button_widget_set_color(
            array->buttons[array->selected_idx], (vec4){0.8f, 0.8f, 0.8f, 1.0f});
        button_widget_set_color_hovered(
            array->buttons[array->selected_idx], (vec4){1.0f, 1.0f, 1.0f, 1.0f});
    }
}

static void
keyframe_button_array_add(
    te_keyframe_button_array* array, te_scene_animation_editor* editor, te_widget* root_widget,
    void* keyframe, enum te_scene_animation_interpolation_type interpolation, float time_sec,
    const char* variable_name, float topleft_x, float topleft_y, float track_width,
    float track_height) {
    if (array->count == array->capacity) {
        keyframe_button_array_select_keyframe(array, 0xFFFFFFFF);

        array->capacity += 50;

        te_button_widget** new_buttons = malloc(sizeof(te_button_widget*) * array->capacity);
        memcpy(new_buttons, array->buttons, sizeof(te_button_widget*) * array->count);

        free(array->buttons);
        array->buttons = new_buttons;

        te_keyframe_button_data* new_datas =
            malloc(sizeof(te_keyframe_button_data) * array->capacity);
        free(array->datas);
        array->datas = new_datas;
    }

    te_button_widget* button = button_widget_create();
    {
        float aspect = track_width / track_height;
        vec2 size;
        size[1] = track_height / 2.0f;
        size[0] = size[1] / aspect;

        te_widget* widget = button_widget_get_widget(button);
        widget_set_parent(widget, root_widget);
        widget_set_relative_position(
            widget, (vec2){topleft_x - size[0] / 2.0f, topleft_y + size[1] / 2.0f});
        widget_set_relative_size(widget, size);

        te_keyframe_button_data* data = &array->datas[array->count];
        data->editor = editor;
        data->keyframe = keyframe;
        data->time_sec = time_sec;
        data->interpolation = interpolation;

        size_t len = strlen(variable_name);
        data->variable_name = malloc(sizeof(char) * (len + 1));
        memcpy(data->variable_name, variable_name, sizeof(char) * len);
        data->variable_name[len] = 0;

        widget_set_custom_ptr(widget, data);
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

        free(array->datas[i].variable_name);
    }
    array->count = 0;
    array->capacity = 0;
    free(array->buttons);
    free(array->datas);
    array->buttons = NULL;
    array->datas = NULL;
    array->selected_idx = 0xFFFFFFFF;
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
    te_editor* engine_editor;

    te_text_widget* left_border_time_text;
    te_text_widget* right_border_time_text;
    te_text_edit_widget* time_widget;

    // Visible only when keyframe is selected.
    te_text_widget* keyframe_time_widget;
    te_text_edit_widget* keyframe_time_edit_widget;
    te_text_widget* keyframe_interpolation_widget;
    te_button_widget* keyframe_interpolation_button_step;
    te_button_widget* keyframe_interpolation_button_linear;
    te_button_widget* keyframe_interpolation_button_cubic;
    te_button_widget* keyframe_update_value_button;

    te_button_widget** track_buttons;

    te_button_widget* play_pause_button;

    te_rect_widget* current_time_separator;

    // Not NULL if showing available tracks from an object.
    void* selected_obj;
    const te_type_info* selected_obj_type_info;

    te_keyframe_button_array* keyframe_button_array;

    float timeline_x_pos;
    float button_height;
    float track_buttons_x;
    float menu_padding_x;
    float keyframe_time_edit_x;
    float keyframe_interpolation_buttons_x;
    float interpolation_button_width;

    unsigned int track_button_count;
    unsigned int track_scroll_count;

    unsigned int left_border_time;
    unsigned int right_border_time;

    // How much seconds between left and right time border on the timeline.
    unsigned int displayed_time_interval;

    enum te_scene_animation_editor_scroll_mode scroll_mode;
};

static void redraw_timeline(te_scene_animation_editor* editor, bool);
static void on_play_pause_clicked(te_button_widget* button);
static void on_stop_clicked(te_button_widget* button);
static void on_track_right_clicked(te_button_widget* button);
static void on_keyframe_time_changed(te_text_edit_widget* text_edit);
static void on_current_time_changed(te_text_edit_widget* text_edit);
static void on_keyframe_interpolation_changed(te_button_widget* button);
static void on_update_keyframe_value_clicked(te_button_widget* button);
static void on_save_as_clicked(te_button_widget* button);
static void on_load_clicked(te_button_widget* button);

te_scene_animation_editor*
scene_animation_editor_create(te_world* world, te_editor* engine_editor) {
    te_scene_animation_editor* editor = malloc(sizeof(te_scene_animation_editor));
    editor->engine_editor = engine_editor;
    editor->world = world;
    editor->displayed_time_interval = 4;
    editor->left_border_time = 0;
    editor->right_border_time = editor->displayed_time_interval;
    editor->track_scroll_count = 0;
    editor->selected_obj = NULL;
    editor->selected_obj_type_info = NULL;
    editor->keyframe_button_array = keyframe_button_array_create();
    editor->scroll_mode = TE_SAESM_VERTICAL_SCROLL;

    vec2 pos;
    glm_vec2_copy((vec2){TE_SCENE_ANIM_EDITOR_X_POS, TE_SCENE_ANIM_EDITOR_Y_POS}, pos);

    vec2 size;
    glm_vec2_copy((vec2){1.0f - pos[0] * 2.0f, 0.99f - pos[1]}, size);

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
            editor->play_pause_button = button;
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

        // Current time.
        {
            float title_width = button_width * 0.5f;

            // Title
            {
                te_text_widget* text = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text);
                    widget_set_relative_position(widget, (vec2){padding_x, y_pos});
                    widget_set_relative_size(widget, (vec2){title_width, button_height});
                    widget_set_parent(widget, editor->root_widget);
                }

                text_widget_set_text_height(text, theme_get_text_height());

                unsigned int text_len;
                wchar_t* wtext = wchar_from_char("Current time: ", &text_len);
                text_widget_set_text_own(text, wtext, text_len);
            }

            // Background.
            te_rect_widget* rect = rect_widget_create();
            {
                te_widget* widget = rect_widget_get_widget(rect);
                widget_set_relative_position(
                    widget, (vec2){padding_x + title_width + padding_x, y_pos});
                widget_set_relative_size(
                    widget,
                    (vec2){button_width - title_width - padding_x * 2.0f, button_height});
                widget_set_parent(widget, editor->root_widget);
            }
            vec4 text_edit_background_color;
            theme_get_text_edit_background_color(text_edit_background_color);
            rect_widget_set_color(rect, text_edit_background_color);

            // Text edit.
            te_text_edit_widget* text = text_edit_widget_create();
            editor->time_widget = text;
            {
                te_widget* widget = text_edit_widget_get_widget(text);
                widget_set_relative_position(widget, (vec2){0.1f, 0.0f});
                widget_set_relative_size(widget, (vec2){0.9f, 1.0f});
                widget_set_parent(widget, rect_widget_get_widget(rect));

                widget_set_custom_ptr(widget, editor);
            }

            text_edit_widget_set_text_height(text, theme_get_text_height());
            text_edit_widget_set_on_text_accepted(text, on_current_time_changed);

            unsigned int text_len;
            wchar_t* wtext = wchar_from_char("0.0", &text_len);
            text_edit_widget_set_text_own(text, wtext, text_len);
        }
        y_pos += button_height + vspacing;

        // When keyframe is selected:
        {
            editor->menu_padding_x = padding_x;

            float title_width = button_width * 0.65f;
            editor->keyframe_time_edit_x = padding_x + title_width;

            // Keyframe time.
            {
                // Title.
                {
                    te_text_widget* text = text_widget_create();
                    editor->keyframe_time_widget = text;
                    {
                        te_widget* widget = text_widget_get_widget(text);
                        widget_set_relative_position(
                            widget, (vec2){BUTTON_HIDDEN_X_POS, y_pos});
                        widget_set_relative_size(widget, (vec2){title_width, button_height});
                        widget_set_parent(widget, editor->root_widget);
                    }

                    text_widget_set_text_height(text, theme_get_text_height());

                    unsigned int text_len;
                    wchar_t* wtext = wchar_from_char("Keyframe time: ", &text_len);
                    text_widget_set_text_own(text, wtext, text_len);
                }

                // Text edit.
                {
                    // Background.
                    te_rect_widget* rect = rect_widget_create();
                    {
                        te_widget* widget = rect_widget_get_widget(rect);
                        widget_set_relative_position(
                            widget, (vec2){BUTTON_HIDDEN_X_POS, y_pos});
                        widget_set_relative_size(
                            widget, (vec2){button_width - title_width, button_height});
                        widget_set_parent(widget, editor->root_widget);
                    }
                    vec4 text_edit_background_color;
                    theme_get_text_edit_background_color(text_edit_background_color);
                    rect_widget_set_color(rect, text_edit_background_color);

                    // Text edit.
                    te_text_edit_widget* text_edit = text_edit_widget_create();
                    editor->keyframe_time_edit_widget = text_edit;
                    {
                        te_widget* widget = text_edit_widget_get_widget(text_edit);
                        widget_set_parent(widget, rect_widget_get_widget(rect));
                        widget_set_relative_position(widget, (vec2){0.05f, 0.0f});
                        widget_set_relative_size(widget, (vec2){1.0f, 1.0f});

                        widget_set_custom_ptr(widget, editor);
                    }

                    text_edit_widget_set_text_height(text_edit, theme_get_text_height());
                    text_edit_widget_set_on_text_accepted(text_edit, on_keyframe_time_changed);
                }
            }
            y_pos += button_height + vspacing;

            title_width = button_width * 0.5f;
            editor->keyframe_interpolation_buttons_x = padding_x + title_width;

            // Keyframe interpolation.
            {
                // Title.
                {
                    te_text_widget* text = text_widget_create();
                    editor->keyframe_interpolation_widget = text;
                    {
                        te_widget* widget = text_widget_get_widget(text);
                        widget_set_relative_position(
                            widget, (vec2){BUTTON_HIDDEN_X_POS, y_pos});
                        widget_set_relative_size(widget, (vec2){title_width, button_height});
                        widget_set_parent(widget, editor->root_widget);
                    }

                    text_widget_set_text_height(text, theme_get_text_height());

                    unsigned int text_len;
                    wchar_t* wtext = wchar_from_char("Interpolation: ", &text_len);
                    text_widget_set_text_own(text, wtext, text_len);
                }

                float interpolation_button_width = (button_width - title_width) / 3;
                editor->interpolation_button_width = interpolation_button_width;

                // Step.
                {
                    te_button_widget* button = button_widget_create();
                    editor->keyframe_interpolation_button_step = button;
                    {
                        te_widget* widget = button_widget_get_widget(button);
                        widget_set_relative_position(
                            widget, (vec2){BUTTON_HIDDEN_X_POS, y_pos});
                        widget_set_relative_size(
                            widget, (vec2){interpolation_button_width, button_height});
                        widget_set_parent(widget, editor->root_widget);

                        widget_set_custom_ptr(widget, editor);
                    }

                    theme_get_button_color(color);
                    button_widget_set_color(button, color);

                    theme_get_button_color_hovered(color);
                    button_widget_set_color_hovered(button, color);

                    theme_get_button_color_pressed(color);
                    button_widget_set_color_pressed(button, color);

                    button_widget_set_on_clicked(button, on_keyframe_interpolation_changed);

                    // Text.
                    {
                        te_text_widget* text = text_widget_create();
                        {
                            te_widget* widget = text_widget_get_widget(text);
                            widget_set_relative_position(widget, (vec2){0.3f, 0.0f});
                            widget_set_relative_size(widget, (vec2){0.7f, 1.0f});
                            widget_set_parent(widget, button_widget_get_widget(button));
                        }

                        text_widget_set_text_height(text, theme_get_text_height());

                        unsigned int text_len;
                        wchar_t* wtext = wchar_from_char("S", &text_len);
                        text_widget_set_text_own(text, wtext, text_len);
                    }
                }

                // Linear.
                {
                    te_button_widget* button = button_widget_create();
                    editor->keyframe_interpolation_button_linear = button;
                    {
                        te_widget* widget = button_widget_get_widget(button);
                        widget_set_relative_position(
                            widget, (vec2){BUTTON_HIDDEN_X_POS, y_pos});
                        widget_set_relative_size(
                            widget, (vec2){interpolation_button_width, button_height});
                        widget_set_parent(widget, editor->root_widget);

                        widget_set_custom_ptr(widget, editor);
                    }

                    theme_get_button_color(color);
                    button_widget_set_color(button, color);

                    theme_get_button_color_hovered(color);
                    button_widget_set_color_hovered(button, color);

                    theme_get_button_color_pressed(color);
                    button_widget_set_color_pressed(button, color);

                    button_widget_set_on_clicked(button, on_keyframe_interpolation_changed);

                    // Text.
                    {
                        te_text_widget* text = text_widget_create();
                        {
                            te_widget* widget = text_widget_get_widget(text);
                            widget_set_relative_position(widget, (vec2){0.3f, 0.0f});
                            widget_set_relative_size(widget, (vec2){0.7f, 1.0f});
                            widget_set_parent(widget, button_widget_get_widget(button));
                        }

                        text_widget_set_text_height(text, theme_get_text_height());

                        unsigned int text_len;
                        wchar_t* wtext = wchar_from_char("L", &text_len);
                        text_widget_set_text_own(text, wtext, text_len);
                    }
                }

                // Cubic spline.
                {
                    te_button_widget* button = button_widget_create();
                    editor->keyframe_interpolation_button_cubic = button;
                    {
                        te_widget* widget = button_widget_get_widget(button);
                        widget_set_relative_position(
                            widget, (vec2){BUTTON_HIDDEN_X_POS, y_pos});
                        widget_set_relative_size(
                            widget, (vec2){interpolation_button_width, button_height});
                        widget_set_parent(widget, editor->root_widget);

                        widget_set_custom_ptr(widget, editor);
                    }

                    theme_get_button_color(color);
                    button_widget_set_color(button, color);

                    theme_get_button_color_hovered(color);
                    button_widget_set_color_hovered(button, color);

                    theme_get_button_color_pressed(color);
                    button_widget_set_color_pressed(button, color);

                    button_widget_set_on_clicked(button, on_keyframe_interpolation_changed);

                    // Text.
                    {
                        te_text_widget* text = text_widget_create();
                        {
                            te_widget* widget = text_widget_get_widget(text);
                            widget_set_relative_position(widget, (vec2){0.3f, 0.0f});
                            widget_set_relative_size(widget, (vec2){0.7f, 1.0f});
                            widget_set_parent(widget, button_widget_get_widget(button));
                        }

                        text_widget_set_text_height(text, theme_get_text_height());

                        unsigned int text_len;
                        wchar_t* wtext = wchar_from_char("C", &text_len);
                        text_widget_set_text_own(text, wtext, text_len);
                    }
                }
            }
            y_pos += button_height + vspacing;

            // Update keyframe value.
            {
                te_button_widget* button = button_widget_create();
                editor->keyframe_update_value_button = button;
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, y_pos});
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

                button_widget_set_on_clicked(button, on_update_keyframe_value_clicked);

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
                    wchar_t* wtext = wchar_from_char("Update keyframe value", &text_len);
                    text_widget_set_text_own(text, wtext, text_len);
                }
            }
            y_pos += button_height + vspacing;
        }

        // Switch to bottom.
        y_pos = 1.0f - padding_y - button_height - padding_y - button_height;

        // Save button.
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

            button_widget_set_on_clicked(button, on_save_as_clicked);

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

        // Load button.
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

            button_widget_set_on_clicked(button, on_load_clicked);

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
                widget_set_relative_position(widget, (vec2){1.0f - button_width / 4.0f, 0.0f});
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
show_selected_keyframe_menu(te_scene_animation_editor* editor) {
    te_keyframe_button_data* keyframe_data =
        &editor->keyframe_button_array->datas[editor->keyframe_button_array->selected_idx];

    // Keyframe time title.
    {
        te_widget* widget = text_widget_get_widget(editor->keyframe_time_widget);
        vec2 pos;
        widget_get_relative_position(widget, pos);

        widget_set_relative_position(widget, (vec2){editor->menu_padding_x, pos[1]});
    }

    // Keyframe time edit.
    {
        te_widget* rect = widget_get_parent( // taking parent rect (background of text edit)
            text_edit_widget_get_widget(editor->keyframe_time_edit_widget));
        vec2 pos;
        widget_get_relative_position(rect, pos);

        widget_set_relative_position(rect, (vec2){editor->keyframe_time_edit_x, pos[1]});

        int len = snprintf(NULL, 0, "%.2f", keyframe_data->time_sec);
        if (len < 0) {
            log_error("snprintf error");
            abort();
        }
        char* src_text = malloc(sizeof(char) * (size_t)(len + 1));
        snprintf(src_text, (size_t)len + 1, "%.2f", keyframe_data->time_sec);

        unsigned int text_len;
        wchar_t* text = wchar_from_char(src_text, &text_len);
        text_edit_widget_set_text_own(editor->keyframe_time_edit_widget, text, text_len);

        free(src_text);
    }

    // Interpolation.
    {
        // Title.
        {
            te_widget* widget = text_widget_get_widget(editor->keyframe_interpolation_widget);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(widget, (vec2){editor->menu_padding_x, pos[1]});
        }

        // Step.
        {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_interpolation_button_step);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(
                widget, (vec2){editor->keyframe_interpolation_buttons_x, pos[1]});

            vec4 color;
            if (keyframe_data->interpolation == TE_SAIT_STEP) {
                theme_get_accent_color(color);
            } else {
                theme_get_button_color(color);
            }
            button_widget_set_color(editor->keyframe_interpolation_button_step, color);
        }

        // Linear.
        {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_interpolation_button_linear);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(
                widget, (vec2){editor->keyframe_interpolation_buttons_x
                                   + editor->interpolation_button_width,
                               pos[1]});

            vec4 color;
            if (keyframe_data->interpolation == TE_SAIT_LINEAR) {
                theme_get_accent_color(color);
            } else {
                theme_get_button_color(color);
            }
            button_widget_set_color(editor->keyframe_interpolation_button_linear, color);
        }

        // Cubic.
        {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_interpolation_button_cubic);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(
                widget, (vec2){editor->keyframe_interpolation_buttons_x
                                   + editor->interpolation_button_width * 2,
                               pos[1]});

            vec4 color;
            if (keyframe_data->interpolation == TE_SAIT_CUBIC_SPLINE) {
                theme_get_accent_color(color);
            } else {
                theme_get_button_color(color);
            }
            button_widget_set_color(editor->keyframe_interpolation_button_cubic, color);
        }
    }

    // Update keyframe value.
    {
        te_widget* widget = button_widget_get_widget(editor->keyframe_update_value_button);
        vec2 pos;
        widget_get_relative_position(widget, pos);

        widget_set_relative_position(widget, (vec2){editor->menu_padding_x, pos[1]});
    }
}

static void
hide_selected_keyframe_menu(te_scene_animation_editor* editor) {
    // Keyframe time title.
    {
        te_widget* widget = text_widget_get_widget(editor->keyframe_time_widget);
        vec2 pos;
        widget_get_relative_position(widget, pos);

        widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, pos[1]});
    }

    // Keyframe time edit.
    {
        te_widget* rect = widget_get_parent( // taking parent rect (background of text edit)
            text_edit_widget_get_widget(editor->keyframe_time_edit_widget));
        vec2 pos;
        widget_get_relative_position(rect, pos);

        widget_set_relative_position(rect, (vec2){BUTTON_HIDDEN_X_POS, pos[1]});
    }

    // Interpolation.
    {
        // Title.
        {
            te_widget* widget = text_widget_get_widget(editor->keyframe_interpolation_widget);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, pos[1]});
        }

        // Step.
        {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_interpolation_button_step);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, pos[1]});

            vec4 color;
            theme_get_button_color(color);
            button_widget_set_color(editor->keyframe_interpolation_button_step, color);
        }

        // Linear.
        {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_interpolation_button_linear);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, pos[1]});

            vec4 color;
            theme_get_button_color(color);
            button_widget_set_color(editor->keyframe_interpolation_button_linear, color);
        }

        // Cubic.
        {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_interpolation_button_cubic);
            vec2 pos;
            widget_get_relative_position(widget, pos);

            widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, pos[1]});

            vec4 color;
            theme_get_button_color(color);
            button_widget_set_color(editor->keyframe_interpolation_button_cubic, color);
        }
    }

    // Update keyframe value.
    {
        te_widget* widget = button_widget_get_widget(editor->keyframe_update_value_button);
        vec2 pos;
        widget_get_relative_position(widget, pos);

        widget_set_relative_position(widget, (vec2){BUTTON_HIDDEN_X_POS, pos[1]});
    }
}

static void
redraw_timeline(te_scene_animation_editor* editor, bool update_keyframes) {
    te_scene_animation* animation = world_get_scene_animation(editor->world);

    // Update current time separator and time text.
    {
        float current_time = 0.0f;
        if (animation != NULL) {
            current_time = scene_animation_get_current_time(animation);
        }

        if (current_time > (float)editor->right_border_time) {
            // Switch to right (useful when playing the animation).
            editor->left_border_time = editor->right_border_time;
            editor->right_border_time =
                editor->left_border_time + editor->displayed_time_interval;
        }

        te_widget* widget = rect_widget_get_widget(editor->current_time_separator);

        if (current_time < (float)editor->left_border_time) {
            // Don't show separator.
            widget_set_relative_position(
                widget, (vec2){BUTTON_HIDDEN_X_POS, editor->button_height});
        } else {
            const float current_portion =
                (current_time - (float)editor->left_border_time)
                / (float)(editor->right_border_time - editor->left_border_time);
            widget_set_relative_position(
                widget, (vec2){editor->timeline_x_pos
                                   + (1.0f - editor->timeline_x_pos) * current_portion,
                               editor->button_height});
        }

        widget_set_relative_size(widget, (vec2){0.004f, 1.0f - editor->button_height});
        widget_set_parent(widget, editor->root_widget);

        // Current time text.
        int len = snprintf(NULL, 0, "%.1f", current_time);
        if (len < 0) {
            log_error("snprintf failed");
            abort();
        }
        char* time_text = malloc(sizeof(char) * (size_t)(len + 1));
        snprintf(time_text, (size_t)(len + 1), "%.1f", current_time);

        unsigned int text_len;
        wchar_t* wtext = wchar_from_char(time_text, &text_len);
        text_edit_widget_set_text_own(editor->time_widget, wtext, text_len);

        free(time_text);
    }

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

    if (editor->keyframe_button_array->selected_idx == 0xFFFFFFFF) {
        hide_selected_keyframe_menu(editor);
    }

    // Play / pause button state.
    if (animation != NULL && scene_animation_is_playing(animation)) {
        vec4 color;
        theme_get_accent_color(color);
        button_widget_set_color(editor->play_pause_button, color);
    } else {
        vec4 color;
        theme_get_button_color(color);
        button_widget_set_color(editor->play_pause_button, color);
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
             var_idx++, total_track_count++) {                                                \
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
            const te_scene_animation_keyframe_##type* keyframes =                             \
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
                    editor->keyframe_button_array, editor, editor->root_widget,               \
                    (void*)&keyframes[keyframe_idx], keyframes[keyframe_idx].interpolation,   \
                    keyframes[keyframe_idx].time, var_names[var_idx], keyframe_x, keyframe_y, \
                    (1.0f - editor->timeline_x_pos), editor->button_height);                  \
            }                                                                                 \
                                                                                              \
            track_button_idx += 1;                                                            \
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
            total_track_count += 1;                                                           \
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
        track_button_idx += 1;                                                                \
        total_track_count += 1;                                                               \
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

    editor->track_scroll_count = 0;
    editor->scroll_mode = TE_SAESM_VERTICAL_SCROLL;

    if (editor->selected_obj == NULL) {
        keyframe_button_array_select_keyframe(editor->keyframe_button_array, 0xFFFFFFFF);
    }

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
        animation = world_create_scene_animation(editor->world, NULL);
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
        keyframe_button_array_select_keyframe(editor->keyframe_button_array, 0xFFFFFFFF);
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
    } else if (button == TE_MB_RIGHT) {
        te_scene_animation* anim = world_get_scene_animation(editor->world);
        if (anim == NULL) {
            return;
        }

        keyframe_button_array_select_keyframe(editor->keyframe_button_array, 0xFFFFFFFF);

        const char* object_name = get_selected_obj_name(editor);
        if (object_name == NULL) {
            return;
        }

        // Delete keyframe.
        for (unsigned int i = 0; i < editor->keyframe_button_array->count; i++) {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_button_array->buttons[i]);

            vec2 pos;
            vec2 size;
            widget_get_relative_position(widget, pos);
            widget_get_relative_size(widget, size);

            if (cursor_pos[0] > pos[0] && cursor_pos[0] < pos[0] + size[0]
                && cursor_pos[1] > pos[1] && cursor_pos[1] < pos[1] + size[1]) {
                te_keyframe_button_data* data = widget_get_custom_ptr(widget);
                scene_animation_remove_keyframe(
                    anim, object_name, data->variable_name, data->keyframe);

                redraw_timeline(editor, true);
                return;
            }
        }
    } else if (button == TE_MB_LEFT) {
        // Change current time or select a keyframe.
        te_scene_animation* anim = world_get_scene_animation(editor->world);
        if (anim == NULL) {
            return;
        }

        for (unsigned int i = 0; i < editor->keyframe_button_array->count; i++) {
            te_widget* widget =
                button_widget_get_widget(editor->keyframe_button_array->buttons[i]);

            vec2 pos;
            vec2 size;
            widget_get_relative_position(widget, pos);
            widget_get_relative_size(widget, size);

            if (cursor_pos[0] > pos[0] && cursor_pos[0] < pos[0] + size[0]
                && cursor_pos[1] > pos[1] && cursor_pos[1] < pos[1] + size[1]) {
                // Selected a keyframe.
                if (editor->keyframe_button_array->selected_idx == i) {
                    // Deselect.
                    keyframe_button_array_select_keyframe(
                        editor->keyframe_button_array, 0xFFFFFFFF);
                } else {
                    keyframe_button_array_select_keyframe(editor->keyframe_button_array, i);
                    show_selected_keyframe_menu(editor);
                }
                redraw_timeline(editor, false);
                return;
            }
        }

        keyframe_button_array_select_keyframe(editor->keyframe_button_array, 0xFFFFFFFF);

        // Move current time.
        float keyframe_time_portion =
            (cursor_pos[0] - editor->timeline_x_pos) / (1.0f - editor->timeline_x_pos);
        float time_sec =
            (float)editor->left_border_time
            + keyframe_time_portion
                  * ((float)editor->right_border_time - (float)editor->left_border_time);
        scene_animation_set_current_time(anim, time_sec);

        redraw_timeline(editor, false);
    }
}

void
prv_scene_animation_editor_on_mouse_scroll_moved(
    te_scene_animation_editor* editor, float offset, vec2 cursor_pos) {
    if (cursor_pos[0] < editor->timeline_x_pos) {
        return;
    }

    int fixed_offset = -(int)offset;

    switch (editor->scroll_mode) {
        case (TE_SAESM_VERTICAL_SCROLL): {
            if (fixed_offset < 0
                && (unsigned int)abs(fixed_offset) >= editor->track_scroll_count) {
                editor->track_scroll_count = 0;
            } else {
                editor->track_scroll_count =
                    (unsigned int)((int)editor->track_scroll_count + fixed_offset);
            }
            break;
        }
        case (TE_SAESM_HORIZONTAL_SCROLL): {
            if (fixed_offset < 0
                && (unsigned int)abs(fixed_offset) >= editor->left_border_time) {
                editor->left_border_time = 0;
                editor->right_border_time = editor->displayed_time_interval;
            } else {
                editor->left_border_time =
                    (unsigned int)((int)editor->left_border_time + fixed_offset);
                editor->right_border_time =
                    editor->left_border_time + editor->displayed_time_interval;
            }
            break;
        }
        case (TE_SAESM_ZOOM): {
            if (fixed_offset < 0
                && (unsigned int)abs(fixed_offset) >= editor->displayed_time_interval) {
                editor->displayed_time_interval = 1;
            } else {
                editor->displayed_time_interval =
                    (unsigned int)((int)editor->displayed_time_interval + fixed_offset);
            }
            editor->right_border_time =
                editor->left_border_time + editor->displayed_time_interval;
            break;
        }
        default: {
            log_error("unhandled case");
            abort();
            break;
        }
    }

    redraw_timeline(editor, true);
}

void
prv_scene_animation_editor_on_keyboard_button_pressed(
    te_scene_animation_editor* editor, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers) {
    (void)button;

    if (keyboard_modifiers_is_ctrl_pressed(&modifiers)) {
        editor->scroll_mode = TE_SAESM_ZOOM;
    } else if (keyboard_modifiers_is_shift_pressed(&modifiers)) {
        editor->scroll_mode = TE_SAESM_HORIZONTAL_SCROLL;
    } else {
        editor->scroll_mode = TE_SAESM_VERTICAL_SCROLL;
    }
}

void
prv_scene_animation_editor_on_keyboard_button_released(
    te_scene_animation_editor* editor, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers) {
    (void)button;

    if (keyboard_modifiers_is_ctrl_pressed(&modifiers)) {
        editor->scroll_mode = TE_SAESM_ZOOM;
    } else if (keyboard_modifiers_is_shift_pressed(&modifiers)) {
        editor->scroll_mode = TE_SAESM_HORIZONTAL_SCROLL;
    } else {
        editor->scroll_mode = TE_SAESM_VERTICAL_SCROLL;
    }
}

void
prv_scene_animation_editor_tick(te_scene_animation_editor* editor) {
    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL || !scene_animation_is_playing(anim)) {
        return;
    }

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

    keyframe_button_array_select_keyframe(editor->keyframe_button_array, 0xFFFFFFFF);
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

    keyframe_button_array_select_keyframe(editor->keyframe_button_array, 0xFFFFFFFF);
}

static void
on_track_right_clicked(te_button_widget* button) {
    te_widget* widget = button_widget_get_widget(button);
    te_scene_animation_editor* editor = widget_get_custom_ptr(widget);

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    keyframe_button_array_select_keyframe(editor->keyframe_button_array, 0xFFFFFFFF);

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
                scene_animation_remove_all_keyframes_bool(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_FLOAT): {
                scene_animation_remove_all_keyframes_float(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_UINT): {
                scene_animation_remove_all_keyframes_uint(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_VEC2): {
                scene_animation_remove_all_keyframes_vec2(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_VEC3): {
                scene_animation_remove_all_keyframes_vec3(anim, obj_name, variable_name);
                break;
            }
            case (TE_VT_VEC4): {
                scene_animation_remove_all_keyframes_vec4(anim, obj_name, variable_name);
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

static void
on_current_time_changed(te_text_edit_widget* text_edit) {
    te_scene_animation_editor* editor =
        widget_get_custom_ptr(text_edit_widget_get_widget(text_edit));

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    // Get text.
    unsigned int text_len;
    const wchar_t* src_text = text_edit_widget_get_text(text_edit, &text_len);
    char* text = wchar_to_char(src_text, &text_len);

    char* endptr;
    const float new_time_sec = globals_convert_string_to_float(text, &endptr);
    free(text);

    scene_animation_set_current_time(anim, new_time_sec);

    // Update left/right borders.
    unsigned int step = (unsigned int)(new_time_sec / (float)editor->displayed_time_interval);
    editor->left_border_time = step * editor->displayed_time_interval;
    editor->right_border_time = editor->left_border_time + editor->displayed_time_interval;

    redraw_timeline(editor, true);
}

static void
on_keyframe_time_changed(te_text_edit_widget* text_edit) {
    // Remove keyframe and re-add with new time.

    te_scene_animation_editor* editor =
        widget_get_custom_ptr(text_edit_widget_get_widget(text_edit));

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    if (editor->keyframe_button_array->selected_idx == 0xFFFFFFFF) {
        return;
    }

    scene_animation_pause(anim);

    // Get text.
    unsigned int text_len;
    const wchar_t* src_text = text_edit_widget_get_text(text_edit, &text_len);
    char* text = wchar_to_char(src_text, &text_len);

    char* endptr;
    const float new_time_sec = globals_convert_string_to_float(text, &endptr);
    free(text);

    te_keyframe_button_data* keyframe_data =
        &editor->keyframe_button_array->datas[editor->keyframe_button_array->selected_idx];

    const char* object_name = get_selected_obj_name(editor);

    // Copy variable name because it will be invalid after we redraw after adding new keyframe.
    size_t var_name_len = strlen(keyframe_data->variable_name);
    char* var_name = malloc(sizeof(char) * (var_name_len + 1));
    memcpy(var_name, keyframe_data->variable_name, sizeof(char) * var_name_len);
    var_name[var_name_len] = 0;

    // Find variable info.
    for (unsigned int i = 0; i < editor->selected_obj_type_info->variable_count; i++) {
        if (strcmp(var_name, editor->selected_obj_type_info->variables[i].name) != 0) {
            continue;
        }

        te_variable_info* var_info = &editor->selected_obj_type_info->variables[i];

        // Add keyframe with new time.
        switch (var_info->type) {
            case (TE_VT_BOOL): {
                te_scene_animation_keyframe_bool* keyframe =
                    (te_scene_animation_keyframe_bool*)keyframe_data->keyframe;
                bool value = keyframe->value;

                scene_animation_remove_keyframe(
                    anim, object_name, keyframe_data->variable_name, keyframe_data->keyframe);
                scene_animation_add_keyframe_bool(
                    anim, object_name, var_name, new_time_sec, value);
                break;
            }
            case (TE_VT_UINT): {
                te_scene_animation_keyframe_uint* keyframe =
                    (te_scene_animation_keyframe_uint*)keyframe_data->keyframe;
                unsigned int value = keyframe->value;

                scene_animation_remove_keyframe(
                    anim, object_name, keyframe_data->variable_name, keyframe_data->keyframe);
                scene_animation_add_keyframe_uint(
                    anim, object_name, var_name, new_time_sec, value);
                break;
            }
            case (TE_VT_FLOAT): {
                te_scene_animation_keyframe_float* keyframe =
                    (te_scene_animation_keyframe_float*)keyframe_data->keyframe;
                float value = keyframe->value;

                scene_animation_remove_keyframe(
                    anim, object_name, keyframe_data->variable_name, keyframe_data->keyframe);
                scene_animation_add_keyframe_float(
                    anim, object_name, var_name, new_time_sec, value);
                break;
            }
            case (TE_VT_VEC2): {
                te_scene_animation_keyframe_vec2* keyframe =
                    (te_scene_animation_keyframe_vec2*)keyframe_data->keyframe;
                vec2 value;
                glm_vec2_copy(keyframe->value, value);

                scene_animation_remove_keyframe(
                    anim, object_name, keyframe_data->variable_name, keyframe_data->keyframe);
                scene_animation_add_keyframe_vec2(
                    anim, object_name, var_name, new_time_sec, value);
                break;
            }
            case (TE_VT_VEC3): {
                te_scene_animation_keyframe_vec3* keyframe =
                    (te_scene_animation_keyframe_vec3*)keyframe_data->keyframe;
                vec3 value;
                glm_vec3_copy(keyframe->value, value);

                scene_animation_remove_keyframe(
                    anim, object_name, keyframe_data->variable_name, keyframe_data->keyframe);
                scene_animation_add_keyframe_vec3(
                    anim, object_name, var_name, new_time_sec, value);
                break;
            }
            case (TE_VT_VEC4): {
                te_scene_animation_keyframe_vec4* keyframe =
                    (te_scene_animation_keyframe_vec4*)keyframe_data->keyframe;
                vec4 value;
                glm_vec4_copy(keyframe->value, value);

                scene_animation_remove_keyframe(
                    anim, object_name, keyframe_data->variable_name, keyframe_data->keyframe);
                scene_animation_add_keyframe_vec4(
                    anim, object_name, var_name, new_time_sec, value);
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

    redraw_timeline(editor, true);

    // Find this keyframe again and reselect it.
    for (unsigned int i = 0; i < editor->keyframe_button_array->count; i++) {
        te_keyframe_button_data* data = &editor->keyframe_button_array->datas[i];
        if (strcmp(data->variable_name, var_name) != 0) {
            continue;
        }
        if (fabsf(data->time_sec - new_time_sec) >= 0.005f) {
            continue;
        }
        keyframe_button_array_select_keyframe(editor->keyframe_button_array, i);
        break;
    }

    redraw_timeline(editor, false);

    free(var_name);
}

static void
on_keyframe_interpolation_changed(te_button_widget* button) {
    te_scene_animation_editor* editor =
        widget_get_custom_ptr(button_widget_get_widget(button));

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    if (editor->keyframe_button_array->selected_idx == 0xFFFFFFFF) {
        return;
    }

    enum te_scene_animation_interpolation_type new_interpolation = TE_SAIT_COUNT;
    if (button == editor->keyframe_interpolation_button_step) {
        new_interpolation = TE_SAIT_STEP;
    } else if (button == editor->keyframe_interpolation_button_linear) {
        new_interpolation = TE_SAIT_LINEAR;
    } else if (button == editor->keyframe_interpolation_button_cubic) {
        new_interpolation = TE_SAIT_CUBIC_SPLINE;
    }

    if (new_interpolation == TE_SAIT_COUNT) {
        log_error("unexpected state");
        return;
    }

    te_keyframe_button_data* data =
        &editor->keyframe_button_array->datas[editor->keyframe_button_array->selected_idx];

    // Find variable info.
    for (unsigned int i = 0; i < editor->selected_obj_type_info->variable_count; i++) {
        if (strcmp(data->variable_name, editor->selected_obj_type_info->variables[i].name)
            != 0) {
            continue;
        }

        te_variable_info* var_info = &editor->selected_obj_type_info->variables[i];

        // Add keyframe with new time.
        switch (var_info->type) {
            case (TE_VT_BOOL): {
                te_scene_animation_keyframe_bool* keyframe =
                    (te_scene_animation_keyframe_bool*)data->keyframe;
                keyframe->interpolation = new_interpolation;
                break;
            }
            case (TE_VT_UINT): {
                te_scene_animation_keyframe_uint* keyframe =
                    (te_scene_animation_keyframe_uint*)data->keyframe;
                keyframe->interpolation = new_interpolation;
                break;
            }
            case (TE_VT_FLOAT): {
                te_scene_animation_keyframe_float* keyframe =
                    (te_scene_animation_keyframe_float*)data->keyframe;
                keyframe->interpolation = new_interpolation;
                break;
            }
            case (TE_VT_VEC2): {
                te_scene_animation_keyframe_vec2* keyframe =
                    (te_scene_animation_keyframe_vec2*)data->keyframe;
                keyframe->interpolation = new_interpolation;
                break;
            }
            case (TE_VT_VEC3): {
                te_scene_animation_keyframe_vec3* keyframe =
                    (te_scene_animation_keyframe_vec3*)data->keyframe;
                keyframe->interpolation = new_interpolation;
                break;
            }
            case (TE_VT_VEC4): {
                te_scene_animation_keyframe_vec4* keyframe =
                    (te_scene_animation_keyframe_vec4*)data->keyframe;
                keyframe->interpolation = new_interpolation;
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

    data->interpolation = new_interpolation;

    // Update highlighted interpolation button.
    show_selected_keyframe_menu(editor);
}

static void
on_update_keyframe_value_clicked(te_button_widget* button) {
    te_scene_animation_editor* editor =
        widget_get_custom_ptr(button_widget_get_widget(button));

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    if (editor->keyframe_button_array->selected_idx == 0xFFFFFFFF) {
        return;
    }

    te_keyframe_button_data* data =
        &editor->keyframe_button_array->datas[editor->keyframe_button_array->selected_idx];

    // Find variable info.
    for (unsigned int i = 0; i < editor->selected_obj_type_info->variable_count; i++) {
        if (strcmp(data->variable_name, editor->selected_obj_type_info->variables[i].name)
            != 0) {
            continue;
        }

        te_variable_info* var_info = &editor->selected_obj_type_info->variables[i];

        // Add keyframe with new time.
        switch (var_info->type) {
            case (TE_VT_BOOL): {
                te_scene_animation_keyframe_bool* keyframe =
                    (te_scene_animation_keyframe_bool*)data->keyframe;
                keyframe->value =
                    editor->selected_obj_type_info->bool_getters[var_info->set_get_index](
                        editor->selected_obj);
                break;
            }
            case (TE_VT_UINT): {
                te_scene_animation_keyframe_uint* keyframe =
                    (te_scene_animation_keyframe_uint*)data->keyframe;
                keyframe->value =
                    editor->selected_obj_type_info->uint_getters[var_info->set_get_index](
                        editor->selected_obj);
                break;
            }
            case (TE_VT_FLOAT): {
                te_scene_animation_keyframe_float* keyframe =
                    (te_scene_animation_keyframe_float*)data->keyframe;
                keyframe->value =
                    editor->selected_obj_type_info->float_getters[var_info->set_get_index](
                        editor->selected_obj);
                break;
            }
            case (TE_VT_VEC2): {
                te_scene_animation_keyframe_vec2* keyframe =
                    (te_scene_animation_keyframe_vec2*)data->keyframe;
                vec2 val;
                editor->selected_obj_type_info->vec2_getters[var_info->set_get_index](
                    editor->selected_obj, val);
                glm_vec2_copy(val, keyframe->value);
                break;
            }
            case (TE_VT_VEC3): {
                te_scene_animation_keyframe_vec3* keyframe =
                    (te_scene_animation_keyframe_vec3*)data->keyframe;
                vec3 val;
                editor->selected_obj_type_info->vec3_getters[var_info->set_get_index](
                    editor->selected_obj, val);
                glm_vec3_copy(val, keyframe->value);
                break;
            }
            case (TE_VT_VEC4): {
                te_scene_animation_keyframe_vec4* keyframe =
                    (te_scene_animation_keyframe_vec4*)data->keyframe;
                vec4 val;
                editor->selected_obj_type_info->vec4_getters[var_info->set_get_index](
                    editor->selected_obj, val);
                glm_vec4_copy(val, keyframe->value);
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
}

static void
on_save_as_path_selected(void* custom, const char* absolute_path) {
    te_scene_animation_editor* editor = custom;

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    char* relative_path = filesystem_convert_path_to_relative(absolute_path);

    scene_animation_save(anim, relative_path);

    free(relative_path);

    editor_refresh_filesystem_view(editor->engine_editor);
}

static void
on_save_as_clicked(te_button_widget* button) {
    te_scene_animation_editor* editor =
        widget_get_custom_ptr(button_widget_get_widget(button));

    te_scene_animation* anim = world_get_scene_animation(editor->world);
    if (anim == NULL) {
        return;
    }

    editor_show_file_dialog(
        editor->engine_editor, editor, on_save_as_path_selected, NULL, TE_FDM_SELECT_NEW_FILE);
}

static void
on_load_path_selected(void* custom, const char* absolute_path) {
    te_scene_animation_editor* editor = custom;

    char* relative_path = filesystem_convert_path_to_relative(absolute_path);

    world_create_scene_animation(editor->world, relative_path);

    free(relative_path);

    redraw_timeline(editor, true);
}

static void
on_load_clicked(te_button_widget* button) {
    te_scene_animation_editor* editor =
        widget_get_custom_ptr(button_widget_get_widget(button));

    editor_show_file_dialog(
        editor->engine_editor, editor, on_load_path_selected, NULL,
        TE_FDM_SELECT_EXISTING_FILE);
}
