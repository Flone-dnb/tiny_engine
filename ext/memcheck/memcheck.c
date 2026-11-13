#include "memcheck.h"

#if defined(WIN32)

#if defined(_MSC_VER) && _MSC_VER > 1200
#include <stdio.h>
#include <Windows.h>
#include <crtdbg.h>

int __cdecl crt_report_hook(int reportType, char* message, int* returnValue) {
    printf("%s", message);
    OutputDebugStringA(message);
    return TRUE;
}
#endif

void
memcheck_init(void) {
#if defined(_MSC_VER) && _MSC_VER > 1200
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, crt_report_hook);
    _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
#endif
}

void
memcheck_deinit(void) {
#if defined(_MSC_VER) && _MSC_VER > 1200
    _CrtDumpMemoryLeaks();
#endif
}

#else

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <execinfo.h>
#include <mem_ht.h>

/* stdbool.h */
#define bool unsigned char
#define true 1
#define false 0

extern void* __real_malloc(size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);
extern void* __real_realloc(void* ptr, size_t size);
extern void __real_free(void* ptr);
extern char* __real_strdup(const char* s);
extern wchar_t* __real_wcsdup(const wchar_t* s);
extern ssize_t __real_getline(char** lineptr, size_t* n, FILE* stream);
extern int
__real_scandir(const char* dirp, struct dirent*** namelist, void* filter, void* compar);
extern char* __real_realpath(const char* name, char* resolved);

typedef struct te_memcheck_mem_info {
    char* caller1;
    char* caller2;
    size_t size;
    uintptr_t user_ptr_value;
} te_memcheck_mem_info;

static pthread_mutex_t mutex;
static struct mem_ht* memhashmap = NULL;
static bool is_disabled = true;

static void
deinit_mem_info(void* ptr) {
    te_memcheck_mem_info* info = ptr;
    __real_free(info->caller1);
    __real_free(info->caller2);
    free(info);
}

void
memcheck_init(void) {
    is_disabled = true;

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("failed to init mutex");
        abort();
    }

    memhashmap = mem_ht_create(200000, sizeof(te_memcheck_mem_info), deinit_mem_info);

    is_disabled = false;
}

void
memcheck_deinit(void) {
    bool found_leaks;

    is_disabled = true;

    found_leaks = mem_ht_get_item_count(memhashmap) > 0;
    if (found_leaks) {
        mem_ht_iterator* it;
        void* item;
        size_t num;

        printf("\nMEMORY LEAKS DETECTED !!!\n\n");

        it = mem_ht_iterator_create(memhashmap);

        num = 1;
        item = mem_ht_iterator_next(it);
        while (item != NULL) {
            const te_memcheck_mem_info* info = item;
            printf("%zu. leaked a pointer of size %zu bytes\n", num, info->size);
            if (info->caller1 != NULL) {
                printf("%s <- here\n", info->caller1);
            }
            if (info->caller2 != NULL) {
                printf("%s <- symbol from above was called from here\n", info->caller2);
            }
            num += 1;

            item = mem_ht_iterator_next(it);
        }

        mem_ht_iterator_destroy(it);

        printf("\nMEMORY LEAKS DETECTED !!!\n");
    } else {
        printf("no memory leaks detected\n");
    }

    mem_ht_destroy(memhashmap);
    memhashmap = NULL;

    pthread_mutex_destroy(&mutex);

    is_disabled = false;
}

static void
memcheck_register_ptr_locked(void* ptr, size_t size) {
    void* entries[8];
    int count;

    te_memcheck_mem_info info;
    info.user_ptr_value = (uintptr_t)ptr;
    info.size = size;
    info.caller1 = NULL;
    info.caller2 = NULL;

    count = backtrace(entries, 8);
    if (count >= 5) {
        char** symbols = backtrace_symbols(entries, count);

        char* src1 = symbols[3];
        char* src2 = symbols[4];

        size_t len = strlen(src1);
        info.caller1 = __real_malloc(sizeof(char) * (len + 1));
        memcpy(info.caller1, src1, len);
        info.caller1[len] = 0;

        len = strlen(src2);
        info.caller2 = __real_malloc(sizeof(char) * (len + 1));
        memcpy(info.caller2, src2, len);
        info.caller2[len] = 0;

        __real_free(symbols);
    }

    is_disabled = true; /* because hashmap can resize here */
    if (mem_ht_insert(memhashmap, info.user_ptr_value, &info) == 0) {
        printf("memcheck ERROR: already have this pointer\n");
    }
    is_disabled = false;
}

static void
memcheck_unregister_ptr_locked(void* ptr) {
    te_memcheck_mem_info* info;

    te_memcheck_mem_info test_info;
    test_info.user_ptr_value = (uintptr_t)ptr;

    info = mem_ht_find(memhashmap, (uintptr_t)ptr);
    if (info == NULL) {
        printf(
            "memcheck ERROR: unknown pointer specified in free, either it was allocated using "
            "a "
            "special function (which is not \"wrapped\") or it's a double-free happening\n");
    }

    is_disabled = true; /* because hashmap can resize here */
    mem_ht_erase(memhashmap, (uintptr_t)ptr);
    is_disabled = false;
}

void*
__wrap_malloc(size_t size) {
    void* ptr;

    if (is_disabled) {
        return __real_malloc(size);
    }

    pthread_mutex_lock(&mutex);
    ptr = NULL;
    {
        ptr = __real_malloc(size);
        if (ptr == NULL) {
            abort();
        }

        memcheck_register_ptr_locked(ptr, size);
    }
    pthread_mutex_unlock(&mutex);

    return ptr;
}

void*
__wrap_calloc(size_t num, size_t size) {
    void* ptr;

    if (is_disabled) {
        return __real_calloc(num, size);
    }

    ptr = NULL;
    pthread_mutex_lock(&mutex);
    {
        ptr = __real_calloc(num, size);
        if (ptr == NULL) {
            abort();
        }

        memcheck_register_ptr_locked(ptr, size);
    }
    pthread_mutex_unlock(&mutex);
    return ptr;
}

void*
__wrap_realloc(void* ptr, size_t size) {
    void* new_ptr;

    if (is_disabled) {
        return __real_realloc(ptr, size);
    }

    if (ptr == NULL) {
        return __wrap_malloc(size);
    }

    pthread_mutex_lock(&mutex);
    new_ptr = NULL;
    {
        te_memcheck_mem_info* found_info;

        te_memcheck_mem_info test_info;
        test_info.user_ptr_value = (uintptr_t)ptr;

        found_info = mem_ht_find(memhashmap, (uintptr_t)ptr);
        if (found_info == NULL) {
            printf("memcheck ERROR: unknown pointer specified in realloc\n");
        }

        new_ptr = __real_realloc(ptr, size);
        if (new_ptr == NULL) {
            abort();
        }

        memcheck_unregister_ptr_locked(ptr);
        memcheck_register_ptr_locked(new_ptr, size);
    }
    pthread_mutex_unlock(&mutex);

    return new_ptr;
}

void
__wrap_free(void* ptr) {
    if (is_disabled) {
        __real_free(ptr);
        return;
    }

    if (ptr == NULL) {
        return;
    }

    pthread_mutex_lock(&mutex);
    {
        memcheck_unregister_ptr_locked(ptr);
        __real_free(ptr);
    }
    pthread_mutex_unlock(&mutex);
}

char*
__wrap_strdup(const char* s) {
    char* ptr = __real_strdup(s);
    if (ptr == NULL) {
        abort();
    }

    pthread_mutex_lock(&mutex);
    {
        memcheck_register_ptr_locked(ptr, strlen(s) + 1);
    }
    pthread_mutex_unlock(&mutex);

    return ptr;
}

wchar_t*
__wrap_wcsdup(const wchar_t* s) {
    wchar_t* ptr = __real_wcsdup(s);

    pthread_mutex_lock(&mutex);
    {
        memcheck_register_ptr_locked(ptr, (wcslen(s) + 1) * sizeof(wchar_t));
    }
    pthread_mutex_unlock(&mutex);

    return ptr;
}

int
__wrap_scandir(const char* dirp, struct dirent*** namelist, void* filter, void* compar) {
    int count = __real_scandir(dirp, namelist, filter, compar);
    if (count <= 0) {
        return count;
    }

    pthread_mutex_lock(&mutex);
    {
        int i;
        struct dirent** entries = *namelist;
        memcheck_register_ptr_locked(entries, (size_t)count * sizeof(struct dirent*));
        for (i = 0; i < count; i++) {
            memcheck_register_ptr_locked(entries[i], sizeof(struct dirent));
        }
    }
    pthread_mutex_unlock(&mutex);

    return count;
}

ssize_t
__wrap_getline(char** lineptr, size_t* n, FILE* stream) {
    char* line = *lineptr;
    ssize_t chars_read = __real_getline(lineptr, n, stream);

    pthread_mutex_lock(&mutex);
    {
        if (line != NULL) {
            memcheck_unregister_ptr_locked(line);
        }
        line = *lineptr;
        if (line != NULL) {
            memcheck_register_ptr_locked(line, *n + 1);
        }
    }
    pthread_mutex_unlock(&mutex);

    return chars_read;
}

char*
__wrap_realpath(const char* name, char* resolved) {
    char* out = __real_realpath(name, resolved);

    if (resolved == NULL && out != NULL) {
        pthread_mutex_lock(&mutex);

        memcheck_register_ptr_locked(out, strlen(out));

        pthread_mutex_unlock(&mutex);
    }

    return out;
}

#endif
