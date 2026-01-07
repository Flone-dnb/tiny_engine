#include "window.h"

#include <stdbool.h>
#include <stdlib.h>
#include "io/filesystem.h"
#include "io/log.h"
#include "io/paths.h"
#include "misc/error.h"

#define SDL_MAIN_HANDLED
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"

te_window*
window_create(const char* window_title) {
    // Destroy old log file.
    filesystem_remove_file(paths_get_log_file());

    // Initialize SDL.
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            show_error_and_abort(SDL_GetError());
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); // IF CHANGING
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1); // ALSO CHANGE GLAD
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
    if (display_count == 0) {
        show_error_and_abort("unable to find at least 1 display");
    }

    // Get display resolution.
    const SDL_DisplayMode* pMode = SDL_GetDesktopDisplayMode(displays[0]);
    int display_width = pMode->w;
    int display_height = pMode->h;
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

    log_info_fmt("created a window of size %dx%d", display_width, display_height);

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
