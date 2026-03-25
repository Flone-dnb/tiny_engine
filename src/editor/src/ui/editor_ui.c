#include <ui/editor_ui.h>

#include <world.h>
#include <ui/theme.h>
#include <ui/property_inspector.h>
#include <ui/world_inspector.h>
#include <widget/rect_widget.h>
#include <widget/widget.h>

struct te_editor_ui {
    te_world_inspector* world_inspector;
    te_property_inspector* property_inspector;
    te_world* game_world;
};

te_editor_ui*
editor_ui_create(void) {
    te_editor_ui* ui = malloc(sizeof(te_editor_ui));

    ui->property_inspector = property_inspector_create(ui);
    ui->world_inspector = world_inspector_create(ui->property_inspector);

    return ui;
}

void
editor_ui_destroy(te_editor_ui* ui) {
    world_inspector_destroy(ui->world_inspector);
    property_inspector_destroy(ui->property_inspector);
    free(ui);
}

void
editor_ui_spawn(te_editor_ui* ui, te_world* editor_world) {
    vec4 background_color;
    theme_get_background_panel_color(background_color);

    // Left panel.
    te_rect_widget* left_rect = rect_widget_create();
    {
        te_widget* widget = rect_widget_get_widget(left_rect);
        widget_set_relative_position(widget, (vec2){0.0f, 0.0f});
        widget_set_relative_size(widget, (vec2){theme_get_left_panel_width(), 1.0f});
    }
    rect_widget_set_color(left_rect, background_color);

    // Right panel.
    te_rect_widget* right_rect = rect_widget_create();
    {
        te_widget* widget = rect_widget_get_widget(right_rect);
        widget_set_relative_position(
            widget, (vec2){1.0f - theme_get_right_panel_width(), 0.0f});
        widget_set_relative_size(widget, (vec2){theme_get_left_panel_width(), 1.0f});
    }
    rect_widget_set_color(right_rect, background_color);

    // Add editor widgets.
    world_inspector_add(ui->world_inspector, rect_widget_get_widget(left_rect));
    property_inspector_set_parent(ui->property_inspector, rect_widget_get_widget(right_rect));

    // Spawn.
    world_spawn_widget(editor_world, rect_widget_get_widget(left_rect));
    world_spawn_widget(editor_world, rect_widget_get_widget(right_rect));
}

te_world_inspector*
editor_ui_get_world_inspector(te_editor_ui* ui) {
    return ui->world_inspector;
}
