#include <render/renderer.h>

#if defined(WIN32)
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__)
#include <time.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <debug_console.h>
#include <game/camera.h>
#include <game_manager.h>
#include <io/log.h>
#include <render/debug_drawer.h>
#include <render/font_manager.h>
#include <render/gpu_section.h>
#include <render/gpu_time_section.h>
#include <render/model_renderer.h>
#include <render/shader_manager.h>
#include <render/texture_manager.h>
#include <render/widget_renderer.h>
#include <type_database.h>
#include <window.h>
#include <world.h>
#include <glad/glad.h>
#include <SDL3/SDL_messagebox.h>

// Stuff needed to calculate FPS and keep frame limit.
typedef struct te_renderer_frame_stats {
    // Stats collected for the last second.
    unsigned int frame_count;
    float time_sec_since_update;

    unsigned int fps;
} te_renderer_frame_stats;

struct te_renderer {
    // Always valid pointer, window that owns the renderer. This pointer should not be freed.
    struct te_window* window;

    struct SDL_GLContextState* gl_context;

    // Created by the renderer.
    te_shader_manager* shader_manager;
    te_texture_manager* texture_manager;
    te_font_manager* font_manager;

    te_light_params* light_params;

    te_renderer_frame_stats frame_stats;

    unsigned int fps_limit;

#if defined(__linux__)
    struct timespec frame_end_time;
#endif

#if defined(ENGINE_DEBUG_TOOLS)
    // GPU time query IDs.
    unsigned int gl_timestamp_frame_start;
    unsigned int gl_timestamp_frame_end;
    unsigned int gl_query_draw_debug;
#endif
};

#if defined(DEBUG)
void GLAPIENTRY
debugMessageCallback(
    GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
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
        log_error(message);
        abort();
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

// Getters/setters for light params for reflection.
#define LIGHT_PARAMS_FUNC(name, body) \
static void light_params_##name(void* obj, float* arg) { \
    te_light_params* params = ((te_light_params*)obj); \
    body; \
}
LIGHT_PARAMS_FUNC(set_clear_color, glm_vec3_copy(arg, params->clear_color));
LIGHT_PARAMS_FUNC(get_clear_color, glm_vec3_copy(params->clear_color, arg));
LIGHT_PARAMS_FUNC(set_ambient_light_color, glm_vec3_copy(arg, params->ambient_light_color));
LIGHT_PARAMS_FUNC(get_ambient_light_color, glm_vec3_copy(params->ambient_light_color, arg));
LIGHT_PARAMS_FUNC(set_dir_light_color, glm_vec4_copy(arg, params->directional_light_color));
LIGHT_PARAMS_FUNC(get_dir_light_color, glm_vec4_copy(params->directional_light_color, arg));
LIGHT_PARAMS_FUNC(set_dir_light_dir, glm_vec3_normalize(arg); glm_vec3_copy(arg, params->directional_light_direction));
LIGHT_PARAMS_FUNC(get_dir_light_dir, glm_vec3_copy(params->directional_light_direction, arg));
LIGHT_PARAMS_FUNC(set_point_light_color, glm_vec4_copy(arg, params->point_light_color));
LIGHT_PARAMS_FUNC(get_point_light_color, glm_vec4_copy(params->point_light_color, arg));
LIGHT_PARAMS_FUNC(
    set_point_light_pos_and_dist, glm_vec4_copy(arg, params->point_light_pos_and_dist));
LIGHT_PARAMS_FUNC(
    get_point_light_pos_and_dist, glm_vec4_copy(params->point_light_pos_and_dist, arg));
LIGHT_PARAMS_FUNC(set_distance_fog_range, glm_vec2_copy(arg, params->distance_fog_range));
LIGHT_PARAMS_FUNC(get_distance_fog_range, glm_vec2_copy(params->distance_fog_range, arg));
LIGHT_PARAMS_FUNC(set_distance_fog_color, glm_vec3_copy(arg, params->distance_fog_color));
LIGHT_PARAMS_FUNC(get_distance_fog_color, glm_vec3_copy(params->distance_fog_color, arg));

te_renderer*
renderer_create(struct te_window* window) {
    te_renderer* renderer = malloc(sizeof(te_renderer));

    renderer->window = window;
    renderer->shader_manager = prv_shader_manager_create();
    renderer->texture_manager = prv_texture_manager_create();
    renderer->font_manager = prv_font_manager_create(renderer);

    renderer->frame_stats.frame_count = 0;
    renderer->frame_stats.time_sec_since_update = 0.0f;
    renderer->frame_stats.fps = 0;

    renderer->fps_limit = 0;

    // Setup base lighting.
    {
        renderer->light_params = malloc(sizeof(te_light_params));
        memset(renderer->light_params, 0, sizeof(te_light_params));

        te_light_params* data = renderer->light_params;

        glm_vec3_copy((vec3){0.5f, 0.5f, 0.5f}, data->ambient_light_color);

        glm_vec3_copy((vec3){1.0f, -1.0f, 1.0f}, data->directional_light_direction);
        glm_vec3_normalize(data->directional_light_direction);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, data->directional_light_color);

        glm_vec2_copy((vec2){-1.0f, -1.0f}, data->distance_fog_range);
    }

#if defined(__linux__)
    clock_gettime(CLOCK_MONOTONIC, &renderer->frame_end_time);
#endif

    // Create GL context.
    renderer->gl_context = SDL_GL_CreateContext(prv_window_get_sdl_window(window));
    if (renderer->gl_context == NULL) {
#if defined(WIN32)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), NULL);
#endif
        log_error(SDL_GetError());
        abort();
    }

    // Initialize GLAD.
    if (gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress) == 0) {
#if defined(WIN32)
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Error", "failed to load OpenGL ES", NULL);
#endif
        log_error("failed to load OpenGL ES");
        abort();
    }

#if defined(ENGINE_DEBUG_TOOLS)
    if (GLAD_GL_EXT_disjoint_timer_query != 1) {
        log_info("the GPU does not support GL_EXT_disjoint_timer_query extension, GPU time "
                 "metrics are disabled");
    } else {
        glGenQueriesEXT(1, &renderer->gl_timestamp_frame_start);
        glGenQueriesEXT(1, &renderer->gl_timestamp_frame_end);
        glGenQueriesEXT(1, &renderer->gl_query_draw_debug);

        // Init timers.

        glQueryCounterEXT(renderer->gl_timestamp_frame_start, GL_TIMESTAMP_EXT);
        glQueryCounterEXT(renderer->gl_timestamp_frame_end, GL_TIMESTAMP_EXT);

        GPU_TIME_SECTION_BEGIN(renderer->gl_query_draw_debug);
        GPU_TIME_SECTION_END;
    }
#endif

#if defined(DEBUG)
    if (GLAD_GL_KHR_debug != 1) {
        log_info("the GPU does not support GL_KHR_DEBUG extension, some debugging features "
                 "are disabled");
    } else {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugMessageCallback, NULL);

        // Enable all error messages
        glDebugMessageControl(
            GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, NULL, GL_TRUE);
    }
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
        log_error(SDL_GetError());
        abort();
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

    // Add light params to type database to be able to display them in the property inspector.
    {
        te_type_info* info = type_info_create(
            "light_params", NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        type_info_add_vec3_variable(info, "clear_color", light_params_set_clear_color, light_params_get_clear_color);
        type_info_add_vec3_variable(
            info, "ambient_light_color", light_params_set_ambient_light_color, light_params_get_ambient_light_color);
        type_info_add_vec4_variable(
            info, "directional_light_color", light_params_set_dir_light_color,
            light_params_get_dir_light_color);
        type_info_add_vec3_variable(
            info, "directional_light_direction", light_params_set_dir_light_dir,
            light_params_get_dir_light_dir);
        type_info_add_vec4_variable(
            info, "point_light_color", light_params_set_point_light_color,
            light_params_get_point_light_color);
        type_info_add_vec4_variable(
            info, "point_light_pos_dist", light_params_set_point_light_pos_and_dist,
            light_params_get_point_light_pos_and_dist);
        type_info_add_vec2_variable(
            info, "distance_fog_range", light_params_set_distance_fog_range,
            light_params_get_distance_fog_range);
        type_info_add_vec3_variable(
            info, "distance_fog_color", light_params_set_distance_fog_color,
            light_params_get_distance_fog_color);

        type_database_register_type(info);
    }

    return renderer;
}

void
renderer_destroy(te_renderer* renderer) {
    prv_texture_manager_destroy(renderer->texture_manager);
    prv_shader_manager_destroy(renderer->shader_manager);
    prv_font_manager_destroy(renderer->font_manager);

    free(renderer->light_params);

#if defined(ENGINE_DEBUG_TOOLS)
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {
        glDeleteQueriesEXT(1, &renderer->gl_timestamp_frame_start);
        glDeleteQueriesEXT(1, &renderer->gl_timestamp_frame_end);
        glDeleteQueriesEXT(1, &renderer->gl_query_draw_debug);
    }
#endif

    if (!SDL_GL_DestroyContext(renderer->gl_context)) {
        log_error(SDL_GetError());
        abort();
    }

    free(renderer);
}

te_light_params*
renderer_get_light_params(te_renderer* renderer) {
    return renderer->light_params;
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
    stats->rendered_opaque_model_count = 0;
    stats->rendered_transparent_model_count = 0;
#endif

    // FPS limit.
    if (renderer->fps_limit > 0) {
#if defined(WIN32)
        const float target_time_ms = 1000.0f / (float)renderer->fps_limit;
        const float time_to_sleep_ms = target_time_ms - delta_time_sec;
        if (time_to_sleep_ms >= 1.0) {
            timeBeginPeriod(1);
            Sleep((unsigned long)time_to_sleep_ms);
            timeEndPeriod(1);
        }
#elif defined(__linux__)
        const long target_time_ns = (long)(1000000000.0 / (double)renderer->fps_limit);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long delta_ns = now.tv_nsec - renderer->frame_end_time.tv_nsec;
        if (delta_ns < 0) {
            delta_ns += 1000000000;
        }

        const long time_to_sleep_ns = target_time_ns - delta_ns;
        if (time_to_sleep_ns > 0) {
            struct timespec requested;
            struct timespec remaining;
            requested.tv_sec = 0;
            requested.tv_nsec = time_to_sleep_ns;
            while (nanosleep(&requested, &remaining) == -1) {
                if (errno == EINTR) {
                    requested = remaining;
                } else {
                    break;
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &renderer->frame_end_time);
#else
#error "unsupported OS"
#endif
    }
}

#if defined(ENGINE_DEBUG_TOOLS)
static float
prv_renderer_get_query_time_ms(unsigned int query) {
    GLuint64 time_elapsed = 0;
    glGetQueryObjectui64vEXT(query, GL_QUERY_RESULT_EXT, &time_elapsed);
    return (float)(time_elapsed) / 1000000.0f; // nanoseconds to milliseconds
}
#endif

void
prv_renderer_draw_frame(te_renderer* renderer, float delta_time_sec) {
    // Make sure there was no GL error during the last frame.
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        switch (gl_error) {
            case GL_INVALID_ENUM: log_error("GL error: INVALID_ENUM"); break;
            case GL_INVALID_VALUE: log_error("GL error: INVALID_VALUE"); break;
            case GL_INVALID_OPERATION: log_error("GL error: INVALID_OPERATION"); break;
            case GL_OUT_OF_MEMORY: log_error("GL error: OUT_OF_MEMORY"); break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                log_error("GL error: INVALID_FRAMEBUFFER_OPERATION");
                break;
            default: {
                char error_msg[128] = {0};
                snprintf(&error_msg[0], 128, "GL error: %u", gl_error);
                log_error(&error_msg[0]);
            } break;
        }

        abort();
    }

#if defined(ENGINE_DEBUG_TOOLS)
    te_debug_stats* debug_stats = prv_debug_console_get_stats();
    bool record_new_queries = true;

    if (GLAD_GL_EXT_disjoint_timer_query == 1) {
        // Check if the GPU finished commands.
        GLuint available = 0;
        glGetQueryObjectuivEXT(
            renderer->gl_timestamp_frame_end, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
        if (available == GL_FALSE) {
            // Don't do new queries on this frame, we will wait for the current queries to be finished.
            record_new_queries = false;
            debug_stats->cpu_ahead_gpu_frame_count += 1;
        } else {
            debug_stats->cpu_ahead_gpu_frame_count = 0;

            debug_stats->gpu_time_draw_debug_ms =
                prv_renderer_get_query_time_ms(renderer->gl_query_draw_debug);

            // Reset world-dependant GPU metrics.
            debug_stats->gpu_time_draw_models_ms = 0.0f;
            debug_stats->gpu_time_draw_widgets_ms = 0.0f;

            {
                GLint64 start_time = 0;
                GLint64 end_time = 0;
                glGetQueryObjecti64vEXT(
                    renderer->gl_timestamp_frame_start, GL_QUERY_RESULT_EXT, &start_time);
                glGetQueryObjecti64vEXT(
                    renderer->gl_timestamp_frame_end, GL_QUERY_RESULT_EXT, &end_time);
                debug_stats->gpu_time_frame_ms =
                    (float)(end_time - start_time) / 1000000.0f; // nanoseconds to milliseconds
            }

            // Mark frame start.
            glQueryCounterEXT(renderer->gl_timestamp_frame_start, GL_TIMESTAMP_EXT);
        }
    }

    // Reset world-dependant CPU metrics.
    debug_stats->cpu_time_submit_models_ms = 0.0f;
    debug_stats->cpu_time_submit_widgets_ms = 0.0f;

    const Uint64 cpu_frame_start_counter = SDL_GetPerformanceCounter();
#endif

    // Get window size (for later).
    unsigned int window_width = 0;
    unsigned int window_height = 0;
    window_get_size(renderer->window, &window_width, &window_height);

    // Rendering to window's framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(
        renderer->light_params->clear_color[0], renderer->light_params->clear_color[1],
        renderer->light_params->clear_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Get worlds.
    te_game_manager* game_manager = window_get_game_manager(renderer->window);
    unsigned int world_count = 0;
    te_world** worlds = game_manager_get_worlds(game_manager, &world_count);

    for (unsigned int i = 0; i < world_count; i++) {
        te_camera* camera = world_get_active_camera(worlds[i]);
        if (camera == NULL) {
            continue;
        }

#if defined(ENGINE_DEBUG_TOOLS)
        if (GLAD_GL_EXT_disjoint_timer_query == 1 && record_new_queries) {
            // Save results of the previous GPU metrics.
            debug_stats->gpu_time_draw_models_ms +=
                prv_renderer_get_query_time_ms(prv_world_get_gl_query_draw_models(worlds[i]));
            debug_stats->gpu_time_draw_widgets_ms +=
                prv_renderer_get_query_time_ms(prv_world_get_gl_query_draw_widgets(worlds[i]));
        }
#endif

        te_model_renderer* opaque_model_renderer = world_get_opaque_model_renderer(worlds[i]);
        te_model_renderer* transparent_model_renderer =
            world_get_transparent_model_renderer(worlds[i]);
        te_widget_renderer* widget_renderer = world_get_widget_renderer(worlds[i]);

        // Set camera's aspect ratio.
        vec4 viewport;
        camera_get_viewport(camera, viewport);
        const unsigned int viewport_width = (unsigned int)((float)window_width * viewport[2]);
        const unsigned int viewport_height =
            (unsigned int)((float)window_height * viewport[3]);
        prv_camera_set_render_target_size(camera, viewport_width, viewport_height);

        mat4* view_proj_mat = camera_get_view_proj_mat(camera);
        mat4* view_mat = camera_get_view_mat(camera);
        struct te_frustum_shape* camera_frustum = camera_get_frustum(camera);

        ivec4 gl_viewport;
        gl_viewport[0] = (int)((float)window_width * viewport[0]);
        gl_viewport[1] =
            (int)((float)window_height * (1.0f - fmin(1.0f, viewport[1] + viewport[3])));
        gl_viewport[2] = (int)viewport_width;
        gl_viewport[3] = (int)viewport_height;

        glViewport(gl_viewport[0], gl_viewport[1], gl_viewport[2], gl_viewport[3]);

        // Draw models.
        {
#if defined(ENGINE_DEBUG_TOOLS)
            const Uint64 cpu_start_counter = SDL_GetPerformanceCounter();
            if (record_new_queries) {
                GPU_TIME_SECTION_BEGIN(prv_world_get_gl_query_draw_models(worlds[i]));
            }
#endif

            GPU_SECTION_BEGIN("opaque models");
#if defined(ENGINE_DEBUG_TOOLS)
            debug_stats->rendered_opaque_model_count =
#endif
                model_renderer_draw(
                    opaque_model_renderer, renderer->light_params, view_mat, view_proj_mat,
                    camera_frustum);
            GPU_SECTION_END;

            if (model_renderer_has_models(transparent_model_renderer)) {
                GPU_SECTION_BEGIN("transparent models");

                glDisable(GL_CULL_FACE);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if defined(ENGINE_DEBUG_TOOLS)
                debug_stats->rendered_transparent_model_count =
#endif
                    model_renderer_draw(
                        transparent_model_renderer, renderer->light_params,
                        view_mat, view_proj_mat,
                        camera_frustum);
                glDisable(GL_BLEND);

                glEnable(GL_CULL_FACE);

                GPU_SECTION_END;
            }

#if defined(ENGINE_DEBUG_TOOLS)
            if (record_new_queries) {
                GPU_TIME_SECTION_END;
            }
            debug_stats->cpu_time_submit_models_ms +=
                (float)(SDL_GetPerformanceCounter() - cpu_start_counter) * 1000.0f
                / (float)(SDL_GetPerformanceFrequency());
#endif
        }

        // Draw widgets.
        {
#if defined(ENGINE_DEBUG_TOOLS)
            GPU_SECTION_BEGIN("widgets");
            const Uint64 cpu_start_counter = SDL_GetPerformanceCounter();
            if (record_new_queries) {
                GPU_TIME_SECTION_BEGIN(prv_world_get_gl_query_draw_widgets(worlds[i]));
            }
#endif

            widget_renderer_draw(widget_renderer);

#if defined(ENGINE_DEBUG_TOOLS)
            if (record_new_queries) {
                GPU_TIME_SECTION_END;
            }
            debug_stats->cpu_time_submit_widgets_ms +=
                (float)(SDL_GetPerformanceCounter() - cpu_start_counter) * 1000.0f
                / (float)(SDL_GetPerformanceFrequency());
            GPU_SECTION_END;
#endif
        }
    }

#if defined(ENGINE_DEBUG_TOOLS)
    te_camera* debug_camera = NULL;
    for (unsigned int i = 0; i < world_count; i++) {
        debug_camera = world_get_active_camera(worlds[i]);
        if (debug_camera == NULL) {
            continue;
        }

#if defined(ENGINE_EDITOR)
        // First camera in the game world.
        if (strcmp(world_get_name(worlds[i]), "game") == 0) {
            break;
        } else {
            debug_camera = NULL;
        }
#else
        // Just use the first active camera.
        break;
#endif
    }

    if (debug_camera != NULL) {
        // Draw debug.
        GPU_SECTION_BEGIN("debug");
        if (record_new_queries) {
            GPU_TIME_SECTION_BEGIN(renderer->gl_query_draw_debug);
        }
        {
            const Uint64 cpu_start_counter = SDL_GetPerformanceCounter();

            prv_debug_console_draw(delta_time_sec);
            prv_debug_drawer_draw(
                renderer, delta_time_sec, camera_get_view_proj_mat(debug_camera));

            debug_stats->cpu_time_submit_debug_ms =
                (float)(SDL_GetPerformanceCounter() - cpu_start_counter) * 1000.0f
                / (float)(SDL_GetPerformanceFrequency());
        }
        if (record_new_queries) {
            GPU_TIME_SECTION_END;
        }
        GPU_SECTION_END;
    }

    if (GLAD_GL_EXT_disjoint_timer_query == 1 && record_new_queries) {
        glQueryCounterEXT(renderer->gl_timestamp_frame_end, GL_TIMESTAMP_EXT);
    }

    // Get CPU time before swap as it might block the current thread.
    debug_stats->cpu_time_frame_ms =
        (float)(SDL_GetPerformanceCounter() - cpu_frame_start_counter) * 1000.0f
        / (float)(SDL_GetPerformanceFrequency());

    const Uint64 cpu_swap_start_counter = SDL_GetPerformanceCounter();
#endif

    SDL_GL_SwapWindow(prv_window_get_sdl_window(renderer->window));

#if defined(ENGINE_DEBUG_TOOLS)
    debug_stats->cpu_time_swap_ms =
        (float)(SDL_GetPerformanceCounter() - cpu_swap_start_counter) * 1000.0f
        / (float)(SDL_GetPerformanceFrequency());
#endif

    prv_renderer_calc_frame_stats(renderer, delta_time_sec);
}

void
prv_renderer_on_window_size_changed(te_renderer* renderer) {
    prv_font_manager_on_window_size_changed(renderer->font_manager);
}
