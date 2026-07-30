project(game_lib C)

# -------------------------------------------------------------------------------------------------
#                                          TARGET SOURCES
# -------------------------------------------------------------------------------------------------

# Sources.
set(PROJECT_SOURCES
    src/game.c
    src/camera_controller.c
    include/game.h
    include/camera_controller.h
)

# Define targets (one without any specific macros and another for compilation with editor).
set(GAME_LIB_TARGET game_lib)
set(GAME_LIB_TARGET_FOR_EDITOR game_lib_for_editor)
add_library(${GAME_LIB_TARGET} STATIC ${PROJECT_SOURCES})
add_library(${GAME_LIB_TARGET_FOR_EDITOR} STATIC ${PROJECT_SOURCES})

# -------------------------------------------------------------------------------------------------
#                                         CONFIGURE TARGET
# -------------------------------------------------------------------------------------------------

tiny_engine_configure_target(${GAME_LIB_TARGET})
tiny_engine_configure_target(${GAME_LIB_TARGET_FOR_EDITOR})

# Define editor macros for editor's version of the game_lib.
target_compile_definitions(${GAME_LIB_TARGET_FOR_EDITOR} PUBLIC ENGINE_EDITOR)

if (IS_RELEASE_BUILD)
    # The editor needs debug drawing and debug console even in release builds.
    target_compile_definitions(${GAME_LIB_TARGET_FOR_EDITOR} PUBLIC ENGINE_DEBUG_TOOLS)
endif()

# -------------------------------------------------------------------------------------------------
#                                       DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# Group all dependencies into a single interface library, later we will link it to 2 versions of the game_lib
# one for the editor (with some editor macros) and one for the game.
set(GAME_LIB_DEPS_TARGET game_lib_deps)
add_library(${GAME_LIB_DEPS_TARGET} INTERFACE)

# Example: add XYZ.
#message(STATUS "${PROJECT_NAME}: adding dependency \"XYZ\"...")
#add_subdirectory(../../ext/xyz dep/xyz SYSTEM)
#target_link_libraries(${GAME_LIB_DEPS_TARGET} INTERFACE xyz)

# And finally add engine library.
set(ENGINE_LIB_TARGET engine_lib)
set(ENGINE_LIB_TARGET_FOR_EDITOR engine_lib_for_editor)
target_link_libraries(${GAME_LIB_TARGET} PUBLIC ${ENGINE_LIB_TARGET})
add_dependencies(${GAME_LIB_TARGET} ${ENGINE_LIB_TARGET})
target_link_libraries(${GAME_LIB_TARGET_FOR_EDITOR} PUBLIC ${ENGINE_LIB_TARGET_FOR_EDITOR})
add_dependencies(${GAME_LIB_TARGET_FOR_EDITOR} ${ENGINE_LIB_TARGET_FOR_EDITOR})

# Link to deps target.
target_link_libraries(${GAME_LIB_TARGET} PUBLIC ${GAME_LIB_DEPS_TARGET})
target_link_libraries(${GAME_LIB_TARGET_FOR_EDITOR} PUBLIC ${GAME_LIB_DEPS_TARGET})
