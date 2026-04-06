project(editor C)

# -------------------------------------------------------------------------------------------------
#                                        DEFINE TARGET
# -------------------------------------------------------------------------------------------------

set(PROJECT_SOURCES
    src/main.c
    src/editor.c
    src/editor_camera.c
    src/editor.h
    src/editor_camera.h
    src/obj_picking.c
    src/obj_picking.h
    src/ui/editor_ui.c
    src/ui/editor_ui.h
    src/ui/theme.h
    src/ui/world_inspector.c
    src/ui/world_inspector.h
    src/ui/property_inspector.c
    src/ui/property_inspector.h
    src/ui/filesystem_view.c
    src/ui/filesystem_view.h
    src/ui/file_dialog.c
    src/ui/file_dialog.h
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

