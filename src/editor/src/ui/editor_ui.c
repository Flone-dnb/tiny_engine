#include <ui/editor_ui.h>

#include <world.h>
#include <ui/theme.h>
#include <ui/property_inspector.h>
#include <ui/world_inspector.h>
#include <ui/filesystem_view.h>
#include <widget/rect_widget.h>
#include <widget/widget.h>

struct te_editor_ui {
    struct te_editor* editor;

    te_world_inspector* world_inspector;
    te_property_inspector* property_inspector;
    te_filesystem_view* filesystem_view;
    te_world* game_world;

    // NULL if not spawned yet. Background (parent) rectangles of left and right editor panels.
    te_rect_widget* left_rect;
    te_rect_widget* right_rect;
};

te_editor_ui*
editor_ui_create(struct te_editor* editor) {
    te_editor_ui* ui = malloc(sizeof(te_editor_ui));

    ui->editor = editor;
    ui->property_inspector = property_inspector_create(ui);
    ui->world_inspector = world_inspector_create(editor, ui->property_inspector);
    ui->filesystem_view = filesystem_view_create(editor);
    ui->left_rect = NULL;
    ui->right_rect = NULL;

    return ui;
}

void
editor_ui_destroy(te_editor_ui* ui) {
    world_inspector_destroy(ui->world_inspector);
    property_inspector_destroy(ui->property_inspector);
    filesystem_view_destroy(ui->filesystem_view);

    free(ui);
}

void
editor_ui_spawn(te_editor_ui* ui, te_world* editor_world) {
    vec4 background_color;
    theme_get_background_panel_color(background_color);

    // Left panel.
    te_rect_widget* left_rect = rect_widget_create();
    ui->left_rect = left_rect;
    {
        te_widget* widget = rect_widget_get_widget(left_rect);
        widget_set_relative_position(widget, (vec2){0.0f, 0.0f});
        widget_set_relative_size(widget, (vec2){theme_get_left_panel_width(), 1.0f});
    }
    rect_widget_set_color(left_rect, background_color);

    // Right panel.
    te_rect_widget* right_rect = rect_widget_create();
    ui->right_rect = right_rect;
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
    filesystem_view_add(ui->filesystem_view, rect_widget_get_widget(left_rect));

    // Spawn.
    world_spawn_widget(editor_world, rect_widget_get_widget(left_rect));
    world_spawn_widget(editor_world, rect_widget_get_widget(right_rect));
}

void
editor_ui_set_visibility(te_editor_ui* ui, bool is_visible) {
    if (ui->left_rect == NULL) {
        return;
    }

    te_widget* left_widget = rect_widget_get_widget(ui->left_rect);
    te_widget* right_widget = rect_widget_get_widget(ui->right_rect);
    if (is_visible) {
        widget_set_relative_position(left_widget, (vec2){0.0f, 0.0f});
        widget_set_relative_position(
            right_widget, (vec2){1.0f - theme_get_right_panel_width(), 0.0f});
    } else {
        widget_set_relative_position(left_widget, (vec2){0.0f, 1.0f});
        widget_set_relative_position(right_widget, (vec2){1.0f, 1.0f});

        world_inspector_select_obj(ui->world_inspector, NULL);
        property_inspector_hide(ui->property_inspector);
    }
}

te_world_inspector*
editor_ui_get_world_inspector(te_editor_ui* ui) {
    return ui->world_inspector;
}

struct te_editor* editor_ui_get_editor(te_editor_ui* ui) {
    return ui->editor;
}

void
editor_ui_refresh_filesystem_view(te_editor_ui* ui) {
    filesystem_view_refresh(ui->filesystem_view);
}

void editor_ui_reset(te_editor_ui* ui) {
    property_inspector_hide(ui->property_inspector);
}