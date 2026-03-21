#include "ui/world_inspector.h"

#include <stdio.h>
#include <stdbool.h>
#include <world.h>
#include <ui/theme.h>
#include <game/model.h>
#include <game/camera.h>
#include <widget/widget.h>
#include <widget/text_widget.h>
#include <widget/button_widget.h>
#include <misc/wchar_funcs.h>
#include <type_database.h>
#include <io/log.h>

#define BUTTON_HIDDEN_X_POS 10.0f
#define CREATE_NEW_OBJ_TEXT "Create new object"

#define OBJ_MENU_OPTION_INDEX_DELETE 0

enum te_object_menu_options {
    TE_OMO_DELETE_OBJ = 0,

    TE_OMO_COUNT, //< Marks the total number of options.
};

enum te_world_item_type {
    TE_WIT_MODEL,
    TE_WIT_CAMERA,
    TE_WIT_WIDGET,
};

enum te_world_inspector_state {
    TE_WIS_SHOW_WORLD_OBJECTS, //< Default state where game objects of the world are shown.
    TE_WIS_CREATE_NEW_OBJECT,  //< When clicked the button to create a new game object.
    TE_WIS_OBJECT_MENU, //< When right clicked on a game object and possible options to manage the object are shown.
};

typedef struct te_world_item_info {
    void* obj;
    enum te_world_item_type type;

    // more than 0 for attached/child objects
    unsigned int indent;
} te_world_item_info;

struct te_world_inspector {
    // Do not free/destroy, parent widget.
    te_widget* left_panel;

    // NULL if not set yet.
    te_world* game_world;

    // Widgets for changing @ref is_3dobj_mode_selected.
    te_button_widget* button_3dobj;
    te_button_widget* button_2dobj;

    // "Create new game object" by default.
    te_button_widget* top_button;
    te_text_widget* top_button_text;

    // Valid while spawned, buttons that fill all available space (moved outside of the viewport if should not be visible).
    // Number of items in this array is @ref item_buttons_count.
    te_button_widget** item_buttons;

    // Number of items in this array is @ref item_list_count.
    // Stores different values depending on the current @ref state.
    void* item_list;

    // Spawned widget that displays page number of the list.
    te_text_widget* page_text;

    // If not NULL stores game object that was selected for object menu/operations (delete, attach, etc.).
    void* selected_obj;
    void* selected_obj_type_id;

    // Number of items in @ref item_buttons.
    unsigned int item_buttons_count;

    // Number of items in @ref item_list.
    unsigned int item_list_count;

    unsigned int current_page;
    unsigned int page_count;

    enum te_world_inspector_state state;

    // `true` if should display world's "3D objects", `false` if "2D objects".
    bool is_3dobj_mode_selected;
};

te_world_inspector*
world_inspector_create(void) {
    te_world_inspector* inspector = malloc(sizeof(te_world_inspector));

    inspector->left_panel = NULL;
    inspector->item_list = NULL;
    inspector->button_2dobj = NULL;
    inspector->button_3dobj = NULL;
    inspector->game_world = NULL;
    inspector->top_button = NULL;
    inspector->top_button_text = NULL;
    inspector->page_text = NULL;
    inspector->selected_obj = NULL;
    inspector->selected_obj_type_id = NULL;
    inspector->item_buttons = NULL;
    inspector->item_buttons_count = 0;
    inspector->item_list_count = 0;
    inspector->current_page = 0;
    inspector->page_count = 0;
    inspector->state = TE_WIS_SHOW_WORLD_OBJECTS;
    inspector->is_3dobj_mode_selected = true;

    return inspector;
}

void
world_inspector_destroy(te_world_inspector* inspector) {
    free(inspector->item_list);
    free(inspector->item_buttons);
    free(inspector);
}

// Uses item_list and updates text on the buttons depending on the current world inspector state.
static void
refresh_item_names(te_world_inspector* inspector) {
    const float hpadding = theme_get_horizontal_padding() / theme_get_left_panel_width();
    const float button_width = 1.0f - hpadding * 2.0f;
    const float indent_size = hpadding;

    unsigned int button_idx = 0;
    for (unsigned int item_idx = inspector->current_page * inspector->item_buttons_count;
         button_idx < inspector->item_buttons_count && item_idx < inspector->item_list_count;
         button_idx++, item_idx++) {
        te_button_widget* button = inspector->item_buttons[button_idx];

        // Get button text widget.
        unsigned int child_count;
        te_widget** child_widgets =
            widget_get_child_widgets_tmp(button_widget_get_widget(button), &child_count);
        te_text_widget* button_text = NULL;
        for (unsigned int i = 0; i < child_count; i++) {
            if (!widget_is_serialization_allowed(child_widgets[i])) {
                // Internal widget (rect) of the button.
                continue;
            }
            button_text = widget_get_owner(child_widgets[i]);
            break;
        }

        unsigned int indent = 0;

        // Prepare new text.
        const char* text_to_display = NULL;
        switch (inspector->state) {
            case (TE_WIS_SHOW_WORLD_OBJECTS): {
                te_world_item_info* data = inspector->item_list;
                te_world_item_info* info = &data[item_idx];
                indent = info->indent;
                switch (info->type) {
                    case (TE_WIT_MODEL): {
                        text_to_display = model_get_name(info->obj);
                        if (text_to_display == NULL) {
                            text_to_display = "model";
                        }
                        break;
                    }
                    case (TE_WIT_WIDGET): {
                        text_to_display = widget_get_name(info->obj);
                        if (text_to_display == NULL) {
                            text_to_display = widget_get_owner_type_id(info->obj);
                        }
                        break;
                    }
                    case (TE_WIT_CAMERA): {
                        text_to_display = camera_get_name(info->obj);
                        if (text_to_display == NULL) {
                            text_to_display = "camera";
                        }
                        break;
                    }
                }
                break;
            }
            case (TE_WIS_CREATE_NEW_OBJECT): {
                const char** type_ids = inspector->item_list;
                text_to_display = type_ids[item_idx];
                break;
            }
            case (TE_WIS_OBJECT_MENU): {
                const char** option_names = inspector->item_list;
                text_to_display = option_names[item_idx];
                break;
            }
        }

        // Fix pos of the button (in case it was hidden previously or had other indentation).
        {
            te_widget* widget = button_widget_get_widget(button);

            vec2 pos;
            widget_get_relative_position(widget, pos);
            pos[0] = hpadding + indent_size * (float)indent;

            vec2 size;
            widget_get_relative_size(widget, size);
            size[0] = button_width - indent_size * (float)indent;

            widget_set_relative_position(widget, pos);
            widget_set_relative_size(widget, size);
        }

        // Display new text.
        unsigned int text_len;
        wchar_t* wtext = wchar_from_char(text_to_display, &text_len);
        text_widget_set_text_own(button_text, wtext, text_len);
    }

    // Hide remaining buttons.
    for (; button_idx < inspector->item_buttons_count; button_idx++) {
        te_widget* widget = button_widget_get_widget(inspector->item_buttons[button_idx]);

        vec2 pos;
        widget_get_relative_position(widget, pos);
        pos[0] = BUTTON_HIDDEN_X_POS;

        widget_set_relative_position(
            button_widget_get_widget(inspector->item_buttons[button_idx]), pos);
    }
}

static void
refresh_page_text(te_world_inspector* inspector) {
    // Update page count (in case list changed).
    unsigned int page_count = inspector->item_list_count / inspector->item_buttons_count;
    if (inspector->item_list_count % inspector->item_buttons_count > 0) {
        page_count += 1;
    }
    if (page_count == 0) {
        page_count = 1;
    }
    inspector->page_count = page_count;

    const int len =
        snprintf(NULL, 0, "%u / %u", inspector->current_page + 1, inspector->page_count);
    if (len < 0) {
        log_error("snprintf error");
        abort();
    }
    unsigned int text_len = (unsigned int)len;

    char* text = malloc(sizeof(char) * (text_len + 1));
    snprintf(
        text, text_len + 1, "%u / %u", inspector->current_page + 1, inspector->page_count);

    wchar_t* wtext = wchar_from_char(text, &text_len);
    text_widget_set_text_own(inspector->page_text, wtext, text_len);

    free(text);
}

static void
save_widgets_to_list_recurive(
    te_widget* widget, te_world_item_info* items, unsigned int* item_idx,
    unsigned int* indent) {
    if (!widget_is_serialization_allowed(widget)) {
        return;
    }

    te_world_item_info* info = &items[(*item_idx)];
    info->obj = widget;
    info->type = TE_WIT_WIDGET;
    info->indent = (*indent);

    (*item_idx) += 1;

    (*indent) += 1;

    unsigned int child_count;
    te_widget** child_widgets = widget_get_child_widgets_tmp(widget, &child_count);
    for (unsigned int i = 0; i < child_count; i++) {
        save_widgets_to_list_recurive(child_widgets[i], items, item_idx, indent);
    }

    (*indent) -= 1;
}

static void
count_widgets_recursive(te_widget* widget, unsigned int* count) {
    if (!widget_is_serialization_allowed(widget)) {
        return;
    }

    (*count) += 1;

    unsigned int child_count;
    te_widget** child_widgets = widget_get_child_widgets_tmp(widget, &child_count);
    for (unsigned int i = 0; i < child_count; i++) {
        count_widgets_recursive(child_widgets[i], count);
    }
}

static void
rebuild_item_list_to_display_world_objects(te_world_inspector* inspector) {
    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS) {
        log_error("unexpected state");
        abort();
    }

    free(inspector->item_list);
    inspector->item_list = NULL;
    te_world_item_info* world_items = NULL;

    te_world* world = inspector->game_world;
    if (world == NULL) {
        log_error("expected a valid game world");
        abort();
    }

    if (inspector->is_3dobj_mode_selected) {
        // First get cameras because they can't have child objects.
        unsigned int root_camera_count;
        te_camera** root_cameras = world_get_cameras_tmp(world, &root_camera_count);

        unsigned int root_model_count;
        te_model** root_models = world_get_models_tmp(world, &root_model_count);

        // Count items.
        inspector->item_list_count = 0;
        for (unsigned int i = 0; i < root_camera_count; i++) {
            inspector->item_list_count += camera_is_serialization_allowed(root_cameras[i]);
        }
        for (unsigned int i = 0; i < root_model_count; i++) {
            if (!model_is_serialization_allowed(root_models[i])) {
                continue;
            }
            inspector->item_list_count += 1;

            te_camera* attached_camera = model_get_attached_camera(root_models[i]);
            if (attached_camera != NULL && camera_is_serialization_allowed(attached_camera)) {
                inspector->item_list_count += 1;
            }

            te_model* child_model = model_get_child_model(root_models[i]);
            if (child_model != NULL && model_is_serialization_allowed(child_model)) {
                inspector->item_list_count += 1;
            }
        }

        if (inspector->item_list_count > 0) {
            world_items = malloc(sizeof(te_world_item_info) * inspector->item_list_count);
            unsigned int item_idx = 0;

            // Save root cameras.
            for (unsigned int i = 0; i < root_camera_count; i++) {
                if (!camera_is_serialization_allowed(root_cameras[i])) {
                    continue;
                }

                te_world_item_info* info = &world_items[item_idx];
                info->type = TE_WIT_CAMERA;
                info->indent = 0;
                info->obj = root_cameras[i];

                item_idx += 1;
            }

            // Save models.
            for (unsigned int i = 0; i < root_model_count; i++) {
                if (!model_is_serialization_allowed(root_models[i])) {
                    continue;
                }

                te_world_item_info* root_info = &world_items[item_idx];
                root_info->type = TE_WIT_MODEL;
                root_info->indent = 0;
                root_info->obj = root_models[i];

                item_idx += 1;

                te_camera* attached_camera = model_get_attached_camera(root_models[i]);
                if (attached_camera != NULL
                    && camera_is_serialization_allowed(attached_camera)) {
                    te_world_item_info* info = &world_items[item_idx];
                    info->type = TE_WIT_CAMERA;
                    info->indent = 1;
                    info->obj = attached_camera;

                    item_idx += 1;
                }

                te_model* child_model = model_get_child_model(root_models[i]);
                if (child_model != NULL && model_is_serialization_allowed(child_model)) {
                    te_world_item_info* info = &world_items[item_idx];
                    info->type = TE_WIT_MODEL;
                    info->indent = 1;
                    info->obj = child_model;

                    item_idx += 1;
                }
            }
        }
    } else {
        unsigned int root_count;
        te_widget** widget = world_get_widgets_tmp(world, &root_count);

        // Count how much items we have in total.
        inspector->item_list_count = 0;
        for (unsigned int i = 0; i < root_count; i++) {
            count_widgets_recursive(widget[i], &inspector->item_list_count);
        }

        // Save items.
        if (inspector->item_list_count > 0) {
            world_items = malloc(sizeof(te_world_item_info) * inspector->item_list_count);
            unsigned int indent = 0;
            unsigned int item_idx = 0;
            for (unsigned int i = 0; i < root_count; i++) {
                save_widgets_to_list_recurive(widget[i], world_items, &item_idx, &indent);
            }
        }
    }

    inspector->item_list = world_items;

    inspector->current_page = 0;
    refresh_page_text(inspector);
    refresh_item_names(inspector);
}

void
world_inspector_rebuild_list(te_world_inspector* inspector, te_world* game_world) {
    inspector->game_world = game_world;

    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS) {
        log_error("unexpected state");
        abort();
    }
    te_world_item_info* info = inspector->item_list;

    if (info == NULL) {
        // Initialize item list.
        rebuild_item_list_to_display_world_objects(inspector);
    } else if (
        (info[0].type == TE_WIT_WIDGET && inspector->is_3dobj_mode_selected)
        || (info[0].type != TE_WIT_WIDGET && !inspector->is_3dobj_mode_selected)) {
        // Mode changed, rebuild list.
        rebuild_item_list_to_display_world_objects(inspector);
    }
}

static void
on_button_3dobj_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS) {
        return;
    }

    inspector->is_3dobj_mode_selected = true;
    world_inspector_rebuild_list(inspector, inspector->game_world);

    vec4 color;
    theme_get_accent_color(color);
    button_widget_set_color(button, color);

    theme_get_button_color(color);
    button_widget_set_color(inspector->button_2dobj, color);
}

static void
on_button_2dobj_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS) {
        return;
    }

    inspector->is_3dobj_mode_selected = false;
    world_inspector_rebuild_list(inspector, inspector->game_world);

    vec4 color;
    theme_get_accent_color(color);
    button_widget_set_color(button, color);

    theme_get_button_color(color);
    button_widget_set_color(inspector->button_3dobj, color);
}

static void
on_top_button_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));

    if (inspector->state == TE_WIS_SHOW_WORLD_OBJECTS) {
        inspector->state = TE_WIS_CREATE_NEW_OBJECT;
    } else if (
        inspector->state == TE_WIS_CREATE_NEW_OBJECT
        || inspector->state == TE_WIS_OBJECT_MENU) {
        inspector->state = TE_WIS_SHOW_WORLD_OBJECTS;
    } else {
        return;
    }

    if (inspector->state == TE_WIS_SHOW_WORLD_OBJECTS) {
        // Restore the original button state.
        unsigned int text_len;
        wchar_t* wtext = wchar_from_char(CREATE_NEW_OBJ_TEXT, &text_len);
        text_widget_set_text_own(inspector->top_button_text, wtext, text_len);

        inspector->selected_obj = NULL;
        inspector->selected_obj_type_id = NULL;

        inspector->current_page = 0;
        rebuild_item_list_to_display_world_objects(inspector);
        refresh_page_text(inspector);
        return;
    }

    // Entered new game object creation.
    // Turn this button into a "cancel" button.
    unsigned int text_len;
    wchar_t* wtext = wchar_from_char("Cancel object creation", &text_len);
    text_widget_set_text_own(inspector->top_button_text, wtext, text_len);

    unsigned int type_count;
    const char** types = type_database_get_all_type_ids(&type_count);

    // Rebuild item list.
    free(inspector->item_list);
    inspector->item_list_count = type_count;
    const char** type_ids = malloc(sizeof(const char*) * type_count);
    for (unsigned int i = 0; i < type_count; i++) {
        type_ids[i] = (void*)(types[i]);
    }
    inspector->item_list = type_ids;

    free(types);

    inspector->current_page = 0;
    refresh_page_text(inspector);
    refresh_item_names(inspector);
}

static void
on_button_list_item_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));

    if (inspector->state == TE_WIS_CREATE_NEW_OBJECT) {
        // Selected type of a new game object.
        // Button name stores type ID from type database.

        // Get button text.
        unsigned int child_count;
        te_widget** child_widgets =
            widget_get_child_widgets_tmp(button_widget_get_widget(button), &child_count);
        te_text_widget* button_text = NULL;
        for (unsigned int i = 0; i < child_count; i++) {
            if (!widget_is_serialization_allowed(child_widgets[i])) {
                // Internal widget (rect) of the button.
                continue;
            }
            button_text = widget_get_owner(child_widgets[i]);
            break;
        }

        unsigned int text_len;
        wchar_t* wtext = text_widget_get_text(button_text, &text_len);
        char* type_id = wchar_to_char(wtext, &text_len);

        const te_type_info* info = type_database_get_type_info(type_id);
        if (info == NULL) {
            log_error_fmt("expected to get a valid type info for type ID \"%s\"", type_id);
            abort();
        }
        free(type_id);

        void* new_game_obj = info->create();
        info->spawn(inspector->game_world, new_game_obj);

        inspector->state = TE_WIS_SHOW_WORLD_OBJECTS;
        wtext = wchar_from_char(CREATE_NEW_OBJ_TEXT, &text_len);
        text_widget_set_text_own(inspector->top_button_text, wtext, text_len);

        rebuild_item_list_to_display_world_objects(inspector);
    } else if (inspector->state == TE_WIS_OBJECT_MENU) {
        const size_t option_index = widget_get_custom_value(button_widget_get_widget(button));
        if (option_index == TE_OMO_DELETE_OBJ) {
            if (inspector->selected_obj == NULL || inspector->selected_obj_type_id == NULL) {
                log_error("expected selected object to be valid");
                abort();
            }
            const te_type_info* info =
                type_database_get_type_info(inspector->selected_obj_type_id);
            info->despawn(inspector->game_world, inspector->selected_obj);
            info->destroy(inspector->selected_obj);
            inspector->selected_obj = NULL;
            inspector->selected_obj_type_id = NULL;

            on_top_button_clicked(inspector->top_button);
        } else {
            log_error_fmt("unexpected option %zu", option_index);
            abort();
        }
    }
}

static void
on_button_list_item_right_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    const size_t button_index = widget_get_custom_value(button_widget_get_widget(button));

    if (inspector->state == TE_WIS_SHOW_WORLD_OBJECTS) {
        inspector->state = TE_WIS_OBJECT_MENU;

        te_world_item_info* old_info =
            &((te_world_item_info*)inspector->item_list)
                [inspector->current_page * inspector->item_buttons_count + button_index];

        inspector->selected_obj = old_info->obj;
        switch (old_info->type) {
            case (TE_WIT_MODEL): {
                inspector->selected_obj_type_id = (void*)model_get_type_id();
                break;
            }
            case (TE_WIT_CAMERA): {
                inspector->selected_obj_type_id = (void*)camera_get_type_id();
                break;
            }
            case (TE_WIT_WIDGET): {
                inspector->selected_obj_type_id =
                    (void*)widget_get_owner_type_id(old_info->obj);
                break;
            }
        }

        inspector->item_list_count = TE_OMO_COUNT;

        const char** option_names = malloc(sizeof(const char*) * inspector->item_list_count);
        option_names[TE_OMO_DELETE_OBJ] = "Delete object";

        free(inspector->item_list);
        inspector->item_list = option_names;

        // Make top button a "cancel" button.
        unsigned int text_len;
        wchar_t* wtext = wchar_from_char("Cancel object options", &text_len);
        text_widget_set_text_own(inspector->top_button_text, wtext, text_len);

        inspector->current_page = 0;
        refresh_page_text(inspector);
        refresh_item_names(inspector);
    }
}

static void
on_button_prev_page_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    if (inspector->current_page == 0) {
        return;
    }

    inspector->current_page -= 1;
    refresh_page_text(inspector);
    refresh_item_names(inspector);
}

static void
on_button_next_page_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    if (inspector->current_page + 1 == inspector->page_count) {
        return;
    }

    inspector->current_page += 1;
    refresh_page_text(inspector);
    refresh_item_names(inspector);
}

void
world_inspector_add(te_world_inspector* inspector, te_widget* left_panel) {
    if (inspector->left_panel != NULL) {
        log_error("world inspector is already displayed");
        abort();
    }

    inspector->left_panel = left_panel;

    // Relative to the left panel.
    const float title_height = theme_get_text_height();
    const float hspacing = theme_get_horizontal_spacing() / theme_get_left_panel_width();
    const float vspacing = theme_get_vertical_spacing() / theme_get_left_panel_width();
    const float hpadding = theme_get_horizontal_padding() / theme_get_left_panel_width();
    const float vpadding = theme_get_vertical_padding();
    const float hpadding_in_button = theme_get_horizontal_padding_in_button();
    const float vpadding_in_button = theme_get_vertical_padding_in_button();
    const float total_width = 1.0f - hpadding * 2.0f;

    // Title text.
    float y_pos = 0.0f;
    {
        y_pos += vpadding;

        te_text_widget* title = text_widget_create();
        {
            te_widget* widget = text_widget_get_widget(title);
            widget_set_parent(widget, left_panel);
            widget_set_relative_position(widget, (vec2){hpadding, y_pos});
            widget_set_relative_size(widget, (vec2){1.0f - hpadding, title_height});
            y_pos += title_height;
        }

        text_widget_set_text_height(title, theme_get_text_height());

        unsigned int title_len;
        wchar_t* title_text = wchar_from_char("World inspector:", &title_len);
        text_widget_set_text_own(title, title_text, title_len);
    }
    y_pos += vspacing / 2.0f;

    // World object type buttons: 3D objects or 2D objects.
    {
        const float button_width = (total_width - hspacing) / 2.0f;

        // "3D objects" button ------------------

        te_button_widget* button_3dobj = button_widget_create();
        inspector->button_3dobj = button_3dobj;
        {
            te_widget* widget = button_widget_get_widget(button_3dobj);
            widget_set_custom_ptr(widget, inspector);
            widget_set_parent(widget, left_panel);
            widget_set_relative_position(widget, (vec2){hpadding, y_pos});
            widget_set_relative_size(widget, (vec2){button_width, theme_get_button_height()});
        }

        vec4 color;
        if (inspector->is_3dobj_mode_selected) {
            theme_get_accent_color(color);
        } else {
            theme_get_button_color(color);
        }
        button_widget_set_color(button_3dobj, color);

        theme_get_button_color_hovered(color);
        button_widget_set_color_hovered(button_3dobj, color);

        theme_get_button_color_pressed(color);
        button_widget_set_color_pressed(button_3dobj, color);

        button_widget_set_on_clicked(button_3dobj, on_button_3dobj_clicked);

        // "2D objects" button ------------------

        te_button_widget* button_2dobj = button_widget_create();
        inspector->button_2dobj = button_2dobj;
        {
            te_widget* widget = button_widget_get_widget(button_2dobj);
            widget_set_custom_ptr(widget, inspector);
            widget_set_parent(widget, left_panel);
            widget_set_relative_position(
                widget, (vec2){hpadding + button_width + hspacing, y_pos});
            widget_set_relative_size(widget, (vec2){button_width, theme_get_button_height()});
        }

        if (!inspector->is_3dobj_mode_selected) {
            theme_get_accent_color(color);
        } else {
            theme_get_button_color(color);
        }
        button_widget_set_color(button_2dobj, color);

        theme_get_button_color_hovered(color);
        button_widget_set_color_hovered(button_2dobj, color);

        theme_get_button_color_pressed(color);
        button_widget_set_color_pressed(button_2dobj, color);

        button_widget_set_on_clicked(button_2dobj, on_button_2dobj_clicked);

        // "3D objects" text --------------------------

        te_text_widget* text_3dobj = text_widget_create();
        {
            te_widget* widget = text_widget_get_widget(text_3dobj);
            widget_set_parent(widget, button_widget_get_widget(button_3dobj));
            widget_set_relative_position(
                widget, (vec2){hpadding_in_button, vpadding_in_button});
            widget_set_relative_size(
                widget, (vec2){1.0f - hpadding_in_button, 1.0f - vpadding_in_button});
        }

        text_widget_set_text_height(text_3dobj, theme_get_text_height());

        unsigned int text_len;
        wchar_t* text = wchar_from_char("3D objects", &text_len);
        text_widget_set_text_own(text_3dobj, text, text_len);

        // "2D objects" text --------------------------

        te_text_widget* text_2dobj = text_widget_create();
        {
            te_widget* widget = text_widget_get_widget(text_2dobj);
            widget_set_parent(widget, button_widget_get_widget(button_2dobj));
            widget_set_relative_position(
                widget, (vec2){hpadding_in_button, vpadding_in_button});
            widget_set_relative_size(
                widget, (vec2){1.0f - hpadding_in_button, 1.0f - vpadding_in_button});
        }

        text_widget_set_text_height(text_2dobj, theme_get_text_height());

        text = wchar_from_char("2D objects", &text_len);
        text_widget_set_text_own(text_2dobj, text, text_len);
    }
    y_pos += theme_get_button_height() + vspacing / 2.0f;

    // Button to create new game objects.
    {
        te_button_widget* button = button_widget_create();
        inspector->top_button = button;
        {
            te_widget* widget = button_widget_get_widget(button);
            widget_set_custom_ptr(widget, inspector);
            widget_set_parent(widget, left_panel);
            widget_set_relative_position(widget, (vec2){hpadding, y_pos});
            widget_set_relative_size(widget, (vec2){total_width, theme_get_button_height()});
        }

        vec4 color;
        theme_get_button_color(color);
        button_widget_set_color(button, color);

        theme_get_button_color_hovered(color);
        button_widget_set_color_hovered(button, color);

        theme_get_button_color_pressed(color);
        button_widget_set_color_pressed(button, color);

        button_widget_set_on_clicked(button, on_top_button_clicked);

        // Button text.
        te_text_widget* text_widget = text_widget_create();
        inspector->top_button_text = text_widget;
        {
            te_widget* widget = text_widget_get_widget(text_widget);
            widget_set_parent(widget, button_widget_get_widget(button));
            widget_set_relative_position(
                widget, (vec2){hpadding_in_button, vpadding_in_button});
            widget_set_relative_size(
                widget, (vec2){1.0f - hpadding_in_button, 1.0f - vpadding_in_button});
        }
        text_widget_set_text_height(text_widget, theme_get_text_height());

        unsigned int text_len;
        wchar_t* text = wchar_from_char(CREATE_NEW_OBJ_TEXT, &text_len);
        text_widget_set_text_own(text_widget, text, text_len);
    }
    y_pos += theme_get_button_height();

    const float world_item_list_y_pos = y_pos;
    const float nav_menu_height = theme_get_button_height();

    // World item list.
    {
        // Count how much buttons for world items we can fit in the list.
        const float list_and_nav_menu_height =
            theme_get_world_inspector_height() - world_item_list_y_pos - nav_menu_height;
        const float list_item_spacing = vspacing / 2.0f;
        unsigned int button_count = 0;
        {
            float test_y = y_pos;
            do {
                test_y += list_item_spacing + theme_get_button_height();
                button_count += 1;
            } while (test_y + list_item_spacing + theme_get_button_height()
                     <= world_item_list_y_pos + list_and_nav_menu_height);
        }

        // Create buttons.
        inspector->item_buttons = malloc(sizeof(te_button_widget*) * button_count);
        inspector->item_buttons_count = button_count;
        for (unsigned int i = 0; i < button_count; i++) {
            y_pos += list_item_spacing;

            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_parent(widget, left_panel);
                widget_set_custom_value(widget, i);
                widget_set_custom_ptr(widget, inspector);
                widget_set_relative_position(widget, (vec2){hpadding, y_pos});
                widget_set_relative_size(
                    widget, (vec2){total_width, theme_get_button_height()});
            }

            vec4 color;
            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_list_item_clicked);
            button_widget_set_on_right_clicked(button, on_button_list_item_right_clicked);

            // Button text.
            te_text_widget* text = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text);
                widget_set_parent(widget, button_widget_get_widget(button));
                widget_set_relative_position(
                    widget, (vec2){hpadding_in_button, vpadding_in_button});
                widget_set_relative_size(
                    widget, (vec2){1.0f - hpadding_in_button, 1.0f - vpadding_in_button});
            }
            text_widget_set_text_height(text, theme_get_text_height());

            inspector->item_buttons[i] = button;

            y_pos += theme_get_button_height();
        }
    }

    // Navigation menu.
    y_pos = theme_get_world_inspector_height() - nav_menu_height;
    {
        const float nav_item_width = (total_width - hspacing * 2.0f) / 3.0f;
        const float nav_button_pad = nav_item_width / 4.0f;
        const float nav_button_width = nav_item_width - nav_button_pad * 2.0f;

        // Left button.
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_custom_ptr(widget, inspector);
                widget_set_parent(widget, left_panel);
                widget_set_relative_position(widget, (vec2){hpadding + nav_button_pad, y_pos});
                widget_set_relative_size(
                    widget, (vec2){nav_button_width, theme_get_button_height()});
            }

            vec4 color;
            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_prev_page_clicked);

            // Button text.
            te_text_widget* text_widget = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, button_widget_get_widget(button));
                widget_set_relative_position(
                    widget, (vec2){hpadding_in_button, vpadding_in_button});
                widget_set_relative_size(
                    widget, (vec2){1.0f - hpadding_in_button, 1.0f - vpadding_in_button});
            }
            text_widget_set_text_height(text_widget, theme_get_text_height());

            unsigned int text_len;
            wchar_t* text = wchar_from_char("<", &text_len);
            text_widget_set_text_own(text_widget, text, text_len);
        }

        // Page text.
        {
            const float nav_tex_total_width =
                1.0f
                - (hpadding + nav_button_pad + nav_button_width + hspacing * 2.0f
                   + nav_button_width + nav_button_pad + hpadding);
            const float nav_tex_width = nav_tex_total_width / 2.0f;
            const float nav_tex_pad = nav_tex_total_width / 4.0f;

            te_text_widget* text_widget = text_widget_create();
            inspector->page_text = text_widget;
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, left_panel);
                widget_set_relative_position(
                    widget, (vec2){hpadding + nav_button_pad + nav_button_width + hspacing
                                       + nav_tex_pad,
                                   y_pos});
                widget_set_relative_size(widget, (vec2){nav_tex_width, nav_menu_height});
            }
            text_widget_set_text_height(text_widget, theme_get_text_height());

            unsigned int text_len;
            wchar_t* text = wchar_from_char("1 / 1", &text_len);
            text_widget_set_text_own(text_widget, text, text_len);
        }

        // Right button.
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_custom_ptr(widget, inspector);
                widget_set_parent(widget, left_panel);
                widget_set_relative_position(
                    widget,
                    (vec2){1.0f - hpadding - nav_button_pad - nav_button_width, y_pos});
                widget_set_relative_size(
                    widget, (vec2){nav_button_width, theme_get_button_height()});
            }

            vec4 color;
            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_next_page_clicked);

            // Button text.
            te_text_widget* text_widget = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, button_widget_get_widget(button));
                widget_set_relative_position(
                    widget, (vec2){hpadding_in_button, vpadding_in_button});
                widget_set_relative_size(
                    widget, (vec2){1.0f - hpadding_in_button, 1.0f - vpadding_in_button});
            }
            text_widget_set_text_height(text_widget, theme_get_text_height());

            unsigned int text_len;
            wchar_t* text = wchar_from_char(">", &text_len);
            text_widget_set_text_own(text_widget, text, text_len);
        }
    }
}
