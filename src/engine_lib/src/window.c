#include <window.h>

#include <stdbool.h>
#include <stdlib.h>
#include <debug_console.h>
#include <game_manager.h>
#include <io/filesystem.h>
#include <io/log.h>
#include <window_system/os_window.h>
#include <io/paths.h>
#include <cglm/util.h>
#include <misc/high_freq_timer.h>
#if defined(ENGINE_DEBUG_TOOLS)
#include <render/renderer.h>
#endif

#if defined(__aarch64__) || defined(__ARM64__)
#define IS_ARM64
#endif

struct te_window {
    te_os_window* os_window;

    // Game manager that window created.
    struct te_game_manager* game_manager;

    // User-specified callbacks. Do not free/destroy this pointer. The user will free it.
    te_window_callbacks* user_callbacks;

    // User's main game system. Do not free this pointer.
    void* game_instance;

    // Refresh rate of the used display.
    unsigned int display_refresh_rate;

#if defined(ENGINE_DEBUG_TOOLS)
    // You can "show_stats" debug command by pressing both "menu" and "start" on gamepad.
    float debug_stats_time_since_menu;
    float debug_stats_time_since_start;
#endif

    // true` if gamepad input was received on this frame. Used to determine when the input device changes.
    bool had_gamepad_input_curr_frame;

    // `true` if gamepad input was received on the last frame. Used to determine when the input device changes.
    bool had_gamepad_input_prev_frame;

    // `true` if mouse captured.
    bool is_mouse_captured;
};

static void on_mouse_button_pressed(te_os_window* os_window, enum te_mouse_button button);
static void on_mouse_button_released(te_os_window* os_window, enum te_mouse_button button);
static void on_mouse_move(te_os_window* os_window, float x_diff, float y_diff);
static void on_mouse_scroll_moved(te_os_window* os_window, float offset);
static void on_keyboard_button_pressed(
    te_os_window* os_window, enum te_keyboard_button button, bool is_repeat,
    te_keyboard_modifiers modifiers);
static void on_keyboard_button_released(
    te_os_window* os_window, enum te_keyboard_button button, te_keyboard_modifiers modifiers);
static void on_text_input(te_os_window* os_window, const char* text);
static void on_gamepad_connected(te_os_window* os_window);
static void on_gamepad_disconnected(te_os_window* os_window);
static void on_gamepad_button_pressed(te_os_window* os_window, enum te_gamepad_button button);
static void on_gamepad_button_released(te_os_window* os_window, enum te_gamepad_button button);
static void
on_gamepad_axis_moved(te_os_window* os_window, enum te_gamepad_axis axis, float new_pos);
static void on_received_focus(te_os_window* os_window);
static void on_lost_focus(te_os_window* os_window);
static void on_resized(te_os_window* os_window, unsigned int width, unsigned int height);

te_window*
window_create(const char* window_title) {
    if (window_title == NULL) {
        log_error("window title text must not be NULL");
        abort();
    }

    // Destroy old log file.
    filesystem_remove_file(paths_get_log_file());
    filesystem_ensure_dirs_exist(paths_get_log_file());

    if (sizeof(te_os_window_callbacks) != sizeof(void*) * 15) {
        log_error("add new callbacks here");
        abort();
    }
    te_os_window_callbacks callbacks;
    callbacks.on_mouse_button_pressed = on_mouse_button_pressed;
    callbacks.on_mouse_button_released = on_mouse_button_released;
    callbacks.on_mouse_move = on_mouse_move;
    callbacks.on_mouse_scroll_moved = on_mouse_scroll_moved;
    callbacks.on_keyboard_button_pressed = on_keyboard_button_pressed;
    callbacks.on_keyboard_button_released = on_keyboard_button_released;
    callbacks.on_text_input = on_text_input;
    callbacks.on_gamepad_connected = on_gamepad_connected;
    callbacks.on_gamepad_disconnected = on_gamepad_disconnected;
    callbacks.on_gamepad_button_pressed = on_gamepad_button_pressed;
    callbacks.on_gamepad_button_released = on_gamepad_button_released;
    callbacks.on_gamepad_axis_moved = on_gamepad_axis_moved;
    callbacks.on_received_focus = on_received_focus;
    callbacks.on_lost_focus = on_lost_focus;
    callbacks.on_resized = on_resized;

    te_window* window = malloc(sizeof(te_window));
    window->os_window = os_window_create(window_title, &callbacks, window);
    window->game_manager = NULL;
    window->user_callbacks = NULL;
    window->game_instance = NULL;
    window->display_refresh_rate = os_window_get_refresh_rate(window->os_window);
#if defined(ENGINE_DEBUG_TOOLS)
    window->debug_stats_time_since_menu = 10.0f;
    window->debug_stats_time_since_start = 10.0f;
#endif
    window->had_gamepad_input_curr_frame = false;
    window->had_gamepad_input_prev_frame = false;
    window->is_mouse_captured = false;

    unsigned int width, height;
    os_window_get_size(window->os_window, &width, &height);
    log_info_fmt("created a window of size %dx%d", width, height);

    return window;
}

void
window_destroy(te_window* window) {
    os_window_destroy(window->os_window);

    free(window);

    // Log warnings/errors count (if were logged).
    const unsigned int warn_count = log_get_warning_count_logged();
    const unsigned int err_count = log_get_error_count_logged();
    if (warn_count > 0 || err_count > 0) {
        log_info("");
        log_info_fmt("WARNINGS logged: %d | ERRORS logged: %d", warn_count, err_count);
    }
}

void
window_process_events(
    te_window* window, te_window_callbacks* window_callbacks, void* game_instance) {
    window->user_callbacks = window_callbacks;
    window->game_instance = game_instance;
    window->game_manager = prv_game_manager_create(window);

    // notify the user of game start
    window->user_callbacks->on_game_started(window->game_instance, window->game_manager);
    // and if gamepad is already connected
    if (os_window_is_gamepad_connected(window->os_window)) {
        window->had_gamepad_input_curr_frame = true;
        window->had_gamepad_input_prev_frame = true;
        window->user_callbacks->on_gamepad_connected(
            window->game_instance, window->game_manager);
    }

    // Used to calculate delta time.
    uint64_t current_time_counter = high_freq_timer_now();
    uint64_t prev_time_counter = 0;
    float delta_time_sec = 0.0f;

    while (!os_window_should_close(window->os_window)) {
        // Process available window events.
        os_window_poll_event(window->os_window);

#if defined(ENGINE_DEBUG_TOOLS)
        if (os_window_is_gamepad_connected(window->os_window)) {
            window->debug_stats_time_since_menu += delta_time_sec;
            window->debug_stats_time_since_start += delta_time_sec;
        }
#endif

        // Calculate delta time.
        prev_time_counter = current_time_counter;
        current_time_counter = high_freq_timer_now();
        const float delta_time_ms =
            high_freq_timer_get_elapsed_ms_range(prev_time_counter, current_time_counter);
        delta_time_sec = delta_time_ms * 0.001f;
        // avoid huge dt as it can cause exceptional situations
        delta_time_sec = glm_min(delta_time_sec, 1.0f);

        // Tick.
        {
            prv_game_manager_tick(window->game_manager, delta_time_sec);
            window->user_callbacks->on_game_tick(
                window->game_instance, window->game_manager, delta_time_sec);

            if (window->had_gamepad_input_prev_frame != window->had_gamepad_input_curr_frame) {
                window->had_gamepad_input_prev_frame = window->had_gamepad_input_curr_frame;

                prv_game_manager_on_input_source_changed(window->game_manager);
                window->user_callbacks->on_input_source_changed(
                    window->game_instance, window->game_manager,
                    window->had_gamepad_input_curr_frame);
            }
        }

        // Draw.
        prv_game_manager_draw_frame(window->game_manager, delta_time_sec);
    }

    log_info("window is closing");

    window->user_callbacks->on_window_close(window->game_instance, window->game_manager);

    // Destroy game manager.
    prv_game_manager_destroy(window->game_manager);
    window->game_manager = NULL;
    window->user_callbacks = NULL;
    window->game_instance = NULL;

    log_info("game manager is destroyed");
}

struct te_game_manager*
window_get_game_manager(te_window* window) {
#if defined(DEBUG)
    if (window->game_manager == NULL) {
        log_error("game manager is not created yet (game not started) or was already "
                  "destroyed (game ended)");
        abort();
    }
#endif

    return window->game_manager;
}

void
window_capture_mouse_cursor(te_window* window, bool enable) {
    if (window->is_mouse_captured == enable) {
        return;
    }

    os_window_capture_mouse_cursor(window->os_window, enable);
    window->is_mouse_captured = enable;

    prv_game_manager_on_mouse_cursor_captured(window->game_manager, enable);
}

bool
window_is_mouse_captured(te_window* window) {
    return window->is_mouse_captured;
}

bool
window_is_gamepad_connected(te_window* window) {
    return os_window_is_gamepad_connected(window->os_window);
}

void
window_get_size(te_window* window, unsigned int* width, unsigned int* height) {
    os_window_get_size(window->os_window, width, height);
}

void
window_get_cursor_position(te_window* window, float* x, float* y) {
    os_window_get_cursor_position(window->os_window, x, y);
}

void
window_set_cursor_position(te_window* window, float x, float y) {
    os_window_set_cursor_position(window->os_window, x, y);
}

unsigned int
window_get_display_refresh_rate(te_window* window) {
    return window->display_refresh_rate;
}

void
window_close(te_window* window) {
    os_window_close(window->os_window);
}

static void
on_mouse_button_pressed(te_os_window* os_window, enum te_mouse_button button) {
    te_window* window = os_window_get_user_data(os_window);

    const bool is_handled =
        prv_game_manager_on_mouse_button_pressed(window->game_manager, button);
    window->user_callbacks->on_mouse_button_pressed(
        window->game_instance, window->game_manager, button, is_handled);
}

static void
on_mouse_button_released(te_os_window* os_window, enum te_mouse_button button) {
    te_window* window = os_window_get_user_data(os_window);

    const bool is_handled =
        prv_game_manager_on_mouse_button_released(window->game_manager, button);
    window->user_callbacks->on_mouse_button_released(
        window->game_instance, window->game_manager, button, is_handled);
}

static void
on_mouse_move(te_os_window* os_window, float x_diff, float y_diff) {
    te_window* window = os_window_get_user_data(os_window);

    prv_game_manager_on_mouse_moved(window->game_manager);
    window->user_callbacks->on_mouse_moved(
        window->game_instance, window->game_manager, x_diff, y_diff);
}

static void
on_mouse_scroll_moved(te_os_window* os_window, float offset) {
    te_window* window = os_window_get_user_data(os_window);

    window->user_callbacks->on_mouse_scroll_moved(
        window->game_instance, window->game_manager, offset);
}

static void
on_keyboard_button_pressed(
    te_os_window* os_window, enum te_keyboard_button button, bool is_repeat,
    te_keyboard_modifiers modifiers) {
    te_window* window = os_window_get_user_data(os_window);

#if defined(IS_ARM64)
    if (os_window_is_gamepad_connected(window->os_window)) {
        // In some cases while using retro-handhelds (which have built in gamepad) gamepad buttons trigger
        // keyboard input before the actual gamepad button which messes up the whole game input.
        return;
    }
#endif

#if defined(ENGINE_DEBUG_TOOLS)
    if (prv_debug_console_is_shown()) {
        if (button == TE_KB_TILDE) {
            return; // key up event is used to show/hide console
        }
        prv_debug_console_on_keyboard_input(window->game_manager, button);
        return; // don't trigger user callbacks
    }
#endif

    window->had_gamepad_input_curr_frame = false;

    prv_game_manager_on_keyboard_input(window->game_manager, button, is_repeat);

    if (!is_repeat) {
        window->user_callbacks->on_keyboard_button_pressed(
            window->game_instance, window->game_manager, button, modifiers);
    }
}

static void
on_keyboard_button_released(
    te_os_window* os_window, enum te_keyboard_button button, te_keyboard_modifiers modifiers) {
    te_window* window = os_window_get_user_data(os_window);

#if defined(IS_ARM64)
    if (os_window_is_gamepad_connected(window->os_window)) {
        // Same as in the "pressed" event.
        return;
    }
#endif

#if defined(ENGINE_DEBUG_TOOLS)
    if (prv_debug_console_is_shown()) {
        if (button == TE_KB_TILDE) {
            prv_debug_console_hide();
        } else {
            // input is handled in key down event
            return; // don't trigger user callbacks
        }
    } else if (!prv_debug_console_is_shown() && button == TE_KB_TILDE) {
        prv_debug_console_show();
    }
#endif
    window->had_gamepad_input_curr_frame = false;

    window->user_callbacks->on_keyboard_button_released(
        window->game_instance, window->game_manager, button, modifiers);
}

static void
on_text_input(te_os_window* os_window, const char* text) {
    te_window* window = os_window_get_user_data(os_window);

#if defined(IS_ARM64)
    if (os_window_is_gamepad_connected(window->os_window)) {
        // Same as in the "pressed" event.
        return;
    }
#endif

#if defined(ENGINE_DEBUG_TOOLS)
    if (prv_debug_console_is_shown()) {
        if (text[0] == '`') {
            return; // "on button released" will handle it
        }
        prv_debug_console_on_keyboard_input_text(text);
        return; // don't trigger user callbacks
    }
#endif

    prv_game_manager_on_keyboard_input_text(window->game_manager, text);
    window->user_callbacks->on_keyboard_input_text(
        window->game_instance, window->game_manager, text);
}

static void
on_gamepad_connected(te_os_window* os_window) {
    te_window* window = os_window_get_user_data(os_window);

    window->user_callbacks->on_gamepad_connected(window->game_instance, window->game_manager);
}

static void
on_gamepad_disconnected(te_os_window* os_window) {
    te_window* window = os_window_get_user_data(os_window);

    window->user_callbacks->on_gamepad_disconnected(
        window->game_instance, window->game_manager);

#if defined(ENGINE_DEBUG_TOOLS)
    window->debug_stats_time_since_menu = 10.0f;
    window->debug_stats_time_since_start = 10.0f;
#endif
}

static void
on_gamepad_button_pressed(te_os_window* os_window, enum te_gamepad_button button) {
    te_window* window = os_window_get_user_data(os_window);

    window->had_gamepad_input_curr_frame = true;
    window->user_callbacks->on_gamepad_button_pressed(
        window->game_instance, window->game_manager, button);
}

static void
on_gamepad_button_released(te_os_window* os_window, enum te_gamepad_button button) {
    te_window* window = os_window_get_user_data(os_window);

#if defined(ENGINE_DEBUG_TOOLS)
    if (button == TE_GB_BACK) {
        window->debug_stats_time_since_menu = 0.0f;
    } else if (button == TE_GB_START) {
        window->debug_stats_time_since_start = 0.0f;
    }
    if (window->debug_stats_time_since_menu < 0.25f
        && window->debug_stats_time_since_start < 0.25f) {
        if (!debug_console_is_stats_shown()) {
            renderer_set_fps_limit(game_manager_get_renderer(window->game_manager), 0);
            debug_console_show_stats();
        } else {
            renderer_set_fps_limit(
                game_manager_get_renderer(window->game_manager),
                window_get_display_refresh_rate(window));
            debug_console_hide_stats();
        }
        window->debug_stats_time_since_menu = 10.0f;
        window->debug_stats_time_since_start = 10.0f;
    }
#endif

    window->had_gamepad_input_curr_frame = true;
    window->user_callbacks->on_gamepad_button_released(
        window->game_instance, window->game_manager, button);
}

static void
on_gamepad_axis_moved(te_os_window* os_window, enum te_gamepad_axis axis, float new_pos) {
    te_window* window = os_window_get_user_data(os_window);

    window->had_gamepad_input_curr_frame = true;

    window->user_callbacks->on_gamepad_axis_moved(
        window->game_instance, window->game_manager, axis, new_pos);
}

static void
on_received_focus(te_os_window* os_window) {
    te_window* window = os_window_get_user_data(os_window);

    window->user_callbacks->on_window_received_focus(
        window->game_instance, window->game_manager);
}

static void
on_lost_focus(te_os_window* os_window) {
    te_window* window = os_window_get_user_data(os_window);

    window->user_callbacks->on_window_lost_focus(window->game_instance, window->game_manager);
}

static void
on_resized(te_os_window* os_window, unsigned int width, unsigned int height) {
    (void)width;
    (void)height;

    te_window* window = os_window_get_user_data(os_window);

    prv_game_manager_on_window_size_changed(window->game_manager);
}

void*
prv_window_get_game_instance(te_window* window) {
    return window->game_instance;
}

void
prv_window_swap_buffers(te_window* window) {
    os_window_swap_buffers(window->os_window);
}
