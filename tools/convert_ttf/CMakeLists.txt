cmake_minimum_required(VERSION 3.13)

project(convert_ttf C)

# Prepare variable for build type.
if(CMAKE_BUILD_TYPE MATCHES "^[Rr]elease")
    set(IS_RELEASE_BUILD ON CACHE BOOL "" FORCE)
else()
    set(IS_RELEASE_BUILD OFF CACHE BOOL "" FORCE)
endif()

# Prepare some variables.
set(BUILD_DIRECTORY_NAME OUTPUT)
set(DEPENDENCY_BUILD_DIR_NAME dep)
set(RELATIVE_EXT_PATH "../../ext")

# Enable memcheck.
if(NOT IS_RELEASE_BUILD AND NOT ENGINE_DISABLE_MEMCHECK_IN_DEBUG)
    # Memcheck is our silly little memory leak checker. It can be used in cases where ASan is not available or if it uses too much RAM.
    set(ENGINE_MEMCHECK_ENABLED ON CACHE BOOL "" FORCE)
    message(STATUS "Memcheck enabled.")
    if(NOT MSVC)
        add_link_options(LINKER:--wrap=malloc,--wrap=realloc,--wrap=calloc,--wrap=free,--wrap=strdup,--wrap=wcsdup,--wrap=scandir,--wrap=getline,--wrap=realpath)
    endif()
    add_subdirectory(../../ext/memcheck ${DEPENDENCY_BUILD_DIR_NAME}/memcheck EXCLUDE_FROM_ALL)
    link_libraries(memcheck)
    if(NOT MSVC)
        link_libraries(m)
    endif()
else()
    set(ENGINE_MEMCHECK_ENABLED OFF CACHE BOOL "" FORCE)
endif()

add_executable(${PROJECT_NAME} src/main.c)

target_compile_definitions(${PROJECT_NAME} PRIVATE PATH_TO_CHARS_TO_LOAD="${CMAKE_CURRENT_LIST_DIR}/font/chars_to_load.txt")
target_compile_definitions(${PROJECT_NAME} PRIVATE PATH_TO_TTF="${CMAKE_CURRENT_LIST_DIR}/font/font.ttf")
target_compile_definitions(${PROJECT_NAME} PRIVATE PATH_TO_BITMAP="${CMAKE_CURRENT_LIST_DIR}/font/font_bitmap.bin")

# More warnings.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /W3 /WX /permissive- /wd4996 /wd4113)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	target_compile_options(${PROJECT_NAME} PUBLIC /wd4090)
    endif()
else()
    target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Werror -Wpedantic --ansi -Wconversion -Wno-unused-but-set-parameter -Wno-incompatible-function-pointer-types)
    if(CMAKE_COMPILER_IS_GNUCC)
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-incompatible-pointer-types -Wno-unused-result -Wno-unknown-warning-option)
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-incompatible-pointer-types -Wno-unknown-warning-option)
    endif()
endif()

# Add includes.
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "../../ext")
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Enable ASan.
if(NOT IS_RELEASE_BUILD AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4 AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER MATCHES "clang"))
    message(STATUS "${PROJECT_NAME}: Address sanitizer is enabled.")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENGINE_ASAN_ENABLED)
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    target_link_libraries(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer)
    target_link_libraries(${PROJECT_NAME} PRIVATE -fsanitize=address)
    if (CMAKE_COMPILER_IS_GNUCC)
        target_compile_options(${PROJECT_NAME} PRIVATE -static-libasan)
    endif()
else()
	if (NOT IS_RELEASE_BUILD AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT MSVC)
	    # 64-bit debug non-msvc build, asan should be used but for some reason is disabled, make it more noticable
	    message(WARNING "${PROJECT_NAME}: Address sanitizer is disabled.")
	else()
	    message(STATUS "${PROJECT_NAME}: Address sanitizer is disabled.")
	endif()
endif()

# Memcheck macro.
if(ENGINE_MEMCHECK_ENABLED)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ENGINE_MEMCHECK_ENABLED)
endif()

message(STATUS "${PROJECT_NAME}: adding external dependency \"FreeType\"...")
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/freetype ${DEPENDENCY_BUILD_DIR_NAME}/freetype)
target_link_libraries(${PROJECT_NAME} PRIVATE freetype)
