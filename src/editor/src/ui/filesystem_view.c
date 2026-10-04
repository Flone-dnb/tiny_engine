#include <ui/filesystem_view.h>

#include <snprintf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <editor.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <io/import.h>
#include <widget/widget.h>
#include <widget/text_widget.h>
#include <widget/button_widget.h>
#include <ui/editor_theme.h>
#include <misc/wchar_funcs.h>

#define BUTTON_HIDDEN_X_POS 10.0f

struct te_filesystem_view {
    te_editor* editor;

    /* current path relative to the "res" directory. NULL if at root (res) directory.
     * ends with '/' */
    char* relative_path;

    /* NULL if not created yet */
    te_text_widget* current_path_text;
    te_text_widget* page_text;

    /* valid while spawned, buttons that fill all available space (moved outside of the viewport if should not be visible)
     * number of items in this array is @ref dir_entry_button_count */
    te_button_widget** dir_entry_buttons;

    /* entries of the current directory. Size of this array is @ref dir_entry_count */
    te_filesystem_entry* dir_entries;

    /* number of elements in @ref dir_entry_buttons */
    unsigned int dir_entry_button_count;

    /* number of elements in @ref dir_entries */
    unsigned int dir_entry_count;

    unsigned int current_page;
    unsigned int page_count;
};

te_filesystem_view*
filesystem_view_create(te_editor* editor) {
    te_filesystem_view* explorer = malloc(sizeof(te_filesystem_view));

    explorer->editor = editor;
    explorer->relative_path = NULL;
    explorer->current_path_text = NULL;
    explorer->page_text = NULL;
    explorer->dir_entry_buttons = NULL;
    explorer->dir_entries = NULL;
    explorer->dir_entry_count = 0;
    explorer->dir_entry_button_count = 0;
    explorer->current_page = 0;
    explorer->page_count = 0;

    return explorer;
}

void
filesystem_view_destroy(te_filesystem_view* explorer) {
    unsigned int i;
    for (i = 0; i < explorer->dir_entry_count; i++) {
        free(explorer->dir_entries[i].name);
    }

    free(explorer->dir_entry_buttons);
    free(explorer->dir_entries);
    free(explorer->relative_path);
    free(explorer);
}

static void
refresh_page_text(te_filesystem_view* explorer) {
    char* text;
    wchar_t* wtext;
    unsigned int text_len;
    int len;

    /* update page count (in case list changed) */
    unsigned int page_count = explorer->dir_entry_count / explorer->dir_entry_button_count;
    if (explorer->dir_entry_count % explorer->dir_entry_button_count > 0) {
        page_count += 1;
    }
    if (page_count == 0) {
        page_count = 1;
    }
    explorer->page_count = page_count;

    len = snprintf(NULL, 0, "%u / %u", explorer->current_page + 1, explorer->page_count);
    if (len < 0) {
        log_error(__FILE__, __LINE__, "snprintf error");
        abort();
    }
    text_len = (unsigned int)len;

    text = malloc(sizeof(char) * (text_len + 1));
    snprintf(text, text_len + 1, "%u / %u", explorer->current_page + 1, explorer->page_count);

    wtext = wchar_from_char(text, &text_len);
    text_widget_set_text_own(explorer->page_text, wtext, text_len);

    free(text);
}

static void
refresh_dir_entry_names(te_filesystem_view* explorer) {
    const float hpadding = editor_theme_get_horizontal_padding() / editor_theme_get_left_panel_width();

    unsigned int button_idx;
    unsigned int item_idx;
    unsigned int i;

    button_idx = 0;
    for (item_idx = explorer->current_page * explorer->dir_entry_button_count;
         button_idx < explorer->dir_entry_button_count && item_idx < explorer->dir_entry_count;
         button_idx++) {
        te_button_widget* button = explorer->dir_entry_buttons[button_idx];

        /* get button text widget */
        unsigned int child_count;
        te_widget** child_widgets =
            widget_get_child_widgets(button_widget_get_widget(button), &child_count);
        te_text_widget* button_text = NULL;
        for (i = 0; i < child_count; i++) {
            if (!widget_is_serialization_allowed(child_widgets[i])) {
                /* internal widget (rect) of the button */
                continue;
            }
            button_text = widget_get_owner(child_widgets[i]);
            break;
        }
        free(child_widgets);

        /* fix pos of the button (in case it was hidden previously) */
        {
            te_widget* widget = button_widget_get_widget(button);

            te_vec2 pos;
            widget_get_relative_position(widget, pos);
            pos[0] = hpadding;

            widget_set_relative_position(widget, pos);
        }

        if (explorer->relative_path != NULL && button_idx == 0) {
            /* button to go up the directory */
            unsigned int text_len;
            wchar_t* wtext = wchar_from_char("..", &text_len);
            text_widget_set_text_own(button_text, wtext, text_len);
        } else {
            te_filesystem_entry* entry = &explorer->dir_entries[item_idx];
            char* text;
            wchar_t* wtext;
            unsigned int text_len;

            text = malloc(sizeof(char) * ((entry->is_dir ? 4 : 0) + entry->name_len + 1));
            if (entry->is_dir) {
                memcpy(text, "[d] ", sizeof(char) * 4);
                memcpy(text + 4, entry->name, sizeof(char) * entry->name_len);
                text[4 + entry->name_len] = 0;
            } else {
                memcpy(text, entry->name, sizeof(char) * entry->name_len);
                text[entry->name_len] = 0;
            }

            wtext = wchar_from_char(text, &text_len);
            text_widget_set_text_own(button_text, wtext, text_len);

            free(text);
            item_idx += 1;
        }
    }

    /* hide remaining buttons */
    for (; button_idx < explorer->dir_entry_button_count; button_idx++) {
        te_widget* widget = button_widget_get_widget(explorer->dir_entry_buttons[button_idx]);

        te_vec2 pos;
        widget_get_relative_position(widget, pos);
        pos[0] = BUTTON_HIDDEN_X_POS;

        widget_set_relative_position(
            button_widget_get_widget(explorer->dir_entry_buttons[button_idx]), pos);
    }
}

static void
refresh_current_path_text(te_filesystem_view* explorer) {
    char* path;
    wchar_t* wtext;
    size_t relative_path_len;
    unsigned int text_len;

    relative_path_len = 0;
    if (explorer->relative_path != NULL) {
        relative_path_len = strlen(explorer->relative_path);
    }

    path = malloc(sizeof(char) * (4 + relative_path_len + 1));
    memcpy(path, "res/", sizeof(char) * 4);
    if (explorer->relative_path != NULL) {
        memcpy(path + 4, explorer->relative_path, sizeof(char) * relative_path_len);
    }
    path[4 + relative_path_len] = 0;

    wtext = wchar_from_char(path, &text_len);
    text_widget_set_text_own(explorer->current_path_text, wtext, text_len);

    free(path);
}

static void
collect_dir_entries(te_filesystem_view* explorer) {
    char* path;
    size_t relative_path_len;
    unsigned int i;

    for (i = 0; i < explorer->dir_entry_count; i++) {
        free(explorer->dir_entries[i].name);
    }
    free(explorer->dir_entries);
    explorer->dir_entry_count = 0;

    relative_path_len = 0;
    if (explorer->relative_path != NULL) {
        relative_path_len = strlen(explorer->relative_path);
    }

    path = malloc(sizeof(char) * (4 + relative_path_len + 1));
    memcpy(path, "res/", sizeof(char) * 4);
    if (explorer->relative_path != NULL) {
        memcpy(path + 4, explorer->relative_path, sizeof(char) * relative_path_len);
    }
    path[4 + relative_path_len] = 0;

    explorer->dir_entries = filesystem_list_directory(path, &explorer->dir_entry_count);
    free(path);

    explorer->current_page = 0;
    refresh_page_text(explorer);
    refresh_dir_entry_names(explorer);
}

static void
on_import_gltf_dir_selected(void* custom, const char* path) {
    te_filesystem_view* explorer = custom;

    const char* relative_path = explorer->relative_path == NULL ? "" : explorer->relative_path;
    import_file_as_world(editor_get_game_manager(explorer->editor), path, relative_path);

    collect_dir_entries(explorer);
}

static void
on_button_import_gltf_clicked(te_button_widget* button) {
    te_filesystem_view* explorer = widget_get_custom_ptr(button_widget_get_widget(button));

    editor_show_file_dialog(
        explorer->editor, explorer, on_import_gltf_dir_selected, NULL,
        TE_FDM_SELECT_EXISTING_FILE);
}

static void
on_button_prev_page_clicked(te_button_widget* button) {
    te_filesystem_view* explorer = widget_get_custom_ptr(button_widget_get_widget(button));
    if (explorer->current_page == 0) {
        return;
    }

    explorer->current_page -= 1;
    refresh_page_text(explorer);
    refresh_dir_entry_names(explorer);
}

static void
on_button_next_page_clicked(te_button_widget* button) {
    te_filesystem_view* explorer = widget_get_custom_ptr(button_widget_get_widget(button));
    if (explorer->current_page + 1 == explorer->page_count) {
        return;
    }

    explorer->current_page += 1;
    refresh_page_text(explorer);
    refresh_dir_entry_names(explorer);
}

static void
on_button_dir_entry_clicked(te_button_widget* button) {
    te_filesystem_entry* entry;
    unsigned int i;

    te_filesystem_view* explorer = widget_get_custom_ptr(button_widget_get_widget(button));
    size_t button_index = widget_get_custom_value(button_widget_get_widget(button));

    if (explorer->relative_path != NULL && button_index == 0) {
        /* go up the directory */
        const unsigned int path_len = (unsigned int)strlen(explorer->relative_path);
        unsigned int slash_pos = 0;
        for (i = path_len - 2; i > 0; i--) {
            if (explorer->relative_path[i] == '/') {
                slash_pos = i;
                break;
            }
        }
        explorer->relative_path[slash_pos] = 0;

        if (slash_pos == 0) {
            free(explorer->relative_path);
            explorer->relative_path = NULL;
        } else {
            char* new_path = malloc(sizeof(char) * (slash_pos + 2));
            memcpy(new_path, explorer->relative_path, sizeof(char) * slash_pos);
            new_path[slash_pos] = '/';
            new_path[slash_pos + 1] = 0;

            free(explorer->relative_path);
            explorer->relative_path = new_path;
        }

        refresh_current_path_text(explorer);
        collect_dir_entries(explorer);
        return;
    }

    if (explorer->relative_path != NULL) {
        /* skip ".." (go up) button */
        button_index -= 1;
    }

    entry = &explorer->dir_entries
                 [explorer->current_page * explorer->dir_entry_button_count + button_index];

    if (entry->is_dir) {
        char* new_path;

        /* change current dir path */
        size_t old_dir_len = 0;
        if (explorer->relative_path != NULL) {
            old_dir_len = strlen(explorer->relative_path);
        }

        new_path = malloc(sizeof(char) * (old_dir_len + entry->name_len + 2));
        if (explorer->relative_path != NULL) {
            memcpy(new_path, explorer->relative_path, sizeof(char) * old_dir_len);
        }
        memcpy(new_path + old_dir_len, entry->name, sizeof(char) * entry->name_len);
        new_path[old_dir_len + entry->name_len] = '/';
        new_path[old_dir_len + entry->name_len + 1] = 0;

        free(explorer->relative_path);
        explorer->relative_path = new_path;

        refresh_current_path_text(explorer);
        collect_dir_entries(explorer);
    } else {
        /* check file extension */
        unsigned int dot_pos = 0;
        for (i = entry->name_len - 1; i > 0; i--) {
            if (entry->name[i] == '.') {
                dot_pos = i;
                break;
            }
        }

        if (strcmp(entry->name + dot_pos + 1, "txt") == 0) {
            char* file_relative_path;
            size_t relative_path_len = 0;
            if (explorer->relative_path != NULL) {
                relative_path_len = strlen(explorer->relative_path);
            }

            file_relative_path =
                malloc(sizeof(char) * (relative_path_len + entry->name_len + 1));
            if (explorer->relative_path != NULL) {
                memcpy(
                    file_relative_path, explorer->relative_path,
                    sizeof(char) * relative_path_len);
            }
            memcpy(
                file_relative_path + relative_path_len, entry->name,
                sizeof(char) * entry->name_len);
            file_relative_path[relative_path_len + entry->name_len] = 0;

            editor_create_game_world(explorer->editor, file_relative_path);
            free(file_relative_path);
        }
    }
}

void
filesystem_view_refresh(te_filesystem_view* explorer) {
    collect_dir_entries(explorer);
}

void
filesystem_view_add(te_filesystem_view* explorer, te_widget* left_panel) {
    te_text_widget* text_widget;
    wchar_t* wtext;
    unsigned int text_len;
    unsigned int i;
    te_vec4 color;
    te_vec2 tmp2;
    te_vec2 size;
    te_vec2 pos;

    const float hpadding = editor_theme_get_horizontal_padding() / editor_theme_get_left_panel_width();
    const float vpadding = editor_theme_get_vertical_padding();
    const float hspacing = editor_theme_get_horizontal_spacing() / editor_theme_get_left_panel_width();
    const float dir_item_spacing = editor_theme_get_vertical_spacing() / 2.0f;
    const float nav_menu_height = editor_theme_get_button_height();
    const float hpadding_in_button = hpadding;
    const float vpadding_in_button = 0.0f;

    pos[0] = hpadding;
    pos[1] = editor_theme_get_world_inspector_height() + vpadding;

    size[0] = 1.0f - hpadding * 2.0f;
    size[1] = editor_theme_get_button_height();

    /* import GLTF button */
    {
        te_button_widget* button = button_widget_create();
        {
            te_widget* widget = button_widget_get_widget(button);
            widget_set_relative_position(widget, pos);
            widget_set_relative_size(widget, size);
            widget_set_parent(widget, left_panel);
            widget_set_custom_ptr(widget, explorer);
        }

        editor_theme_get_button_color(color);
        button_widget_set_color(button, color);

        editor_theme_get_button_color_hovered(color);
        button_widget_set_color_hovered(button, color);

        editor_theme_get_button_color_pressed(color);
        button_widget_set_color_pressed(button, color);

        button_widget_set_on_clicked(button, on_button_import_gltf_clicked);

        /* button text */
        text_widget = text_widget_create();
        {
            te_widget* widget = text_widget_get_widget(text_widget);
            widget_set_parent(widget, button_widget_get_widget(button));

            vec2_set(hpadding_in_button, vpadding_in_button, tmp2);
            widget_set_relative_position(widget, tmp2);

            vec2_set(1.0f - hpadding_in_button, 1.0f - vpadding_in_button, tmp2);
            widget_set_relative_size(widget, tmp2);
        }
        text_widget_set_text_height(text_widget, editor_theme_get_text_height());

        wtext = wchar_from_char("Import GLTF here", &text_len);
        text_widget_set_text_own(text_widget, wtext, text_len);
    }
    pos[1] += size[1];

    /* current dir name text */
    {
        explorer->current_path_text = text_widget_create();
        {
            te_widget* widget = text_widget_get_widget(explorer->current_path_text);
            widget_set_relative_position(widget, pos);
            widget_set_relative_size(widget, size);
            widget_set_parent(widget, left_panel);
        }

        text_widget_set_text_height(explorer->current_path_text, editor_theme_get_text_height());

        wtext = wchar_from_char("res/", &text_len);
        text_widget_set_text_own(explorer->current_path_text, wtext, text_len);
    }
    pos[1] += size[1];

    /* directory items */
    {
        float test_y;

        /* count how much buttons we can fit */
        explorer->dir_entry_button_count = 0;
        test_y = pos[1];
        do {
            test_y += dir_item_spacing + editor_theme_get_button_height();
            explorer->dir_entry_button_count += 1;
        } while (test_y + dir_item_spacing + editor_theme_get_button_height()
                 <= 1.0f - nav_menu_height);

        explorer->dir_entry_buttons =
            malloc(sizeof(te_button_widget*) * explorer->dir_entry_button_count);
        for (i = 0; i < explorer->dir_entry_button_count; i++) {
            te_button_widget* button;

            pos[1] += dir_item_spacing;

            button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, size);
                widget_set_parent(widget, left_panel);
                widget_set_custom_ptr(widget, explorer);
                widget_set_custom_value(widget, i);
            }

            editor_theme_get_button_color(color);
            button_widget_set_color(button, color);

            editor_theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            editor_theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_dir_entry_clicked);

            /* button text */
            text_widget = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, button_widget_get_widget(button));

                vec2_set(hpadding_in_button, vpadding_in_button, tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(1.0f - hpadding_in_button, 1.0f - vpadding_in_button, tmp2);
                widget_set_relative_size(widget, tmp2);
            }
            text_widget_set_text_height(text_widget, editor_theme_get_text_height());

            explorer->dir_entry_buttons[i] = button;
            pos[1] += editor_theme_get_button_height();
        }
    }

    /* navigation buttons */
    pos[1] = 1.0f - nav_menu_height;
    {
        const float nav_item_width = (1.0f - hspacing * 2.0f) / 3.0f;
        const float nav_button_pad = nav_item_width / 4.0f;
        const float nav_button_width = nav_item_width - nav_button_pad * 2.0f;

        /* left button */
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);

                vec2_set(hpadding + nav_button_pad, pos[1], tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(nav_button_width, editor_theme_get_button_height(), tmp2);
                widget_set_relative_size(widget, tmp2);

                widget_set_parent(widget, left_panel);
                widget_set_custom_ptr(widget, explorer);
            }

            editor_theme_get_button_color(color);
            button_widget_set_color(button, color);

            editor_theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            editor_theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_prev_page_clicked);

            /* button text */
            text_widget = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, button_widget_get_widget(button));

                vec2_set(hpadding_in_button, vpadding_in_button, tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(1.0f - hpadding_in_button, 1.0f - vpadding_in_button, tmp2);
                widget_set_relative_size(widget, tmp2);
            }
            text_widget_set_text_height(text_widget, editor_theme_get_text_height());

            wtext = wchar_from_char("<", &text_len);
            text_widget_set_text_own(text_widget, wtext, text_len);
        }

        /* page text */
        {
            const float nav_tex_total_width =
                1.0f
                - (hpadding + nav_button_pad + nav_button_width + hspacing * 2.0f
                   + nav_button_width + nav_button_pad + hpadding);
            const float nav_tex_width = nav_tex_total_width / 2.0f;
            const float nav_tex_pad = nav_tex_total_width / 4.0f;

            te_text_widget* text_widget = text_widget_create();
            explorer->page_text = text_widget;
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, left_panel);

                vec2_set(
                    hpadding + nav_button_pad + nav_button_width + hspacing + nav_tex_pad,
                    pos[1], tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(nav_tex_width, nav_menu_height, tmp2);
                widget_set_relative_size(widget, tmp2);
            }
            text_widget_set_text_height(text_widget, editor_theme_get_text_height());

            wtext = wchar_from_char("1 / 1", &text_len);
            text_widget_set_text_own(text_widget, wtext, text_len);
        }

        /* right button */
        {
            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);

                vec2_set(1.0f - hpadding - nav_button_pad - nav_button_width, pos[1], tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(nav_button_width, editor_theme_get_button_height(), tmp2);
                widget_set_relative_size(widget, tmp2);

                widget_set_custom_ptr(widget, explorer);
                widget_set_parent(widget, left_panel);
            }

            editor_theme_get_button_color(color);
            button_widget_set_color(button, color);

            editor_theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            editor_theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

            button_widget_set_on_clicked(button, on_button_next_page_clicked);

            /* button text */
            text_widget = text_widget_create();
            {
                te_widget* widget = text_widget_get_widget(text_widget);
                widget_set_parent(widget, button_widget_get_widget(button));

                vec2_set(hpadding_in_button, vpadding_in_button, tmp2);
                widget_set_relative_position(widget, tmp2);

                vec2_set(1.0f - hpadding_in_button, 1.0f - vpadding_in_button, tmp2);
                widget_set_relative_size(widget, tmp2);
            }
            text_widget_set_text_height(text_widget, editor_theme_get_text_height());

            wtext = wchar_from_char(">", &text_len);
            text_widget_set_text_own(text_widget, wtext, text_len);
        }
    }

    collect_dir_entries(explorer);
}
