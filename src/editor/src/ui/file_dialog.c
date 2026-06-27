#include "ui/file_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <world.h>
#include <ui/theme.h>
#include <io/log.h>
#include <misc/wchar_funcs.h>
#include <io/filesystem.h>
#include <widget/widget.h>
#include <widget/rect_widget.h>
#include <widget/button_widget.h>
#include <widget/text_widget.h>
#include <widget/text_edit_widget.h>

#define BUTTON_HIDDEN_X_POS 10.0f
#define DIALOG_WIDTH 0.45f
#define DIALOG_HEIGHT 0.4f

struct te_file_dialog {
    // Always valid, absolute path to the current directory.
    char* current_path;

    // Custom user-specified pointer to pass to @ref on_selected.
    void* custom;

    void (*on_selected)(void* custom, const char* path_to_file);
    void (*on_cancel)(void* custom);

    // Always valid, parent widget of all file explorer widgets.
    te_widget* spawned_widget;

    // Always valid, Widget used to display the current path.
    te_text_widget* current_path_text;

    // Always valid, text that displays the page number.
    te_text_widget* page_text;

    // Not NULL if selecting a new file. Has filename for new file.
    te_text_edit_widget* filename_text_edit;

    // Always valid, buttons for filesystem entries.
    // The number of elements in this array is @ref entry_button_count.
    te_button_widget** entry_buttons;

    // Entries of @ref current_path. Size of this array is @ref dir_entry_count.
    te_filesystem_entry* dir_entries;

    // Number of elements in @ref entry_buttons.
    unsigned int entry_button_count;

    unsigned int current_page;
    unsigned int page_count;

    // 0xFFFFFFFF if nothing selected.
    unsigned int selected_button_index;

    // Number of elements in @ref dir_entries.
    unsigned int dir_entry_count;

    enum te_file_dialog_mode mode;
};

void
file_dialog_destroy(te_file_dialog* file_dialog) {
    te_world* world = widget_get_world(file_dialog->spawned_widget);
    world_despawn_widget(world, file_dialog->spawned_widget);
    widget_destroy(file_dialog->spawned_widget);

    for (unsigned int i = 0; i < file_dialog->dir_entry_count; i++) {
        free(file_dialog->dir_entries[i].name);
    }
    free(file_dialog->dir_entries);

    free(file_dialog->entry_buttons);
    free(file_dialog->current_path);
    free(file_dialog);
}

static void
refresh_entry_button_names(te_file_dialog* file_dialog) {
    const float hpadding = theme_get_horizontal_padding() / DIALOG_WIDTH;

    unsigned int button_idx = 0;
    for (unsigned int item_idx =
             file_dialog->current_page * (file_dialog->entry_button_count - 1);
         button_idx < file_dialog->entry_button_count; button_idx++) {
        if (button_idx > 0) {
            // Do this check here instead of in the loop definition to be able
            // to create ".." (go up) buttons in empty directories.
            if (item_idx >= file_dialog->dir_entry_count) {
                break;
            }
        }

        te_button_widget* button = file_dialog->entry_buttons[button_idx];

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

        // Fix pos of the button (in case it was hidden previously).
        {
            te_widget* widget = button_widget_get_widget(button);

            vec2 pos;
            widget_get_relative_position(widget, pos);
            pos[0] = hpadding;

            widget_set_relative_position(widget, pos);
        }

        if (button_idx == 0) {
            // Go up ".." button.
            unsigned int text_len;
            wchar_t* wtext = wchar_from_char("..", &text_len);
            text_widget_set_text_own(button_text, wtext, text_len);
        } else {
            te_filesystem_entry* entry = &file_dialog->dir_entries[item_idx];

            char* text =
                malloc(sizeof(char) * ((entry->is_dir ? 4 : 0) + entry->name_len + 1));
            if (entry->is_dir) {
                memcpy(text, "[d] ", sizeof(char) * 4);
                memcpy(text + 4, entry->name, sizeof(char) * entry->name_len);
                text[4 + entry->name_len] = 0;
            } else {
                memcpy(text, entry->name, sizeof(char) * entry->name_len);
                text[entry->name_len] = 0;
            }

            unsigned int text_len;
            wchar_t* wtext = wchar_from_char(text, &text_len);
            text_widget_set_text_own(button_text, wtext, text_len);

            free(text);
            item_idx += 1;
        }
    }

    // Hide remaining buttons.
    for (; button_idx < file_dialog->entry_button_count; button_idx++) {
        te_widget* widget = button_widget_get_widget(file_dialog->entry_buttons[button_idx]);

        vec2 pos;
        widget_get_relative_position(widget, pos);
        pos[0] = BUTTON_HIDDEN_X_POS;

        widget_set_relative_position(
            button_widget_get_widget(file_dialog->entry_buttons[button_idx]), pos);
    }

    // Update current path text.
    unsigned int text_len;
    wchar_t* text = wchar_from_char(file_dialog->current_path, &text_len);
    text_widget_set_text_own(file_dialog->current_path_text, text, text_len);
}

static void
refresh_page_text(te_file_dialog* file_dialog) {
    // Update page count (in case list changed).
    unsigned int page_count = file_dialog->dir_entry_count / file_dialog->entry_button_count;
    if (file_dialog->dir_entry_count % file_dialog->entry_button_count > 0) {
        page_count += 1;
    }
    if (page_count == 0) {
        page_count = 1;
    }
    file_dialog->page_count = page_count;

    const int len =
        snprintf(NULL, 0, "%u / %u", file_dialog->current_page + 1, file_dialog->page_count);
    if (len < 0) {
        log_error("snprintf error");
        abort();
    }
    unsigned int text_len = (unsigned int)len;

    char* text = malloc(sizeof(char) * (text_len + 1));
    snprintf(
        text, text_len + 1, "%u / %u", file_dialog->current_page + 1, file_dialog->page_count);

    wchar_t* wtext = wchar_from_char(text, &text_len);
    text_widget_set_text_own(file_dialog->page_text, wtext, text_len);

    free(text);
}

static void
on_button_prev_page_clicked(te_button_widget* button) {
    te_file_dialog* file_dialog = widget_get_custom_ptr(button_widget_get_widget(button));
    if (file_dialog->current_page == 0) {
        return;
    }

    file_dialog->current_page -= 1;
    refresh_page_text(file_dialog);
    refresh_entry_button_names(file_dialog);
}

static void
on_button_next_page_clicked(te_button_widget* button) {
    te_file_dialog* file_dialog = widget_get_custom_ptr(button_widget_get_widget(button));
    if (file_dialog->current_page + 1 == file_dialog->page_count) {
        return;
    }

    file_dialog->current_page += 1;
    refresh_page_text(file_dialog);
    refresh_entry_button_names(file_dialog);
}

static void
on_button_cancel_clicked(te_button_widget* button) {
    te_file_dialog* file_dialog = widget_get_custom_ptr(button_widget_get_widget(button));

    void (*on_cancel)(void* custom) = file_dialog->on_cancel;
    void* custom = file_dialog->custom;

    on_cancel(custom);
}

static void
on_button_entry_clicked(te_button_widget* button) {
    te_file_dialog* file_dialog = widget_get_custom_ptr(button_widget_get_widget(button));
    const unsigned int button_index =
        (unsigned int)widget_get_custom_value(button_widget_get_widget(button));

    if (file_dialog->selected_button_index < file_dialog->entry_button_count) {
        // Clear selected button.
        vec4 color;
        theme_get_button_color(color);
        button_widget_set_color(
            file_dialog->entry_buttons[file_dialog->selected_button_index], color);

        file_dialog->selected_button_index = 0xFFFFFFFF;
    }

    if (button_index == 0) {
        // ".." (go up) button.
        const size_t old_path_len = strlen(file_dialog->current_path);
        size_t slash_pos = 0;
        for (size_t i = old_path_len - 1; i > 1; i--) {
            if (file_dialog->current_path[i] == '/' || file_dialog->current_path[i] == '\\') {
                slash_pos = i;
                break;
            }
        }
        if (slash_pos == 0) {
            return;
        }
        file_dialog->current_path[slash_pos] = 0;

#if defined(WIN32)
        if (slash_pos == 2) {
            // Drive root (for example: "D:"). Need a trailing slash.
            file_dialog->current_path[slash_pos] = '\\';
            file_dialog->current_path[slash_pos + 1] = 0;
        }
#endif

        for (unsigned int i = 0; i < file_dialog->dir_entry_count; i++) {
            free(file_dialog->dir_entries[i].name);
        }
        free(file_dialog->dir_entries);
        file_dialog->dir_entries = filesystem_list_directory(
            file_dialog->current_path, &file_dialog->dir_entry_count);

        file_dialog->current_page = 0;
        refresh_page_text(file_dialog);
        refresh_entry_button_names(file_dialog);
    } else {
        te_filesystem_entry* entry =
            &file_dialog->dir_entries
                 [file_dialog->current_page * (file_dialog->entry_button_count - 1)
                  + button_index - 1];
        if (entry->is_dir) {
            size_t len = strlen(file_dialog->current_path);
            const bool need_slash = file_dialog->current_path[len - 1] != '/'
                                    && file_dialog->current_path[len - 1] != '\\';

            char* new_path = malloc(sizeof(char) * (len + need_slash + entry->name_len + 1));
            memcpy(new_path, file_dialog->current_path, sizeof(char) * len);
#if defined(WIN32)
            new_path[len] = '\\';
#else
            new_path[len] = '/';
#endif
            memcpy(new_path + len + need_slash, entry->name, sizeof(char) * entry->name_len);
            new_path[len + need_slash + entry->name_len] = 0;

            free(file_dialog->current_path);
            file_dialog->current_path = new_path;

            for (unsigned int i = 0; i < file_dialog->dir_entry_count; i++) {
                free(file_dialog->dir_entries[i].name);
            }
            free(file_dialog->dir_entries);
            file_dialog->dir_entries = filesystem_list_directory(
                file_dialog->current_path, &file_dialog->dir_entry_count);

            file_dialog->current_page = 0;
            refresh_page_text(file_dialog);
            refresh_entry_button_names(file_dialog);
        } else if (file_dialog->mode == TE_FDM_SELECT_EXISTING_FILE) {
            file_dialog->selected_button_index = button_index;

            vec4 color;
            theme_get_accent_color(color);
            button_widget_set_color(file_dialog->entry_buttons[button_index], color);
        }
    }
}

static void
on_button_select_clicked(te_button_widget* button) {
    te_file_dialog* file_dialog = widget_get_custom_ptr(button_widget_get_widget(button));

    switch (file_dialog->mode) {
        case (TE_FDM_SELECT_EXISTING_FILE): {
            if (file_dialog->selected_button_index >= file_dialog->entry_button_count) {
                return;
            }

            te_filesystem_entry* entry =
                &file_dialog->dir_entries
                     [file_dialog->current_page * (file_dialog->entry_button_count - 1)
                      + file_dialog->selected_button_index - 1];
            if (entry->is_dir) {
                return;
            }

            // Build path to file.
            size_t len = strlen(file_dialog->current_path);
            const bool need_slash = file_dialog->current_path[len - 1] != '/'
                                    && file_dialog->current_path[len - 1] != '\\';

            char* selected_path =
                malloc(sizeof(char) * (len + need_slash + entry->name_len + 1));
            memcpy(selected_path, file_dialog->current_path, sizeof(char) * len);
#if defined(WIN32)
            selected_path[len] = '\\';
#else
            selected_path[len] = '/';
#endif
            memcpy(
                selected_path + len + need_slash, entry->name, sizeof(char) * entry->name_len);
            selected_path[len + need_slash + entry->name_len] = 0;

            void (*on_selected)(void* custom, const char*) = file_dialog->on_selected;
            void* custom = file_dialog->custom;

            on_selected(custom, selected_path);
            free(selected_path);
            break;
        }
        case (TE_FDM_SELECT_NEW_FILE): {
            if (file_dialog->filename_text_edit == NULL) {
                log_error("expected filename text edit to be valid");
                abort();
            }

            unsigned int filename_len;
            const wchar_t* wtext =
                text_edit_widget_get_text(file_dialog->filename_text_edit, &filename_len);

            char* filename = wchar_to_char(wtext, &filename_len);

            // Build path to file.
            size_t len = strlen(file_dialog->current_path);
            const bool need_slash = file_dialog->current_path[len - 1] != '/'
                                    && file_dialog->current_path[len - 1] != '\\';

            char* selected_path = malloc(sizeof(char) * (len + need_slash + filename_len + 1));
            memcpy(selected_path, file_dialog->current_path, sizeof(char) * len);
#if defined(WIN32)
            selected_path[len] = '\\';
#else
            selected_path[len] = '/';
#endif
            memcpy(selected_path + len + need_slash, filename, sizeof(char) * filename_len);
            selected_path[len + need_slash + filename_len] = 0;

            void (*on_selected)(void* custom, const char*) = file_dialog->on_selected;
            void* custom = file_dialog->custom;

            on_selected(custom, selected_path);
            free(selected_path);
            free(filename);
            break;
        }
        case (TE_FDM_SELECT_DIR): {
            const size_t len = strlen(file_dialog->current_path);
            char* path = malloc(sizeof(char) * (len + 1));
            memcpy(path, file_dialog->current_path, sizeof(char) * len);
            path[len] = 0;

            void (*on_selected)(void* custom, const char*) = file_dialog->on_selected;
            void* custom = file_dialog->custom;

            on_selected(custom, path);
            free(path);
            break;
        }
    }
}

te_file_dialog*
file_dialog_create(
    te_world* world, void* custom, void (*on_selected)(void* custom, const char* path),
    void (*on_cancel)(void* custom), enum te_file_dialog_mode mode) {
    te_file_dialog* file_dialog = malloc(sizeof(te_file_dialog));

    file_dialog->current_path = filesystem_convert_path_to_absolute("res");
    file_dialog->filename_text_edit = NULL;
    file_dialog->on_selected = on_selected;
    file_dialog->on_cancel = on_cancel;
    file_dialog->custom = custom;
    file_dialog->mode = mode;
    file_dialog->current_page = 0;
    file_dialog->page_count = 1;
    file_dialog->selected_button_index = 0xFFFFFFFF;

    file_dialog->dir_entries =
        filesystem_list_directory(file_dialog->current_path, &file_dialog->dir_entry_count);

    // Dark full screen background.
    te_rect_widget* top_widget = rect_widget_create();
    {
        te_widget* widget = rect_widget_get_widget(top_widget);
        file_dialog->spawned_widget = widget;
        widget_set_relative_position(widget, (vec2){0.0f, 0.0f});
        widget_set_relative_size(widget, (vec2){1.0f, 1.0f});
    }
    rect_widget_set_color(top_widget, (vec4){0.0f, 0.0f, 0.0f, 0.85f});

    {
        const float button_height = theme_get_button_height() / DIALOG_HEIGHT;
        const float hpadding = theme_get_horizontal_padding() / DIALOG_WIDTH;
        const float vpadding = theme_get_vertical_padding() / DIALOG_HEIGHT;
        const float hspacing = theme_get_horizontal_spacing() / DIALOG_WIDTH;
        const float vspacing = theme_get_vertical_spacing() / DIALOG_HEIGHT;

        // Widget background.
        te_rect_widget* back_rect = rect_widget_create();
        {
            te_widget* widget = rect_widget_get_widget(back_rect);
            widget_set_relative_position(
                widget, (vec2){(1.0f - DIALOG_WIDTH) / 2.0f, (1.0f - DIALOG_HEIGHT) / 2.0f});
            widget_set_relative_size(widget, (vec2){DIALOG_WIDTH, DIALOG_HEIGHT});
            widget_set_parent(widget, rect_widget_get_widget(top_widget));
        }
        vec4 color;
        theme_get_background_panel_color(color);
        rect_widget_set_color(back_rect, color);

        vec2 pos;
        pos[0] = hpadding;
        pos[1] = vpadding;

        // Current path.
        {
            te_rect_widget* rect = rect_widget_create();
            {
                te_widget* widget = rect_widget_get_widget(rect);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(
                    widget, (vec2){1.0f - hpadding * 2.0f, button_height});
                widget_set_parent(widget, rect_widget_get_widget(back_rect));
            }
            theme_get_text_edit_background_color(color);
            rect_widget_set_color(rect, color);

            te_text_widget* text = text_widget_create();
            file_dialog->current_path_text = text;
            {
                te_widget* widget = text_widget_get_widget(text);
                widget_set_relative_position(widget, (vec2){hpadding, 0.0f});
                widget_set_relative_size(widget, (vec2){1.0f, 1.0f});
                widget_set_parent(widget, rect_widget_get_widget(rect));
            }

            text_widget_set_text_height(text, theme_get_text_height());

            unsigned int text_len;
            wchar_t* wtext = wchar_from_char(file_dialog->current_path, &text_len);
            text_widget_set_text_own(text, wtext, text_len);
        }
        pos[0] = hpadding;
        pos[1] += button_height + vspacing;

        // Filesystem entries.
        {
            const float below_items_height =
                (mode == TE_FDM_SELECT_NEW_FILE
                     ? button_height + vspacing // <- text edit for new file name
                     : 0.0f)
                + vspacing + button_height; // <- nav menu height

            const float entry_spacing = vspacing / 2.0f;
            vec2 size;
            size[0] = 1.0f - hpadding * 2.0f;
            size[1] = button_height;

            // Count how much buttons we can fit.
            file_dialog->entry_button_count = 0;
            float test_y = pos[1];
            do {
                test_y += entry_spacing + button_height;
                file_dialog->entry_button_count += 1;
            } while (test_y + entry_spacing + button_height <= 1.0f - below_items_height);

            file_dialog->entry_buttons =
                malloc(sizeof(te_button_widget*) * file_dialog->entry_button_count);
            for (unsigned int i = 0; i < file_dialog->entry_button_count; i++) {
                pos[1] += entry_spacing;

                te_button_widget* button = button_widget_create();
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);
                    widget_set_parent(widget, rect_widget_get_widget(back_rect));
                    widget_set_custom_ptr(widget, file_dialog);
                    widget_set_custom_value(widget, i);
                }

                vec4 color;
                theme_get_button_color(color);
                button_widget_set_color(button, color);

                theme_get_button_color_hovered(color);
                button_widget_set_color_hovered(button, color);

                theme_get_button_color_pressed(color);
                button_widget_set_color_pressed(button, color);

                button_widget_set_on_clicked(button, on_button_entry_clicked);

                // Button text.
                te_text_widget* text = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text);
                    widget_set_parent(widget, button_widget_get_widget(button));
                    widget_set_relative_position(widget, (vec2){hpadding, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - hpadding, 1.0f});
                }
                text_widget_set_text_height(text, theme_get_text_height());

                file_dialog->entry_buttons[i] = button;
                pos[1] += button_height;
            }
        }
        pos[1] += vspacing;

        if (mode == TE_FDM_SELECT_NEW_FILE) {
            pos[0] = hpadding;
            const float text_width = 0.19f;

            {
                te_text_widget* text_widget = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text_widget);
                    widget_set_parent(widget, rect_widget_get_widget(back_rect));
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, (vec2){text_width, button_height});
                }
                text_widget_set_text_height(text_widget, theme_get_text_height());

                unsigned int text_len;
                wchar_t* text = wchar_from_char("New filename: ", &text_len);
                text_widget_set_text_own(text_widget, text, text_len);
            }
            pos[0] += text_width + hspacing;
            const float text_edit_width = 1.0f - hpadding - pos[0];

            // Background for text edit.
            te_rect_widget* rect_widget = rect_widget_create();
            {
                te_widget* widget = rect_widget_get_widget(rect_widget);
                widget_set_relative_position(widget, pos);
                widget_set_relative_size(widget, (vec2){text_edit_width, button_height});
                widget_set_parent(widget, rect_widget_get_widget(back_rect));
            }
            theme_get_text_edit_background_color(color);
            rect_widget_set_color(rect_widget, color);

            {
                te_text_edit_widget* text_edit = text_edit_widget_create();
                {
                    te_widget* widget = text_edit_widget_get_widget(text_edit);
                    widget_set_relative_position(widget, (vec2){hpadding, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f, 1.0f});
                    widget_set_parent(widget, rect_widget_get_widget(rect_widget));
                    widget_set_custom_ptr(widget, file_dialog);
                }

                text_edit_widget_set_text_height(text_edit, theme_get_text_height());
                file_dialog->filename_text_edit = text_edit;

                unsigned int text_len;
                wchar_t* text = wchar_from_char("level.txt", &text_len);
                text_edit_widget_set_text_own(text_edit, text, text_len);
            }

            pos[1] += button_height + vspacing;
        }

        // Navigation menu and "cancel", "select" buttons.
        {
            const float nav_item_width = (1.0f - hpadding * 2.0f - hspacing * 4.0f) / 5.0f;

            pos[0] = hpadding;

            vec2 size;
            size[0] = nav_item_width;
            size[1] = button_height;

            // Cancel button.
            {
                te_button_widget* button = button_widget_create();
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);
                    widget_set_parent(widget, rect_widget_get_widget(back_rect));
                    widget_set_custom_ptr(widget, file_dialog);
                }

                theme_get_button_color(color);
                button_widget_set_color(button, color);

                theme_get_button_color_hovered(color);
                button_widget_set_color_hovered(button, color);

                theme_get_button_color_pressed(color);
                button_widget_set_color_pressed(button, color);

                button_widget_set_on_clicked(button, on_button_cancel_clicked);

                // Button text.
                te_text_widget* text_widget = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text_widget);
                    widget_set_parent(widget, button_widget_get_widget(button));
                    widget_set_relative_position(widget, (vec2){hpadding, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - hpadding, 1.0f});
                }
                text_widget_set_text_height(text_widget, theme_get_text_height());

                unsigned int text_len;
                wchar_t* text = wchar_from_char("Cancel", &text_len);
                text_widget_set_text_own(text_widget, text, text_len);
            }
            pos[0] += nav_item_width + hspacing;

            // Left button.
            {
                te_button_widget* button = button_widget_create();
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);
                    widget_set_parent(widget, rect_widget_get_widget(back_rect));
                    widget_set_custom_ptr(widget, file_dialog);
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
                    widget_set_relative_position(widget, (vec2){hpadding, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - hpadding, 1.0f});
                }
                text_widget_set_text_height(text_widget, theme_get_text_height());

                unsigned int text_len;
                wchar_t* text = wchar_from_char("<", &text_len);
                text_widget_set_text_own(text_widget, text, text_len);
            }
            pos[0] += nav_item_width + hspacing;

            // Page text.
            {
                const float adjust = size[0] / 3.0f; // adjust text to be somewhat centered

                te_text_widget* text_widget = text_widget_create();
                file_dialog->page_text = text_widget;
                {
                    te_widget* widget = text_widget_get_widget(text_widget);
                    widget_set_parent(widget, rect_widget_get_widget(back_rect));
                    widget_set_relative_position(widget, (vec2){pos[0] + adjust, pos[1]});
                    widget_set_relative_size(widget, (vec2){size[0] - adjust, size[1]});
                }
                text_widget_set_text_height(text_widget, theme_get_text_height());

                unsigned int text_len;
                wchar_t* text = wchar_from_char("1 / 1", &text_len);
                text_widget_set_text_own(text_widget, text, text_len);
            }
            pos[0] += nav_item_width + hspacing;

            // Right button.
            {
                te_button_widget* button = button_widget_create();
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);
                    widget_set_custom_ptr(widget, file_dialog);
                    widget_set_parent(widget, rect_widget_get_widget(back_rect));
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
                    widget_set_relative_position(widget, (vec2){hpadding, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - hpadding, 1.0f});
                }
                text_widget_set_text_height(text_widget, theme_get_text_height());

                unsigned int text_len;
                wchar_t* text = wchar_from_char(">", &text_len);
                text_widget_set_text_own(text_widget, text, text_len);
            }
            pos[0] += nav_item_width + hspacing;

            // Select button.
            {
                te_button_widget* button = button_widget_create();
                {
                    te_widget* widget = button_widget_get_widget(button);
                    widget_set_relative_position(widget, pos);
                    widget_set_relative_size(widget, size);
                    widget_set_parent(widget, rect_widget_get_widget(back_rect));
                    widget_set_custom_ptr(widget, file_dialog);
                }

                theme_get_button_color(color);
                button_widget_set_color(button, color);

                theme_get_button_color_hovered(color);
                button_widget_set_color_hovered(button, color);

                theme_get_button_color_pressed(color);
                button_widget_set_color_pressed(button, color);

                button_widget_set_on_clicked(button, on_button_select_clicked);

                // Button text.
                te_text_widget* text_widget = text_widget_create();
                {
                    te_widget* widget = text_widget_get_widget(text_widget);
                    widget_set_parent(widget, button_widget_get_widget(button));
                    widget_set_relative_position(widget, (vec2){hpadding, 0.0f});
                    widget_set_relative_size(widget, (vec2){1.0f - hpadding, 1.0f});
                }
                text_widget_set_text_height(text_widget, theme_get_text_height());

                unsigned int text_len;
                wchar_t* text = wchar_from_char("Select", &text_len);
                text_widget_set_text_own(text_widget, text, text_len);
            }
        }
    }

    refresh_entry_button_names(file_dialog);
    refresh_page_text(file_dialog);
    world_spawn_widget(world, rect_widget_get_widget(top_widget));

    return file_dialog;
}
