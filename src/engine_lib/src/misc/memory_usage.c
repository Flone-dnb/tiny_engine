#include "misc/memory_usage.h"

#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define NOMINMAX
#include <psapi.h>
#include <windows.h>
#elif defined(__linux__)
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>
#endif

size_t
memory_usage_get_process_used_memory(void) {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.WorkingSetSize;
#elif defined(__linux__)
    long rss = 0;
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp == NULL) {
        return (size_t)0;
    }
    if (fscanf(fp, "%*s%ld", &rss) != 1) {
        fclose(fp);
        return (size_t)0;
    }
    fclose(fp);
    return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);

#else
#error "unsupported OS"
#endif
}

size_t
memory_usage_get_total_memory(void) {
#if defined(_WIN32)
    MEMORYSTATUSEX mem_info{};
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);
    return (size_t)mem_info.ullTotalPhys;
#elif defined(__linux__)
    struct sysinfo mem_info;
    sysinfo(&mem_info);
    unsigned long total_phys_mem = mem_info.totalram;
    total_phys_mem *= mem_info.mem_unit;
    return (size_t)total_phys_mem;
#else
#error "unsupported OS"
#endif
}

size_t
memory_usage_get_total_used_memory(void) {
#if defined(_WIN32)
    MEMORYSTATUSEX mem_info{};
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);
    return (size_t)(mem_info.ullTotalPhys - mem_info.ullAvailPhys);
#elif defined(__linux__)
    struct sysinfo mem_info;
    sysinfo(&mem_info);

    // sysinfo.freeram is not what it means thus we use a different approach here:

    FILE* fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return 0;
    }

    char* line = NULL;
    size_t len = 0;
    const char* key = "MemAvailable:";
    const size_t key_len = strlen(key);
    unsigned long freeram = 0;
    while (getline(&line, &len, fp) != -1) {
        if (len < key_len) {
            continue;
        }
        if (strncmp(line, key, key_len) != 0) {
            continue;
        }

        unsigned int start_pos = 0xffffffff;
        unsigned int char_count = 0;
        for (unsigned i = (unsigned int)key_len; i < len; i++) {
            if (line[i] == ' ') {
                if (start_pos == 0xffffffff) {
                    continue;
                } else {
                    break;
                }
            }
            if (start_pos == 0xffffffff) {
                start_pos = i;
            }
            char_count += 1;
        }

        if (start_pos == 0xffffffff) {
            free(line);
            fclose(fp);
            return 0;
        }

        line[start_pos + char_count] = 0;
        freeram = (unsigned int)strtoul(line + start_pos, NULL, 10);
        freeram *= 1024;
        break;
    }
    free(line);
    fclose(fp);

    unsigned long phys_mem_used = mem_info.totalram - freeram;
    phys_mem_used *= mem_info.mem_unit;
    return (size_t)phys_mem_used;
#else
#error "unsupported OS"
#endif
}
