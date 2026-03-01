#include "editor.h"

#include <stdio.h>
#include "editor_camera.h"
#include "game/model.h"
#include "game_manager.h"
#include "io/log.h"
#include "misc/memory_usage.h"
#include "misc/wchar_funcs.h"
#include "render/font_manager.h"
#include "render/renderer.h"
#include "widget/text_widget.h"
#include "widget/widget.h"
#include "window.h"
#include "world.h"

struct te_editor {
    // Always valid pointer. Must be destroyed during the editor's destruction.
    te_editor_camera* editor_camera;

    // Non NULL if @ref game_world is valid. Displays FPS and RAM in the corner of the viewport.
    te_text_widget* game_world_stats_widget;

    // Not NULL if game world exists.
    te_world* game_world;

    // Time (in seconds) since @ref game_world_stats_widget was updated.
    float time_since_stats_update_sec;
};

te_editor*
editor_create() {
    te_editor* editor = malloc(sizeof(te_editor));
    editor->editor_camera = editor_camera_create();
    editor->game_world_stats_widget = NULL;
    editor->game_world = NULL;
    editor->time_since_stats_update_sec = 10.0f;

    return editor;
}

void
editor_destroy(te_editor* editor) {
    editor_camera_destroy(editor->editor_camera);

    free(editor);
}

void
editor_on_game_started(void* game_instance, te_game_manager* game_manager) {
    // Load font.
    te_renderer* renderer = game_manager_get_renderer(game_manager);
    te_font_manager* font_manager = renderer_get_font_manager(renderer);
    font_manager_load_font(font_manager, "engine/font/font.ttf");

    // Create default game world.
    te_editor* editor = game_instance;
    editor_create_game_world(editor, game_manager);
}

static void
prv_editor_destroy_game_world(te_editor* editor, te_game_manager* game_manager) {
    // Despawn editor camera because we manage its destruction manually.
    editor_camera_despawn(editor->editor_camera, editor->game_world);

    // Destroy world.
    game_manager_destroy_world(game_manager, editor->game_world);
    editor->game_world = NULL;
    editor->game_world_stats_widget = NULL;
}

void
editor_create_game_world(te_editor* editor, te_game_manager* game_manager) {
    if (editor->game_world != NULL) {
        prv_editor_destroy_game_world(editor, game_manager);
    }

    editor->game_world = game_manager_create_world(game_manager, "game");
    editor_camera_spawn(editor->editor_camera, editor->game_world);

    // Prepare a sample scene.
    {
        te_model* floor = model_create();
        model_set_scale(floor, (vec3){5.0f, 1.0f, 5.0f});
        model_set_color(floor, (vec4){1.0f, 0.5f, 0.0f, 1.0f});
        world_spawn_model(editor->game_world, floor);

        te_model* box = model_create();
        model_set_position(box, (vec3){0.0f, 1.0f, -1.0f});
        world_spawn_model(editor->game_world, box);
    }

    // Prepare stats widget.
    editor->game_world_stats_widget = text_widget_create();
    widget_set_relative_position(text_widget_get_widget(editor->game_world_stats_widget), (vec2){0.01f, 0.01f});
    widget_set_relative_size(text_widget_get_widget(editor->game_world_stats_widget), (vec2){0.4f, 0.2f});
    text_widget_set_is_multiline(editor->game_world_stats_widget, true);
    editor->time_since_stats_update_sec = 10.0f;

    unsigned int text_len;
    wchar_t* stats_text = wchar_from_char("", &text_len);
    text_widget_set_text_own(editor->game_world_stats_widget, stats_text, text_len);

    // Spawn stats widget.
    world_spawn_widget(editor->game_world, text_widget_get_widget(editor->game_world_stats_widget));
}

void
editor_on_game_tick(void* game_instance, te_game_manager* game_manager, float delta_time_sec) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_game_tick(editor->editor_camera, delta_time_sec);

    // Update stats.
    editor->time_since_stats_update_sec += delta_time_sec;
    if (editor->game_world_stats_widget != NULL && editor->time_since_stats_update_sec >= 2.0f) {
        editor->time_since_stats_update_sec = 0.0f;

        te_renderer* renderer = game_manager_get_renderer(game_manager);
        const unsigned int fps = renderer_get_fps(renderer);
        const unsigned int fps_limit = renderer_get_fps_limit(renderer);

        const unsigned int process_mem = (unsigned int)(memory_usage_get_process_used_memory() / 1024 / 1024);
        const unsigned int total_used_mem = (unsigned int)(memory_usage_get_total_used_memory() / 1024 / 1024);
        const unsigned int total_mem = (unsigned int)(memory_usage_get_total_memory() / 1024 / 1024);

        const char* fmt = "FPS: %u (limit: %u)\nRAM used (MB): %u (%u/%u)";
#if defined(ENGINE_ASAN_ENABLED)
        fmt = "FPS: %u (limit: %u)\nRAM used (MB): %u (%u/%u) (ASan enabled)";
#endif

        int len = snprintf(NULL, 0, fmt, fps, fps_limit, process_mem, total_used_mem, total_mem);
        if (len < 0) {
            log_error("snprintf error");
            abort();
        }
        char* src_text = malloc(sizeof(char) * (size_t)(len + 1));
        snprintf(src_text, (size_t)len + 1, fmt, fps, fps_limit, process_mem, total_used_mem, total_mem);

        unsigned int text_len;
        wchar_t* stats_text = wchar_from_char(src_text, &text_len);
        text_widget_set_text_own(editor->game_world_stats_widget, stats_text, text_len);

        free(src_text);
    }
}

void
editor_on_keyboard_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button, te_keyboard_modifiers modifiers) {
    (void)game_manager;
    (void)modifiers;

    te_editor* editor = game_instance;
    editor_camera_on_keyboard_button_pressed(editor->editor_camera, button);
}

void
editor_on_keyboard_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button, te_keyboard_modifiers modifiers) {
    (void)modifiers;

    te_editor* editor = game_instance;
    editor_camera_on_keyboard_button_released(editor->editor_camera, button);

    if (button == TE_KB_ESCAPE) {
        window_close(game_manager_get_window(game_manager));
    }
}

void
editor_on_keyboard_input_text(void* game_instance, struct te_game_manager* game_manager, const char* text) {
    (void)game_instance;
    (void)game_manager;
    (void)text;
}

void
editor_on_gamepad_button_pressed(void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
editor_on_gamepad_button_released(void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button) {
    (void)game_instance;

    if (button == TE_GB_BUTTON_RIGHT) {
        window_close(game_manager_get_window(game_manager));
    }
}

void
editor_on_gamepad_axis_moved(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_axis axis, float new_pos) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_gamepad_axis_moved(editor->editor_camera, axis, new_pos);
}

void
editor_on_mouse_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button, bool was_handled_by_widget) {
    (void)was_handled_by_widget;
    te_editor* editor = game_instance;

    if (button == TE_MB_RIGHT) {
        if (editor->game_world == NULL) {
            // No point in doing something.
            return;
        }

        te_window* window = game_manager_get_window(game_manager);
        window_capture_mouse_cursor(window, true);

        editor_camera_enable_input(editor->editor_camera, true);
    }
}

void
editor_on_mouse_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button, bool was_handled_by_widget) {
    (void)was_handled_by_widget;
    te_editor* editor = game_instance;

    if (button == TE_MB_RIGHT) {
        if (editor->game_world == NULL) {
            // No point in doing something.
            return;
        }

        te_window* window = game_manager_get_window(game_manager);
        window_capture_mouse_cursor(window, false);

        editor_camera_enable_input(editor->editor_camera, false);
    }
}

void
editor_on_mouse_moved(void* game_instance, struct te_game_manager* game_manager, float x_offset, float y_offset) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_mouse_moved(editor->editor_camera, x_offset, y_offset);
}

void
editor_on_mouse_scroll_moved(void* game_instance, struct te_game_manager* game_manager, float offset) {
    (void)game_instance;
    (void)game_manager;
    (void)offset;
}

void
editor_on_gamepad_connected(void* game_instance, struct te_game_manager* game_manager, const char* gamepad_name) {
    (void)game_manager;
    (void)gamepad_name;

    te_editor* editor = game_instance;
    editor_camera_on_gamepad_connected(editor->editor_camera);
}

void
editor_on_gamepad_disconnected(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_gamepad_disconnected(editor->editor_camera);
}

void
editor_on_input_source_changed(void* game_instance, struct te_game_manager* game_manager, bool is_gamepad_current) {
    (void)game_instance;
    (void)game_manager;
    (void)is_gamepad_current;
}

void
editor_on_window_received_focus(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;
    (void)game_manager;
}

void
editor_on_window_lost_focus(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_manager;

    te_editor* editor = game_instance;
    if (editor->game_world == NULL) {
        // No point in doing something.
        return;
    }

    te_window* window = game_manager_get_window(game_manager);
    window_capture_mouse_cursor(window, false);

    editor_camera_enable_input(editor->editor_camera, false);
}

void
editor_on_window_close(void* game_instance, struct te_game_manager* game_manager) {
    te_editor* editor = game_instance;

    if (editor->game_world != NULL) {
        prv_editor_destroy_game_world(editor, game_manager);
    }
}
