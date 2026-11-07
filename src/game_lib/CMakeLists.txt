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

# First define dependency libraries here (but don't link them yet), example:
#add_subdirectory(${RELATIVE_EXT_PATH}/xyz_lib ${DEPENDENCY_BUILD_DIR_NAME}/xyz_lib SYSTEM)

# Then since we have 2 versions of the game_lib (1 for game and 1 for editor) this function will add
# link dependencies for both versions:
function(add_game_dependencies TARGET_NAME)
    # Example:
    #target_link_libraries(${TARGET_NAME} PUBLIC xyz_lib)
    #add_dependencies(${TARGET_NAME} xyz_lib)
endfunction()

# Add engine library.
set(ENGINE_LIB_TARGET engine_lib)
set(ENGINE_LIB_TARGET_FOR_EDITOR engine_lib_for_editor)
target_link_libraries(${GAME_LIB_TARGET} PUBLIC ${ENGINE_LIB_TARGET})
add_dependencies(${GAME_LIB_TARGET} ${ENGINE_LIB_TARGET})
target_link_libraries(${GAME_LIB_TARGET_FOR_EDITOR} PUBLIC ${ENGINE_LIB_TARGET_FOR_EDITOR})
add_dependencies(${GAME_LIB_TARGET_FOR_EDITOR} ${ENGINE_LIB_TARGET_FOR_EDITOR})

# Link to game's dependencies.
add_game_dependencies(${GAME_LIB_TARGET})
add_game_dependencies(${GAME_LIB_TARGET_FOR_EDITOR})
