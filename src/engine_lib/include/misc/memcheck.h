#if defined(ENGINE_MEMCHECK_ENABLED)
#if !defined(DEBUG)
#error "memcheck should only be included in non-release builds"
#endif

#pragma once

#include <stddef.h>
#include <wchar.h>
#include <dirent.h>
#include <stdio.h>

void memcheck_init(void);
void memcheck_deinit(void);

void* __wrap_malloc(size_t size);
void* __wrap_realloc(void* ptr, size_t size);
void* __wrap_calloc(size_t num, size_t size);
void __wrap_free(void* ptr);

char* __wrap_strdup(const char* s);
wchar_t* __wrap_wcsdup(const wchar_t* s);

int __wrap_scandir(
    const char* restrict dirp, struct dirent*** restrict namelist,
    typeof(int(const struct dirent*))* filter,
    typeof(int(const struct dirent**, const struct dirent**))* compar);

ssize_t __wrap_getline(char** restrict lineptr, size_t* restrict n, FILE* restrict stream);

#endif
