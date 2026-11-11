project(engine_lib C)

# -------------------------------------------------------------------------------------------------
#                                          TARGET SOURCES
# -------------------------------------------------------------------------------------------------

# Sources.
set(PROJECT_SOURCES
    other/snprintf.c
    other/snprintf.h
    src/window.c
    src/io/log.c
    src/io/paths.c
    src/io/filesystem.c
    src/io/config.c
    src/misc/globals.c
    src/misc/memory_usage.c
    src/misc/wchar_funcs.c
    src/misc/mesh_generator.c
    src/misc/high_freq_timer.c
    src/misc/num_hashtable.c
    src/game_manager.c
    src/sound/sound_manager.c
    src/sound/audio_mixer.c
    src/sound/audio_mixer.h
    src/sound/os_audio_system.c
    src/sound/os_audio_system.h
    src/sound/sound_constants.h
    src/world.c
    src/debug_console.c
    src/type_database.c
    src/window_system/os_window.h
    src/window_system/os_window.c
    src/shape/aabb_shape.c
    src/shape/cone_shape.c
    src/shape/frustum_shape.c
    src/shape/plane_shape.c
    src/shape/sphere_shape.c
    src/math/math_funcs.c
    src/math/vec2.c
    src/math/vec3.c
    src/math/vec4.c
    src/math/mat3.c
    src/math/mat4.c
    src/render/renderer.c
    src/render/model_renderer.c
    src/render/texture_manager.c
    src/render/font_manager.c
    src/render/debug_drawer.c
    src/render/widget_renderer.c
    src/render/widget_renderer.h
    src/render/particle_renderer.c
    src/render/particle_renderer.h
    src/render/render_data_array.c
    src/render/render_data_array.h
    src/render/gpu_section.h
    src/render/gpu_time_section.h
    src/thread/thread.c
    src/thread/mutex.c
    src/input/mouse_button.c
    src/input/keyboard_button.c
    src/game/camera.c
    src/game/model.c
    src/game/skeleton.c
    src/game/scene_animation.c
    src/game/particle_emitter.c
    src/widget/widget.c
    src/widget/text_widget.c
    src/widget/rect_widget.c
    src/widget/button_widget.c
    src/widget/text_edit_widget.c
    src/widget/slider_widget.c
    src/widget/checkbox_widget.c
    src/widget/progress_widget.c
    src/widget/vbox_widget.c
    include/math/math_funcs.h
    include/math/vec2.h
    include/math/vec3.h
    include/math/vec4.h
    include/math/mat3.h
    include/math/mat4.h
    include/window.h
    include/debug_console.h
    include/type_database.h
    include/shape/plane_shape.h
    include/shape/sphere_shape.h
    include/shape/cone_shape.h
    include/shape/aabb_shape.h
    include/shape/frustum_shape.h
    include/input/keyboard_button.h
    include/input/mouse_button.h
    include/input/gamepad_button.h
    include/io/log.h
    include/io/paths.h
    include/io/filesystem.h
    include/io/config.h
    include/misc/globals.h
    include/misc/memory_usage.h
    include/misc/wchar_funcs.h
    include/misc/mesh_generator.h
    include/misc/high_freq_timer.h
    include/misc/num_hashtable.h
    include/thread/thread.h
    include/thread/mutex.h
    include/game_manager.h
    include/sound_manager.h
    include/render/renderer.h
    include/render/texture_manager.h
    include/render/font_manager.h
    include/render/debug_drawer.h
    include/render/model_renderer.h
    include/world.h
    include/game/camera.h
    include/game/model.h
    include/game/game_object_info.h
    include/game/skeleton.h
    include/game/scene_animation.h
    include/game/particle_emitter.h
    include/widget/widget.h
    include/widget/text_widget.h
    include/widget/rect_widget.h
    include/widget/button_widget.h
    include/widget/text_edit_widget.h
    include/widget/slider_widget.h
    include/widget/checkbox_widget.h
    include/widget/progress_widget.h
    include/widget/vbox_widget.h
)
set(WIN32_SOURCES
    src/window_system/win32_window.c
    src/window_system/win32_window.h
    src/thread/win32_thread.c
    src/thread/win32_thread.h
    src/thread/win32_mutex.c
    src/thread/win32_mutex.h
    src/sound/win32_audio_system.c
    src/sound/win32_audio_system.h
)
set(LINUX_SOURCES
    src/window_system/x11_window.c
    src/window_system/x11_window.h
    src/thread/posix_thread.c
    src/thread/posix_thread.h
    src/thread/posix_mutex.c
    src/thread/posix_mutex.h
    src/sound/alsa_audio_system.c
    src/sound/alsa_audio_system.h
)
if (WIN32)
    list(APPEND PROJECT_SOURCES ${WIN32_SOURCES})
else()
    list(APPEND PROJECT_SOURCES ${LINUX_SOURCES})
endif()

# Define targets (one without any specific macros and another for compilation with editor).
set(ENGINE_LIB_TARGET engine_lib)
set(ENGINE_LIB_TARGET_FOR_EDITOR engine_lib_for_editor)
add_library(${ENGINE_LIB_TARGET} STATIC ${PROJECT_SOURCES})
add_library(${ENGINE_LIB_TARGET_FOR_EDITOR} STATIC ${PROJECT_SOURCES})

# -------------------------------------------------------------------------------------------------
#                                         CONFIGURE TARGET
# -------------------------------------------------------------------------------------------------

tiny_engine_configure_target(${ENGINE_LIB_TARGET})
tiny_engine_configure_target(${ENGINE_LIB_TARGET_FOR_EDITOR})

target_include_directories(${ENGINE_LIB_TARGET} PUBLIC other)
target_include_directories(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC other)

# Define editor macros for editor's version of the engine_lib.
target_compile_definitions(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC ENGINE_EDITOR)

if (IS_RELEASE_BUILD)
    # The editor needs debug drawing and debug console even in release builds.
    target_compile_definitions(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC ENGINE_DEBUG_TOOLS)
endif()

# -------------------------------------------------------------------------------------------------
#                                       DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# Setup GLAD library.
set(GLAD_TARGET_NAME glad)
set(GLAD_PATH ${RELATIVE_EXT_PATH}/glad)
add_library(${GLAD_TARGET_NAME} STATIC
    ${GLAD_PATH}/src/gl.c
    ${GLAD_PATH}/include/KHR/khrplatform.h
    ${GLAD_PATH}/include/glad/gl.h
)
if(NOT MSVC)
    target_sources(${GLAD_TARGET_NAME} PRIVATE
        ${GLAD_PATH}/src/glx.c
        ${GLAD_PATH}/include/glad/glx.h
    )
endif()

# we have 2 versions of the engine_lib one for the editor (with some editor macros) and one for the game
# this function will add dependencies to both versions (we will call this function twice)
function(add_engine_lib_dependencies TARGET_NAME)
    if(NOT MSVC)
        # for our custom windowing system
        find_package(X11 REQUIRED COMPONENTS Xrandr Xfixes)
        target_link_libraries(${TARGET_NAME} PUBLIC
            X11::X11
            X11::Xrandr
            X11::Xfixes
        )

	# for sound
	find_package(ALSA REQUIRED)
	target_link_libraries(${TARGET_NAME} PUBLIC
	    ALSA::ALSA
	)

        # for math
        target_link_libraries(${TARGET_NAME} PUBLIC m)

        # for threads
        find_package(Threads REQUIRED)
        target_link_libraries(${TARGET_NAME} ${CMAKE_THREAD_LIBS_INIT})
        if(THREADS_HAVE_PTHREAD_ARG)
            target_compile_options(${TARGET_NAME} PUBLIC "-pthread")
        endif()
    endif()

    # Add GLAD.
    target_include_directories(${GLAD_TARGET_NAME} SYSTEM PUBLIC ${GLAD_PATH}/include)
    target_link_libraries(${TARGET_NAME} PUBLIC ${GLAD_TARGET_NAME})
    add_dependencies(${TARGET_NAME} ${GLAD_TARGET_NAME})
endfunction()

add_engine_lib_dependencies(${ENGINE_LIB_TARGET})
add_engine_lib_dependencies(${ENGINE_LIB_TARGET_FOR_EDITOR})
