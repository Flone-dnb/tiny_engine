#include "render/renderer.h"

#include <stdlib.h>
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_video.h"
#include "game/camera.h"
#include "game_manager.h"
#include "glad/glad.h"
#include "io/log.h"
#include "limits.h"
#include "misc/error.h"
#include "render/model_renderer.h"
#include "render/shader_manager.h"
#include "window.h"
#include "world.h"

/** Groups info used during the rendering. */
typedef struct te_world_render_info {
    /** World. */
    te_world* world;

    /** View projection matrix of the camera. */
    mat4 view_proj_mat;

    /** OpenGL viewport in pixels (left-bottom origin). */
    ivec4 gl_viewport;
} te_world_render_info;

/** Draws on the window. */
struct te_renderer {
    /** Always valid pointer, window that owns the renderer. This pointer should not be freed. */
    struct te_window* window;

    /** GL context. */
    struct SDL_GLContextState* gl_context;

    /** Shader manager. */
    te_shader_manager* shader_manager;

    /**
     * Preallocated array to keep active cameras while submitting a new frame.
     * Size of this array is @ref worlds_render_info_array_size.
     */
    te_world_render_info* worlds_render_info;

    /** Size of the array @ref worlds_render_info. */
    unsigned int worlds_render_info_array_size;

    /** GL depth function used. */
    unsigned int gl_depth_func;
};

#if defined(DEBUG)
void GLAPIENTRY
debugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                     const GLchar* message, const void* userParam) {
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
    renderer->shader_manager = prv_shader_manager_create();
    renderer->worlds_render_info_array_size = 2;
    renderer->worlds_render_info =
        malloc(sizeof(te_world_render_info) * renderer->worlds_render_info_array_size);

    // Create GL context.
    renderer->gl_context = SDL_GL_CreateContext(prv_window_get_sdl_window(window));
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
        show_error_and_abort(
            "the GPU does not support GL_KHR_DEBUG extension which is required for debug builds");
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
    prv_shader_manager_destroy(renderer->shader_manager);
    free(renderer->worlds_render_info);

    if (!SDL_GL_DestroyContext(renderer->gl_context)) {
        show_error_and_abort(SDL_GetError());
    }

    free(renderer);
}

void
renderer_set_fps_limit(te_renderer* renderer, unsigned int limit) {
    (void)renderer;
    (void)limit;
    // TODO
}

te_shader_manager*
renderer_get_shader_manager(te_renderer* renderer) {
    return renderer->shader_manager;
}

void
prv_renderer_draw_frame(te_renderer* renderer) {
    // Make sure there was no GL error during the last frame.
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        show_gl_error_and_abort(gl_error);
    }

    // Get window size (for later).
    unsigned int window_width = 0;
    unsigned int window_height = 0;
    window_get_size(renderer->window, &window_width, &window_height);

    // Get worlds.
    te_game_manager* game_manager = window_get_game_manager(renderer->window);
    unsigned int world_count = 0;
    te_world** worlds = game_manager_get_worlds(game_manager, &world_count);

    if (world_count > renderer->worlds_render_info_array_size) {
        // Expand the array.
        te_world_render_info* new_worlds = malloc(sizeof(te_world_render_info) * world_count);
        free(renderer->worlds_render_info);
        renderer->worlds_render_info = new_worlds;
        renderer->worlds_render_info_array_size = world_count;
    }

    // Get active cameras.
    unsigned int active_world_count = 0;
    for (unsigned int i = 0; i < world_count; i++) {
        te_camera* camera = world_get_active_camera(worlds[i]);
        if (camera == NULL) {
            continue;
        }

        // Set aspect ratio to the camera.
        vec4 viewport;
        camera_get_viewport(camera, viewport);
        const unsigned int viewport_width = (unsigned int)((float)window_width * viewport[2]);
        const unsigned int viewport_height = (unsigned int)((float)window_height * viewport[3]);
        prv_camera_set_render_target_size(camera, viewport_width, viewport_height);

        // Save info.
        te_world_render_info* info = &renderer->worlds_render_info[active_world_count];

        info->world = worlds[i];

        mat4 view_mat;
        mat4 proj_mat;
        camera_get_view_mat(camera, view_mat);
        camera_get_proj_mat(camera, proj_mat);
        glm_mat4_mul(proj_mat, view_mat, info->view_proj_mat);

        info->gl_viewport[0] = (int)((float)window_width * viewport[0]);
        info->gl_viewport[1] = (int)((float)window_height * (1.0f - fmin(1.0f, viewport[1] + viewport[3])));
        info->gl_viewport[2] = (int)viewport_width;
        info->gl_viewport[3] = (int)viewport_height;

        active_world_count += 1;
    }

    // Rendering to window's framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (active_world_count > 0) {
        // Draw models.
        for (unsigned int world_idx = 0; world_idx < active_world_count; world_idx++) {
            te_world_render_info* info = &renderer->worlds_render_info[world_idx];
            te_model_renderer* model_renderer = world_get_model_renderer(info->world);

            model_renderer_draw(model_renderer, info->gl_viewport, info->view_proj_mat);
        }
    }

    SDL_GL_SwapWindow(prv_window_get_sdl_window(renderer->window));
}

void
prv_renderer_on_window_size_changed(te_renderer* renderer) {
    (void)renderer; // <- unused for now
    // TODO: update font size
}
