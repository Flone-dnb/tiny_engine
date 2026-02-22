#include "window.h"

#include <SDL3/SDL_error.h>
#include <stdbool.h>
#include <stdlib.h>
#include "debug_console.h"
#include "game_manager.h"
#include "io/filesystem.h"
#include "io/log.h"
#include "io/paths.h"
#include "misc/error.h"
#if defined(ENGINE_DEBUG_TOOLS)
#include "render/renderer.h"
#endif
#define SDL_MAIN_HANDLED
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_video.h"

#if defined(__aarch64__) || defined(__ARM64__)
#define IS_ARM64
#endif

struct te_window {
    struct SDL_Window* sdl_window;

    // Game manager that window created.
    struct te_game_manager* game_manager;

    // User-specified callbacks. Do not free/destroy this pointer. The user will free it.
    te_window_callbacks* user_callbacks;

    // User's main game system. Do not free this pointer.
    void* game_instance;

    // Non NULL if have a connected gamepad. Do not free this pointer.
    SDL_Gamepad* connected_gamepad;

    // Current size of the window (in pixels).
    unsigned int width;
    unsigned int height;

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

    // `true` if the window needs to be closed.
    bool quit_requested;

    // `true` if mouse captured.
    bool is_mouse_captured;
};

te_window*
window_create(const char* window_title) {
    if (window_title == NULL) {
        show_error_and_abort("window title text must not be NULL");
    }

    // Destroy old log file.
    filesystem_remove_file(paths_get_log_file());
    filesystem_ensure_dirs_exist(paths_get_log_file());

    // Initialize SDL.
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            show_error_and_abort(SDL_GetError());
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); // IF CHANGING
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0); // ALSO CHANGE GLAD
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

#if defined(DEBUG)
        if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG)) {
            show_error_and_abort(SDL_GetError());
        }
#endif

        // Set depth buffer size.
        if (!SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) {
            show_error_and_abort(SDL_GetError());
        }
    }

    // Get available displays.
    int display_count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&display_count);
    if (display_count <= 0) {
        show_error_and_abort("unable to find at least 1 display");
    }

    // Get display resolution.
    const unsigned int used_display_id = displays[0];
    const SDL_DisplayMode* display_info = SDL_GetDesktopDisplayMode(used_display_id);
    const int display_width = display_info->w;
    const int display_height = display_info->h;
    const unsigned int display_refresh_rate = (unsigned int)display_info->refresh_rate;
    SDL_free(displays);

    // Create SDL window.
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, window_title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, display_width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, display_height);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, true);
    SDL_Window* sdl_window = SDL_CreateWindowWithProperties(props);
    if (sdl_window == NULL) {
        show_error_and_abort(SDL_GetError());
    }
    SDL_DestroyProperties(props);

    te_window* window = malloc(sizeof(te_window));
    window->sdl_window = sdl_window;
    window->connected_gamepad = NULL;
    window->game_manager = NULL;
    window->user_callbacks = NULL;
    window->game_instance = NULL;
    window->width = (unsigned int)display_width;
    window->height = (unsigned int)display_height;
    window->display_refresh_rate = display_refresh_rate;
#if defined(ENGINE_DEBUG_TOOLS)
    window->debug_stats_time_since_menu = 10.0f;
    window->debug_stats_time_since_start = 10.0f;
#endif
    window->had_gamepad_input_curr_frame = false;
    window->had_gamepad_input_prev_frame = false;
    window->quit_requested = false;
    window->is_mouse_captured = false;

    log_info_fmt("created a window of size %dx%d", window->width, window->height);

    return window;
}

void
window_destroy(te_window* window) {
    SDL_DestroyWindow(window->sdl_window);

    free(window);

    SDL_Quit();

    // Log warnings/errors count (if were logged).
    const unsigned int warn_count = log_get_warning_count_logged();
    const unsigned int err_count = log_get_error_count_logged();
    if (warn_count > 0 || err_count > 0) {
        log_info("");
        log_info_fmt("WARNINGS logged: %d | ERRORS logged: %d", warn_count, err_count);
    }
}

void
window_process_events(te_window* window, te_window_callbacks* window_callbacks, void* game_instance) {
    window->user_callbacks = window_callbacks;
    window->game_instance = game_instance;
    window->game_manager = prv_game_manager_create(window);

    // See if we have a gamepad connected.
    {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);

        for (int i = 0; i < count; i++) {
            if (SDL_IsGamepad(ids[i])) {
                window->connected_gamepad = SDL_OpenGamepad(ids[i]);
                break;
            }
        }

        SDL_free(ids);
    }

    // Notify the user.
    window->user_callbacks->on_game_started(window->game_instance, window->game_manager);
    if (window->connected_gamepad != NULL) {
        window->had_gamepad_input_curr_frame = true;
        window->had_gamepad_input_prev_frame = true;
        window->user_callbacks->on_gamepad_connected(
            window->game_instance, window->game_manager, SDL_GetGamepadName(window->connected_gamepad));
    }

    // Used to calculate delta time.
    Uint64 current_time_counter = SDL_GetPerformanceCounter();
    Uint64 prev_time_counter = 0;
    float delta_time_sec = 0.0f;

    while (!window->quit_requested) {
        // Process available window events.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const bool received_quit_event = prv_window_process_event(window, event, delta_time_sec);

            // Use `OR` instead of assignment because the user can call `window_close`.
            window->quit_requested |= received_quit_event;
        }

        // Calculate delta time.
        prev_time_counter = current_time_counter;
        current_time_counter = SDL_GetPerformanceCounter();
        const double delta_time_ms =
            (double)((current_time_counter - prev_time_counter) * 1000) / (double)(SDL_GetPerformanceFrequency());
        delta_time_sec = (float)(delta_time_ms * 0.001);

        // Tick.
        {
            prv_game_manager_tick(window->game_manager, delta_time_sec);
            window->user_callbacks->on_game_tick(window->game_instance, window->game_manager, delta_time_sec);

            if (window->had_gamepad_input_prev_frame != window->had_gamepad_input_curr_frame) {
                window->had_gamepad_input_prev_frame = window->had_gamepad_input_curr_frame;

                prv_game_manager_on_input_source_changed(window->game_manager);
                window->user_callbacks->on_input_source_changed(
                    window->game_instance, window->game_manager, window->had_gamepad_input_curr_frame);
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
    window->connected_gamepad = NULL;

    log_info("game manager is destroyed");
}

struct te_game_manager*
window_get_game_manager(te_window* window) {
#if defined(DEBUG)
    if (window->game_manager == NULL) {
        show_error_and_abort("game manager is not created yet (game not started) or was already destroyed (game ended)");
    }
#endif

    return window->game_manager;
}

void
window_capture_mouse_cursor(te_window* window, bool enable) {
    if (window->is_mouse_captured == enable) {
        return;
    }

    if (!SDL_SetWindowRelativeMouseMode(window->sdl_window, enable)) {
        log_info(SDL_GetError());
        return;
    }

    window->is_mouse_captured = enable;
}

bool
window_is_mouse_captured(te_window* window) {
    return window->is_mouse_captured;
}

bool
window_is_gamepad_connected(te_window* window) {
    return window->connected_gamepad != NULL;
}

void
window_get_size(te_window* window, unsigned int* width, unsigned int* height) {
    *width = window->width;
    *height = window->height;
}

void
window_get_cursor_position(te_window* window, float* x, float* y) {
    (void)window;
    SDL_GetMouseState(x, y);
}

unsigned int
window_get_display_refresh_rate(te_window* window) {
    return window->display_refresh_rate;
}

void
window_close(te_window* window) {
    window->quit_requested = true;
}

bool
prv_window_process_event(te_window* window, union SDL_Event event, float delta_time_sec) {
    (void)delta_time_sec;

    switch (event.type) {
        case (SDL_EVENT_MOUSE_MOTION): {
            prv_game_manager_on_mouse_moved(window->game_manager);
            window->user_callbacks->on_mouse_moved(
                window->game_instance, window->game_manager, event.motion.xrel, event.motion.yrel);
            break;
        }
        case (SDL_EVENT_MOUSE_BUTTON_DOWN): {
            const bool is_handled =
                prv_game_manager_on_mouse_button_pressed(window->game_manager, (enum te_mouse_button)event.button.button);
            window->user_callbacks->on_mouse_button_pressed(
                window->game_instance, window->game_manager, (enum te_mouse_button)event.button.button, is_handled);
            break;
        }
        case (SDL_EVENT_MOUSE_BUTTON_UP): {
            const bool is_handled =
                prv_game_manager_on_mouse_button_released(window->game_manager, (enum te_mouse_button)event.button.button);
            window->user_callbacks->on_mouse_button_released(
                window->game_instance, window->game_manager, (enum te_mouse_button)event.button.button, is_handled);
            break;
        }
        case (SDL_EVENT_KEY_DOWN): {
            const bool is_repeat = event.key.repeat != 0;
#if defined(IS_ARM64)
            if (window->connected_gamepad != NULL) {
                // In some cases while using retro-handhelds (which have built in gamepad) gamepad buttons trigger
                // keyboard input before the actual gamepad button input.
                break;
            }
#endif
#if defined(ENGINE_DEBUG_TOOLS)
            if (prv_debug_console_is_shown()) {
                if ((enum te_keyboard_button)event.key.scancode == TE_KB_TILDE) {
                    break; // key up event is used to show/hide console
                }
                prv_debug_console_on_keyboard_input(window->game_manager, (enum te_keyboard_button)event.key.scancode);
                break; // don't trigger user callbacks
            }
#endif
            window->had_gamepad_input_curr_frame = false;

            prv_game_manager_on_keyboard_input(window->game_manager, (enum te_keyboard_button)event.key.scancode, is_repeat);

            if (!is_repeat) {
                te_keyboard_modifiers mods;
                mods.mod = event.key.mod;
                window->user_callbacks->on_keyboard_button_pressed(
                    window->game_instance, window->game_manager, (enum te_keyboard_button)event.key.scancode, mods);
            }
            break;
        }
        case (SDL_EVENT_KEY_UP): {
            if (event.key.repeat != 0) {
                // Ignore repeat events.
                break;
            }
#if defined(IS_ARM64)
            if (window->connected_gamepad != NULL) {
                // Same as in "pressed" event.
                break;
            }
#endif
#if defined(ENGINE_DEBUG_TOOLS)
            if (prv_debug_console_is_shown()) {
                if ((enum te_keyboard_button)event.key.scancode == TE_KB_TILDE) {
                    prv_debug_console_hide();
                    SDL_StopTextInput(window->sdl_window);
                } else {
                    // input is handled in key down event
                    break; // don't trigger user callbacks
                }
            } else if (!prv_debug_console_is_shown() && (enum te_keyboard_button)event.key.scancode == TE_KB_TILDE) {
                prv_debug_console_show();
                SDL_StartTextInput(window->sdl_window);
            }
#endif
            window->had_gamepad_input_curr_frame = false;

            te_keyboard_modifiers mods;
            mods.mod = event.key.mod;
            window->user_callbacks->on_keyboard_button_released(
                window->game_instance, window->game_manager, (enum te_keyboard_button)event.key.scancode, mods);
            break;
        }
        case (SDL_EVENT_TEXT_INPUT): {
#if defined(IS_ARM64)
            if (window->connected_gamepad != NULL) {
                // Same as in "pressed" event.
                break;
            }
#endif
#if defined(ENGINE_DEBUG_TOOLS)
            if (prv_debug_console_is_shown()) {
                if (event.text.text[0] == '`') {
                    break; // "on button released" will handle it
                }
                prv_debug_console_on_keyboard_input_text(event.text.text);
                break; // don't trigger user callbacks
            }
#endif
            prv_game_manager_on_keyboard_input_text(window->game_manager, event.text.text);
            window->user_callbacks->on_keyboard_input_text(window->game_instance, window->game_manager, event.text.text);
            break;
        }
        case (SDL_EVENT_GAMEPAD_AXIS_MOTION): {
#if defined(DEBUG)
            if (sizeof(event.gaxis.value) != 2) {
                show_error_and_abort("expected \"short\" type");
            }
#endif
            window->had_gamepad_input_curr_frame = true;

            const float new_pos = (float)event.gaxis.value / 32767.0f;
            window->user_callbacks->on_gamepad_axis_moved(
                window->game_instance, window->game_manager, (enum te_gamepad_axis)event.gaxis.axis, new_pos);
            break;
        }
        case (SDL_EVENT_GAMEPAD_BUTTON_DOWN): {
            window->had_gamepad_input_curr_frame = true;
            window->user_callbacks->on_gamepad_button_pressed(
                window->game_instance, window->game_manager, (enum te_gamepad_button)event.gbutton.button);
            break;
        }
        case (SDL_EVENT_GAMEPAD_BUTTON_UP): {
#if defined(ENGINE_DEBUG_TOOLS)
            if ((enum te_gamepad_button)event.gbutton.button == TE_GB_BACK) {
                window->debug_stats_time_since_menu = 0.0f;
            } else if ((enum te_gamepad_button)event.gbutton.button == TE_GB_START) {
                window->debug_stats_time_since_start = 0.0f;
            }
            if (window->debug_stats_time_since_menu < 0.25f && window->debug_stats_time_since_start < 0.25f) {
                if (!debug_console_is_stats_shown()) {
                    renderer_set_fps_limit(game_manager_get_renderer(window->game_manager), 0);
                    debug_console_show_stats();
                } else {
                    renderer_set_fps_limit(
                        game_manager_get_renderer(window->game_manager), window_get_display_refresh_rate(window));
                    debug_console_hide_stats();
                }
                window->debug_stats_time_since_menu = 10.0f;
                window->debug_stats_time_since_start = 10.0f;
            }
#endif

            window->had_gamepad_input_curr_frame = true;
            window->user_callbacks->on_gamepad_button_released(
                window->game_instance, window->game_manager, (enum te_gamepad_button)event.gbutton.button);
            break;
        }
        case (SDL_EVENT_MOUSE_WHEEL): {
            window->user_callbacks->on_mouse_scroll_moved(window->game_instance, window->game_manager, event.wheel.y);
            break;
        }
        case (SDL_EVENT_WINDOW_FOCUS_GAINED): {
            window->user_callbacks->on_window_received_focus(window->game_instance, window->game_manager);
            break;
        }
        case (SDL_EVENT_WINDOW_FOCUS_LOST): {
            window->user_callbacks->on_window_lost_focus(window->game_instance, window->game_manager);
            break;
        }
        case (SDL_EVENT_WINDOW_RESIZED):
        case (SDL_EVENT_WINDOW_MAXIMIZED):
        case (SDL_EVENT_WINDOW_MINIMIZED): {
            // Save new size.
            int width;
            int height;
            if (SDL_GetWindowSizeInPixels(window->sdl_window, &width, &height) == false) {
                show_error_and_abort(SDL_GetError());
            }
            window->width = (unsigned int)width;
            window->height = (unsigned int)height;

            // Notify.
            prv_game_manager_on_window_size_changed(window->game_manager);
            break;
        }
        case (SDL_EVENT_GAMEPAD_ADDED): {
            if (window->connected_gamepad == NULL) {
                window->connected_gamepad = SDL_OpenGamepad(event.cdevice.which);
                window->user_callbacks->on_gamepad_connected(
                    window->game_instance, window->game_manager, SDL_GetGamepadName(window->connected_gamepad));
            }
            break;
        }
        case (SDL_EVENT_GAMEPAD_REMOVED): {
            if (window->connected_gamepad != NULL
                && event.cdevice.which == SDL_GetJoystickID(SDL_GetGamepadJoystick(window->connected_gamepad))) {
                SDL_CloseGamepad(window->connected_gamepad);
                window->connected_gamepad = NULL;
                window->user_callbacks->on_gamepad_disconnected(window->game_instance, window->game_manager);

#if defined(ENGINE_DEBUG_TOOLS)
                window->debug_stats_time_since_menu = 10.0f;
                window->debug_stats_time_since_start = 10.0f;
#endif
            }
            break;
        }
        case (SDL_EVENT_QUIT): {
            return true;
        }
    }

#if defined(ENGINE_DEBUG_TOOLS)
    if (window->connected_gamepad != NULL) {
        window->debug_stats_time_since_menu += delta_time_sec;
        window->debug_stats_time_since_start += delta_time_sec;
    }
#endif

    return false;
}

SDL_Window*
prv_window_get_sdl_window(te_window* window) {
    return window->sdl_window;
}

void*
prv_window_get_game_instance(te_window* window) {
    return window->game_instance;
}
