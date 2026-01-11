#include "renderer.h"

#include <stdlib.h>
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_video.h"
#include "glad/glad.h"
#include "io/log.h"
#include "misc/error.h"
#include "window.h"

#if defined(DEBUG)
void GLAPIENTRY
debugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
                     const void* userParam) {
    (void)id;
    (void)length;
    (void)userParam;

    if (source == GL_DEBUG_SOURCE_SHADER_COMPILER) {
        // We display shader compilation errors using shader manager.
        return;
    }

    if (type != GL_DEBUG_TYPE_ERROR) {
        return;
    }

    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        show_error_and_abort(message);
    }
}
#endif

te_renderer*
renderer_create(struct te_window* window) {
    te_renderer* renderer = malloc(sizeof(te_renderer));
    renderer->window = window;
    renderer->gl_depth_func = GL_LEQUAL; // less/equal is needed for main pass (after z prepass)

    // Create GL context.
    renderer->gl_context = SDL_GL_CreateContext(window->sdl_window);
    if (renderer->gl_context == NULL) {
        show_error_and_abort(SDL_GetError());
    }

    // Initialize GLAD.
    if (gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress) == 0) {
        show_error_and_abort("failed to load OpenGL ES");
    }

#if defined(ENGINE_DEBUG_TOOLS)
    if (GLAD_GL_EXT_disjoint_timer_query != 1) {
        show_error_and_abort("the GPU does not support OpenGL extension "
                             "GL_EXT_disjoint_timer_query which is required for debug builds");
    }
#endif

#if defined(DEBUG)
    if (GLAD_GL_KHR_debug != 1) {
        show_error_and_abort("the GPU does not support GL_KHR_DEBUG extension which is required for debug builds");
    }
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debugMessageCallback, NULL);

    // Enable all error messages
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, NULL, GL_TRUE);
#endif

    // Enable back face culling.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Setup clear values.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(renderer->gl_depth_func);

    // Disable VSync.
    if (!SDL_GL_SetSwapInterval(0)) {
        show_error_and_abort(SDL_GetError());
    }

    // Set FPS limit.
    const unsigned int refresh_rate = window_get_display_refresh_rate(window);
    log_info_fmt("setting FPS limit to %u (display's refresh rate)", refresh_rate);
    renderer_set_fps_limit(renderer, refresh_rate);

    return renderer;
}

void
renderer_destroy(te_renderer* renderer) {
    free(renderer);
}

void
renderer_set_fps_limit(te_renderer* renderer, unsigned int limit) {
    (void)renderer;
    (void)limit;
    log_info("TODO: FPS limit not implemented");
}

void
prv_renderer_draw_frame(te_renderer* renderer) {
    // Make sure there was no GL error during the last frame.
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        show_gl_error_and_abort(gl_error);
    }

    // Rendering to window's framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SDL_GL_SwapWindow(renderer->window->sdl_window);
}

void
prv_renderer_on_window_size_changed(te_renderer* renderer) {
    (void)renderer; // <- unused for now
    // TODO: update font size
}
