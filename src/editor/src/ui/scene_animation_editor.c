#include <ui/scene_animation_editor.h>

#include <world.h>
#include <widget/widget.h>
#include <widget/rect_widget.h>
#include <ui/theme.h>

#define TE_SCENE_ANIM_EDITOR_X_POS 0.05f

struct te_scene_animation_editor {
    te_world* world;
    te_widget* root_widget;
};

te_scene_animation_editor*
scene_animation_editor_create(te_world* world) {
    te_scene_animation_editor* editor = malloc(sizeof(te_scene_animation_editor));
    editor->world = world;

    te_rect_widget* background = rect_widget_create();
    editor->root_widget = rect_widget_get_widget(background);
    {
        {
            vec2 pos;
            glm_vec2_copy((vec2){TE_SCENE_ANIM_EDITOR_X_POS, 0.75f}, pos);

            te_widget* widget = editor->root_widget;
            widget_set_relative_position(widget, pos);
            widget_set_relative_size(widget, (vec2){1.0f - pos[0] * 2.0f, 0.98f - pos[1]});
            widget_set_is_serialization_allowed(widget, false);
        }

        vec4 color;
        theme_get_background_panel_color(color);
        rect_widget_set_color(background, color);
    }

    world_spawn_widget(world, editor->root_widget);

    return editor;
}

void
scene_animation_editor_destroy(te_scene_animation_editor* editor) {
    world_despawn_widget(editor->world, editor->root_widget);
    widget_destroy(editor->root_widget);

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
