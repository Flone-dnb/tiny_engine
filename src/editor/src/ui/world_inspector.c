#include "ui/world_inspector.h"

#include <stdio.h>
#include <stdbool.h>
#include <world.h>
#include <ui/theme.h>
#include <ui/property_inspector.h>
#include <game/game_object_info.h>
#include <game/model.h>
#include <game/camera.h>
#include <widget/widget.h>
#include <widget/text_widget.h>
#include <widget/button_widget.h>
#include <misc/wchar_funcs.h>
#include <type_database.h>
#include <editor.h>
#include <game_manager.h>
#include <render/renderer.h>
#include <io/filesystem.h>
#include <io/log.h>

#define BUTTON_HIDDEN_X_POS 10.0f
#define CREATE_NEW_OBJ_TEXT "Create new object"

enum te_object_menu_options {
    TE_OMO_ATTACH_TO_OBJ = 0,
    TE_OMO_REMOVE_ATTACHMENT,
    TE_OMO_DELETE_OBJ,

    TE_OMO_COUNT, //< Marks the total number of options.
};

enum te_world_inspector_state {
    TE_WIS_SHOW_WORLD_OBJECTS, //< Default state where game objects of the world are shown.
    TE_WIS_CREATE_NEW_OBJECT,  //< When clicked the button to create a new game object.
    TE_WIS_OBJECT_MENU, //< When right clicked on a game object and possible options to manage the object are shown.
    TE_WIS_SHOW_ATTACH_TO, //< When choosing to attach a selected object.
};

typedef struct te_world_item_info {
    // NULL if @ref widget is valid.
    te_game_object_info* game_object_info;

    // NULL if @ref game_object_info is valid.
    te_widget* widget;

    // More than 0 for attached/child objects.
    unsigned int indent;
} te_world_item_info;

struct te_world_inspector {
    // Do not free/destroy, parent widget.
    te_widget* left_panel;

    te_editor* editor;

    // Do not free/destroy.
    te_property_inspector* property_inspector;

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

    // If not NULL stores a copy from @ref item_list that was selected for object menu/operations (delete, attach, etc.).
    // For widgets stores pointer to te_widget object while type_id stores the owner's type ID.
    te_world_item_info* selected_item;

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
world_inspector_create(te_editor* editor, te_property_inspector* property_inspector) {
    te_world_inspector* inspector = malloc(sizeof(te_world_inspector));

    inspector->property_inspector = property_inspector;
    inspector->left_panel = NULL;
    inspector->editor = editor;
    inspector->item_list = NULL;
    inspector->button_2dobj = NULL;
    inspector->button_3dobj = NULL;
    inspector->game_world = NULL;
    inspector->top_button = NULL;
    inspector->top_button_text = NULL;
    inspector->page_text = NULL;
    inspector->selected_item = NULL;
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
    free(inspector->selected_item);
    free(inspector);
}

static void
refresh_button_highlight(te_world_inspector* inspector) {
    // Clear highlight.
    vec4 color;
    theme_get_button_color(color);
    for (unsigned int i = 0; i < inspector->item_buttons_count; i++) {
        button_widget_set_color(inspector->item_buttons[i], color);
    }

    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS) {
        return;
    }

    // Might be a model, camera, widget or something else.
    void* inspected_object =
        property_inspector_get_inspected_obj(inspector->property_inspector);
    if (inspected_object == NULL) {
        return;
    }

    bool is_widget = false;
    {
        // For widgets, property inspector stores the final type (not the base type te_widget) but
        // world inspector expects the base type (te_widget), check if this is the case:
        const char* game_obj_type_id =
            property_inspector_get_inspected_obj_type_id(inspector->property_inspector);
        if (game_obj_type_id == NULL) {
            log_error("expected type id to be valid");
            abort();
        }

        const te_type_info* type_info = type_database_get_type_info(game_obj_type_id);
        if (type_info == NULL) {
            log_error("expected type info to be valid");
            abort();
        }

        if (type_info->get_widget != NULL) {
            inspected_object = type_info->get_widget(inspected_object);
            is_widget = true;
        }
    }

    for (unsigned int i = 0; i < inspector->item_buttons_count; i++) {
        const unsigned int item_idx =
            inspector->current_page * inspector->item_buttons_count + i;
        if (item_idx >= inspector->item_list_count) {
            break;
        }

        te_world_item_info* info = &((te_world_item_info*)inspector->item_list)[item_idx];
        if (is_widget) {
            if (info->widget != inspected_object) {
                continue;
            }
        } else {
            if (info->game_object_info->game_object != inspected_object) {
                continue;
            }
        }

        theme_get_accent_color(color);
        button_widget_set_color(inspector->item_buttons[i], color);
        break;
    }
}

// Uses item_list and updates text on the buttons depending on the current world inspector state.
static void
refresh_item_names(te_world_inspector* inspector) {
    const float hpadding = theme_get_horizontal_padding() / theme_get_left_panel_width();
    const float button_width = 1.0f - hpadding * 2.0f;
    const float indent_size = hpadding * 2.0f;

    unsigned int button_idx = 0;
    for (unsigned int item_idx = inspector->current_page * inspector->item_buttons_count;
         button_idx < inspector->item_buttons_count && item_idx < inspector->item_list_count;
         button_idx++, item_idx++) {
        te_button_widget* button = inspector->item_buttons[button_idx];

        // Get button text widget.
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

        unsigned int indent = 0;

        // Prepare new text.
        const char* text_to_display = "";
        switch (inspector->state) {
            case (TE_WIS_SHOW_WORLD_OBJECTS):
            case (TE_WIS_SHOW_ATTACH_TO): {
                te_world_item_info* info =
                    &((te_world_item_info*)inspector->item_list)[item_idx];
                indent = info->indent;
                if (info->game_object_info != NULL) {
                    text_to_display =
                        info->game_object_info->get_name(info->game_object_info->game_object);
                    if (text_to_display == NULL) {
                        text_to_display = info->game_object_info->type_id;
                    }
                } else {
                    if (info->widget == NULL) {
                        log_error("expected widget pointer to be valid");
                        abort();
                    }
                    text_to_display = widget_get_name(info->widget);
                    if (text_to_display == NULL) {
                        text_to_display = widget_get_owner_type_id(info->widget);
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

    refresh_button_highlight(inspector);
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
    info->widget = widget;
    info->game_object_info = NULL;
    info->indent = (*indent);

    (*item_idx) += 1;

    (*indent) += 1;

    unsigned int child_count;
    te_widget** child_widgets = widget_get_child_widgets(widget, &child_count);
    for (unsigned int i = 0; i < child_count; i++) {
        save_widgets_to_list_recurive(child_widgets[i], items, item_idx, indent);
    }
    free(child_widgets);

    (*indent) -= 1;
}

static void
count_widgets_recursive(te_widget* widget, unsigned int* count) {
    if (!widget_is_serialization_allowed(widget)) {
        return;
    }

    (*count) += 1;

    unsigned int child_count;
    te_widget** child_widgets = widget_get_child_widgets(widget, &child_count);
    for (unsigned int i = 0; i < child_count; i++) {
        count_widgets_recursive(child_widgets[i], count);
    }
    free(child_widgets);
}

static void
rebuild_item_list_to_display_world_objects(te_world_inspector* inspector) {
    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS
        && inspector->state != TE_WIS_SHOW_ATTACH_TO) {
        log_error("unexpected state");
        abort();
    }

    if (inspector->state == TE_WIS_SHOW_ATTACH_TO) {
        if (inspector->selected_item == NULL) {
            log_error("expected selected object to be valid");
            abort();
        }
    }

    property_inspector_hide(inspector->property_inspector);
    editor_set_gizmo(inspector->editor, NULL);

    free(inspector->item_list);
    inspector->item_list = NULL;
    inspector->item_list_count = 0;

    te_world_item_info* world_items = NULL;

    te_world* world = inspector->game_world;
    if (world == NULL) {
        log_error("expected a valid game world");
        abort();
    }

    if (inspector->is_3dobj_mode_selected) {
        unsigned int root_game_objects = 0;
        te_game_object_info** infos = world_get_root_game_objects(world, &root_game_objects);

        // Count items.
        inspector->item_list_count = 0;
        for (unsigned int i = 0; i < root_game_objects; i++) {
            te_game_object_info* info = infos[i];

            const te_type_info* type_info = type_database_get_type_info(info->type_id);
            if (info->type_id == NULL) {
                continue;
            }
            if (!type_info->is_serialization_allowed(info->game_object)) {
                continue;
            }

            if (info->type == TE_GOT_MODEL) {
                // Models are special because they can have child models or attached camera.
                te_model* model = info->game_object;

                te_camera* attached_camera = model_get_attached_camera(model);
                if (attached_camera != NULL
                    && camera_is_serialization_allowed(attached_camera)) {
                    inspector->item_list_count += 1;
                }

                unsigned int child_idx = 0;
                while (true) {
                    te_model* child_model = model_get_child_model(model, child_idx);
                    if (child_model == NULL) {
                        break;
                    }

                    if (model_is_serialization_allowed(child_model)) {
                        inspector->item_list_count += 1;
                    }
                    child_idx += 1;
                }
            }

            inspector->item_list_count += 1;
        }

        // Add counted items to list.
        if (inspector->item_list_count > 0) {
            world_items = malloc(sizeof(te_world_item_info) * inspector->item_list_count);
            unsigned int item_idx = 0;

            for (unsigned int i = 0; i < root_game_objects; i++) {
                te_game_object_info* game_object_info = infos[i];

                const te_type_info* type_info =
                    type_database_get_type_info(game_object_info->type_id);
                if (game_object_info->type_id == NULL) {
                    continue;
                }
                if (!type_info->is_serialization_allowed(game_object_info->game_object)) {
                    continue;
                }

                te_world_item_info* item_info = &world_items[item_idx];
                item_info->indent = 0;
                item_info->game_object_info = game_object_info;
                item_info->widget = NULL;

                item_idx += 1;

                if (game_object_info->type == TE_GOT_MODEL) {
                    // Models are special because they can have child model or attached camera.
                    te_model* model = game_object_info->game_object;

                    te_camera* attached_camera = model_get_attached_camera(model);
                    if (attached_camera != NULL
                        && camera_is_serialization_allowed(attached_camera)) {
                        te_world_item_info* item_info = &world_items[item_idx];
                        item_info->indent = 1;
                        item_info->game_object_info =
                            camera_get_game_object_info(attached_camera);
                        item_info->widget = NULL;

                        item_idx += 1;
                    }

                    unsigned int child_idx = 0;
                    while (true) {
                        te_model* child_model = model_get_child_model(model, child_idx);
                        if (child_model == NULL) {
                            break;
                        }

                        if (model_is_serialization_allowed(child_model)) {
                            te_world_item_info* item_info = &world_items[item_idx];
                            item_info->indent = 1;
                            item_info->game_object_info =
                                model_get_game_object_info(child_model);
                            item_info->widget = NULL;

                            item_idx += 1;
                        }
                        child_idx += 1;
                    }
                }
            }
        }

        free(infos);
    } else {
        unsigned int root_count = 0;
        te_widget** root_widgets = world_get_widgets(world, &root_count);

        // Count how much items we have in total.
        inspector->item_list_count = 0;
        for (unsigned int i = 0; i < root_count; i++) {
            count_widgets_recursive(root_widgets[i], &inspector->item_list_count);
        }

        // Save items.
        if (inspector->item_list_count > 0) {
            world_items = malloc(sizeof(te_world_item_info) * inspector->item_list_count);
            unsigned int indent = 0;
            unsigned int item_idx = 0;
            for (unsigned int i = 0; i < root_count; i++) {
                save_widgets_to_list_recurive(
                    root_widgets[i], world_items, &item_idx, &indent);
            }
        }

        free(root_widgets);
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
        inspector->state = TE_WIS_SHOW_WORLD_OBJECTS;

        // Restore the original button state.
        unsigned int text_len;
        wchar_t* wtext = wchar_from_char(CREATE_NEW_OBJ_TEXT, &text_len);
        text_widget_set_text_own(inspector->top_button_text, wtext, text_len);

        free(inspector->selected_item);
        inspector->selected_item = NULL;

        inspector->current_page = 0;
    }

    rebuild_item_list_to_display_world_objects(inspector);
}

void
world_inspector_select_obj(te_world_inspector* inspector, te_game_object_info* target_info) {
    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS) {
        return;
    }

    if (target_info == NULL) {
        property_inspector_hide(inspector->property_inspector);
        refresh_button_highlight(inspector);
        editor_set_gizmo(inspector->editor, NULL);
        return;
    }

    // Switch the current page to show the selected item.
    bool found = false;
    for (unsigned int page_idx = 0; page_idx < inspector->page_count; page_idx++) {
        for (unsigned int i = 0; i < inspector->item_buttons_count; i++) {
            te_world_item_info* info =
                &((te_world_item_info*)
                      inspector->item_list)[page_idx * inspector->item_buttons_count + i];
            if (info->game_object_info != target_info) {
                continue;
            }

            inspector->current_page = page_idx;
            found = true;
            break;
        }
        if (found) {
            break;
        }
    }

    bool is_special = false;
    if (!found) {
        // Some objects (such as not serializable) are not displayed in the world inspector so it's fine.
        // Check if the specified object is a special case (for example it's a camera's visualization model).
        if (target_info->type != TE_GOT_MODEL) {
            return;
        }
        void* custom_ptr = model_get_custom_ptr(target_info->game_object);
        if (custom_ptr == NULL) {
            return;
        }

        // Check custom_ptr.
        for (unsigned int page_idx = 0; page_idx < inspector->page_count; page_idx++) {
            for (unsigned int i = 0; i < inspector->item_buttons_count; i++) {
                te_world_item_info* info =
                    &((te_world_item_info*)
                          inspector->item_list)[page_idx * inspector->item_buttons_count + i];
                if (info->game_object_info->game_object == custom_ptr) {
                    // Selected camera visualization model, display camera properties.
                    target_info = info->game_object_info;
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }

        if (!found) {
            return;
        }
    }

    if (target_info->type == TE_GOT_MODEL) {
        editor_set_gizmo(inspector->editor, target_info->game_object);
    }

    if (!is_special) {
        property_inspector_show(
            inspector->property_inspector, target_info->game_object, target_info->type_id);
        refresh_item_names(inspector);
        refresh_page_text(inspector);
    }
}

void
world_inspector_refresh_names(te_world_inspector* inspector) {
    if (inspector->state == TE_WIS_SHOW_WORLD_OBJECTS
        || inspector->state == TE_WIS_SHOW_ATTACH_TO) {
        refresh_item_names(inspector);
    }
}

static void
on_button_3dobj_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS
        && inspector->state != TE_WIS_SHOW_ATTACH_TO) {
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
    if (inspector->state != TE_WIS_SHOW_WORLD_OBJECTS
        && inspector->state != TE_WIS_SHOW_ATTACH_TO) {
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

    property_inspector_hide(inspector->property_inspector);
    editor_set_gizmo(inspector->editor, NULL);

    if (inspector->state == TE_WIS_SHOW_WORLD_OBJECTS) {
        inspector->state = TE_WIS_CREATE_NEW_OBJECT;
    } else if (
        inspector->state == TE_WIS_CREATE_NEW_OBJECT || inspector->state == TE_WIS_OBJECT_MENU
        || inspector->state == TE_WIS_SHOW_ATTACH_TO) {
        inspector->state = TE_WIS_SHOW_WORLD_OBJECTS;
    } else {
        return;
    }

    if (inspector->state == TE_WIS_SHOW_WORLD_OBJECTS) {
        // Restore the original button state.
        unsigned int text_len;
        wchar_t* wtext = wchar_from_char(CREATE_NEW_OBJ_TEXT, &text_len);
        text_widget_set_text_own(inspector->top_button_text, wtext, text_len);

        free(inspector->selected_item);
        inspector->selected_item = NULL;

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
    editor_set_gizmo(inspector->editor, NULL);

    switch (inspector->state) {
        case (TE_WIS_SHOW_WORLD_OBJECTS): {
            const size_t button_index =
                widget_get_custom_value(button_widget_get_widget(button));

            te_world_item_info* selected_info =
                &((te_world_item_info*)inspector->item_list)
                    [inspector->current_page * inspector->item_buttons_count + button_index];

            // Prepare variables for property inspector.
            void* target_object = NULL;
            const char* target_type_id = NULL;

            if (selected_info->game_object_info != NULL) {
                target_object = selected_info->game_object_info->game_object;
                target_type_id = selected_info->game_object_info->type_id;
                if (selected_info->game_object_info->type == TE_GOT_MODEL) {
                    editor_set_gizmo(
                        inspector->editor, selected_info->game_object_info->game_object);
                }
            } else {
                if (selected_info->widget == NULL) {
                    log_error("expected widget pointer to be valid");
                    abort();
                }
                target_object = widget_get_owner(selected_info->widget);
                target_type_id = widget_get_owner_type_id(selected_info->widget);
            }

            property_inspector_show(
                inspector->property_inspector, target_object, target_type_id);
            refresh_button_highlight(inspector);
            break;
        }
        case (TE_WIS_CREATE_NEW_OBJECT): {
            // Selected type of a new game object.
            // Button name stores type ID from type database.

            // Get button text.
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
            break;
        }
        case (TE_WIS_OBJECT_MENU): {
            if (inspector->selected_item == NULL) {
                log_error("expected selected object to be valid");
                abort();
            }

            const size_t option_index =
                widget_get_custom_value(button_widget_get_widget(button));
            if (option_index == TE_OMO_ATTACH_TO_OBJ) {
                inspector->state = TE_WIS_SHOW_ATTACH_TO;
                rebuild_item_list_to_display_world_objects(inspector);
                refresh_page_text(inspector);
            } else if (option_index == TE_OMO_DELETE_OBJ) {
                // Get type info.
                const te_type_info* type_info = NULL;
                if (inspector->selected_item->game_object_info != NULL) {
                    type_info = type_database_get_type_info(
                        inspector->selected_item->game_object_info->type_id);

                    // Also check if we have gizmo on the object we are about to delete.
                    editor_on_before_game_obj_deleted(
                        inspector->editor, inspector->selected_item->game_object_info);
                } else {
                    if (inspector->selected_item->widget == NULL) {
                        log_error("expected widget pointer to be valid");
                        abort();
                    }
                    type_info = type_database_get_type_info(
                        widget_get_owner_type_id(inspector->selected_item->widget));
                }

                // Despawn and destroy.
                if (inspector->selected_item->widget != NULL) {
                    void* widget_owner = widget_get_owner(inspector->selected_item->widget);
                    type_info->despawn(inspector->game_world, widget_owner);
                    type_info->destroy(widget_owner);
                } else {
                    void* game_object =
                        inspector->selected_item->game_object_info->game_object;
                    type_info->despawn(inspector->game_world, game_object);
                    type_info->destroy(game_object);
                }

                free(inspector->selected_item);
                inspector->selected_item = NULL;

                on_top_button_clicked(inspector->top_button);
            } else if (option_index == TE_OMO_REMOVE_ATTACHMENT) {
                if (inspector->selected_item->game_object_info != NULL) {
                    switch (inspector->selected_item->game_object_info->type) {
                        case (TE_GOT_CAMERA): {
                            te_model* parent = camera_get_parent_model(
                                inspector->selected_item->game_object_info->game_object);
                            if (parent != NULL) {
                                model_attach_camera(parent, NULL);
                            }
                            break;
                        }
                        case (TE_GOT_MODEL): {
                            model_set_parent(
                                inspector->selected_item->game_object_info->game_object, NULL);
                            break;
                        }
                    }
                } else {
                    if (inspector->selected_item->widget == NULL) {
                        log_error("expected widget pointer to be valid");
                        abort();
                    }
                    widget_set_parent(inspector->selected_item->widget, NULL);
                }

                on_top_button_clicked(inspector->top_button);
            } else {
                log_error_fmt("unexpected option %zu", option_index);
                abort();
            }
            break;
        }
        case (TE_WIS_SHOW_ATTACH_TO): {
            if (inspector->selected_item == NULL) {
                log_error("expected selected object to be valid");
                abort();
            }

            const size_t button_index =
                widget_get_custom_value(button_widget_get_widget(button));
            te_world_item_info* target_info =
                &((te_world_item_info*)inspector->item_list)
                    [inspector->current_page * inspector->item_buttons_count + button_index];

            if (inspector->selected_item->game_object_info != NULL) {
                switch (inspector->selected_item->game_object_info->type) {
                    case (TE_GOT_CAMERA): {
                        if (target_info->game_object_info == NULL) {
                            log_error("can't attach camera to a non game object");
                            abort();
                        }
                        if (target_info->game_object_info->type != TE_GOT_MODEL) {
                            log_error("can't attach camera to a non-model game object");
                            abort();
                        }
                        model_attach_camera(
                            target_info->game_object_info->game_object,
                            inspector->selected_item->game_object_info->game_object);
                        break;
                    }
                    case (TE_GOT_MODEL): {
                        if (target_info->game_object_info == NULL) {
                            log_error("can't attach model to a non game object");
                            abort();
                        }
                        if (target_info->game_object_info->type != TE_GOT_MODEL) {
                            log_error("can't attach model to a non-model game object");
                            abort();
                        }
                        model_set_parent(
                            inspector->selected_item->game_object_info->game_object,
                            target_info->game_object_info->game_object);
                        break;
                    }
                }
            } else {
                if (inspector->selected_item->widget == NULL) {
                    log_error("expected widget pointer to be valid");
                    abort();
                }
                if (target_info->widget == NULL) {
                    log_error("can't attach widget to a non-widget object");
                    abort();
                }
                widget_set_parent(inspector->selected_item->widget, target_info->widget);
            }

            on_top_button_clicked(inspector->top_button);
            break;
        }
    }
}

static void
on_button_list_item_right_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    property_inspector_hide(inspector->property_inspector);
    editor_set_gizmo(inspector->editor, NULL);
    refresh_button_highlight(inspector);

    const size_t button_index = widget_get_custom_value(button_widget_get_widget(button));

    if (inspector->state == TE_WIS_SHOW_WORLD_OBJECTS) {
        inspector->state = TE_WIS_OBJECT_MENU;

        te_world_item_info* target_info =
            &((te_world_item_info*)inspector->item_list)
                [inspector->current_page * inspector->item_buttons_count + button_index];

        inspector->selected_item = malloc(sizeof(te_world_item_info));
        memcpy(inspector->selected_item, target_info, sizeof(te_world_item_info));

        inspector->item_list_count = TE_OMO_COUNT;

        const char** option_names = malloc(sizeof(const char*) * inspector->item_list_count);
        option_names[TE_OMO_ATTACH_TO_OBJ] = "attach to...";
        option_names[TE_OMO_REMOVE_ATTACHMENT] = "remove attachment";
        option_names[TE_OMO_DELETE_OBJ] = "delete object";

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

static void
on_button_world_settings_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));

    property_inspector_hide(inspector->property_inspector);
    refresh_button_highlight(inspector);
    editor_set_gizmo(inspector->editor, NULL);

    te_light_params* light_params = renderer_get_light_params(
        game_manager_get_renderer(editor_get_game_manager(inspector->editor)));
    property_inspector_show(inspector->property_inspector, light_params, "light_params");
}

static void on_add_world_selected(void* custom, const char* absolute_path) {
    te_world_inspector* inspector = custom;

    char* relative_path = filesystem_convert_path_to_relative(absolute_path);

    te_world* game_world = editor_get_game_world(inspector->editor);
    world_add_from_file(game_world, relative_path, false);

    world_inspector_rebuild_list(inspector, game_world);

    free(relative_path);
}

static void
on_button_add_world_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));

    editor_show_file_dialog(
        inspector->editor, inspector, on_add_world_selected, NULL,
        TE_FDM_SELECT_EXISTING_FILE);
}

void
world_inspector_add(te_world_inspector* inspector, te_widget* left_panel) {
    if (inspector->left_panel != NULL) {
        log_error("world inspector is already displayed");
        abort();
    }

    inspector->left_panel = left_panel;

    // Relative to the left panel.
    const float hspacing = theme_get_horizontal_spacing() / theme_get_left_panel_width();
    const float vspacing = theme_get_vertical_spacing();
    const float hpadding = theme_get_horizontal_padding() / theme_get_left_panel_width();
    const float hpadding_in_button = hpadding;
    const float vpadding_in_button = 0.0f;
    const float total_width = 1.0f - hpadding * 2.0f;

    // Title text.
    float y_pos = vspacing;

    // World settings.
    {
        te_button_widget* button = button_widget_create();
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

        button_widget_set_on_clicked(button, on_button_world_settings_clicked);

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
        wchar_t* text = wchar_from_char("World settings", &text_len);
        text_widget_set_text_own(text_widget, text, text_len);
    }
    y_pos += theme_get_button_height() + vspacing;

    // Add world.
    {
        te_button_widget* button = button_widget_create();
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

        button_widget_set_on_clicked(button, on_button_add_world_clicked);

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
        wchar_t* text = wchar_from_char("Add world from file", &text_len);
        text_widget_set_text_own(text_widget, text, text_len);
    }
    y_pos += theme_get_button_height() + vspacing;

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
    y_pos += theme_get_button_height() + vspacing;

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
        const float list_item_spacing = vspacing;
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
