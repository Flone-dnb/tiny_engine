#include <editor.h>

#include <stdio.h>
#include <editor_camera.h>
#include <game/model.h>
#include <game/camera.h>
#include <game_manager.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <misc/memory_usage.h>
#include <misc/wchar_funcs.h>
#include <render/font_manager.h>
#include <render/renderer.h>
#include <widget/text_widget.h>
#include <widget/widget.h>
#include <window.h>
#include <world.h>
#include <ui/file_dialog.h>
#include <ui/editor_ui.h>
#include <ui/world_inspector.h>
#include <obj_picking.h>
#include <gizmo.h>

struct te_editor {
    // Not NULL if @ref game_world was loaded from a file (relative to the `res` directory).
    char* game_world_relative_path;

    // NULL if the game is not started yet.
    te_game_manager* game_manager;

    // Always valid pointer. Must be destroyed during the editor's destruction.
    te_editor_camera* editor_camera;

    // Non NULL if @ref game_world is valid. Displays FPS and RAM in the corner of the viewport.
    te_text_widget* game_world_stats_widget;

    // Not NULL if game world exists.
    te_world* game_world;

    // Not NULL if exists.
    te_world* editor_world;

    // Not NULL if world for dialog widgets exists.
    te_world* dialog_world;

    // Not NULL if showing a file dialog.
    te_file_dialog* file_dialog;

    // Always valid.
    te_editor_ui* ui;

    // Not NULL if shown.
    te_gizmo* gizmo;

    // Time (in seconds) since @ref game_world_stats_widget was updated.
    float time_since_stats_update_sec;
};

te_editor*
editor_create() {
    te_editor* editor = malloc(sizeof(te_editor));
    editor->editor_camera = editor_camera_create();
    editor->game_manager = NULL;
    editor->ui = editor_ui_create(editor);
    editor->game_world_stats_widget = NULL;
    editor->game_world = NULL;
    editor->dialog_world = NULL;
    editor->file_dialog = NULL;
    editor->game_world_relative_path = NULL;
    editor->editor_world = NULL;
    editor->gizmo = NULL;
    editor->time_since_stats_update_sec = 10.0f;

    return editor;
}

void
editor_destroy(te_editor* editor) {
    editor_camera_destroy(editor->editor_camera);
    editor_ui_destroy(editor->ui);

    free(editor->game_world_relative_path);

    free(editor);
}

static void
destroy_game_world(te_editor* editor, te_game_manager* game_manager) {
    if (editor->file_dialog != NULL) {
        file_dialog_destroy(editor->file_dialog);
        editor->file_dialog = NULL;

        game_manager_destroy_world(editor->game_manager, editor->dialog_world);
        editor->dialog_world = NULL;
    }

    // Despawn editor camera because we manage its destruction manually.
    editor_camera_despawn(editor->editor_camera, editor->game_world);

    // Destroy world.
    game_manager_destroy_world(game_manager, editor->game_world);
    editor->game_world = NULL;
    editor->game_world_stats_widget = NULL;

    editor->gizmo = NULL;
}

void
editor_on_window_close(void* game_instance, struct te_game_manager* game_manager) {
    te_editor* editor = game_instance;

    if (editor->game_world != NULL) {
        destroy_game_world(editor, game_manager);
    }

    if (editor->file_dialog != NULL) {
        file_dialog_destroy(editor->file_dialog);
        editor->file_dialog = NULL;

        game_manager_destroy_world(editor->game_manager, editor->dialog_world);
        editor->dialog_world = NULL;
    }
}

static void
editor_create_editor_world(te_editor* editor, struct te_game_manager* game_manager) {
    editor->editor_world = game_manager_create_world(game_manager, "editor world");

    // Create a dummy camera to display editor's UI.
    te_camera* camera = camera_create();
    world_spawn_camera(editor->editor_world, camera);
    world_set_active_camera(editor->editor_world, camera);

    editor_ui_spawn(editor->ui, editor->editor_world);
}

void
editor_on_game_started(void* game_instance, te_game_manager* game_manager) {
    // Load font.
    te_renderer* renderer = game_manager_get_renderer(game_manager);
    te_font_manager* font_manager = renderer_get_font_manager(renderer);
    font_manager_load_font(font_manager, "engine/font/font.ttf");

    te_editor* editor = game_instance;
    editor->game_manager = game_manager;

    // Create worlds.
    editor_create_editor_world(editor, game_manager);
    editor_create_game_world(editor, NULL);
}

void
editor_create_game_world(te_editor* editor, const char* relative_path_to_world) {
    // Cleanup.
    editor_ui_reset(editor->ui);
    if (editor->game_world != NULL) {
        destroy_game_world(editor, editor->game_manager);
    }

    editor->game_world = game_manager_create_world(editor->game_manager, "game");
    if (relative_path_to_world == NULL) {
        // Prepare a sample scene.
        te_model* floor = model_create();
        model_set_name(floor, "floor");
        model_set_scale(floor, (vec3){5.0f, 1.0f, 5.0f});
        model_set_color(floor, (vec4){1.0f, 0.5f, 0.0f, 1.0f});
        world_spawn_model(editor->game_world, floor);

        te_model* box = model_create();
        model_set_name(box, "box");
        model_set_position(box, (vec3){0.0f, 1.0f, -1.0f});
        world_spawn_model(editor->game_world, box);
    } else {
        world_add_from_file(editor->game_world, relative_path_to_world);
    }

    editor_camera_spawn(editor->editor_camera, editor->game_world);

    // Prepare stats widget.
    editor->game_world_stats_widget = text_widget_create();
    widget_set_relative_position(
        text_widget_get_widget(editor->game_world_stats_widget), (vec2){0.01f, 0.01f});
    widget_set_relative_size(
        text_widget_get_widget(editor->game_world_stats_widget), (vec2){0.4f, 0.2f});
    widget_set_is_serialization_allowed(
        text_widget_get_widget(editor->game_world_stats_widget), false);
    text_widget_set_is_multiline(editor->game_world_stats_widget, true);
    editor->time_since_stats_update_sec = 10.0f;

    unsigned int text_len;
    wchar_t* stats_text = wchar_from_char("", &text_len);
    text_widget_set_text_own(editor->game_world_stats_widget, stats_text, text_len);

    // Spawn stats widget.
    world_spawn_widget(
        editor->game_world, text_widget_get_widget(editor->game_world_stats_widget));

    // Refresh world inspector.
    te_world_inspector* inspector = editor_ui_get_world_inspector(editor->ui);
    world_inspector_rebuild_list(inspector, editor->game_world);
}

void
editor_set_gizmo(te_editor* editor, te_model* target) {
    if (editor->game_world == NULL){
        log_error("expected the game world to be valid");
        abort();
    }

    if (editor->gizmo != NULL) {
        gizmo_destroy_in_world_now(editor->gizmo, editor->game_world);
        editor->gizmo = NULL;
    }

    if (target != NULL) {
        editor->gizmo = gizmo_create_in_world(editor->game_world, target);
    }
}

void
editor_on_game_tick(void* game_instance, te_game_manager* game_manager, float delta_time_sec) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_game_tick(editor->editor_camera, delta_time_sec);

    // Update stats.
    editor->time_since_stats_update_sec += delta_time_sec;
    if (editor->game_world_stats_widget != NULL
        && editor->time_since_stats_update_sec >= 2.0f) {
        editor->time_since_stats_update_sec = 0.0f;

        te_renderer* renderer = game_manager_get_renderer(game_manager);
        const unsigned int fps = renderer_get_fps(renderer);
        const unsigned int fps_limit = renderer_get_fps_limit(renderer);

        const unsigned int process_mem =
            (unsigned int)(memory_usage_get_process_used_memory() / 1024 / 1024);
        const unsigned int total_used_mem =
            (unsigned int)(memory_usage_get_total_used_memory() / 1024 / 1024);
        const unsigned int total_mem =
            (unsigned int)(memory_usage_get_total_memory() / 1024 / 1024);

        const char* fmt = "FPS: %u (limit: %u)\nRAM used (MB): %u (%u/%u)";
#if defined(ENGINE_ASAN_ENABLED)
        fmt = "FPS: %u (limit: %u)\nRAM used (MB): %u (%u/%u) (ASan enabled)";
#endif

        int len =
            snprintf(NULL, 0, fmt, fps, fps_limit, process_mem, total_used_mem, total_mem);
        if (len < 0) {
            log_error("snprintf error");
            abort();
        }
        char* src_text = malloc(sizeof(char) * (size_t)(len + 1));
        snprintf(
            src_text, (size_t)len + 1, fmt, fps, fps_limit, process_mem, total_used_mem,
            total_mem);

        unsigned int text_len;
        wchar_t* stats_text = wchar_from_char(src_text, &text_len);
        text_widget_set_text_own(editor->game_world_stats_widget, stats_text, text_len);

        free(src_text);
    }
}

static void
on_new_world_file_selected(void* custom, const char* path_to_file) {
    te_editor* editor = custom;

    file_dialog_destroy(editor->file_dialog);
    editor->file_dialog = NULL;

    game_manager_destroy_world(editor->game_manager, editor->dialog_world);
    editor->dialog_world = NULL;

    if (editor->game_world == NULL) {
        return;
    }

    char* relative_path = filesystem_convert_path_to_relative(path_to_file);
    if (relative_path == NULL) {
        log_warn("new world must be in the \"res\" directory");
        return;
    }

    world_save_to_file(editor->game_world, relative_path);

    free(relative_path);

    editor_ui_refresh_filesystem_view(editor->ui);
}

static void
on_new_world_file_cancel(void* custom) {
    te_editor* editor = custom;

    file_dialog_destroy(editor->file_dialog);
    editor->file_dialog = NULL;

    game_manager_destroy_world(editor->game_manager, editor->dialog_world);
    editor->dialog_world = NULL;
}

void
editor_on_keyboard_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers) {
    te_editor* editor = game_instance;

    if (editor->game_world != NULL && keyboard_modifiers_is_ctrl_pressed(&modifiers)
        && button == TE_KB_S) {
        if (editor->game_world_relative_path == NULL) {
            // Create a new world for dialog widget to be displayed on top of both the editor and the game worlds.
            editor->dialog_world = game_manager_create_world(game_manager, "dialog");
            te_camera* camera = camera_create();
            world_spawn_camera(editor->dialog_world, camera);
            world_set_active_camera(editor->dialog_world, camera);

            editor->file_dialog = file_dialog_create(
                editor->dialog_world, editor, on_new_world_file_selected,
                on_new_world_file_cancel, TE_FDM_SELECT_NEW_FILE);
        } else {
            world_save_to_file(editor->game_world, editor->game_world_relative_path);
        }
        return;
    }

    editor_camera_on_keyboard_button_pressed(editor->editor_camera, button);
}

void
editor_on_keyboard_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers) {
    (void)modifiers;

    te_editor* editor = game_instance;
    editor_camera_on_keyboard_button_released(editor->editor_camera, button);

    if (button == TE_KB_ESCAPE) {
        window_close(game_manager_get_window(game_manager));
    }
}

void
editor_on_keyboard_input_text(
    void* game_instance, struct te_game_manager* game_manager, const char* text) {
    (void)game_instance;
    (void)game_manager;
    (void)text;
}

void
editor_on_gamepad_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
editor_on_gamepad_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button) {
    (void)game_instance;

    if (button == TE_GB_BUTTON_RIGHT) {
        window_close(game_manager_get_window(game_manager));
    }
}

void
editor_on_gamepad_axis_moved(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_axis axis,
    float new_pos) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_gamepad_axis_moved(editor->editor_camera, axis, new_pos);
}

void
editor_on_mouse_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget) {
    (void)was_handled_by_widget;
    te_editor* editor = game_instance;

    if (editor->game_world == NULL) {
        return;
    }
    te_camera* game_camera = world_get_active_camera(editor->game_world);
    if (game_camera == NULL) {
        return;
    }

    vec4 viewport;
    camera_get_viewport(game_camera, viewport);

    vec2 cursor_pos;
    te_window* window = game_manager_get_window(game_manager);
    window_get_cursor_position(window, &cursor_pos[0], &cursor_pos[1]);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);
    glm_vec2_div(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

    if (cursor_pos[0] < viewport[0] || cursor_pos[1] < viewport[1]
        || cursor_pos[0] > viewport[0] + viewport[2]
        || cursor_pos[1] > viewport[1] + viewport[3]) {
        // Outside of the game viewport.
        return;
    }

    if (button == TE_MB_RIGHT) {
        window_capture_mouse_cursor(window, true);
        editor_camera_enable_input(editor->editor_camera, true);
    }else if (button == TE_MB_LEFT) {
        void* obj = obj_picking_find_obj_under_cursor(cursor_pos, game_camera, editor->game_world, editor->gizmo);

        if (editor->gizmo != NULL) {
            if (obj == gizmo_get_model_x(editor->gizmo)) {
                gizmo_start_grab_x(editor->gizmo);
                return;
            } else if (obj == gizmo_get_model_y(editor->gizmo)) {
                gizmo_start_grab_y(editor->gizmo);
                return;
            } else if (obj == gizmo_get_model_z(editor->gizmo)) {
                gizmo_start_grab_z(editor->gizmo);
                return;
            }
        }
        world_inspector_select_obj(editor_ui_get_world_inspector(editor->ui), obj);
    }
}

void
editor_on_mouse_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget) {
    (void)was_handled_by_widget;

    te_editor* editor = game_instance;
    te_window* window = game_manager_get_window(game_manager);

    if (button == TE_MB_RIGHT) {
        if (window_is_mouse_captured(window)) {
            if (editor->game_world == NULL) {
                return;
            }

            window_capture_mouse_cursor(window, false);
            editor_camera_enable_input(editor->editor_camera, false);
        }
    }else if (button == TE_MB_LEFT) {
        if (editor->gizmo == NULL) {
            return;
        }
        gizmo_end_grab(editor->gizmo);
    }
}

void
editor_on_mouse_moved(
    void* game_instance, struct te_game_manager* game_manager, float x_offset,
    float y_offset) {
    (void)game_manager;

    te_editor* editor = game_instance;
    if (editor->game_world == NULL) {
        return;
    }

    editor_camera_on_mouse_moved(editor->editor_camera, x_offset, y_offset);

    if (editor->gizmo != NULL && gizmo_is_grabbed(editor->gizmo)) {
        gizmo_move(editor->gizmo, editor_camera_get_camera(editor->editor_camera), x_offset, y_offset);
    }
}

void
editor_on_mouse_scroll_moved(
    void* game_instance, struct te_game_manager* game_manager, float offset) {
    (void)game_instance;
    (void)game_manager;
    (void)offset;
}

void
editor_on_gamepad_connected(
    void* game_instance, struct te_game_manager* game_manager, const char* gamepad_name) {
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
editor_on_input_source_changed(
    void* game_instance, struct te_game_manager* game_manager, bool is_gamepad_current) {
    (void)game_instance;
    (void)game_manager;
    (void)is_gamepad_current;
}

void
editor_on_window_received_focus(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_ui_refresh_filesystem_view(editor->ui);
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
