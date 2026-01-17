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
    src/misc/globals.c
    src/misc/error.c
    src/game_manager.c
    src/renderer.c
    src/world.c
    src/ecs/ecs.c
    src/ecs/transform_component.c
    src/ecs/camera_component.c
    src/ecs/camera_system.c
    include/window.h
    include/io/log.h
    include/io/paths.h
    include/io/filesystem.h
    include/misc/globals.h
    include/misc/error.h
    include/game_manager.h
    include/renderer.h
    include/world.h
    include/ecs/ecs.h
    include/ecs/transform_component.h
    include/ecs/camera_component.h
    include/ecs/camera_system.h
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
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
target_include_directories(${ENGINE_LIB_DEPS_TARGET} SYSTEM INTERFACE ${RELATIVE_EXT_PATH}/SDL/include)
add_subdirectory(${RELATIVE_EXT_PATH}/SDL ${DEPENDENCY_BUILD_DIR_NAME}/SDL EXCLUDE_FROM_ALL)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE SDL3::SDL3)

# Add GLAD.
message(STATUS "${PROJECT_NAME}: adding dependency \"GLAD\"...")
set(GLAD_TARGET_NAME glad)
add_library(${GLAD_TARGET_NAME} STATIC
    ${RELATIVE_EXT_PATH}/glad_gles20/src/glad.c
    ${RELATIVE_EXT_PATH}/glad_gles20/include/KHR/khrplatform.h
    ${RELATIVE_EXT_PATH}/glad_gles20/include/glad/glad.h
)
target_include_directories(${GLAD_TARGET_NAME} SYSTEM PUBLIC ${RELATIVE_EXT_PATH}/glad_gles20/include)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE ${GLAD_TARGET_NAME})
add_dependencies(${ENGINE_LIB_DEPS_TARGET} ${GLAD_TARGET_NAME})

# Add cglm.
message(STATUS "${PROJECT_NAME}: adding dependency \"cglm\"...")
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC ON CACHE BOOL "" FORCE)
set(CGLM_USE_C99 ON CACHE BOOL "" FORCE)
set(CGLM_USE_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/cglm ${DEPENDENCY_BUILD_DIR_NAME}/cglm EXCLUDE_FROM_ALL)
target_link_libraries(${ENGINE_LIB_DEPS_TARGET} INTERFACE cglm_headers)
target_compile_definitions(cglm_headers INTERFACE CGLM_ALL_UNALIGNED)

# Link engine library to dependencies.
target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC ${ENGINE_LIB_DEPS_TARGET})
target_link_libraries(${ENGINE_LIB_TARGET_FOR_EDITOR} PUBLIC ${ENGINE_LIB_DEPS_TARGET})
