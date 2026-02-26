cmake_minimum_required(VERSION 3.10)

project(tiny_engine C)

option(ENGINE_FORCE_ENABLE_DEBUG_TOOLS "Can be useful in Release builds to measure performance, enabled in non-release builds by default" OFF)

# -------------------------------------------------------------------------------------------------
#                              PREPARE FOR ADDING ENGINE TARGETS
# -------------------------------------------------------------------------------------------------

# Prepare variable for build type.
if(CMAKE_BUILD_TYPE MATCHES "^[Rr]elease")
    set(IS_RELEASE_BUILD ON CACHE BOOL "" FORCE)
else()
    set(IS_RELEASE_BUILD OFF CACHE BOOL "" FORCE)
endif()

# Prepare some variables.
set(BUILD_DIRECTORY_NAME OUTPUT)
set(DEPENDENCY_BUILD_DIR_NAME dep)
set(RELATIVE_EXT_PATH "../../ext") # relative to a subdirectory target (from the src directory)

# Define a function to configure engine targets (i.e. common configuration for targets).
function(tiny_engine_configure_target TARGET_NAME)
    # Set C standard.
    set_property(TARGET ${TARGET_NAME} PROPERTY C_STANDARD 99)
    set_property(TARGET ${TARGET_NAME} PROPERTY C_STANDARD_REQUIRED ON)

    # More warnings.
    if(MSVC)
        target_compile_options(${TARGET_NAME} PUBLIC /W3 /WX /wd4996) # disable "unsafe function" warning
    else()
        target_compile_options(${TARGET_NAME} PUBLIC -Wall -Wextra -Werror -Wconversion -Wno-unused-but-set-parameter -Wno-incompatible-function-pointer-types)
    endif()

    # Add `DEBUG` macro in debug builds.
    if(NOT IS_RELEASE_BUILD)
	message(STATUS "${PROJECT_NAME}: adding DEBUG macro")
	target_compile_definitions(${TARGET_NAME} PUBLIC DEBUG)
    endif()

    # Debug tools.
    if(NOT IS_RELEASE_BUILD)
	target_compile_definitions(${TARGET_NAME} PUBLIC ENGINE_DEBUG_TOOLS)
    elseif(ENGINE_FORCE_ENABLE_DEBUG_TOOLS)
	message(WARNING "Note: you enabled ENGINE_DEBUG_TOOLS in release build.")
	target_compile_definitions(${TARGET_NAME} PUBLIC ENGINE_DEBUG_TOOLS)
    endif()

    # Add includes (relative to the target's directory).
    target_include_directories(${TARGET_NAME} SYSTEM PUBLIC "../../ext")
    target_include_directories(${TARGET_NAME} PUBLIC include)

    # Enable ASan.
    if(NOT IS_RELEASE_BUILD AND NOT MSVC)
	message(STATUS "${TARGET_NAME}: Address sanitizer is enabled.")
	target_compile_definitions(${TARGET_NAME} PRIVATE ENGINE_ASAN_ENABLED)
	target_compile_options(${TARGET_NAME} PRIVATE -fno-omit-frame-pointer)
	target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=address)
	target_link_libraries(${TARGET_NAME} PRIVATE -fno-omit-frame-pointer)
	target_link_libraries(${TARGET_NAME} PRIVATE -fsanitize=address)
	if (CMAKE_COMPILER_IS_GNUCC)
	    target_compile_options(${TARGET_NAME} PRIVATE -static-libasan)
	endif()
    endif()
endfunction()

function(create_symlink_to_res DIR_NAME)
    set(PROJ_BIN_DIR ${CMAKE_BINARY_DIR}/${BUILD_DIRECTORY_NAME}/${DIR_NAME})
    set(RES_SYMLINK_PATH ${CMAKE_BINARY_DIR}/${BUILD_DIRECTORY_NAME}/${DIR_NAME}/res)

    if(NOT EXISTS ${PROJ_BIN_DIR})
        file(MAKE_DIRECTORY ${PROJ_BIN_DIR})
    endif()
    
    if(NOT EXISTS ${RES_SYMLINK_PATH})
        message(STATUS "Creating symlink to `res` directory at ${RES_SYMLINK_PATH}")
        file(CREATE_LINK
            ${CMAKE_CURRENT_LIST_DIR}/../../res
            ${RES_SYMLINK_PATH}
            SYMBOLIC
        )
    endif()
endfunction()

# -------------------------------------------------------------------------------------------------
#                                       ADD ENGINE TARGETS
# -------------------------------------------------------------------------------------------------

# Add engine_lib.
set(ENGINE_LIB_DIR engine_lib)
message(STATUS "Adding ${ENGINE_LIB_DIR}...")
add_subdirectory(src/${ENGINE_LIB_DIR} ${BUILD_DIRECTORY_NAME}/${ENGINE_LIB_DIR})

# Add editor.
set(EDITOR_TARGET_DIR_NAME editor)
message(STATUS "Adding ${EDITOR_TARGET_DIR_NAME}...")
add_subdirectory(src/${EDITOR_TARGET_DIR_NAME} ${BUILD_DIRECTORY_NAME}/${EDITOR_TARGET_DIR_NAME})

