#include "ui/world_inspector.h"

#include <world.h>
#include <ui/theme.h>
#include <widget/widget.h>
#include <widget/text_widget.h>
#include <widget/button_widget.h>
#include <misc/wchar_funcs.h>
#include <stdbool.h>
#include <io/log.h>

struct te_world_inspector {
    // Do not free/destroy, parent widget.
    te_widget* left_panel;

    te_button_widget** world_item_buttons;

    // `true` if should display world's "3D objects", `false` if "2D objects".
    bool is_3dobj_mode_selected;
};

te_world_inspector*
world_inspector_create(void) {
    te_world_inspector* inspector = malloc(sizeof(te_world_inspector));

    inspector->left_panel = NULL;
    inspector->world_item_buttons = NULL;
    inspector->is_3dobj_mode_selected = true;

    return inspector;
}

void
world_inspector_destroy(te_world_inspector* inspector) {
    free(inspector->world_item_buttons);
    free(inspector);
}

void
world_inspector_refresh(te_world_inspector* inspector) {
    te_world* world = widget_get_world(inspector->left_panel);
    if (world == NULL) {
        log_error("expected a valid world");
        abort();
    }

    //    TOOD;
}

static void
on_button_3dobj_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    inspector->is_3dobj_mode_selected = true;
    world_inspector_refresh(inspector);
}

static void
on_button_2dobj_clicked(te_button_widget* button) {
    te_world_inspector* inspector = widget_get_custom_ptr(button_widget_get_widget(button));
    inspector->is_3dobj_mode_selected = false;
    world_inspector_refresh(inspector);
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
    y_pos += vspacing;

    // World object type buttons: 3D objects or 2D objects.
    {
        const float button_width = (total_width - hspacing) / 2.0f;

        // "3D objects" button ------------------

        te_button_widget* button_3dobj = button_widget_create();
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

        const float list_button_width = total_width;
        inspector->world_item_buttons = malloc(sizeof(te_button_widget*) * button_count);
        for (unsigned int i = 0; i < button_count; i++) {
            y_pos += list_item_spacing;

            te_button_widget* button = button_widget_create();
            {
                te_widget* widget = button_widget_get_widget(button);
                widget_set_parent(widget, left_panel);
                widget_set_relative_position(widget, (vec2){hpadding, y_pos});
                widget_set_relative_size(
                    widget, (vec2){list_button_width, theme_get_button_height()});
            }

            vec4 color;
            theme_get_button_color(color);
            button_widget_set_color(button, color);

            theme_get_button_color_hovered(color);
            button_widget_set_color_hovered(button, color);

            theme_get_button_color_pressed(color);
            button_widget_set_color_pressed(button, color);

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

            inspector->world_item_buttons[i] = button;

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
