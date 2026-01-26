#include "window.h"

#include <SDL3/SDL_error.h>
#include <stdbool.h>
#include <stdlib.h>
#include "game_manager.h"
#include "io/filesystem.h"
#include "io/log.h"
#include "io/paths.h"
#include "misc/error.h"
#define SDL_MAIN_HANDLED
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_video.h"

/** Game window. */
struct te_window {
    /** SDL window. */
    struct SDL_Window* sdl_window;

    /** Game manager that window created. */
    struct te_game_manager* game_manager;

    /** Current width of the window. */
    unsigned int width;

    /** Current height of the window. */
    unsigned int height;

    /** Refresh rate of the used display. */
    unsigned int display_refresh_rate;

    /** `true` if the window needs to be closed. */
    bool quit_requested;
};

te_window*
window_create(const char* window_title) {

    // Destroy old log file.
    filesystem_remove_file(paths_get_log_file());

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
    window->game_manager = NULL;
    window->width = (unsigned int)display_width;
    window->height = (unsigned int)display_height;
    window->display_refresh_rate = display_refresh_rate;
    window->quit_requested = false;

    log_info_fmt("created a window of size %dx%d", window->width, window->height);

    return window;
}

void
window_process_events(te_window* window, te_game_window_callbacks* game_callbacks) {
    window->game_manager = game_manager_create(window, game_callbacks->on_game_tick);

    game_callbacks->on_game_started(window->game_manager);

    // Used to calculate delta time.
    unsigned long current_time_counter = SDL_GetPerformanceCounter();
    unsigned long prev_time_counter = 0;

    while (!window->quit_requested) {
        // Process available window events.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const bool received_quit_event = prv_window_process_event(window, event);

            // Use `OR` instead of assignment because the user can call `window_close`.
            window->quit_requested |= received_quit_event;
        }

        // Calculate delta time.
        prev_time_counter = current_time_counter;
        current_time_counter = SDL_GetPerformanceCounter();
        const double delta_time_ms = (double)((current_time_counter - prev_time_counter) * 1000)
                                     / (double)(SDL_GetPerformanceFrequency());
        const float delta_time_sec = (float)(delta_time_ms * 0.001);

        prv_game_manager_tick(window->game_manager, delta_time_sec);
        prv_game_manager_draw_frame(window->game_manager);
    }

    log_info("window is closing");

    game_callbacks->on_window_close(window->game_manager);

    // Destroy game manager.
    game_manager_destroy(window->game_manager);
    window->game_manager = NULL;

    log_info("game manager is destroyed");
}

struct te_game_manager*
window_get_game_manager(te_window* window) {
#if defined(DEBUG)
    if (window->game_manager == NULL) {
        show_error_and_abort(
            "game manager is not created yet (game not started) or was already destroyed (game ended)");
    }
#endif

    return window->game_manager;
}

void
window_get_size(te_window* window, unsigned int* width, unsigned int* height) {
    *width = window->width;
    *height = window->height;
}

unsigned int
window_get_display_refresh_rate(te_window* window) {
    return window->display_refresh_rate;
}

void
window_close(te_window* window) {
    window->quit_requested = true;
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

bool
prv_window_process_event(te_window* window, union SDL_Event event) {
    switch (event.type) {
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
        case (SDL_EVENT_QUIT): {
            return true;
        }
    }

    return false;
}

SDL_Window*
prv_window_get_sdl_window(te_window* window) {
    return window->sdl_window;
}
