#pragma once

#if defined(WIN32)

void memcheck_init(void);
void memcheck_deinit(void);

#else

#define _POSIX_C_SOURCE 200809L /* for ssize_t */
#include <stddef.h>
#include <wchar.h>
#include <sys/types.h>
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

int __wrap_scandir(const char* dirp, struct dirent*** namelist, void* filter, void* compar);

ssize_t __wrap_getline(char** lineptr, size_t* n, FILE* stream);

char* __wrap_realpath(const char* name, char* resolved);

#endif
