#include <ui/property_inspector.h>

#include <snprintf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <type_database.h>
#include <misc/globals.h>
#include <game/game_object_info.h>
#include <game/model.h>
#include <game/skeleton.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <ui/editor_theme.h>
#include <ui/editor_ui.h>
#include <ui/world_inspector.h>
#include <ui/scene_animation_editor.h>
#include <widget/widget.h>
#include <widget/checkbox_widget.h>
#include <widget/text_widget.h>
#include <widget/text_edit_widget.h>
#include <widget/button_widget.h>
#include <widget/rect_widget.h>
#include <misc/wchar_funcs.h>
#include <math/math_funcs.h>
#include <editor.h>
#include <world.h>

struct te_property_inspector {
    /* not NULL if inspecting some object */
    void* obj;
    const char* obj_type_id;

    te_editor_ui* ui;

    /* do not free/destroy, parent widget */
    te_widget* right_panel;
};

te_property_inspector*
property_inspector_create(te_editor_ui* ui) {
    te_property_inspector* inspector = malloc(sizeof(te_property_inspector));

    inspector->obj = NULL;
    inspector->obj_type_id = NULL;
    inspector->right_panel = NULL;
    inspector->ui = ui;

    return inspector;
}

void
property_inspector_destroy(te_property_inspector* inspector) {
    free(inspector);
}

void
property_inspector_set_parent(te_property_inspector* inspector, te_widget* right_panel) {
    inspector->right_panel = right_panel;
}

static void
on_variable_checkbox_changed(te_checkbox_widget* checkbox, bool is_checked) {
    const te_type_info* info;
    unsigned int var_idx;

    te_widget* widget = checkbox_widget_get_widget(checkbox);
    te_property_inspector* inspector = widget_get_custom_ptr(widget);

    if (inspector->obj == NULL || inspector->obj_type_id == NULL) {
        log_error(__FILE__, __LINE__, "expected to have valid object and type ID");
        abort();
    }

    var_idx = (unsigned int)widget_get_custom_value(widget);

    info = type_database_get_type_info(inspector->obj_type_id);
    if (info == NULL) {
        log_error(__FILE__, __LINE__, "unable to find type info");
        abort();
    }

    info->bool_setters[info->variables[var_idx].set_get_index](inspector->obj, is_checked);
}

static void
on_variable_text_edit_changed(te_text_edit_widget* text_edit) {
    const char* widget_name;
    const te_type_info* info;
    char* endptr;
    char* text;
    unsigned int src_text_len;
    unsigned int var_idx;
    unsigned int comp_idx;
    unsigned int set_get_index;
    unsigned int text_len;
    const wchar_t* src_text = text_edit_widget_get_text(text_edit, &src_text_len);

    te_widget* widget = text_edit_widget_get_widget(text_edit);
    te_property_inspector* inspector = widget_get_custom_ptr(widget);

    if (inspector->obj == NULL || inspector->obj_type_id == NULL) {
        log_error(__FILE__, __LINE__, "expected to have valid object and type ID");
        abort();
    }

    var_idx = (unsigned int)widget_get_custom_value(widget);

    comp_idx = 0;
    widget_name = widget_get_name(widget);
    if (widget_name != NULL) {
        comp_idx = (unsigned int)widget_name[0];
    }

    text = wchar_to_char(src_text, &text_len);

    info = type_database_get_type_info(inspector->obj_type_id);
    if (info == NULL) {
        log_error(__FILE__, __LINE__, "unable to find type info");
        abort();
    }

    set_get_index = info->variables[var_idx].set_get_index;

    switch (info->variables[var_idx].type) {
        case (TE_VT_BOOL): {
            log_error(__FILE__, __LINE__, "unexpected type");
            abort();
            break;
        }
        case (TE_VT_UINT): {
            const unsigned long test = strtoul(text, &endptr, 10);
            unsigned int value = 0;
            if (test < 0xFFFFFFFFu) {
                value = (unsigned int)test;
            }

            info->uint_setters[set_get_index](inspector->obj, value);
            break;
        }
        case (TE_VT_FLOAT): {
            const float value = globals_convert_string_to_float(text, &endptr);
            info->float_setters[set_get_index](inspector->obj, value);
            break;
        }
        case (TE_VT_VEC2): {
            const float item = globals_convert_string_to_float(text, &endptr);
            te_vec2 value;
            info->vec2_getters[set_get_index](inspector->obj, value);
            value[comp_idx] = item;
            info->vec2_setters[set_get_index](inspector->obj, value);
            break;
        }
        case (TE_VT_VEC3): {
            const float item = globals_convert_string_to_float(text, &endptr);
            te_vec3 value;
            info->vec3_getters[set_get_index](inspector->obj, value);
            value[comp_idx] = item;
            info->vec3_setters[set_get_index](inspector->obj, value);
            break;
        }
        case (TE_VT_VEC4): {
            const float item = globals_convert_string_to_float(text, &endptr);
            te_vec4 value;
            info->vec4_getters[set_get_index](inspector->obj, value);
            value[comp_idx] = item;
            info->vec4_setters[set_get_index](inspector->obj, value);
            break;
        }
        case (TE_VT_STRING): {
            info->string_setters[set_get_index](inspector->obj, text);
            break;
        }
        case (TE_VT_WSTRING): {
            info->wstring_setters[set_get_index](inspector->obj, src_text);
            break;
        }
    }

    free(text);

    if (strcmp(info->variables[var_idx].name, "name") == 0) {
        /* object name changed, update world inspector  */
        te_world_inspector* world_inspector = editor_ui_get_world_inspector(inspector->ui);
        world_inspector_refresh_names(world_inspector);
    }
}

static void
add_name_widget(te_widget* parent, const char* name, te_vec2 pos, te_vec2 size) {
    wchar_t* wtext;
    unsigned int text_len;

    te_text_widget* text_widget = text_widget_create();
    {
        te_widget* widget = text_widget_get_widget(text_widget);
        widget_set_parent(widget, parent);
        widget_set_relative_position(widget, pos);
        widget_set_relative_size(widget, size);
    }

    text_widget_set_text_height(text_widget, editor_theme_get_text_height());

    wtext = wchar_from_char(name, &text_len);
    text_widget_set_text_own(text_widget, wtext, text_len);
}

static void
add_float_widget(
    te_property_inspector* inspector, unsigned int var_idx, unsigned int comp_idx,
    te_widget* parent, float value, te_vec2 pos, te_vec2 size) {
    char* src_text;
    wchar_t* wtext;
    unsigned int text_len;
    int len;

    te_text_edit_widget* text_edit = text_edit_widget_create();
    {
        te_widget* widget = text_edit_widget_get_widget(text_edit);
        widget_set_parent(widget, parent);
        widget_set_relative_position(widget, pos);
        widget_set_relative_size(widget, size);

        widget_set_custom_ptr(widget, inspector);
        widget_set_custom_value(widget, var_idx);

        if (comp_idx == 1) {
            widget_set_name(widget, "\1");
        } else if (comp_idx == 2) {
            widget_set_name(widget, "\2");
        } else if (comp_idx == 3) {
            widget_set_name(widget, "\3");
        }
    }
    text_edit_widget_set_text_height(text_edit, editor_theme_get_text_height() * 0.95f);
    text_edit_widget_set_on_text_accepted(text_edit, on_variable_text_edit_changed);

    len = snprintf(NULL, 0, "%.2f", value);
    if (len < 0) {
        log_error(__FILE__, __LINE__, "snprintf error");
        abort();
    }
    src_text = malloc(sizeof(char) * (size_t)(len + 1));
    snprintf(src_text, (size_t)len + 1, "%.2f", value);

    wtext = wchar_from_char(src_text, &text_len);
    text_edit_widget_set_text_own(text_edit, wtext, text_len);

    free(src_text);
}

static void
on_button_pilot_camera_clicked(te_button_widget* button) {
    te_property_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));

    editor_pilot_camera(editor_ui_get_editor(inspector->ui), inspector->obj);
    property_inspector_hide(inspector);
}

static void
on_preview_animation_selected(void* custom, const char* absolute_path_to_file) {
    const char* filename;
    char* relative_path_to_anim;
    te_model* model;
    te_skeleton* skeleton;

    te_property_inspector* inspector = custom;

    model = inspector->obj;
    skeleton = model_get_skeleton(model);

    relative_path_to_anim = filesystem_convert_path_to_relative(absolute_path_to_file);

    skeleton_load_animations(skeleton, relative_path_to_anim);

    filename = filesystem_find_filename(relative_path_to_anim, false, NULL);
    skeleton_play_animation(skeleton, filename, true, 0.0f);

    free(relative_path_to_anim);
}

static void
on_button_preview_animation_clicked(te_button_widget* button) {
    te_property_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));

    te_editor* editor = editor_ui_get_editor(inspector->ui);
    editor_show_file_dialog(
        editor, inspector, on_preview_animation_selected, NULL, TE_FDM_SELECT_EXISTING_FILE);
}

void
property_inspector_show(te_property_inspector* inspector, void* obj, const char* obj_type_id) {
    const te_type_info* type_info;
    te_text_widget* text_widget;
    wchar_t* wtext;
    te_scene_animation_editor* anim_editor;
    te_vec4 text_edit_background_color;
    te_vec4 checkbox_checked_color;
    te_vec4 color;
    te_vec2 tmp2;
    te_vec2 size;
    te_vec2 pos;
    unsigned int text_len;
    unsigned int var_idx;
    float hpadding;
    float vec_item_width;

    if (inspector->obj == obj) {
        return;
    }

    if (inspector->right_panel == NULL) {
        log_error(__FILE__, __LINE__, "expected the parent widget to be valid");
        abort();
    }

    /* remove old widgets (if any existed) */
    property_inspector_hide(inspector);

    inspector->obj = obj;
    inspector->obj_type_id = obj_type_id;

    type_info = type_database_get_type_info(obj_type_id);
    if (type_info == NULL) {
        log_error(__FILE__, __LINE__, "invalid object type ID");
        abort();
    }

    hpadding = editor_theme_get_horizontal_padding() / editor_theme_get_right_panel_width();
    vec_item_width = (1.0f - hpadding) / 4.0f - (hpadding / 2.0f) * 3.0f;

    editor_theme_get_text_edit_background_color(text_edit_background_color);
    editor_theme_get_accent_color(checkbox_checked_color);

    size[0] = 1.0f - hpadding * 2.0f;
    size[1] = editor_theme_get_button_height();

    pos[0] = hpadding;
    pos[1] = editor_theme_get_vertical_padding() / 2.0f;

    if (type_info->game_object_info != NULL) {
        if (type_info->game_object_info->type == TE_GOT_CAMERA) {
            /* add a button to pilot the camera */
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_custom_ptr(widget, inspector);
                widget_set_parent(widget, inspector->right_panel);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);
            }

            editor_theme_get_button_color(color);
            button_widget_set_color(button, color);

            editor_theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            editor_theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_pilot_camera_clicked);

            /* button text */
            text_widget = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, button_widget_get_widget(button));

                vec2_set(hpadding, 0.0f, tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(1.0f - hpadding, 1.0f, tmp2);
                widget_set_relative_size(widget, tmp2);
            }

            text_widget_set_text_height(text_widget, editor_theme_get_text_height());

            wtext = wchar_from_char("Pilot camera", &text_len);
            text_widget_set_text_own(text_widget, wtext, text_len);

            pos[1] += size[1];
        } else if (
            type_info->game_object_info->type == TE_GOT_MODEL
            && model_get_skeleton(obj) != NULL) {
            /* add a button to preview skeleton animation */
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_custom_ptr(widget, inspector);
                widget_set_parent(widget, inspector->right_panel);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);
            }

            editor_theme_get_button_color(color);
            button_widget_set_color(button, color);

            editor_theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            editor_theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_preview_animation_clicked);

            /* button text */
            text_widget = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, button_widget_get_widget(button));

                vec2_set(hpadding, 0.0f, tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(1.0f - hpadding, 1.0f, tmp2);
                widget_set_relative_size(widget, tmp2);
            }

            text_widget_set_text_height(text_widget, editor_theme_get_text_height());

            wtext = wchar_from_char("Preview animation", &text_len);
            text_widget_set_text_own(text_widget, wtext, text_len);

            pos[1] += size[1];
        }
    }

    anim_editor = world_inspector_get_scene_animation_editor(
        editor_ui_get_world_inspector(inspector->ui));
    if (anim_editor != NULL) {
        scene_animation_editor_show_tracks(anim_editor, obj, type_info);
    }

    for (var_idx = 0; var_idx < type_info->variable_count; var_idx++) {
        te_variable_info* var_info = &type_info->variables[var_idx];

        add_name_widget(inspector->right_panel, var_info->name, pos, size);
        pos[1] += size[1];

        switch (var_info->type) {
            case (TE_VT_BOOL): {
                te_checkbox_widget* checkbox = checkbox_widget_create();
                {
                    te_widget* widget = checkbox_widget_get_widget(checkbox);
                    widget_set_parent(widget, inspector->right_panel);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);

                    widget_set_custom_ptr(widget, inspector);
                    widget_set_custom_value(widget, var_idx);
                }
                checkbox_widget_set_background_color(checkbox, text_edit_background_color);
                checkbox_widget_set_checked_color(checkbox, checkbox_checked_color);
                checkbox_widget_set_is_checked(
                    checkbox, type_info->bool_getters[var_info->set_get_index](obj));
                checkbox_widget_set_on_changed(checkbox, on_variable_checkbox_changed);
                break;
            }
            case (TE_VT_UINT): {
                te_text_edit_widget* text_edit;
                char* src_text;
                unsigned int value;
                int len;

                /* text edit background  ---------------------------- */

                te_rect_widget* rect = rect_widget_create();
                {
                    te_widget* widget = rect_widget_get_widget(rect);
                    widget_set_parent(widget, inspector->right_panel);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);
                }
                rect_widget_set_color(rect, text_edit_background_color);

                /* text edit ---------------------------------------- */

                value = type_info->uint_getters[var_info->set_get_index](obj);

                text_edit = text_edit_widget_create();
                {
                    te_widget* widget = text_edit_widget_get_widget(text_edit);
                    widget_set_parent(widget, rect_widget_get_widget(rect));

                    vec2_set(hpadding, 0.0f, tmp2);
                    widget_set_relative_position(widget, tmp2);

                    vec2_set(1.0f - hpadding, 1.0f, tmp2);
                    widget_set_relative_size(widget, tmp2);

                    widget_set_custom_ptr(widget, inspector);
                    widget_set_custom_value(widget, var_idx);
                }
                text_edit_widget_set_text_height(text_edit, editor_theme_get_text_height());
                text_edit_widget_set_on_text_accepted(
                    text_edit, on_variable_text_edit_changed);

                len = snprintf(NULL, 0, "%u", value);
                if (len < 0) {
                    log_error(__FILE__, __LINE__, "snprintf error");
                    abort();
                }
                src_text = malloc(sizeof(char) * (size_t)(len + 1));
                snprintf(src_text, (size_t)len + 1, "%u", value);

                wtext = wchar_from_char(src_text, &text_len);
                text_edit_widget_set_text_own(text_edit, wtext, text_len);

                free(src_text);
                break;
            }
            case (TE_VT_FLOAT): {
                float value;
                te_vec2 wpos;
                te_vec2 wsize;

                /* text edit background  ---------------------------- */

                te_rect_widget* rect = rect_widget_create();
                {
                    te_widget* widget = rect_widget_get_widget(rect);
                    widget_set_parent(widget, inspector->right_panel);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);
                }
                rect_widget_set_color(rect, text_edit_background_color);

                /* text edit ---------------------------------------- */

                value = type_info->float_getters[var_info->set_get_index](obj);

                vec2_set(hpadding, 0.0f, wpos);
                vec2_set(1.0f - hpadding, 1.0f, wsize);
                add_float_widget(
                    inspector, var_idx, 0, rect_widget_get_widget(rect), value, wpos, wsize);
                break;
            }
            case (TE_VT_VEC2): {
                te_rect_widget* rect;
                te_widget* widget;
                te_vec2 wpos;
                te_vec2 wsize;
                te_vec2 value;

                type_info->vec2_getters[var_info->set_get_index](obj, value);

                /* text edit background  ---------------------------- */

                rect = rect_widget_create();

                widget = rect_widget_get_widget(rect);
                widget_set_parent(widget, inspector->right_panel);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);

                rect_widget_set_color(rect, text_edit_background_color);

                /* text edit ---------------------------------------- */

                vec2_set(hpadding, 0.0f, wpos);
                vec2_set(vec_item_width, 1.0f, wsize);
                add_float_widget(inspector, var_idx, 0, widget, value[0], wpos, wsize);

                vec2_set(hpadding + vec_item_width + hpadding, 0.0f, wpos);
                add_float_widget(inspector, var_idx, 1, widget, value[1], wpos, wsize);

                break;
            }
            case (TE_VT_VEC3): {
                te_rect_widget* rect;
                te_widget* widget;
                te_vec3 value;
                te_vec2 wpos;
                te_vec2 wsize;

                type_info->vec3_getters[var_info->set_get_index](obj, value);

                /* text edit background  ---------------------------- */

                rect = rect_widget_create();

                widget = rect_widget_get_widget(rect);
                widget_set_parent(widget, inspector->right_panel);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);

                rect_widget_set_color(rect, text_edit_background_color);

                /* text edit ---------------------------------------- */

                vec2_set(hpadding, 0.0f, wpos);
                vec2_set(vec_item_width, 1.0f, wsize);
                add_float_widget(inspector, var_idx, 0, widget, value[0], wpos, wsize);

                vec2_set(hpadding + vec_item_width + hpadding, 0.0f, wpos);
                add_float_widget(inspector, var_idx, 1, widget, value[1], wpos, wsize);

                vec2_set(
                    hpadding + vec_item_width + hpadding + vec_item_width + hpadding, 0.0f,
                    wpos);
                add_float_widget(inspector, var_idx, 2, widget, value[2], wpos, wsize);

                break;
            }
            case (TE_VT_VEC4): {
                te_rect_widget* rect;
                te_widget* widget;
                te_vec2 wpos;
                te_vec2 wsize;
                te_vec4 value;

                type_info->vec4_getters[var_info->set_get_index](obj, value);

                /* text edit background  ---------------------------- */

                rect = rect_widget_create();

                widget = rect_widget_get_widget(rect);
                widget_set_parent(widget, inspector->right_panel);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);

                rect_widget_set_color(rect, text_edit_background_color);

                /* text edit ---------------------------------------- */

                vec2_set(hpadding, 0.0f, wpos);
                vec2_set(vec_item_width, 1.0f, wsize);
                add_float_widget(inspector, var_idx, 0, widget, value[0], wpos, wsize);

                vec2_set(hpadding + vec_item_width + hpadding, 0.0f, wpos);
                add_float_widget(inspector, var_idx, 1, widget, value[1], wpos, wsize);

                vec2_set(
                    hpadding + vec_item_width + hpadding + vec_item_width + hpadding, 0.0f,
                    wpos);
                add_float_widget(inspector, var_idx, 2, widget, value[2], wpos, wsize);

                vec2_set(
                    hpadding + vec_item_width + hpadding + vec_item_width + hpadding
                        + vec_item_width + hpadding,
                    0.0f, wpos);
                add_float_widget(inspector, var_idx, 3, widget, value[3], wpos, wsize);

                break;
            }
            case (TE_VT_STRING): {
                te_text_edit_widget* text_edit;
                te_rect_widget* rect;
                te_widget* widget;

                const char* var_text = type_info->string_getters[var_info->set_get_index](obj);

                /* text edit background  ---------------------------- */

                rect = rect_widget_create();

                widget = rect_widget_get_widget(rect);
                widget_set_parent(widget, inspector->right_panel);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);

                rect_widget_set_color(rect, text_edit_background_color);

                /* text edit ---------------------------------------- */

                text_edit = text_edit_widget_create();
                {
                    te_widget* widget = text_edit_widget_get_widget(text_edit);
                    widget_set_parent(widget, rect_widget_get_widget(rect));

                    vec2_set(hpadding, 0.0f, tmp2);
                    widget_set_relative_position(widget, tmp2);

                    vec2_set(1.0f - hpadding, 1.0f, tmp2);
                    widget_set_relative_size(widget, tmp2);

                    widget_set_custom_ptr(widget, inspector);
                    widget_set_custom_value(widget, var_idx);
                }
                text_edit_widget_set_text_height(text_edit, editor_theme_get_text_height());
                text_edit_widget_set_on_text_accepted(
                    text_edit, on_variable_text_edit_changed);

                if (var_text == NULL) {
                    text_edit_widget_set_text(text_edit, L"");
                } else {
                    unsigned int text_len;
                    wchar_t* text = wchar_from_char(var_text, &text_len);
                    text_edit_widget_set_text_own(text_edit, text, text_len);
                }
                break;
            }
            case (TE_VT_WSTRING): {
                te_text_edit_widget* text_edit;

                const wchar_t* var_text =
                    type_info->wstring_getters[var_info->set_get_index](obj);

                /* text edit background  ---------------------------- */

                te_rect_widget* rect = rect_widget_create();

                te_widget* widget = rect_widget_get_widget(rect);
                widget_set_parent(widget, inspector->right_panel);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);

                rect_widget_set_color(rect, text_edit_background_color);

                /* text edit ---------------------------------------- */

                text_edit = text_edit_widget_create();
                {
                    te_widget* widget = text_edit_widget_get_widget(text_edit);
                    widget_set_parent(widget, rect_widget_get_widget(rect));

                    vec2_set(hpadding, 0.0f, tmp2);
                    widget_set_relative_position(widget, tmp2);

                    vec2_set(1.0f - hpadding, 1.0f, tmp2);
                    widget_set_relative_size(widget, tmp2);

                    widget_set_custom_ptr(widget, inspector);
                    widget_set_custom_value(widget, var_idx);
                }
                text_edit_widget_set_text_height(text_edit, editor_theme_get_text_height());
                text_edit_widget_set_on_text_accepted(
                    text_edit, on_variable_text_edit_changed);

                if (var_text == NULL) {
                    text_edit_widget_set_text(text_edit, L"");
                } else {
                    text_edit_widget_set_text(text_edit, var_text);
                }
                break;
            }
        }
        pos[1] += size[1];
    }
}

void
property_inspector_hide(te_property_inspector* inspector) {
    te_widget** widgets;
    te_scene_animation_editor* anim_editor;
    te_world* world;
    unsigned int count;
    unsigned int i;

    {
        /* check if we have a model with skeleton animation preview playing */
        const te_type_info* type_info = type_database_get_type_info(inspector->obj_type_id);
        if (type_info != NULL && type_info->game_object_info != NULL
            && type_info->game_object_info->type == TE_GOT_MODEL) {
            te_skeleton* skeleton = model_get_skeleton(inspector->obj);
            if (skeleton != NULL) {
                skeleton_unload_animations(skeleton);
            }
        }
    }

    inspector->obj = NULL;
    inspector->obj_type_id = NULL;

    anim_editor = world_inspector_get_scene_animation_editor(
        editor_ui_get_world_inspector(inspector->ui));
    if (anim_editor != NULL) {
        scene_animation_editor_show_tracks(anim_editor, NULL, NULL);
    }

    if (inspector->right_panel == NULL) {
        return;
    }

    world = widget_get_world(inspector->right_panel);
    if (world == NULL) {
        return;
    }

    widgets = widget_get_child_widgets(inspector->right_panel, &count);

    for (i = 0; i < count; i++) {
        widget_set_parent(widgets[i], NULL);

        world_despawn_widget(world, widgets[i]);
        widget_destroy(widgets[i]);
    }

    free(widgets);
}

void*
property_inspector_get_inspected_obj(te_property_inspector* inspector) {
    return inspector->obj;
}

const char*
property_inspector_get_inspected_obj_type_id(te_property_inspector* inspector) {
    return inspector->obj_type_id;
}
