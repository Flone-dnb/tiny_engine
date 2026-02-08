#include "render/renderer.h"

#include <stdlib.h>
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_video.h"
#include "debug_console.h"
#include "game/camera.h"
#include "game_manager.h"
#if defined(ENGINE_DEBUG_TOOLS)
#include "game_manager.h"
#endif
#include "glad/glad.h"
#include "io/log.h"
#include "limits.h"
#include "misc/error.h"
#include "render/debug_drawer.h"
#include "render/font_manager.h"
#include "render/model_renderer.h"
#include "render/shader_manager.h"
#include "render/texture_manager.h"
#include "window.h"
#include "world.h"
#if defined(WIN32)
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__)
#include "time.h"
#endif

// Stuff needed to calculate FPS and keep frame limit.
typedef struct te_renderer_frame_stats {
    // Stats collected for the last second.
    unsigned int frame_count;
    float time_sec_since_update;

    unsigned int fps;
} te_renderer_frame_stats;

// Groups info used during the rendering of a world.
typedef struct te_world_render_info {
    // Do not free/destroy this pointer.
    te_world* world;

    struct te_frustum_shape* camera_frustum;

    // View projection matrix of the camera.
    mat4 view_proj_mat;

    // OpenGL viewport in pixels (left-bottom origin).
    ivec4 gl_viewport;
} te_world_render_info;

struct te_renderer {
    // Always valid pointer, window that owns the renderer. This pointer should not be freed.
    struct te_window* window;

    struct SDL_GLContextState* gl_context;

    // Created by the renderer.
    te_shader_manager* shader_manager;
    te_texture_manager* texture_manager;
    te_font_manager* font_manager;

    // Preallocated array to keep active cameras while submitting a new frame.
    // Size of this array is @ref worlds_render_info_array_size.
    te_world_render_info* worlds_render_info;

    te_renderer_frame_stats frame_stats;

    // Size of the array @ref worlds_render_info.
    unsigned int worlds_render_info_array_size;

    unsigned int fps_limit;
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

#if defined(ENGINE_DEBUG_TOOLS)
// Callback for the renderer's debug console command.
void
debug_command_set_fps_limit(te_game_manager* game_manager, unsigned int new_limit) {
    te_renderer* renderer = game_manager_get_renderer(game_manager);
    renderer_set_fps_limit(renderer, new_limit);
}
#endif

te_renderer*
renderer_create(struct te_window* window) {
    te_renderer* renderer = malloc(sizeof(te_renderer));
    renderer->window = window;
    renderer->shader_manager = prv_shader_manager_create();
    renderer->texture_manager = prv_texture_manager_create();
    renderer->font_manager = prv_font_manager_create(renderer);

    renderer->worlds_render_info_array_size = 2;
    renderer->worlds_render_info =
        malloc(sizeof(te_world_render_info) * renderer->worlds_render_info_array_size);

    renderer->frame_stats.frame_count = 0;
    renderer->frame_stats.time_sec_since_update = 0.0f;
    renderer->frame_stats.fps = 0;

    renderer->fps_limit = 0;

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
    glDepthFunc(GL_LEQUAL);

    // Disable VSync.
    if (!SDL_GL_SetSwapInterval(0)) {
        show_error_and_abort(SDL_GetError());
    }

    // Set FPS limit.
    const unsigned int refresh_rate = window_get_display_refresh_rate(window);
    log_info_fmt("setting FPS limit to %u (display's refresh rate)", refresh_rate);
    renderer_set_fps_limit(renderer, refresh_rate);

#if defined(ENGINE_DEBUG_TOOLS)
    te_debug_console_command fps_command = {0};
    fps_command.name = "set_fps_limit";
    fps_command.arg_uint = debug_command_set_fps_limit;
    debug_console_register_command(fps_command);
#endif

    return renderer;
}

void
renderer_destroy(te_renderer* renderer) {
    prv_texture_manager_destroy(renderer->texture_manager);
    prv_shader_manager_destroy(renderer->shader_manager);
    prv_font_manager_destroy(renderer->font_manager);
    free(renderer->worlds_render_info);

    if (!SDL_GL_DestroyContext(renderer->gl_context)) {
        show_error_and_abort(SDL_GetError());
    }

    free(renderer);
}

void
renderer_set_fps_limit(te_renderer* renderer, unsigned int limit) {
    renderer->fps_limit = limit;
}

te_window*
renderer_get_window(te_renderer* renderer) {
    return renderer->window;
}

te_shader_manager*
renderer_get_shader_manager(te_renderer* renderer) {
    return renderer->shader_manager;
}

te_texture_manager*
renderer_get_texture_manager(te_renderer* renderer) {
    return renderer->texture_manager;
}

struct te_font_manager*
renderer_get_font_manager(te_renderer* renderer) {
    return renderer->font_manager;
}

unsigned int
renderer_get_fps(te_renderer* renderer) {
    return renderer->frame_stats.fps;
}

unsigned int
renderer_get_fps_limit(te_renderer* renderer) {
    return renderer->fps_limit;
}

void
prv_renderer_calc_frame_stats(te_renderer* renderer, float delta_time_sec) {
    // Update FPS stat.
    {
        te_renderer_frame_stats* stats = &renderer->frame_stats;

        renderer->frame_stats.frame_count += 1;
        stats->time_sec_since_update += delta_time_sec;

        if (stats->time_sec_since_update >= 1.0f) {
            stats->fps = stats->frame_count;
            stats->frame_count = 0;
            stats->time_sec_since_update = 0.0f;
        }
    }

#if defined(ENGINE_DEBUG_TOOLS)
    te_debug_stats* stats = prv_debug_console_get_stats();
    stats->fps = renderer->frame_stats.fps;

    // Reset some stats to accumulate on the next frame.
    stats->rendered_model_count = 0;
#endif

    // FPS limit.
    if (renderer->fps_limit > 0) {
        // Should use perf counters and high precision timers here but this already works fine.
        const float target_time_ms = 1000.0f / (float)renderer->fps_limit;
        const float time_to_sleep_ms = target_time_ms - delta_time_sec;
        if (time_to_sleep_ms >= 1.0) {
#if defined(WIN32)
            timeBeginPeriod(1);
            Sleep((unsigned long)time_to_sleep_ms);
            timeEndPeriod(1);
#elif defined(__linux__)
            struct timespec tim;
            struct timespec tim2;
            tim.tv_sec = 0;
            tim.tv_nsec = (long)floor(time_to_sleep_ms * 1000000.0f * 0.965f);
            nanosleep(&tim, &tim2);
#else
#error "unsupported OS"
#endif
        }
    }
}

void
prv_renderer_draw_frame(te_renderer* renderer, float delta_time_sec) {
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

        info->camera_frustum = camera_get_frustum(camera);

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

            model_renderer_draw(model_renderer, info->gl_viewport, info->view_proj_mat, info->camera_frustum);
        }
    }

#if defined(ENGINE_DEBUG_TOOLS)
    prv_debug_console_draw(delta_time_sec);
    prv_debug_drawer_draw(renderer, delta_time_sec);
#endif

    SDL_GL_SwapWindow(prv_window_get_sdl_window(renderer->window));

    prv_renderer_calc_frame_stats(renderer, delta_time_sec);
}

void
prv_renderer_on_window_size_changed(te_renderer* renderer) {
    prv_font_manager_on_window_size_changed(renderer->font_manager);
}
