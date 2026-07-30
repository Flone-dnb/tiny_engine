project(game C)

# -------------------------------------------------------------------------------------------------
#                                        DEFINE TARGET
# -------------------------------------------------------------------------------------------------

set(PROJECT_SOURCES
    src/main.c
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

tiny_engine_configure_target(${PROJECT_NAME})

create_symlink_to_res(${PROJECT_NAME})

# -------------------------------------------------------------------------------------------------
#                                         DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# Add game lib.
set(GAME_LIB_TARGET game_lib)
target_link_libraries(${PROJECT_NAME} PUBLIC ${GAME_LIB_TARGET})
add_dependencies(${PROJECT_NAME} ${GAME_LIB_TARGET})
