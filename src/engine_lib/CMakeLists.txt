project(engine_lib C)

# -------------------------------------------------------------------------------------------------
#                                          TARGET SOURCES
# -------------------------------------------------------------------------------------------------

# Sources.
set(PROJECT_SOURCES
    src/window.c
    src/io/log.c
    src/io/paths.c
    src/io/filesystem.c
    src/io/config.c
    src/io/import.c
    src/misc/globals.c
    src/misc/memory_usage.c
    src/misc/wchar_funcs.c
    src/misc/mesh_generator.c
    src/misc/high_freq_timer.c
    src/misc/num_hashtable.c
    src/game_manager.c
    src/sound_manager.c
    src/world.c
    src/debug_console.c
    src/type_database.c
    src/window_system/os_window.h
    src/window_system/os_window.c
    src/window_system/x11_window.c
    src/window_system/x11_window.h
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
    src/render/shader_manager.c
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
    include/io/import.h
    include/misc/globals.h
    include/misc/memory_usage.h
    include/misc/wchar_funcs.h
    include/misc/mesh_generator.h
    include/misc/high_freq_timer.h
    include/misc/num_hashtable.h
    include/game_manager.h
    include/sound_manager.h
    include/render/renderer.h
    include/render/shader_manager.h
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
)
if (WIN32)
    list(APPEND PROJECT_SOURCES ${WIN32_SOURCES})
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

# Group all dependencies into a single interface library, later we will link it to 2 versions of the engine_lib
# one for the editor (with some editor macros) and one for the game.
set(ENGINE_LIB_DEPS_TARGET engine_lib_deps)
add_library(${ENGINE_LIB_DEPS_TARGET} INTERFACE)

if(NOT MSVC)
    # for our custom windowing system
    find_package(X11 REQUIRED COMPONENTS Xrandr Xfixes)
    target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE
        X11::X11
        X11::Xrandr
        X11::Xfixes
    )

    # for math
    target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE m)
endif()

# Add GLAD.
message(STATUS "${PROJECT_NAME}: adding dependency \"GLAD\"...")
set(GLAD_TARGET_NAME glad)
if(ENGINE_GLES)
    set(GLAD_PATH ${RELATIVE_EXT_PATH}/glad_gles20)
else()
    set(GLAD_PATH ${RELATIVE_EXT_PATH}/glad_gl45)
endif()
add_library(${GLAD_TARGET_NAME} STATIC
    ${GLAD_PATH}/src/gl.c
    ${GLAD_PATH}/include/KHR/khrplatform.h
    ${GLAD_PATH}/include/glad/gl.h
)
if(ENGINE_GLES)
    target_sources(${GLAD_TARGET_NAME} PRIVATE
        ${GLAD_PATH}/src/egl.c
        ${GLAD_PATH}/include/glad/egl.h
    )
endif()
if(NOT MSVC)
    target_sources(${GLAD_TARGET_NAME} PRIVATE
        ${GLAD_PATH}/src/glx.c
        ${GLAD_PATH}/include/glad/glx.h
    )
endif()
target_include_directories(${GLAD_TARGET_NAME} SYSTEM PUBLIC ${GLAD_PATH}/include)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE ${GLAD_TARGET_NAME})
add_dependencies(${ENGINE_LIB_DEPS_TARGET} ${GLAD_TARGET_NAME})

# Add FreeType.
message(STATUS "${PROJECT_NAME}: adding external dependency \"FreeType\"...")
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/freetype ${DEPENDENCY_BUILD_DIR_NAME}/freetype)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE freetype)

# Miniaudio.
message(STATUS "${PROJECT_NAME}: adding external dependency \"miniaudio\"...")
set(MINIAUDIO_NO_WEBAUDIO ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_ENCODING ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_FLAC ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_AVX2 ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_RUNTIME_LINKING OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)
if(ENGINE_MEMCHECK_ENABLED AND CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT MSVC)
    set(MINIAUDIO_NO_RUNTIME_LINKING ON CACHE BOOL "" FORCE)
    set(MINIAUDIO_ENABLE_ONLY_SPECIFIC_BACKENDS ON CACHE BOOL "" FORCE)
    set(MINIAUDIO_ENABLE_NULL ON CACHE BOOL "" FORCE)
    message(WARNING "Sound is disabled on 32-bit debug builds with memcheck enabled because memcheck is unable to track some of miniaudio's allocations on old 32-bit Linux systems.")
else()
    set(MINIAUDIO_NO_RUNTIME_LINKING OFF CACHE BOOL "" FORCE)
    set(MINIAUDIO_ENABLE_ONLY_SPECIFIC_BACKENDS OFF CACHE BOOL "" FORCE)
    set(MINIAUDIO_ENABLE_NULL OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(${RELATIVE_EXT_PATH}/miniaudio ${DEPENDENCY_BUILD_DIR_NAME}/miniaudio)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE miniaudio)
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT MSVC)
    cmake_policy(SET CMP0079 NEW)
    target_link_libraries(miniaudio PUBLIC atomic)
endif()

# Link engine library to dependencies.
target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC ${ENGINE_LIB_DEPS_TARGET})
target_link_libraries(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC ${ENGINE_LIB_DEPS_TARGET})
