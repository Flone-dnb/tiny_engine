#if defined(ENGINE_MEMCHECK_ENABLED)

#include <misc/memcheck.h>

#include <stdlib.h>
#include <stdbool.h>
#include <io/log.h>
#include <string.h>
#include <pthread.h>
#include <hashmap.c/hashmap.h>

extern void* __real_malloc(size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);
extern void* __real_realloc(void* ptr, size_t size);
extern void __real_free(void* ptr);
extern char* __real_strdup(const char* s);
extern wchar_t* __real_wcsdup(const wchar_t* s);
extern ssize_t
__real_getline(char** restrict lineptr, size_t* restrict n, FILE* restrict stream);
extern int __real_scandir(
    const char* restrict dirp, struct dirent*** restrict namelist,
    typeof(int(const struct dirent*))* filter,
    typeof(int(const struct dirent**, const struct dirent**))* compar);

typedef struct te_memcheck_mem_info {
    size_t size;
    uintptr_t user_ptr_value;
} te_memcheck_mem_info;

static pthread_mutex_t mutex;
static struct hashmap* memhashmap = NULL;
static bool is_disabled = false;

static uint64_t
memcheck_mem_info_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    (void)seed0;
    (void)seed1;

    const te_memcheck_mem_info* info = item;
    return info->user_ptr_value;
}

static int
memcheck_mem_info_compare(const void* a, const void* b, void* udata) {
    (void)udata;

    const te_memcheck_mem_info* info1 = a;
    const te_memcheck_mem_info* info2 = b;

    return !(info1->user_ptr_value == info2->user_ptr_value);
}

void
memcheck_init(void) {
    is_disabled = true;

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        log_error("failed to init mutex");
        abort();
    }

    memhashmap = hashmap_new(
        sizeof(te_memcheck_mem_info), 256, 0, 0, memcheck_mem_info_hash,
        memcheck_mem_info_compare, NULL, NULL);

    is_disabled = false;
}

void
memcheck_deinit(void) {
    is_disabled = true;

    const bool found_leaks = hashmap_count(memhashmap) > 0;
    if (found_leaks) {
        log_error("");
        log_error("MEMORY LEAKS DETECTED !!!");
        log_error("");

        size_t iter = 0;
        size_t num = 1;
        void* item;
        while (hashmap_iter(memhashmap, &iter, &item)) {
            const te_memcheck_mem_info* info = item;
            log_error_fmt("%zu. leaked a pointer of size %zu bytes", num, info->size);
            num += 1;
        }

        log_error("");
        log_error("MEMORY LEAKS DETECTED !!!");
        log_error("");
    } else {
        log_info("no memory leaks detected");
    }

    hashmap_free(memhashmap);
    memhashmap = NULL;

    pthread_mutex_destroy(&mutex);

    is_disabled = false;
}

static void
memcheck_register_ptr_locked(void* ptr, size_t size) {
    te_memcheck_mem_info info;
    info.user_ptr_value = (uintptr_t)ptr;
    info.size = size;

    is_disabled = true; // because hashmap can resize here
    if (hashmap_set(memhashmap, &info) != NULL) {
        log_error("bug: already have this pointer");
        abort();
    }
    is_disabled = false;
}

static void
memcheck_unregister_ptr_locked(void* ptr) {
    te_memcheck_mem_info test_info;
    test_info.user_ptr_value = (uintptr_t)ptr;

    is_disabled = true; // because hashmap can resize here
    if (hashmap_delete(memhashmap, &test_info) == NULL) {
        log_error(
            "unknown pointer specified in free, either it was allocated using a "
            "special function (which is not \"wrapped\") or it's a double-free happening");
        abort();
    }
    is_disabled = false;
}

void*
__wrap_malloc(size_t size) {
    if (is_disabled) {
        return __real_malloc(size);
    }

    pthread_mutex_lock(&mutex);
    void* ptr = NULL;
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
    if (is_disabled) {
        return __real_calloc(num, size);
    }

    void* ptr = NULL;
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
    if (is_disabled) {
        return __real_realloc(ptr, size);
    }

    if (ptr == NULL) {
        return __wrap_malloc(size);
    }

    pthread_mutex_lock(&mutex);
    void* new_ptr = NULL;
    {
        te_memcheck_mem_info test_info;
        test_info.user_ptr_value = (uintptr_t)ptr;

        const te_memcheck_mem_info* found_info = hashmap_get(memhashmap, &test_info);
        if (found_info == NULL) {
            log_error("unknown pointer specified in realloc");
            abort();
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
__wrap_scandir(
    const char* restrict dirp, struct dirent*** restrict namelist,
    typeof(int(const struct dirent*))* filter,
    typeof(int(const struct dirent**, const struct dirent**))* compar) {
    int count = __real_scandir(dirp, namelist, filter, compar);
    if (count <= 0) {
        return count;
    }

    pthread_mutex_lock(&mutex);
    {
        struct dirent** entries = *namelist;
        memcheck_register_ptr_locked(entries, (size_t)count * sizeof(struct dirent*));
        for (int i = 0; i < count; i++) {
            memcheck_register_ptr_locked(entries[i], sizeof(struct dirent));
        }
    }
    pthread_mutex_unlock(&mutex);

    return count;
}

ssize_t
__wrap_getline(char** restrict lineptr, size_t* restrict n, FILE* restrict stream) {
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

#endif
