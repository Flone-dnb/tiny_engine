#include <editor.h>

#include <stdio.h>
#include <editor_camera.h>
#include <game/model.h>
#include <game/camera.h>
#include <game/game_object_info.h>
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
#include <ui/scene_animation_editor.h>
#include <game/scene_animation.h>
#include <obj_picking.h>
#include <gizmo.h>

#define EDITOR_STATS_POS_OFFSET 0.01f
#define EDITOR_SHORTCUTS_X_POS 0.01f
#define EDITOR_SHORTCUTS_Y_POS 0.075f

struct te_editor {
    // Not NULL if @ref game_world was loaded from a file (relative to the `res` directory).
    char* game_world_relative_path;

    // NULL if the game is not started yet.
    te_game_manager* game_manager;

    // Always valid pointer. Must be destroyed during the editor's destruction.
    te_editor_camera* editor_camera;

    // Not NULL if @ref game_world is valid. Displays FPS and RAM in the corner of the viewport.
    te_text_widget* game_world_stats_widget;

    // Not NULL if @ref game_world is  valid. Displays keyboard shortcuts.
    te_text_widget* shortcuts_widget;

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
    editor->shortcuts_widget = NULL;
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

te_game_manager*
editor_get_game_manager(te_editor* editor) {
    return editor->game_manager;
}

te_world*
editor_get_game_world(te_editor* editor) {
    return editor->game_world;
}

static void
destroy_game_world(te_editor* editor, te_game_manager* game_manager) {
    editor_ui_reset(editor->ui);

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
    editor->shortcuts_widget = NULL;

    free(editor->game_world_relative_path);
    editor->game_world_relative_path = NULL;

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
    world_spawn_game_object(editor->editor_world, camera_get_game_object_info(camera));
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
    if (editor->game_world != NULL) {
        destroy_game_world(editor, editor->game_manager);
    }

    editor->game_world = game_manager_create_world(editor->game_manager, "game");
    if (relative_path_to_world == NULL) {
        // Prepare a sample scene.
        te_model* floor = model_create();
        model_set_name(floor, "floor");
        model_set_scale(floor, (vec3){4.0f, 1.0f, 4.0f});
        model_set_color(floor, (vec4){1.0f, 0.5f, 0.0f, 1.0f});
        world_spawn_game_object(editor->game_world, model_get_game_object_info(floor));

        te_model* box = model_create();
        model_set_name(box, "box");
        model_set_position(box, (vec3){0.0f, 1.0f, -1.0f});
        world_spawn_game_object(editor->game_world, model_get_game_object_info(box));
    } else {
        world_add_from_file(editor->game_world, relative_path_to_world, true);

        free(editor->game_world_relative_path);
        const size_t len = strlen(relative_path_to_world);

        editor->game_world_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(editor->game_world_relative_path, relative_path_to_world, sizeof(char) * len);
        editor->game_world_relative_path[len] = 0;
    }

    // Setup light.
    te_light_params* light_params =
        renderer_get_light_params(game_manager_get_renderer(editor->game_manager));
    glm_vec3_copy((vec3){1.0f, -1.0f, 1.0f}, light_params->directional_light_direction);
    glm_vec3_normalize(light_params->directional_light_direction);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, light_params->directional_light_color);

    editor_camera_spawn(editor->editor_camera, editor->game_world);

    // Prepare and spawn stats widget.
    {
        editor->game_world_stats_widget = text_widget_create();
        widget_set_relative_position(
            text_widget_get_widget(editor->game_world_stats_widget),
            (vec2){EDITOR_STATS_POS_OFFSET, EDITOR_STATS_POS_OFFSET});
        widget_set_relative_size(
            text_widget_get_widget(editor->game_world_stats_widget), (vec2){0.5f, 0.2f});
        widget_set_is_serialization_allowed(
            text_widget_get_widget(editor->game_world_stats_widget), false);
        text_widget_set_is_multiline(editor->game_world_stats_widget, true);
        editor->time_since_stats_update_sec = 10.0f;

        text_widget_set_text_height(editor->game_world_stats_widget, 0.025f);

        unsigned int text_len;
        wchar_t* stats_text = wchar_from_char("", &text_len);
        text_widget_set_text_own(editor->game_world_stats_widget, stats_text, text_len);

        world_spawn_widget(
            editor->game_world, text_widget_get_widget(editor->game_world_stats_widget));
    }

    // Prepare and spawn shortcuts widget.
    {
        editor->shortcuts_widget = text_widget_create();
        widget_set_relative_position(
            text_widget_get_widget(editor->shortcuts_widget),
            (vec2){EDITOR_SHORTCUTS_X_POS, EDITOR_SHORTCUTS_Y_POS});
        widget_set_relative_size(
            text_widget_get_widget(editor->shortcuts_widget), (vec2){0.5f, 0.2f});
        widget_set_is_serialization_allowed(
            text_widget_get_widget(editor->shortcuts_widget), false);
        text_widget_set_is_multiline(editor->shortcuts_widget, true);

        text_widget_set_text_height(editor->shortcuts_widget, 0.018f);

        unsigned int text_len;
        wchar_t* shortcuts_text = wchar_from_char(
            "Ctrl+N - new world\nCtrl+S - save world\nCtrl+Shift+S - save world as\nTab - "
            "toggle fullscreen",
            &text_len);
        text_widget_set_text_own(editor->shortcuts_widget, shortcuts_text, text_len);

        world_spawn_widget(
            editor->game_world, text_widget_get_widget(editor->shortcuts_widget));
    }

    // Refresh world inspector.
    te_world_inspector* inspector = editor_ui_get_world_inspector(editor->ui);
    world_inspector_rebuild_list(inspector, editor->game_world);
}

// ------------------------------------------------------------------------------------------------
static void* file_dialog_custom = NULL;
static void (*file_dialog_on_selected)(void* custom, const char* path) = NULL;
static void (*file_dialog_on_cancel)(void* custom) = NULL;
void
prv_file_dialog_on_selected(void* custom, const char* path) {
    te_editor* editor = custom;

    file_dialog_destroy(editor->file_dialog);
    editor->file_dialog = NULL;

    game_manager_destroy_world(editor->game_manager, editor->dialog_world);
    editor->dialog_world = NULL;

    file_dialog_on_selected(file_dialog_custom, path);
}
void
prv_file_dialog_on_cancel(void* custom) {
    te_editor* editor = custom;

    file_dialog_destroy(editor->file_dialog);
    editor->file_dialog = NULL;

    game_manager_destroy_world(editor->game_manager, editor->dialog_world);
    editor->dialog_world = NULL;

    if (file_dialog_on_cancel != NULL) {
        file_dialog_on_cancel(file_dialog_custom);
    }
}
void
editor_show_file_dialog(
    te_editor* editor, void* custom, void (*on_selected)(void* custom, const char* path),
    void (*on_cancel)(void* custom), enum te_file_dialog_mode mode) {
    // Create a new world for dialog widget to be displayed on top of both the editor and the game worlds.
    editor->dialog_world = game_manager_create_world(editor->game_manager, "dialog");
    te_camera* camera = camera_create();
    world_spawn_game_object(editor->dialog_world, camera_get_game_object_info(camera));
    world_set_active_camera(editor->dialog_world, camera);

    file_dialog_custom = custom;
    file_dialog_on_selected = on_selected;
    file_dialog_on_cancel = on_cancel;

    editor->file_dialog = file_dialog_create(
        editor->dialog_world, editor, prv_file_dialog_on_selected, prv_file_dialog_on_cancel,
        mode);
}
// ------------------------------------------------------------------------------------------------

static void
set_camera_editor_shape_visibility(te_world* world, bool is_visible) {
    bool found_camera = false;
    do {
        found_camera = false;

        unsigned int count;
        te_game_object_info** infos = world_get_root_game_objects(world, &count);

        for (unsigned int i = 0; i < count; i++) {
            if (infos[i]->type == TE_GOT_CAMERA) {
                te_camera* camera = (te_camera*)infos[i]->game_object;
                if (world_get_active_camera(world) == camera) {
                    continue;
                }
                if (is_visible != prv_camera_is_editor_shape_visible(camera)) {
                    prv_camera_set_editor_shape_visibility(camera, is_visible);
                    found_camera = true;
                    break; // editor shape was despawned and world root objects array changed
                }
            } else if (infos[i]->type == TE_GOT_MODEL) {
                te_model* model = (te_model*)infos[i]->game_object;
                te_camera* attached_camera = model_get_attached_camera(model);
                if (attached_camera != NULL) {
                    if (world_get_active_camera(world) == attached_camera) {
                        continue;
                    }
                    if (is_visible != prv_camera_is_editor_shape_visible(attached_camera)) {
                        prv_camera_set_editor_shape_visibility(attached_camera, is_visible);
                        found_camera = true;
                        break; // same reason
                    }
                }
            }
        }
        free(infos);
    } while (found_camera);
}

static void
show_ui(te_editor* editor) {
    // Show stats.
    te_widget* widget = text_widget_get_widget(editor->game_world_stats_widget);
    widget_set_relative_position(
        widget, (vec2){EDITOR_STATS_POS_OFFSET, EDITOR_STATS_POS_OFFSET});

    // Show shortcuts.
    widget = text_widget_get_widget(editor->shortcuts_widget);
    widget_set_relative_position(
        widget, (vec2){EDITOR_SHORTCUTS_X_POS, EDITOR_SHORTCUTS_Y_POS});

    editor_ui_set_visibility(editor->ui, true);
    set_camera_editor_shape_visibility(editor->game_world, true);

    editor_camera_set_is_fullscreen(editor->editor_camera, false);

    te_scene_animation_editor* anim_editor =
        world_inspector_get_scene_animation_editor(editor_ui_get_world_inspector(editor->ui));
    if (anim_editor != NULL) {
        scene_animation_editor_show(anim_editor);
    }
}

static void
hide_ui(te_editor* editor) {
    editor_camera_set_is_fullscreen(editor->editor_camera, true);

    // Hide stats.
    te_widget* widget = text_widget_get_widget(editor->game_world_stats_widget);
    widget_set_relative_position(widget, (vec2){1.0f, 1.0f});

    // Hide shortcuts.
    widget = text_widget_get_widget(editor->shortcuts_widget);
    widget_set_relative_position(widget, (vec2){1.0f, 1.0f});

    editor_ui_set_visibility(editor->ui, false);
    set_camera_editor_shape_visibility(editor->game_world, false);

    te_scene_animation_editor* anim_editor =
        world_inspector_get_scene_animation_editor(editor_ui_get_world_inspector(editor->ui));
    if (anim_editor != NULL) {
        scene_animation_editor_hide(anim_editor);
    }
}

void
editor_set_gizmo(te_editor* editor, te_model* target) {
    if (editor->game_world == NULL) {
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
editor_pilot_camera(te_editor* editor, te_camera* camera) {
    editor_camera_pilot_custom_camera(editor->editor_camera, camera);
    hide_ui(editor);
}

void
editor_on_before_game_obj_deleted(te_editor* editor, te_game_object_info* info) {
    te_scene_animation* anim = world_get_scene_animation(editor->game_world);
    const char* go_name = info->get_name(info->game_object);
    if (go_name != NULL && anim != NULL && scene_animation_is_playing(anim)) {
        // Is animated?
        unsigned int obj_count;
        char** obj_names = scene_animation_get_object_names(anim, &obj_count);
        for (unsigned int i = 0; i < obj_count; i++) {
            if (strcmp(obj_names[i], go_name) != 0) {
                continue;
            }
            scene_animation_stop(anim);
            break;
        }
        free(obj_names);
    }

    if (editor->gizmo != NULL) {
        // Is selected with gizmo?
        if (gizmo_get_target(editor->gizmo) == info->game_object) {
            gizmo_destroy_in_world_now(editor->gizmo, editor->game_world);
            editor->gizmo = NULL;
        }
    }
}

void
editor_on_game_tick(void* game_instance, te_game_manager* game_manager, float delta_time_sec) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_game_tick(editor->editor_camera, delta_time_sec);

    te_scene_animation_editor* anim_editor =
        world_inspector_get_scene_animation_editor(editor_ui_get_world_inspector(editor->ui));
    if (anim_editor != NULL) {
        prv_scene_animation_editor_tick(anim_editor);
    }

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

    if (editor->game_world == NULL) {
        return;
    }

    char* relative_path = filesystem_convert_path_to_relative(path_to_file);
    if (relative_path == NULL) {
        log_warn("new world must be in the \"res\" directory");
        return;
    }

    world_save_to_file(editor->game_world, relative_path, true);

    if (editor->game_world_relative_path != NULL) {
        free(editor->game_world_relative_path);
    }
    editor->game_world_relative_path = relative_path;

    editor_ui_refresh_filesystem_view(editor->ui);
}

void
editor_on_keyboard_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers) {
    (void)game_manager;

    te_editor* editor = game_instance;
    if (editor->game_world == NULL) {
        return;
    }

    if (keyboard_modifiers_is_ctrl_pressed(&modifiers) && button == TE_KB_N) {
        // Create new world.
        editor_create_game_world(editor, NULL);
        return;
    } else if (
        keyboard_modifiers_is_ctrl_pressed(&modifiers)
        && keyboard_modifiers_is_shift_pressed(&modifiers) && button == TE_KB_S) {
        // Save world as.
        editor_show_file_dialog(
            editor, editor, on_new_world_file_selected, NULL, TE_FDM_SELECT_NEW_FILE);
        return;
    } else if (keyboard_modifiers_is_ctrl_pressed(&modifiers) && button == TE_KB_S) {
        // Save world.
        if (editor->game_world_relative_path == NULL) {
            editor_show_file_dialog(
                editor, editor, on_new_world_file_selected, NULL, TE_FDM_SELECT_NEW_FILE);
        } else {
            world_save_to_file(editor->game_world, editor->game_world_relative_path, true);
        }
        return;
    } else if (button == TE_KB_TAB) {
        // Toggle fullscreen.
        if (editor_camera_is_fullscreen(editor->editor_camera)) {
            show_ui(editor);
        } else {
            hide_ui(editor);
        }
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
        if (editor_camera_is_piloting_custom_camera(editor->editor_camera)) {
            editor_camera_pilot_custom_camera(editor->editor_camera, NULL);
            show_ui(editor);
        } else {
            window_close(game_manager_get_window(game_manager));
        }
    }

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

    if ((button == TE_KB_W || button == TE_KB_E || button == TE_KB_R)
        && !window_is_mouse_captured(game_manager_get_window(game_manager))
        && editor->gizmo != NULL) {
        if (button == TE_KB_W) {
            gizmo_set_mode(editor->gizmo, TE_GM_MOVE);
        } else if (button == TE_KB_E) {
            gizmo_set_mode(editor->gizmo, TE_GM_ROTATE);
        } else if (button == TE_KB_R) {
            gizmo_set_mode(editor->gizmo, TE_GM_SCALE);
        }
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

    if (editor->dialog_world != NULL) {
        return;
    }

    if (editor->game_world == NULL) {
        return;
    }
    te_camera* game_camera = world_get_active_camera(editor->game_world);
    if (game_camera == NULL) {
        return;
    }

    vec4 viewport;
    camera_get_viewport(game_camera, viewport);

    vec2 window_cursor_pos;
    te_window* window = game_manager_get_window(game_manager);
    window_get_cursor_position(window, &window_cursor_pos[0], &window_cursor_pos[1]);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);
    glm_vec2_div(
        window_cursor_pos, (vec2){(float)window_width, (float)window_height},
        window_cursor_pos);

    if (window_cursor_pos[0] < viewport[0] || window_cursor_pos[1] < viewport[1]
        || window_cursor_pos[0] > viewport[0] + viewport[2]
        || window_cursor_pos[1] > viewport[1] + viewport[3]) {
        // Outside of the game viewport.
        return;
    }

    // Check if we clicked on scene animation editor.
    te_scene_animation_editor* anim_editor =
        world_inspector_get_scene_animation_editor(editor_ui_get_world_inspector(editor->ui));
    if (anim_editor != NULL) {
        // Remap to viewport for scene animation.
        vec2 cursor_pos;
        glm_vec2_sub(window_cursor_pos, viewport, cursor_pos);
        glm_vec2_div(cursor_pos, &viewport[2], cursor_pos);

        te_widget* widget = scene_animation_editor_get_root_widget(anim_editor);

        vec2 pos;
        widget_get_screen_position(widget, pos);
        vec2 size;
        widget_get_screen_size(widget, size);

        if (cursor_pos[0] > pos[0] && cursor_pos[0] < pos[0] + size[0]
            && cursor_pos[1] > pos[1] && cursor_pos[1] < pos[1] + size[1]) {
            glm_vec2_sub(cursor_pos, pos, cursor_pos);
            glm_vec2_div(cursor_pos, size, cursor_pos);
            prv_scene_animation_editor_on_mouse_click(anim_editor, button, cursor_pos);
            return;
        }
    }

    if (button == TE_MB_RIGHT) {
        window_capture_mouse_cursor(window, true);
        editor_camera_enable_input(editor->editor_camera, true);
        return;
    }
    if (button == TE_MB_LEFT && !editor_camera_is_fullscreen(editor->editor_camera)) {
        te_game_object_info* obj_info = obj_picking_find_obj_under_cursor(
            window_cursor_pos, game_camera, editor->game_world, editor->gizmo);

        if (editor->gizmo != NULL && obj_info != NULL) {
            if (obj_info->game_object == gizmo_get_model_x(editor->gizmo)) {
                gizmo_start_grab_x(editor->gizmo);
                return;
            } else if (obj_info->game_object == gizmo_get_model_y(editor->gizmo)) {
                gizmo_start_grab_y(editor->gizmo);
                return;
            } else if (obj_info->game_object == gizmo_get_model_z(editor->gizmo)) {
                gizmo_start_grab_z(editor->gizmo);
                return;
            }
        }

        world_inspector_select_obj(editor_ui_get_world_inspector(editor->ui), obj_info);
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
    } else if (button == TE_MB_LEFT) {
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
        gizmo_move(
            editor->gizmo, editor_camera_get_camera(editor->editor_camera), x_offset,
            y_offset);
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
