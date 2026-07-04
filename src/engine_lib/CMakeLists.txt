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
    src/game_manager.c
    src/sound_manager.c
    src/world.c
    src/debug_console.c
    src/type_database.c
    src/render/renderer.c
    src/render/model_renderer.c
    src/render/shader_manager.c
    src/render/texture_manager.c
    src/render/font_manager.c
    src/render/debug_drawer.c
    src/render/widget_renderer.c
    src/render/widget_renderer.h
    src/render/gpu_section.h
    src/render/gpu_time_section.h
    src/game/camera.c
    src/game/model.c
    src/game/skeleton.c
    src/widget/widget.c
    src/widget/text_widget.c
    src/widget/rect_widget.c
    src/widget/button_widget.c
    src/widget/text_edit_widget.c
    src/widget/slider_widget.c
    src/widget/checkbox_widget.c
    src/widget/progress_widget.c
    src/widget/vbox_widget.c
    include/math_funcs.h
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

# Define editor macros for editor's version of the engine_lib.
target_compile_definitions(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC ENGINE_EDITOR)

if (IS_RELEASE_BUILD)
    # The editor needs debug drawing and debug console even in release builds.
    target_compile_definitions(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC ENGINE_DEBUG_TOOLS)
endif()

# -------------------------------------------------------------------------------------------------
#                                       DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# Group all dependencies into a single interface library, later we will link it to 2 version of the engine_lib
# one for the editor (with some editor macros) and one for the game.
set(ENGINE_LIB_DEPS_TARGET engine_lib_deps)
add_library(${ENGINE_LIB_DEPS_TARGET} INTERFACE)

# Add SDL.
message(STATUS "${PROJECT_NAME}: adding dependency \"SDL\"...")
if(ENGINE_MEMCHECK_ENABLED)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
else()
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
endif()
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)
target_include_directories(${ENGINE_LIB_DEPS_TARGET} SYSTEM INTERFACE ${RELATIVE_EXT_PATH}/SDL/include)
add_subdirectory(${RELATIVE_EXT_PATH}/SDL ${DEPENDENCY_BUILD_DIR_NAME}/SDL EXCLUDE_FROM_ALL)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE SDL3::SDL3)
if(ENGINE_MEMCHECK_ENABLED)
    target_compile_definitions(SDL3-shared PRIVATE ENGINE_MEMCHECK_ENABLED)
endif()

# Add GLAD.
message(STATUS "${PROJECT_NAME}: adding dependency \"GLAD\"...")
set(GLAD_TARGET_NAME glad)
add_library(${GLAD_TARGET_NAME} STATIC
    ${RELATIVE_EXT_PATH}/glad/src/glad.c
    ${RELATIVE_EXT_PATH}/glad/include/KHR/khrplatform.h
    ${RELATIVE_EXT_PATH}/glad/include/glad/glad.h
)
target_include_directories(${GLAD_TARGET_NAME} SYSTEM PUBLIC ${RELATIVE_EXT_PATH}/glad/include)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE ${GLAD_TARGET_NAME})
add_dependencies(${ENGINE_LIB_DEPS_TARGET} ${GLAD_TARGET_NAME})

# Add cglm.
message(STATUS "${PROJECT_NAME}: adding dependency \"cglm\"...")
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC ON CACHE BOOL "" FORCE)
set(CGLM_USE_C99 ON CACHE BOOL "" FORCE)
set(CGLM_USE_TEST OFF CACHE BOOL "" FORCE)
set(CGLM_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/cglm ${DEPENDENCY_BUILD_DIR_NAME}/cglm EXCLUDE_FROM_ALL)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE cglm_headers)
target_compile_definitions(cglm_headers INTERFACE CGLM_ALL_UNALIGNED)

# Add FreeType.
message(STATUS "${PROJECT_NAME}: adding external dependency \"FreeType\"...")
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/freetype ${DEPENDENCY_BUILD_DIR_NAME}/freetype EXCLUDE_FROM_ALL)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE freetype)

# Add hashmap.c
message(STATUS "${PROJECT_NAME}: adding external dependency \"hashmap.c\"...")
add_library(hashmapc ${RELATIVE_EXT_PATH}/hashmap.c/hashmap.h ${RELATIVE_EXT_PATH}/hashmap.c/hashmap.c)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE hashmapc)

# Miniaudio.
message(STATUS "${PROJECT_NAME}: adding external dependency \"miniaudio\"...")
set(MINIAUDIO_NO_WEBAUDIO ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_ENCODING ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_FLAC ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_AVX2 ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_RUNTIME_LINKING OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/miniaudio ${DEPENDENCY_BUILD_DIR_NAME}/miniaudio EXCLUDE_FROM_ALL)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE miniaudio)

# Link engine library to dependencies.
target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC ${ENGINE_LIB_DEPS_TARGET})
target_link_libraries(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC ${ENGINE_LIB_DEPS_TARGET})
