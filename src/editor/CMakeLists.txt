project(editor C)

# -------------------------------------------------------------------------------------------------
#                                        DEFINE TARGET
# -------------------------------------------------------------------------------------------------

set(PROJECT_SOURCES
    src/main.c
    src/editor.c
    include/editor.h
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

tiny_engine_configure_target(${PROJECT_NAME})

create_symlink_to_res(${PROJECT_NAME})

# -------------------------------------------------------------------------------------------------
#                                         DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# Add engine library.
set(ENGINE_LIB_TARGET_FOR_EDITOR engine_lib_for_editor)
target_link_libraries(${PROJECT_NAME} PUBLIC ${ENGINE_LIB_TARGET_FOR_EDITOR})
add_dependencies(${PROJECT_NAME} ${ENGINE_LIB_TARGET_FOR_EDITOR})

