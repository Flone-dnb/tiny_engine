cmake_minimum_required(VERSION 2.8.12...3.10)

project(memcheck C)

# on Linux define this before "add_subdirectory" to this project
#add_link_options(LINKER:--wrap=malloc,--wrap=realloc,--wrap=calloc,--wrap=free,--wrap=strdup,--wrap=wcsdup,--wrap=scandir,--wrap=getline,--wrap=realpath)

set(LIBRARY_TYPE SHARED)
if(MSVC)
    set(LIBRARY_TYPE STATIC)
endif()

add_library(${PROJECT_NAME} ${LIBRARY_TYPE} memcheck.h memcheck.c mem_ht.c mem_ht.h)
target_include_directories(${PROJECT_NAME} PUBLIC .)

# Add 32-bit macro.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MEMCHECK_32BIT)
endif()

if(NOT MSVC)
    # Add pthread
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
    if(THREADS_HAVE_PTHREAD_ARG)
        target_compile_options(${PROJECT_NAME} PUBLIC "-pthread")
    endif()
endif()
