#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#define NOMINMAX
#include <direct.h>
#include <windows.h>
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD) - 1)
#endif
#include <tchar.h>
#define mkdir(dir, mode) _mkdir(dir)
#if defined(DEBUG) && defined(_MSC_VER) && _MSC_VER > 1200
#include <crtdbg.h>
#endif
#elif defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#else
#error "unsupported OS"
#endif

#define bool unsigned char
#define true 1
#define false 0

#if defined(_MSC_VER)

#if _MSC_VER >= 1500
#define HAVE_VSNPRINTF
#endif

#if _MSC_VER >= 1900
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF
#endif

#elif defined(__linux__)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF
#elif defined(_BSD_SOURCE) || defined(_GNU_SOURCE) || defined(_DEFAULT_SOURCE)
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF
#endif

#endif

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)

#include <stdarg.h>
#include <stdio.h>

#if !defined(HAVE_VSNPRINTF)
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
#endif

#if !defined(HAVE_SNPRINTF)
int snprintf(char* str, size_t size, const char* format, ...);
#endif

#endif

#define BUILD_FILE "build.tb"
#define CACHED_BUILD_CONFIG_FILE ".cached_build_config.txt"

typedef struct tb_user_function {
    char* name;
    char* code;

    unsigned int name_len;
    unsigned int code_count;
    unsigned int code_capacity;

    /* used when called */
    unsigned int pos;
} tb_user_function;

/* global build configuration */
typedef struct tb_config {
    const char* cc;
    const char* link;
    const char* ar;

    /* /D or -D */
    const char* macro_flag;

    /* /I or -I */
    const char* include_flag;

    const char* cc_flags;
    const char* ld_flags;
    const char* ar_flags;

    const char* obj_ext;

    /* NULL if all */
    const char* target_to_build;

    /* always a valid path (ends with a slash if not just "") to prefix all paths */
    const char* curr_build_prefix_have_slash;

    tb_user_function* user_functions;

    /* not NULL if between create_function / end_function */
    tb_user_function* creating_user_function;
    tb_user_function* inside_user_function;

    bool (*repeat_func)(char* args, unsigned int args_len);

    unsigned int user_function_count;

    bool is_32bit;
    bool is_release;
    bool verbose;
} tb_config;

enum tb_target_type {
    TB_TT_EXE = 0, /* don't change order of existing elements because we save values on disk */
    TB_TT_LIB,
    TB_TT_DLL
};

/* information of a single project (executable or library) to compile */
typedef struct tb_target_info {
    char* name;

    /* NULL if nothing to define, otherwise compiler specific string like
     * "/DMYDEF1 /DMYDEF2" */
    char* macros;

    /* NULL if nothing to include, otherwise compiler specific string like
     * "/IMYDEF1 /IMYDEF2" */
    char* private_includes;
    char* public_includes;

    /* NULL if not specified, otherwise compiler flags separated by a space */
    char* additional_ld_flags;

    char** sources;

    char** headers;

    /* targets that expose their includes to this target and this target also links to them (LIB or DLL)
     * only direct dependencies are specified here (not recursively) */
    struct tb_target_info** dependencies;

    unsigned int name_len;
    unsigned int source_count;
    unsigned int header_count;
    unsigned int dependency_count;

    unsigned int sources_capacity;
    unsigned int headers_capacity;

    enum tb_target_type type;

    /* used during target building */
    bool recompile_all_sources;
    bool built;
    bool was_outdated;
    bool headers_outdated;
} tb_target_info;

/* variable used during build configuration */
typedef struct tb_variable {
    char* name;
    unsigned int name_len;

    /* NULL if defined but has no value */
    char* value;
} tb_variable;

/* function used during build configuration */
typedef struct tb_function {
    char* name;
    unsigned int name_len;

    /* returns `false` if an error occurred */
    bool (*process)(char* args, unsigned int args_len);
} tb_function;

static tb_config config;

static tb_target_info** targets;
static unsigned int target_count;

static tb_target_info* current_target;

static tb_variable* variables;
static unsigned int variable_count;

static tb_function* functions;
static unsigned int function_count;

static bool have_variable(const char* name);
static void add_variable(const char* name, const char* value);
static void add_function(const char* name, bool (*process)(char* args, unsigned int args_len));

static tb_target_info* get_target_info(const char* name, bool create_new_if_didnt_exist);

static bool build_target(tb_target_info* target);
static void check_cached_build_outdated(
    tb_target_info* target, const char* target_dir, unsigned int target_dir_len,
    const char* ldflags, bool* recompile_all_sources, bool* ldflags_changed,
    bool* headers_changed);

static bool read_line(char** lineptr, size_t* len, FILE* stream);
static bool read_line_with_user_func(char** lineptr, size_t* len, FILE* stream);

static void filesystem_create_directory(const char* path);
static char* filesystem_append_path(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    unsigned int* ret_strlen);
static char* filesystem_append_path_ext(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    const char* extension, unsigned int extension_len, unsigned int* ret_strlen);
static const char*
filesystem_find_filename(const char* path, bool include_extension, unsigned int* ret_len);
static bool filesystem_is_src_newer_than_obj(const char* path_to_src, const char* path_to_obj);
static bool filesystem_does_path_exists(const char* path);
static void filesystem_ensure_dirs_exist(const char* file_path);
static void filesystem_remove_file(const char* path);

static int system_cmd(const char* cmd);

static bool process_build_file(const char* prefix_path);

static bool parse_path_list(
    char* text, unsigned int text_len,
    bool (*on_item_found)(char* text, unsigned int text_len));
static void add_header_file(char* relative_path, unsigned int path_len);

static bool func_target(char* text, unsigned int text_len);
static bool func_add_macro(char* text, unsigned int text_len);
static bool func_add_private_includes(char* text, unsigned int text_len);
static bool func_add_public_includes(char* text, unsigned int text_len);
static bool func_add_sources(char* text, unsigned int text_len);
static bool func_set_target_type(char* text, unsigned int text_len);
static bool func_add_linker_flags(char* text, unsigned int text_len);
static bool func_dump_state(char* text, unsigned int text_len);
static bool func_info(char* text, unsigned int text_len);
static bool func_warn(char* text, unsigned int text_len);
static bool func_error(char* text, unsigned int text_len);
static bool func_add_subproject(char* text, unsigned int text_len);
static bool func_link_target(char* text, unsigned int text_len);
static bool func_unlink_target(char* text, unsigned int text_len);
static bool func_create_symlink(char* text, unsigned int text_len);
static bool func_create_function(char* text, unsigned int text_len);
static bool func_duplicate_target(char* text, unsigned int text_len);

static bool append_user_function(char* text, unsigned int text_len);

#if defined(WIN32) && defined(DEBUG) && defined(_MSC_VER) && _MSC_VER > 1200
int __cdecl crt_report_hook(int reportType, char* message, int* returnValue) {
    printf("- %s", message);
    OutputDebugStringA(message);
    return TRUE;
}
#endif

int
main(int argc, char* argv[]) {
    FILE* build_file;
    unsigned int i;

#if defined(WIN32) && defined(DEBUG) && defined(_MSC_VER) && _MSC_VER > 1200
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, crt_report_hook);
    _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
#endif

    /* check that build file exists here */
    build_file = fopen(BUILD_FILE, "r");
    if (build_file == NULL) {
        printf("error: the file %s was not found, exiting\n", BUILD_FILE);
        return 1;
    }
    fclose(build_file);

    filesystem_create_directory("build");

#if defined(WIN32)
    config.cc = "cl.exe";
    config.link = "link.exe";
    config.ar = "lib.exe";
    config.macro_flag = "/D";
    config.include_flag = "/I";
    config.cc_flags = NULL; /* will fill later */
    config.ld_flags = NULL;
    config.ar_flags = NULL;
    config.obj_ext = ".obj";
#else
    config.cc = "clang";
    config.link = "clang";
    config.ar = "ar rcs";
    config.macro_flag = "-D";
    config.include_flag = "-I";
    config.cc_flags = NULL; /* will fill later */
    config.ld_flags = NULL;
    config.ar_flags = NULL;
    config.obj_ext = ".o";
#endif

    config.curr_build_prefix_have_slash = "";

    config.user_functions = NULL;
    config.creating_user_function = NULL;
    config.inside_user_function = NULL;
    config.user_function_count = 0;

    config.target_to_build = NULL;
    config.is_release = false;
    config.is_32bit = false;
    config.verbose = false;

    if (argc > 1) {
        int args_idx = 1;

        if (args_idx != argc && strcmp(argv[args_idx], "-v") == 0) {
            config.verbose = true;
            args_idx += 1;
        }

        if (args_idx != argc && strcmp(argv[args_idx], "CC") == 0) {
            args_idx += 1;
            if (args_idx == argc) {
                printf("expected compiler to be specified\n");
                return 1;
            }
            config.cc = argv[args_idx];
            if (strcmp(config.cc, "gcc") == 0 || strcmp(config.cc, "clang") == 0) {
                config.link = config.cc;
            }
            args_idx += 1;
        }

        if (args_idx != argc) {
            config.target_to_build = argv[args_idx];
            args_idx += 1;
        }

        if (args_idx != argc) {
            if (strcmp(argv[args_idx], "debug") == 0) {
                config.is_release = false;
            } else if (strcmp(argv[args_idx], "release") == 0) {
                config.is_release = true;
            } else {
                printf("error: unknown build mode: %s\n", argv[args_idx]);
                return 1;
            }
            args_idx += 1;
        }

        if (args_idx != argc) {
            for (; args_idx != argc; args_idx++) {
                add_variable(argv[args_idx], NULL);
            }
        }
    }

    /* detect environment */
    {
        int sizeof_voidp = 4;
#if defined(WIN32)
        FILE* fp = fopen("build/arch_test.exe", "rb");
        if (fp == NULL) {
            if (system_cmd("cd build && echo int main(void){ return "
                           "sizeof(void*); } > arch_test.c && cl /nologo arch_test.c && del "
                           "arch_test.c && del arch_test.obj")
                != 0) {
                printf("failed to detect the environment\n");
                return 1;
            }
        } else {
            fclose(fp);
        }
        sizeof_voidp = system_cmd("build\\arch_test.exe");
#elif defined(__linux__)
        FILE* fp = fopen("build/arch_test", "rb");
        if (fp == NULL) {
            if (system_cmd("cd build && echo \"int main(void){ return "
                           "sizeof(void*); }\" > arch_test.c && gcc arch_test.c -o "
                           "arch_test && rm arch_test.c")
                != 0) {
                printf("failed to detect the environment\n");
                return 1;
            }
        } else {
            fclose(fp);
        }
        sizeof_voidp = system_cmd("./build/arch_test");
#endif
        if (sizeof_voidp == 8) {
            config.is_32bit = false;
        } else if (sizeof_voidp == 4) {
            config.is_32bit = true;
        } else {
            printf("unknown architecture, sizeof(void*) is %i\n", sizeof_voidp);
            return 1;
        }
    }

    /* print environment */
    if (config.is_32bit) {
        printf("- detected 32-bit environment\n");
    } else {
        printf("- detected 64-bit environment\n");
    }

    /* print mode */
    if (config.is_release) {
        printf("- building in RELEASE mode\n");
    } else {
        printf("- building in DEBUG mode\n");
    }

    /* prepare default compiler flags */
    if (strcmp(config.cc, "cl.exe") == 0) {
#define CL_CC_FLAGS "/c /D_CRT_SECURE_NO_WARNINGS /TC /W3 /WX /nologo /DWIN32 /D_WINDOWS /EHsc"
#define CL_LD_FLAGS "/nologo"
        if (config.is_release) {
            config.cc_flags = CL_CC_FLAGS " /O2 /Ob2 /MD";
            config.ld_flags = CL_LD_FLAGS " /INCREMENTAL:NO";
        } else {
            config.cc_flags = CL_CC_FLAGS " /Od /Ob0 /Zi /MDd /DDEBUG";
            config.ld_flags = CL_LD_FLAGS " /DEBUG";
        }
        config.ar_flags = "/nologo /WX";
    } else if (strcmp(config.cc, "gcc") == 0 || strcmp(config.cc, "clang") == 0) {
#define GCC_CC_FLAGS                                                                          \
    "-c -pthread -Wall -Wextra -Werror -Wpedantic -ansi -std=c90 -Wconversion "               \
    "-Wno-unused-but-set-parameter -Wno-incompatible-function-pointer-types "                 \
    "-Wno-incompatible-pointer-types -Wno-unused-result "                                     \
    "-Wno-unknown-warning-option"
#define GCC_LD_FLAGS "-pthread -Wl,-rpath,'$ORIGIN'"
        if (config.is_release) {
            config.cc_flags = "-O3 -DNDEBUG " GCC_CC_FLAGS;
            config.ld_flags = " " GCC_LD_FLAGS;
        } else {
            config.cc_flags = "-g3 -O0 -DDEBUG " GCC_CC_FLAGS;
            if (!config.is_32bit) {
                config.ld_flags = "-fno-omit-frame-pointer -fsanitize=address "
                                  "-static-libasan " GCC_LD_FLAGS;
            } else {
                config.ld_flags = " " GCC_LD_FLAGS;
            }
        }
    } else {
        printf("- unsupported compiler: %s", config.cc);
        return 1;
    }

    /* prepare built-in variables */
    if (config.is_32bit) {
        add_variable("SYSTEM_32BIT", NULL);
    } else {
        add_variable("SYSTEM_64BIT", NULL);
    }
    if (config.is_release) {
        add_variable("BUILD_RELEASE", NULL);
    } else {
        add_variable("BUILD_DEBUG", NULL);
    }
#if defined(WIN32)
    add_variable("OS_WINDOWS", NULL);
#endif
#if defined(__linux__)
    add_variable("OS_LINUX", NULL);
#endif
    if (strcmp(config.cc, "cl.exe") == 0) {
        add_variable("COMPILER_MSVC", NULL);
    } else if (strcmp(config.cc, "gcc") == 0) {
        add_variable("COMPILER_GCC", NULL);
    } else if (strcmp(config.cc, "clang") == 0) {
        add_variable("COMPILER_CLANG", NULL);
    } else {
        printf("- unsupported compiler: %s", config.cc);
        return 1;
    }

    /* prepare built-in functions */
    add_function("target", func_target);
    add_function("set_target_type", func_set_target_type);
    add_function("add_macro", func_add_macro);
    add_function("add_private_includes", func_add_private_includes);
    add_function("add_public_includes", func_add_public_includes);
    add_function("add_sources", func_add_sources);
    add_function("add_linker_flags", func_add_linker_flags);
    add_function("info", func_info);
    add_function("warn", func_warn);
    add_function("error", func_error);
    add_function("add_subproject", func_add_subproject);
    add_function("link_target", func_link_target);
    add_function("unlink_target", func_unlink_target);
    add_function("create_symlink", func_create_symlink);
    add_function("create_function", func_create_function);
    add_function("duplicate_target", func_duplicate_target);
    add_function("dump_state", func_dump_state);

    current_target = NULL;

    if (!process_build_file("")) {
        return 1;
    }

    if (target_count == 0) {
        printf("- not a single target found\n");
    } else {
        /* find requested target */
        if (config.target_to_build == NULL || strcmp(config.target_to_build, "all") == 0) {
            for (i = 0; i < target_count; i++) {
                if (!build_target(targets[i])) {
                    return 1;
                }
            }
        } else {
            bool found = false;
            for (i = 0; i < target_count; i++) {
                if (strcmp(config.target_to_build, targets[i]->name) != 0) {
                    continue;
                }
                if (!build_target(targets[i])) {
                    return 1;
                }
                found = true;
                break;
            }
            if (!found) {
                printf("- error: can't find target \"%s\" to build\n", config.target_to_build);
                return 1;
            }
        }
    }

    /* cleanup */
    for (i = 0; i < target_count; i++) {
        unsigned int j;

        free(targets[i]->name);
        free(targets[i]->macros);
        free(targets[i]->private_includes);
        free(targets[i]->public_includes);
        free(targets[i]->additional_ld_flags);
        for (j = 0; j < targets[i]->source_count; j++) {
            free(targets[i]->sources[j]);
        }
        free(targets[i]->sources);
        for (j = 0; j < targets[i]->header_count; j++) {
            free(targets[i]->headers[j]);
        }
        free(targets[i]->dependencies);
        free(targets[i]->headers);
        free(targets[i]);
    }
    free(targets);

    for (i = 0; i < variable_count; i++) {
        free(variables[i].name);
        free(variables[i].value);
    }
    free(variables);

    for (i = 0; i < function_count; i++) {
        free(functions[i].name);
    }
    free(functions);

    for (i = 0; i < config.user_function_count; i++) {
        free(config.user_functions[i].name);
        free(config.user_functions[i].code);
    }
    free(config.user_functions);

#if defined(WIN32) && defined(DEBUG) && defined(_MSC_VER) && _MSC_VER > 1200
    printf("dumping memory leak report:\n");
    _CrtDumpMemoryLeaks();
    printf("end of report\n");
#endif

    return 0;
}

typedef struct tb_branch {
    bool is_inside_true_branch;
    bool branch_state;
} tb_branch;

static bool
process_build_file(const char* prefix_path) {
    FILE* build_file;
    char* line;
    char* build_prefix_path_have_slash;
    tb_branch branches[4];
    size_t line_len;
    size_t start_idx, prev_start_idx;
    unsigned int i, j;
    unsigned int line_num;
    int branch_idx = -1;

    /* prepare prefix */
    {
        char* path;
        size_t len = strlen(prefix_path);
        if (len == 0) {
            build_prefix_path_have_slash = malloc(sizeof(char));
            build_prefix_path_have_slash[0] = 0;
        } else {
            if (prefix_path[len - 1] == '/' || prefix_path[len - 1] == '\\') {
                build_prefix_path_have_slash = malloc(sizeof(char) * (len + 1));
                memcpy(build_prefix_path_have_slash, prefix_path, sizeof(char) * len);
                build_prefix_path_have_slash[len] = 0;
            } else {
                build_prefix_path_have_slash = malloc(sizeof(char) * (len + 2));
                memcpy(build_prefix_path_have_slash, prefix_path, sizeof(char) * len);
                build_prefix_path_have_slash[len] = '/';
                build_prefix_path_have_slash[len + 1] = 0;
            }
        }

        /* open build file */
        if (len == 0) {
            build_file = fopen(BUILD_FILE, "r");
            if (build_file == NULL) {
                printf("error: the file \"%s\" was not found, exiting\n", BUILD_FILE);
                return false;
            }
        } else {
            path = filesystem_append_path(
                build_prefix_path_have_slash, (unsigned int)len, BUILD_FILE, 0, NULL);
            build_file = fopen(path, "r");
            if (build_file == NULL) {
                printf("error: the file \"%s\" was not found, exiting\n", path);
                return false;
            }
            free(path);

            printf("- processing directory %s\n", prefix_path);
        }
    }

    config.repeat_func = NULL;
    config.inside_user_function = NULL;

    line = NULL;
    line_num = 0;

    while (read_line_with_user_func(&line, &line_len, build_file)) {
        line_num += 1;
        if (line_len == 0) {
            continue;
        }

        /* skip any starting garbage */
        start_idx = 0;
        for (; start_idx < line_len; start_idx++) {
            if (line[start_idx] == ' ' || line[start_idx] == '\t' || line[start_idx] == '\n'
                || line[start_idx] == '\r') {
                continue;
            }
            break;
        }
        if (start_idx == line_len) {
            continue;
        }

        if (line[start_idx] == '#') {
            /* a line with a comment */
            continue;
        }

        /* skip any trailing garbage */
        for (; line_len > 1 && line_len > start_idx; line_len--) {
            if (line[line_len - 1] == ' ' || line[line_len - 1] == '\t'
                || line[line_len - 1] == '\n' || line[line_len - 1] == '\r') {
                line[line_len - 1] = 0;
                continue;
            }
            break;
        }

        if (config.creating_user_function != NULL) {
            if (strcmp("end_function()", line) == 0) {
                config.creating_user_function = NULL;
            } else {
                append_user_function(line, (unsigned int)line_len);
            }
            continue;
        }

        /* set every cycle because can be changed inside add_subproject */
        config.curr_build_prefix_have_slash = build_prefix_path_have_slash;

        if (config.repeat_func != NULL) {
            size_t len = 0;
            bool found_end = false;
            /* find `)` */
            for (j = (unsigned int)start_idx; j < line_len; j++) {
                if (line[j] == ')') {
                    found_end = true;
                    break;
                }
                if (line[j] == '\n' || line[j] == '\r') {
                    break;
                }
                len += 1;
            }
            if (len > 0) {
                if (!config.repeat_func(line + start_idx, (unsigned int)len)) {
                    printf("error %s on line %u: function failed\n", prefix_path, line_num);
                    return false;
                }
            }
            if (found_end) {
                config.repeat_func = NULL;
            }
            continue;
        }

        if ((line_len - start_idx) >= 4 && strncmp(line + start_idx, "if ", 3) == 0) {
            size_t var_len = 0;

            if (branch_idx == 3) {
                printf("reached nested branching limit on line %u\n", line_num);
                return false;
            }
            branch_idx += 1;
            branches[branch_idx].is_inside_true_branch = true;

            start_idx += 3;

            /* read until space or end of line */
            for (i = (unsigned int)start_idx; i < line_len; i++) {
                if (line[i] == ' ') {
                    break;
                }
                var_len += 1;
            }

            if (i == line_len) {
                /* checking a defined variable */
                branches[branch_idx].branch_state = have_variable(line + start_idx) == true;
            } else {
                /* complex condition */
                printf(
                    "error on %s line %u: unexpected complex condition\n", prefix_path,
                    line_num);
                return false;
            }
            continue;
        } else if ((line_len - start_idx) >= 4 && strncmp(line + start_idx, "else", 4) == 0) {
            branches[branch_idx].is_inside_true_branch = false;
            continue;
        } else if ((line_len - start_idx) >= 5 && strncmp(line + start_idx, "endif", 5) == 0) {
            if (branch_idx == -1) {
                printf("branch ifdef/endif mismatch reached on line %u\n", line_num);
                return false;
            }
            branch_idx -= 1;
            continue;
        }

        if (branch_idx >= 0) {
            int idx = 0;
            bool skip_line = false;
            for (idx = 0; idx <= branch_idx; idx++) {
                if (branches[idx].is_inside_true_branch) {
                    if (!branches[idx].branch_state) {
                        skip_line = true;
                        break;
                    }
                } else {
                    if (branches[idx].branch_state) {
                        skip_line = true;
                        break;
                    }
                }
            }
            if (skip_line) {
                continue;
            }
        }

        /* maybe a function, try reading until ( */
        prev_start_idx = start_idx;
        for (; start_idx < line_len; start_idx++) {
            if (line[start_idx] == '(') {
                break;
            }
        }
        if (line[start_idx] == '(') {
            bool found = false;
            size_t len = start_idx - prev_start_idx;
            if (len == 0) {
                printf(
                    "error on %s line %u: expected a function name before `(`\n", prefix_path,
                    line_num);
                return false;
            }
            for (i = 0; i < function_count; i++) {
                if (functions[i].name_len != len) {
                    continue;
                }
                if (strncmp(functions[i].name, line + prev_start_idx, len) != 0) {
                    continue;
                }
                found = true;

                /* find `)` */
                len = 0;
                for (j = (unsigned int)start_idx + 1; j < line_len; j++) {
                    if (line[j] == ')') {
                        line[j] = 0;
                        break;
                    }
                    if (line[j] == '\n' || line[j] == '\r') {
                        line[j] = 0;
                        break;
                    }
                    len += 1;
                }
                if (len == 0) {
                    /* no arguments */
                    if (!functions[i].process(NULL, 0)) {
                        printf(
                            "error on %s line %u: function failed\n", prefix_path, line_num);
                        return false;
                    }
                } else {
                    if (!functions[i].process(line + start_idx + 1, (unsigned int)len)) {
                        printf(
                            "error on %s line %u: function failed\n", prefix_path, line_num);
                        return false;
                    }
                }
                break;
            }
            if (!found) {
                /* maybe it's a user function */
                for (i = 0; i < config.user_function_count; i++) {
                    if (config.user_functions[i].name_len != len) {
                        continue;
                    }
                    if (strncmp(config.user_functions[i].name, line + prev_start_idx, len)
                        != 0) {
                        continue;
                    }
                    found = true;
                    /* currently user functions don't have arguments so no need to process further */
                    config.inside_user_function = &config.user_functions[i];
                    config.inside_user_function->pos = 0;
                    break;
                }
            }
            if (!found) {
                line[start_idx] = 0;
                printf(
                    "error on %s line %u: unable to find function \"%s\"\n", prefix_path,
                    line_num, line + prev_start_idx);
                return false;
            } else {
                continue;
            }
        } else {
            start_idx = prev_start_idx;
        }

        /* variable operation, read until space */
        prev_start_idx = start_idx;
        for (; start_idx < line_len; start_idx++) {
            if (line[start_idx] == ' ') {
                break;
            }
        }
        if (start_idx > prev_start_idx) {
            size_t name_len = start_idx - prev_start_idx;

            /* search for = */
            for (; start_idx < line_len; start_idx++) {
                if (line[start_idx] == '=') {
                    break;
                }
            }
            if (line[start_idx] == '=') {
                size_t value_len = 0;

                start_idx += 2;
                if (start_idx >= line_len) {
                    printf(
                        "error on %s line %u: expected to find variable value after `=`\n",
                        prefix_path, line_num);
                    return false;
                }
                value_len = line_len - start_idx;

                /* variable with value */
                line[prev_start_idx + name_len] = 0;
                add_variable(line + prev_start_idx, line + start_idx);
            } else {
                /* simple defined variable */
                line[prev_start_idx + name_len] = 0;
                add_variable(line + prev_start_idx, NULL);
            }
        }
    }
    free(line);

    fclose(build_file);
    free(build_prefix_path_have_slash);

    return true;
}

static bool
func_target(char* text, unsigned int text_len) {
    text[text_len] = 0;
    current_target = get_target_info(text, true);
    return true;
}

static bool
func_add_macro(char* text, unsigned int text_len) {
    char* macros;
    size_t flag_len, curr_idx, i;

    if (current_target == NULL) {
        printf("no current target to add macro to\n");
        return false;
    }

    flag_len = strlen(config.macro_flag);
    curr_idx = current_target->macros == NULL ? 0 : strlen(current_target->macros);

    macros = (char*)malloc(
        sizeof(char) * ((curr_idx > 0 ? (curr_idx + 1) : 0) + flag_len + text_len + 1));
    memcpy(macros, current_target->macros, sizeof(char) * curr_idx);

    free(current_target->macros);
    current_target->macros = macros;

    if (curr_idx > 0) {
        current_target->macros[curr_idx] = ' ';
        curr_idx += 1;
    }

    for (i = 0; i < flag_len; i++) {
        current_target->macros[curr_idx] = config.macro_flag[i];
        curr_idx += 1;
    }

    memcpy(current_target->macros + curr_idx, text, sizeof(char) * text_len);
    curr_idx += text_len;

    current_target->macros[curr_idx] = 0;

    return true;
}

static bool
func_add_private_includes_internal(char* text, unsigned int text_len) {
    char* includes;
    size_t flag_len, curr_idx, i;

    flag_len = strlen(config.include_flag);
    curr_idx = current_target->private_includes == NULL
                   ? 0
                   : strlen(current_target->private_includes);

    includes = (char*)malloc(
        sizeof(char) * ((curr_idx > 0 ? (curr_idx + 1) : 0) + flag_len + text_len + 1));
    memcpy(includes, current_target->private_includes, sizeof(char) * curr_idx);

    free(current_target->private_includes);
    current_target->private_includes = includes;

    if (curr_idx > 0) {
        current_target->private_includes[curr_idx] = ' ';
        curr_idx += 1;
    }

    for (i = 0; i < flag_len; i++) {
        current_target->private_includes[curr_idx] = config.include_flag[i];
        curr_idx += 1;
    }

    memcpy(current_target->private_includes + curr_idx, text, sizeof(char) * text_len);
    curr_idx += text_len;

    current_target->private_includes[curr_idx] = 0;

    return true;
}

static bool
func_add_public_includes_internal(char* text, unsigned int text_len) {
    char* includes;
    size_t flag_len, curr_idx, i;

    flag_len = strlen(config.include_flag);
    curr_idx =
        current_target->public_includes == NULL ? 0 : strlen(current_target->public_includes);

    includes = (char*)malloc(
        sizeof(char) * ((curr_idx > 0 ? (curr_idx + 1) : 0) + flag_len + text_len + 1));
    memcpy(includes, current_target->public_includes, sizeof(char) * curr_idx);

    free(current_target->public_includes);
    current_target->public_includes = includes;

    if (curr_idx > 0) {
        current_target->public_includes[curr_idx] = ' ';
        curr_idx += 1;
    }

    for (i = 0; i < flag_len; i++) {
        current_target->public_includes[curr_idx] = config.include_flag[i];
        curr_idx += 1;
    }

    memcpy(current_target->public_includes + curr_idx, text, sizeof(char) * text_len);
    curr_idx += text_len;

    current_target->public_includes[curr_idx] = 0;

    return true;
}

static bool
func_add_sources_internal(char* text, unsigned int text_len) {
    char* src;

    if (text[text_len - 1] == 'h') {
        add_header_file(text, text_len);
        return true;
    } else if (text[text_len - 1] != 'c') {
        printf("- unexpected file extension on file %s\n", text);
        return false;
    }

    if (current_target->source_count == current_target->sources_capacity) {
        char** sources;

        current_target->sources_capacity += 16;

        sources = (char**)malloc(sizeof(char*) * current_target->sources_capacity);
        memcpy(sources, current_target->sources, sizeof(char*) * current_target->source_count);

        free(current_target->sources);
        current_target->sources = sources;
    }

    current_target->sources[current_target->source_count] =
        (char*)malloc(sizeof(char) * (text_len + 1));
    src = current_target->sources[current_target->source_count];

    memcpy(src, text, sizeof(char) * text_len);
    src[text_len] = 0;

    current_target->source_count += 1;

    return true;
}

static void
add_header_file(char* relative_path, unsigned int path_len) {
    char* item;

    if (current_target->header_count == current_target->headers_capacity) {
        char** headers;

        current_target->headers_capacity += 16;

        headers = (char**)malloc(sizeof(char*) * current_target->headers_capacity);
        memcpy(headers, current_target->headers, sizeof(char*) * current_target->header_count);

        free(current_target->headers);
        current_target->headers = headers;
    }

    current_target->headers[current_target->header_count] =
        (char*)malloc(sizeof(char) * (path_len + 1));
    item = current_target->headers[current_target->header_count];

    memcpy(item, relative_path, sizeof(char) * path_len);
    item[path_len] = 0;

    current_target->header_count += 1;
}

static bool
func_add_private_includes(char* text, unsigned int text_len) {
    if (text == NULL) {
        config.repeat_func = func_add_private_includes;
        return true;
    }
    return parse_path_list(text, text_len, func_add_private_includes_internal);
}

static bool
func_add_public_includes(char* text, unsigned int text_len) {
    if (text == NULL) {
        config.repeat_func = func_add_public_includes;
        return true;
    }
    return parse_path_list(text, text_len, func_add_public_includes_internal);
}

static bool
func_add_sources(char* text, unsigned int text_len) {
    if (text == NULL) {
        config.repeat_func = func_add_sources;
        return true;
    }
    return parse_path_list(text, text_len, func_add_sources_internal);
}

static bool
func_set_target_type(char* text, unsigned int text_len) {
    if (current_target == NULL) {
        printf("no current target\n");
        return false;
    }

    if (strcmp(text, "exe") == 0) {
        current_target->type = TB_TT_EXE;
    } else if (strcmp(text, "lib") == 0) {
        current_target->type = TB_TT_LIB;
    } else if (strcmp(text, "dll") == 0) {
        current_target->type = TB_TT_DLL;
    } else {
        printf("unknown target type: %s\n", text);
        return false;
    }

    return true;
}

static bool
func_add_linker_flags(char* text, unsigned int text_len) {
    char* flags;
    unsigned int old_flags_len = 0;

    if (current_target == NULL) {
        printf("no current target\n");
        return false;
    }

    if (current_target->additional_ld_flags != NULL) {
        old_flags_len = (unsigned int)strlen(current_target->additional_ld_flags);
    }

    flags =
        malloc(sizeof(char) * (old_flags_len + (old_flags_len > 0 ? 1 : 0) + text_len + 1));
    memcpy(flags, current_target->additional_ld_flags, sizeof(char) * old_flags_len);
    if (old_flags_len > 0) {
        flags[old_flags_len] = ' ';
        old_flags_len += 1;
    }
    memcpy(flags + old_flags_len, text, sizeof(char) * text_len);
    flags[old_flags_len + text_len] = 0;

    free(current_target->additional_ld_flags);
    current_target->additional_ld_flags = flags;

    return true;
}

static bool
func_info(char* text, unsigned int text_len) {
    /* skip " chars */
    text += 1;
    text_len -= 1;

    text[text_len - 1] = 0;
    printf("- info: %s\n", text);

    return true;
}

static bool
func_warn(char* text, unsigned int text_len) {
    /* skip " chars */
    text += 1;
    text_len -= 1;

    text[text_len - 1] = 0;
    printf("\n- WARNING: %s\n\n", text);

    return true;
}

static bool
func_error(char* text, unsigned int text_len) {
    /* skip " chars */
    text += 1;
    text_len -= 1;

    text[text_len - 1] = 0;
    printf("\n- ERROR: %s\n\n", text);

    return false;
}

static bool
func_add_subproject(char* text, unsigned int text_len) {
    tb_target_info* prev_target = current_target;
    char* path =
        filesystem_append_path(config.curr_build_prefix_have_slash, 0, text, text_len, NULL);

    if (!process_build_file(path)) {
        return false;
    }

    /* restore old current target */
    current_target = prev_target;

    free(path);
    return true;
}

static bool
func_link_target(char* text, unsigned int text_len) {
    tb_target_info** deps;
    tb_target_info* other_target;

    if (current_target == NULL) {
        printf("no current target to link to\n");
        return false;
    }

    other_target = get_target_info(text, false);
    if (other_target == NULL) {
        printf("unable to find target \"%s\" (is it not defined yet?)\n", text);
        return false;
    }

    deps = malloc(sizeof(tb_target_info*) * (current_target->dependency_count + 1));
    memcpy(
        deps, current_target->dependencies,
        sizeof(tb_target_info*) * current_target->dependency_count);

    free(current_target->dependencies);
    current_target->dependencies = deps;

    current_target->dependencies[current_target->dependency_count] = other_target;
    current_target->dependency_count += 1;

    return true;
}

static bool
func_unlink_target(char* text, unsigned int text_len) {
    tb_target_info* other_target;
    unsigned int i;

    if (current_target == NULL) {
        printf("no current target to unlink\n");
        return false;
    }

    other_target = get_target_info(text, false);
    if (other_target == NULL) {
        printf("unable to find target \"%s\" (is it not defined yet?)\n", text);
        return false;
    }

    for (i = 0; i < current_target->dependency_count; i++) {
        if (current_target->dependencies[i] != other_target) {
            continue;
        }

        if (current_target->dependency_count == 1) {
            free(current_target->dependencies);
            current_target->dependencies = NULL;
        } else if (i != current_target->dependency_count - 1) {
            current_target->dependencies[i] =
                current_target->dependencies[current_target->dependency_count - 1];
        }
        current_target->dependency_count -= 1;
        return true;
    }

    printf("target \"%s\" is not linked to the target \"%s\"\n", current_target->name, text);
    return false;
}

static bool
func_create_symlink(char* text, unsigned int text_len) {
    char* path = NULL;
    char* symlink = NULL;
    char* cmd = NULL;

    unsigned int i;
    for (i = 0; i < text_len; i++) {
        if (text[i] == ' ') {
            text[i] = 0;
            i += 1;
            break;
        }
    }

    if (config.curr_build_prefix_have_slash[0] != 0) {
        /* add prefix */
        path =
            filesystem_append_path(config.curr_build_prefix_have_slash, 0, text, i - 1, NULL);
    } else {
        path = malloc(sizeof(char) * (i + 1));
        memcpy(path, text, sizeof(char) * i);
        path[i] = 0;
    }

    text += i;
    if (config.curr_build_prefix_have_slash[0] != 0) {
        /* add prefix */
        symlink =
            filesystem_append_path(config.curr_build_prefix_have_slash, 0, text, 0, NULL);
    } else {
        unsigned int len = text_len - i;
        symlink = malloc(sizeof(char) * (len + 1));
        memcpy(symlink, text, sizeof(char) * len);
        symlink[len] = 0;
    }

    if (!filesystem_does_path_exists(symlink)) {
        int len;
        filesystem_ensure_dirs_exist(symlink);

#if defined(WIN32)
        len = snprintf(NULL, 0, "mklink /J \"%s\" \"%s\"", symlink, path);
        if (len <= 0) {
            printf("snprintf failed");
            return false;
        }
        cmd = malloc(sizeof(char) * (len + 1));
        snprintf(cmd, (size_t)(len + 1), "mklink /J \"%s\" \"%s\"", symlink, path);
        if (system_cmd(cmd) != 0) {
            if (config.is_32bit) {
                /* on windows xp and below it can fail because mklink does not exist */
                printf(
                    "\n- WARNING: failed to create a symlink from \"%s\" to \"%s\"\n\n", path,
                    symlink);
            } else {
                printf(
                    "\n- failed to create a symlink from \"%s\" to \"%s\"\n\n", path, symlink);
            }
            free(cmd);
            free(path);
            free(symlink);
            if (config.is_32bit) {
                return true;
            } else {
                return false;
            }
        }
#elif defined(__linux__)
        len = snprintf(NULL, 0, "ln -sfnr %s %s", path, symlink);
        if (len <= 0) {
            printf("snprintf failed");
            return false;
        }
        cmd = malloc(sizeof(char) * (len + 1));
        snprintf(cmd, (size_t)(len + 1), "ln -sfnr %s %s", path, symlink);
        len = system_cmd(cmd);
        if (len == -1 || WIFSIGNALED(len) || WEXITSTATUS(len) != 0) {
            printf(
                "failed to create a symlink from \"%s\" to \"%s\", returned code: %i", path,
                symlink, len);
            return false;
        }
#else
#error "unsupported OS"
#endif
    }

    printf("- created a symlink from \"%s\" to \"%s\"\n", path, symlink);

    free(path);
    free(symlink);
    free(cmd);

    return true;
}

static bool
func_create_function(char* text, unsigned int text_len) {
    tb_user_function* funcs;
    tb_user_function* f;

    if (config.creating_user_function != NULL) {
        printf("can't create a new function inside of a function\n");
        return false;
    }

    funcs = malloc(sizeof(tb_user_function) * (config.user_function_count + 1));
    memcpy(
        funcs, config.user_functions, sizeof(tb_user_function) * config.user_function_count);

    free(config.user_functions);
    config.user_functions = funcs;

    f = &config.user_functions[config.user_function_count];

    f->name = malloc(sizeof(char) * (text_len + 1));
    memcpy(f->name, text, sizeof(char) * text_len);
    f->name[text_len] = 0;
    f->name_len = text_len;

    f->code_count = 0;
    f->code_capacity = 256 + 1;
    f->code = malloc(sizeof(char) * f->code_capacity);
    f->code[0] = 0;

    f->pos = 0;

    config.user_function_count += 1;

    config.creating_user_function = f;

    return true;
}

static bool
func_duplicate_target(char* text, unsigned int text_len) {
    tb_target_info* old_target;
    tb_target_info* new_target;
    unsigned int i;

    for (i = 0; i < text_len; i++) {
        if (text[i] == ' ') {
            text[i] = 0;
            i += 1;
            break;
        }
    }

    old_target = get_target_info(text, false);
    if (old_target == NULL) {
        printf("unable to find target \"%s\"\n", text);
        return false;
    }

    new_target = get_target_info(text + i, false);
    if (new_target != NULL) {
        printf("target \"%s\" already exists\n", text + i);
        return false;
    }
    new_target = get_target_info(text + i, true);

    /* duplicate target */
    {
#define DUPLICATE_TARGET_STR(str)                                                             \
    if (old_target->str != NULL) {                                                            \
        size_t len = strlen(old_target->str);                                                 \
        new_target->str = malloc(sizeof(char) * (len + 1));                                   \
        memcpy(new_target->str, old_target->str, sizeof(char) * len);                         \
        new_target->str[len] = 0;                                                             \
    }
        DUPLICATE_TARGET_STR(macros)
        DUPLICATE_TARGET_STR(private_includes)
        DUPLICATE_TARGET_STR(public_includes)
        DUPLICATE_TARGET_STR(additional_ld_flags)

        new_target->type = old_target->type;

        /* copy dependencies */
        if (old_target->dependency_count > 0) {
            new_target->dependency_count = old_target->dependency_count;
            new_target->dependencies =
                malloc(sizeof(tb_target_info*) * new_target->dependency_count);
            memcpy(
                new_target->dependencies, old_target->dependencies,
                sizeof(tb_target_info*) * new_target->dependency_count);
        }

        /* copy sources */
        if (old_target->source_count > 0) {
            unsigned int i;
            new_target->source_count = old_target->source_count;
            new_target->sources_capacity = old_target->sources_capacity;
            new_target->sources = malloc(sizeof(char*) * new_target->sources_capacity);
            for (i = 0; i < new_target->source_count; i++) {
                size_t len = strlen(old_target->sources[i]);
                new_target->sources[i] = malloc(sizeof(char) * (len + 1));
                memcpy(new_target->sources[i], old_target->sources[i], sizeof(char) * len);
                new_target->sources[i][len] = 0;
            }
        }

        /* copy headers */
        if (old_target->header_count > 0) {
            unsigned int i;
            new_target->header_count = old_target->header_count;
            new_target->headers_capacity = old_target->headers_capacity;
            new_target->headers = malloc(sizeof(char*) * new_target->headers_capacity);
            for (i = 0; i < new_target->header_count; i++) {
                size_t len = strlen(old_target->headers[i]);
                new_target->headers[i] = malloc(sizeof(char) * (len + 1));
                memcpy(new_target->headers[i], old_target->headers[i], sizeof(char) * len);
                new_target->headers[i][len] = 0;
            }
        }
    }

    current_target = new_target;

    return true;
}

static bool
append_user_function(char* text, unsigned int text_len) {
    tb_user_function* f;

    if (config.creating_user_function == NULL) {
        printf("no current function\n");
        return false;
    }

    f = config.creating_user_function;
    if (f->code_count + text_len + 1 > f->code_capacity) {
        char* code;
        f->code_capacity += text_len + 1 + 256;
        code = malloc(sizeof(char) * f->code_capacity);
        memcpy(code, f->code, sizeof(char) * f->code_count);

        free(f->code);
        f->code = code;
    }

    memcpy(f->code + f->code_count, text, sizeof(char) * text_len);
    f->code[f->code_count + text_len] = 0; /* our end of line marker */
    f->code_count += text_len + 1;

    return true;
}

static bool
parse_path_list(
    char* text, unsigned int text_len,
    bool (*on_item_found)(char* text, unsigned int text_len)) {
    unsigned int processed_len;
    unsigned int i;

    if (current_target == NULL) {
        printf("no current target\n");
        return false;
    }

    /* remove any trailing spaces */
    for (i = text_len - 1; i > 1; i--) {
        if (text[i] != ' ') {
            break;
        }
        text_len -= 1;
    }

    /* check if have multiple items separated by a space */
    processed_len = 0;
    for (i = 0; i < text_len; i++) {
        if (text[i] == ' ') {
            if (config.curr_build_prefix_have_slash[0] == 0) {
                if (!on_item_found(text, i)) {
                    return false;
                }
            } else {
                /* add build prefix */
                unsigned int path_len;
                char* path = filesystem_append_path(
                    config.curr_build_prefix_have_slash, 0, text, i, &path_len);
                if (!on_item_found(path, path_len)) {
                    return false;
                }
                free(path);
            }

            text += i + 1;
            processed_len += i + 1;
        }
    }

    if (processed_len < text_len) {
        if (config.curr_build_prefix_have_slash[0] == 0) {
            if (!on_item_found(text, text_len - processed_len)) {
                return false;
            }
        } else {
            /* add build prefix */
            unsigned int path_len;
            char* path = filesystem_append_path(
                config.curr_build_prefix_have_slash, 0, text, text_len - processed_len,
                &path_len);
            if (!on_item_found(path, path_len)) {
                return false;
            }
            free(path);
        }
    }

    return true;
}

static bool
func_dump_state(char* text, unsigned int text_len) {
    if (current_target == NULL) {
        printf("- no current target to dump state\n");
    } else {
        unsigned int i;

        printf("\n- dump_state start -\n\n");

        printf("- defined variables:\n");
        for (i = 0; i < variable_count; i++) {
            printf("    - %s\n", variables[i].name);
        }

        printf("- defined functions:\n");
        for (i = 0; i < function_count; i++) {
            printf("    - %s\n", functions[i].name);
        }

        printf("- defined user functions:\n");
        for (i = 0; i < config.user_function_count; i++) {
            printf("    - %s\n", config.user_functions[i].name);
        }

        printf("- target: %s\n", current_target->name);
        printf("- macros: %s\n", current_target->macros);
        printf("- private includes: %s\n", current_target->private_includes);
        printf("- public includes: %s\n", current_target->private_includes);
        printf("- additional ld flags: %s\n", current_target->additional_ld_flags);
        printf("- sources:\n");
        for (i = 0; i < current_target->source_count; i++) {
            printf("    - %s\n", current_target->sources[i]);
        }
        printf("- headers:\n");
        for (i = 0; i < current_target->header_count; i++) {
            printf("    - %s\n", current_target->headers[i]);
        }

        printf("\n- dump_state end - \n\n");
    }

    return true;
}

static void
filesystem_create_directory(const char* path) {
    mkdir(path, 0755);
};

static char*
filesystem_append_path(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    unsigned int* ret_strlen) {
    char* out;
    unsigned int out_len;
    bool have_slash;

    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (add_len == 0) {
        add_len = (unsigned int)strlen(add);
    }

    if (path_len > 0) {
        have_slash = path[path_len - 1] == '/' || path[path_len - 1] == '\\';
    } else {
        have_slash = true;
    }

    out_len = path_len + !have_slash + add_len;
    out = malloc(sizeof(char) * (out_len + 1));

    memcpy(out, path, sizeof(char) * path_len);
#if defined(WIN32)
    memcpy(out + path_len, "\\", sizeof(char) * !have_slash);
#else
    memcpy(out + path_len, "/", sizeof(char) * !have_slash);
#endif
    memcpy(out + path_len + !have_slash, add, sizeof(char) * add_len);

    out[out_len] = 0;

    if (ret_strlen != NULL) {
        (*ret_strlen) = out_len;
    }

    return out;
}

static char*
filesystem_append_path_ext(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    const char* extension, unsigned int extension_len, unsigned int* ret_strlen) {
    char* out;
    unsigned int out_len;
    bool have_slash;

    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (add_len == 0) {
        add_len = (unsigned int)strlen(add);
    }
    if (extension_len == 0) {
        extension_len = (unsigned int)strlen(extension);
    }

    if (path_len > 0) {
        have_slash = path[path_len - 1] == '/' || path[path_len - 1] == '\\';
    } else {
        have_slash = true;
    }

    out_len = path_len + !have_slash + add_len + extension_len;
    out = malloc(sizeof(char) * (out_len + 1));

    memcpy(out, path, sizeof(char) * path_len);
#if defined(WIN32)
    memcpy(out + path_len, "\\", sizeof(char) * !have_slash);
#else
    memcpy(out + path_len, "/", sizeof(char) * !have_slash);
#endif
    memcpy(out + path_len + !have_slash, add, sizeof(char) * add_len);
    memcpy(out + path_len + !have_slash + add_len, extension, sizeof(char) * extension_len);

    out[out_len] = 0;

    if (ret_strlen != NULL) {
        (*ret_strlen) = out_len;
    }

    return out;
}

static const char*
filesystem_find_filename(const char* path, bool include_extension, unsigned int* ret_len) {
    size_t i;
    size_t idx;
    const size_t len = strlen(path);

    idx = len;
    for (i = len - 1; i > 0; i--) {
#if defined(WIN32)
        if (path[i] == '/' || path[i] == '\\') {
#else
        if (path[i] == '/') {
#endif
            idx = i + 1;
            break;
        }
    }
    if (idx >= len) {
        idx = 0;
    }

    if (include_extension) {
        if (ret_len != NULL) {
            (*ret_len) = (unsigned int)(len - idx);
        }
        return path + idx;
    }

    for (i = idx; i < len; i++) {
        if (path[i] == '.') {
            if (ret_len != NULL) {
                (*ret_len) = (unsigned int)(i - idx);
            }
            break;
        }
    }

    return path + idx;
}

static bool
filesystem_is_src_newer_than_obj(const char* path_to_src, const char* path_to_obj) {
#if defined(WIN32)
    HANDLE hf;
    FILETIME src_time, obj_time;
    BOOL success;

    hf = CreateFileA(
        path_to_src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        return true;
    }
    success = GetFileTime(hf, NULL, NULL, &src_time);
    if (!success) {
        return true;
    }
    CloseHandle(hf);

    hf = CreateFileA(
        path_to_obj, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        return true;
    }
    success = GetFileTime(hf, NULL, NULL, &obj_time);
    if (!success) {
        return true;
    }
    CloseHandle(hf);

    if (CompareFileTime(&src_time, &obj_time) == 1) {
        return true;
    }
#elif defined(__linux__)
    struct stat src_stat;
    struct stat obj_stat;
    time_t src_time;
    time_t obj_time;

    if (stat(path_to_src, &src_stat) != 0) {
        return true;
    }
    if (stat(path_to_obj, &obj_stat) != 0) {
        return true;
    }

    if (difftime(src_stat.st_mtime, obj_stat.st_mtime) > 0.0) {
        return true;
    }
#else
#error "unsupported OS"
#endif

    return false;
}

static bool
filesystem_does_path_exists(const char* path) {
#if defined(WIN32)
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        DWORD attrib = GetFileAttributesA(path);
        return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
    } else {
        fclose(fp);
        return true;
    }
#elif __linux__
    struct stat buffer;
    return stat(path, &buffer) == 0;
#else
#error "unsupported OS"
#endif
}

static void
filesystem_ensure_dirs_exist(const char* file_path) {
    const char pathSeparator = '/'; /* we convert \\ to / on windows */

    char* dir_path = malloc(strlen(file_path) + 1);

    char* next_sep = strchr(file_path, pathSeparator);
    while (next_sep != NULL) {
        const size_t dir_path_len = (size_t)(next_sep - file_path);
        memcpy(dir_path, file_path, dir_path_len);
        dir_path[dir_path_len] = 0;

        if (!filesystem_does_path_exists(dir_path)) {
            mkdir(dir_path, 0755);
        }

        next_sep = strchr(next_sep + 1, pathSeparator);
    }

    free(dir_path);
}

static void
filesystem_remove_file(const char* path) {
#if defined(WIN32)
    DeleteFileA(path);
#elif __linux__
    remove(path);
#else
#error "unsupported OS"
#endif
}

static char*
create_compile_obj_cmd_cl(
    const char* ccflags, const char* target_name, const char* target_dir,
    const char* source_file) {
    char* cmd;
    size_t len;

    const char* source_name = filesystem_find_filename(source_file, true, NULL);

#define CL_COMPILE_OBJ_FMT                                                                    \
    "%s %s %s /Fd\"%s\\%s.pdb\" /Fo\"%s\\%s%s\"", config.cc, ccflags, source_file,            \
        target_dir, target_name, target_dir, source_name, config.obj_ext

    len = snprintf(NULL, 0, CL_COMPILE_OBJ_FMT);
    if (len <= 0) {
        printf("- snprintf failed\n");
        return NULL;
    }

    cmd = malloc(sizeof(char) * (len + 1));
    snprintf(cmd, (size_t)(len + 1), CL_COMPILE_OBJ_FMT);

    return cmd;
}

static char*
create_compile_obj_cmd_gcc(
    const char* ccflags, const char* target_name, const char* target_dir,
    const char* source_file) {
    char* cmd;
    size_t len;

    const char* source_name = filesystem_find_filename(source_file, true, NULL);

#define GCC_COMPILE_OBJ_FMT                                                                   \
    "%s %s %s -o \"./%s/%s%s\"", config.cc, ccflags, source_file, target_dir, source_name,    \
        config.obj_ext

    len = snprintf(NULL, 0, GCC_COMPILE_OBJ_FMT);
    if (len <= 0) {
        printf("- snprintf failed\n");
        return NULL;
    }

    cmd = malloc(sizeof(char) * (len + 1));
    snprintf(cmd, (size_t)(len + 1), GCC_COMPILE_OBJ_FMT);

    return cmd;
}

static char*
create_compile_obj_cmd(
    const char* ccflags, const char* target_name, const char* target_dir,
    const char* source_file) {
    if (strcmp(config.cc, "cl.exe") == 0) {
        return create_compile_obj_cmd_cl(ccflags, target_name, target_dir, source_file);
    } else if (strcmp(config.cc, "gcc") == 0 || strcmp(config.cc, "clang") == 0) {
        return create_compile_obj_cmd_gcc(ccflags, target_name, target_dir, source_file);
    } else {
        printf("- unsupported compiler: %s\n", config.cc);
        return NULL;
    }
}

static bool
build_link_dependencies_cl(tb_target_info* target, char** deps) {
    char* dep_target_dir;
    char* add;
    char* d;
    size_t len;
    unsigned int dep_target_dir_len, add_len;
    unsigned int i;

    for (i = 0; i < target->dependency_count; i++) {
        if (target->dependencies[i]->type == TB_TT_EXE) {
            printf(
                "- error: target %s can't link to an executable target %s\n", target->name,
                target->dependencies[i]->name);
            return false;
        }
        if (target->dependencies[i]->type == TB_TT_DLL) {
            /* TODO */
            printf("- TODO: linking to DLLs on MSVC is not implemented yet!\n");
            return false;
        }

        dep_target_dir = filesystem_append_path(
            "build", 5, target->dependencies[i]->name, 0, &dep_target_dir_len);

        add = filesystem_append_path_ext(
            dep_target_dir, dep_target_dir_len, target->dependencies[i]->name, 0, ".lib", 4,
            &add_len);

        len = strlen(*deps);
        d = malloc(sizeof(char) * ((len > 0 ? len + 1 : 0) + add_len + 1));
        if (len > 0) {
            memcpy(d, *deps, sizeof(char) * len);
            d[len] = ' ';
            len += 1;
        }
        memcpy(d + len, add, sizeof(char) * add_len);
        d[len + add_len] = 0;

        free(*deps);
        *deps = d;

        free(add);
        free(dep_target_dir);

        // note: no need to collect libs recursively on msvc
    }

    return true;
}

static char*
create_link_cmd_cl(
    const char* ld_flags, const char* objs, const char* target_dir, tb_target_info* target) {
    char* cmd;
    char* deps;
    size_t len;

    const char* bin = config.link;
    const char* ext = ".exe";
    const char* flags = ld_flags;

    if (target->type == TB_TT_LIB) {
        bin = config.ar;
        ext = ".lib";
        flags = config.ar_flags;
    } else if (target->type == TB_TT_DLL) {
        /* TODO */
        printf("- TODO: building DLLs on MSVC is not implemented yet!\n");
        return NULL;
    }

    /* prepare dependencies to link to */
    deps = malloc(sizeof(char));
    deps[0] = 0;
    if (!build_link_dependencies_cl(target, &deps)) {
        return NULL;
    }

#define CL_LINK_OBJ_FMT                                                                       \
    "%s %s %s %s /OUT:%s\\%s%s", bin, flags, objs, deps, target_dir, target->name, ext

    len = snprintf(NULL, 0, CL_LINK_OBJ_FMT);
    if (len <= 0) {
        printf("- snprintf failed\n");
        return NULL;
    }

    cmd = malloc(sizeof(char) * (len + 1));
    snprintf(cmd, (size_t)(len + 1), CL_LINK_OBJ_FMT);

    free(deps);

    return cmd;
}

static bool
build_link_dependencies_recursive_gcc(tb_target_info* target, char** deps) {
    char* d;
    char* dep_target_dir;
    unsigned int i;
    int buf_len;

    for (i = 0; i < target->dependency_count; i++) {
        if (target->dependencies[i]->type == TB_TT_EXE) {
            printf(
                "- error: target %s can't link to an executable target %s\n", target->name,
                target->dependencies[i]->name);
            return false;
        }

        dep_target_dir =
            filesystem_append_path("build", 5, target->dependencies[i]->name, 0, NULL);

        buf_len = snprintf(
            NULL, 0, "%s -L./%s -l%s", (*deps), dep_target_dir, target->dependencies[i]->name);
        if (buf_len <= 0) {
            printf("- snprintf error while building dependency link list\n");
            return false;
        }
        d = malloc(sizeof(char) * (buf_len + 1));

        snprintf(
            d, (size_t)(buf_len + 1), "%s -L./%s -l%s", (*deps), dep_target_dir,
            target->dependencies[i]->name);
        free(*deps);
        (*deps) = d;

        free(dep_target_dir);

        build_link_dependencies_recursive_gcc(target->dependencies[i], deps);
    }

    return true;
}

static char*
create_link_cmd_gcc(
    const char* ld_flags, const char* objs, const char* target_dir, tb_target_info* target) {
    char* cmd;
    char* deps;
    size_t len;

    const char* ext = "";
    const char* prefix = "";
    if (target->type == TB_TT_DLL) {
        prefix = "lib";
        ext = ".so";
    }

    /* prepare dependencies to link to */
    deps = malloc(sizeof(char));
    deps[0] = 0;
    if (!build_link_dependencies_recursive_gcc(target, &deps)) {
        return NULL;
    }

#define GCC_LINK_OBJ_FMT                                                                      \
    "%s %s %s %s -o \"./%s/%s%s%s\"", config.link, ld_flags, objs, deps, target_dir, prefix,  \
        target->name, ext
#define GCC_LIB_OBJ_FMT "%s \"./%s/lib%s.a\" %s", config.ar, target_dir, target->name, objs

    if (target->type == TB_TT_EXE || target->type == TB_TT_DLL) {
        len = snprintf(NULL, 0, GCC_LINK_OBJ_FMT);
    } else {
        len = snprintf(NULL, 0, GCC_LIB_OBJ_FMT);
    }
    if (len <= 0) {
        printf("- snprintf failed\n");
        return NULL;
    }

    cmd = malloc(sizeof(char) * (len + 1));
    if (target->type == TB_TT_EXE || target->type == TB_TT_DLL) {
        snprintf(cmd, (size_t)(len + 1), GCC_LINK_OBJ_FMT);
    } else {
        snprintf(cmd, (size_t)(len + 1), GCC_LIB_OBJ_FMT);
    }

    free(deps);

    return cmd;
}

static char*
create_link_cmd(
    const char* ld_flags, const char* objs, const char* target_dir, tb_target_info* target) {
    if (strcmp(config.cc, "cl.exe") == 0) {
        return create_link_cmd_cl(ld_flags, objs, target_dir, target);
    } else if (strcmp(config.cc, "gcc") == 0 || strcmp(config.cc, "clang") == 0) {
        return create_link_cmd_gcc(ld_flags, objs, target_dir, target);
    } else {
        printf("- unsupported compiler: %s\n", config.cc);
        return NULL;
    }
}

static bool
build_target(tb_target_info* target) {
    char* target_dir;
    char* ccflags;
    char* ldflags;
    char* objs = NULL;
    char** sources_to_recompile = NULL;
    unsigned int target_dir_len;
    unsigned int ccflags_len, ldflags_len;
    unsigned int i;
    unsigned int source_count_to_recompile = 0;
    bool ldflags_changed = false;
    bool dependency_was_changed = false;
    bool dependency_headers_changed = false;

    if (target->source_count == 0) {
        target->built = true;
        printf("- no sources found for target %s\n", target->name);
        return true;
    }

    if (target->built) {
        return true;
    }

    /* make pointers valid */
    if (target->macros == NULL) {
        target->macros = malloc(sizeof(char));
        target->macros[0] = 0;
    }
    if (target->private_includes == NULL) {
        target->private_includes = malloc(sizeof(char));
        target->private_includes[0] = 0;
    }
    if (target->public_includes == NULL) {
        target->public_includes = malloc(sizeof(char));
        target->public_includes[0] = 0;
    }

    for (i = 0; i < target->dependency_count; i++) {
        size_t curr_len;
        size_t add_len;

        if (target->dependencies[i]->source_count == 0) {
            continue;
        }
        if (!target->dependencies[i]->built) {
            if (!build_target(target->dependencies[i])) {
                return false;
            }
        }
        if (target->dependencies[i]->was_outdated) {
            dependency_was_changed = true;
            dependency_headers_changed |= target->dependencies[i]->headers_outdated;
        }

        /* add dependency includes (it already added all of its dependency includes recursively) */
        {
            char* inc;
            curr_len = strlen(target->public_includes);
            add_len = strlen(target->dependencies[i]->public_includes);
            inc = malloc(sizeof(char) * (curr_len + 1 + add_len + 1));
            memcpy(inc, target->public_includes, sizeof(char) * curr_len);
            inc[curr_len] = ' ';
            memcpy(
                inc + curr_len + 1, target->dependencies[i]->public_includes,
                sizeof(char) * add_len);
            inc[curr_len + 1 + add_len] = 0;

            free(target->public_includes);
            target->public_includes = inc;
        }

        /* add dependency macros */
        {
            char* mac;
            curr_len = strlen(target->macros);
            add_len = strlen(target->dependencies[i]->macros);
            mac = malloc(sizeof(char) * (curr_len + 1 + add_len + 1));
            memcpy(mac, target->macros, sizeof(char) * curr_len);
            mac[curr_len] = ' ';
            memcpy(
                mac + curr_len + 1, target->dependencies[i]->macros, sizeof(char) * add_len);
            mac[curr_len + 1 + add_len] = 0;

            free(target->macros);
            target->macros = mac;
        }
    }

    printf("\n- building target %s\n", target->name);

    /* we will check the cache later */
    target->recompile_all_sources = false;
    target->was_outdated = false;
    target->headers_outdated = false;

    /* need a special directory for the target */
    target_dir =
        filesystem_append_path("build", 5, target->name, target->name_len, &target_dir_len);
    filesystem_create_directory(target_dir);

    /* prepare ccflags */
    {
        int len;

        const char* type_flags = "";
        if ((strcmp(config.cc, "gcc") == 0 || strcmp(config.cc, "clang") == 0)
            && target->type == TB_TT_DLL) {
            type_flags = "-fPIC";
        }

#define CCFLAGS_FMT                                                                           \
    "%s %s %s %s %s", config.cc_flags, type_flags, target->macros, target->private_includes,  \
        target->public_includes
        len = snprintf(NULL, 0, CCFLAGS_FMT);
        if (len <= 2) {
            printf(
                "- snprintf failed while building cc flags, base CC flags: %s\n",
                config.cc_flags);
            return false;
        }
        ccflags_len = (unsigned int)len;

        ccflags = (char*)malloc(sizeof(char) * (len + 1));
        snprintf(ccflags, (size_t)(len + 1), CCFLAGS_FMT);
    }

    /* prepare ld flags */
    {
        int len;

        const char* type_flags = "";
        if ((strcmp(config.cc, "gcc") == 0 || strcmp(config.cc, "clang") == 0)
            && target->type == TB_TT_DLL) {
            type_flags = "-shared";
        }

        if (target->additional_ld_flags == NULL) {
            target->additional_ld_flags = malloc(1);
            target->additional_ld_flags[0] = 0;
        }

        len = snprintf(
            NULL, 0, "%s %s %s", config.ld_flags, type_flags, target->additional_ld_flags);
        if (len <= 0) {
            printf("- snprintf failed\n");
            return false;
        }
        ldflags_len = (unsigned int)len;

        ldflags = (char*)malloc(sizeof(char) * (len + 1));
        snprintf(
            ldflags, (size_t)(len + 1), "%s %s %s", config.ld_flags, type_flags,
            target->additional_ld_flags);
    }

    check_cached_build_outdated(
        target, target_dir, target_dir_len, ldflags, &target->recompile_all_sources,
        &ldflags_changed, &target->headers_outdated);

    target->recompile_all_sources |= dependency_headers_changed;
    ldflags_changed |= dependency_was_changed;

    /* cache header file timestamps */
    for (i = 0; i < target->header_count; i++) {
        FILE* fp;
        char* h_path;
        const char* filename;
        unsigned int filename_len;
        char ch = 0;

        filename = filesystem_find_filename(target->headers[i], true, &filename_len);
        h_path = filesystem_append_path_ext(
            target_dir, target_dir_len, filename, filename_len, ".time", 5, NULL);

        if (filesystem_is_src_newer_than_obj(target->headers[i], h_path)) {
            target->recompile_all_sources = true;
            target->headers_outdated = true;
            /* note: don't break here we need to finish creating/updating all header timestamp files */

            fp = fopen(h_path, "wb");
            if (fp == NULL) {
                printf("- failed to create a file %s\n", h_path);
                return false;
            }
            fwrite(&ch, 1, sizeof(ch), fp);
            fclose(fp);
        }

        free(h_path);
    }

    if (!target->recompile_all_sources) {
        /* count if (and how much) object files are outdated */
        char* obj_path;
        unsigned int source_name_len;

        sources_to_recompile = malloc(sizeof(char*) * target->source_count);
        memset(sources_to_recompile, 0, sizeof(char*) * target->source_count);

        for (i = 0; i < target->source_count; i++) {
            const char* source_name =
                filesystem_find_filename(target->sources[i], true, &source_name_len);
            obj_path = filesystem_append_path_ext(
                target_dir, target_dir_len, source_name, source_name_len, config.obj_ext, 0,
                NULL);

            if (filesystem_is_src_newer_than_obj(target->sources[i], obj_path)) {
                size_t len = strlen(target->sources[i]);
                sources_to_recompile[source_count_to_recompile] =
                    malloc(sizeof(char) * (len + 1));
                memcpy(
                    sources_to_recompile[source_count_to_recompile], target->sources[i],
                    sizeof(char) * len);
                sources_to_recompile[source_count_to_recompile][len] = 0;

                source_count_to_recompile += 1;
            }

            free(obj_path);
        }
    }

    /* compile object files */
    if (target->recompile_all_sources || source_count_to_recompile > 0) {
        char** sources = target->sources;
        unsigned int src_count = target->source_count;

        if (sources_to_recompile > 0) {
            sources = sources_to_recompile;
            src_count = source_count_to_recompile;
        }
        for (i = 0; i < src_count; i++) {
            char* cmd;
            const char* source_name = filesystem_find_filename(sources[i], true, NULL);

            cmd = create_compile_obj_cmd(ccflags, target->name, target_dir, sources[i]);
            if (cmd == NULL) {
                return false;
            }

            printf("- [%s] [%u/%u] %s\n", target->name, i + 1, src_count, source_name);
            if (config.verbose) {
                printf("- [%s] %s\n", target->name, cmd);
            }
            if (system_cmd(cmd) != 0) {
                char* cache_file_path;

                printf(
                    "- [%s] compilation failed, full compilation command:\n%s\n", target->name,
                    cmd);

                /* kill cache to rebuild later */
                cache_file_path = filesystem_append_path(
                    target_dir, target_dir_len, CACHED_BUILD_CONFIG_FILE, 0, NULL);
                filesystem_remove_file(cache_file_path);
                free(cache_file_path);
                return false;
            }

            free(cmd);
        }
    }

    if (target->recompile_all_sources || source_count_to_recompile > 0 || ldflags_changed) {
        /* link object files */
        char* cmd;
        char tmp[1024 * 4];

        const char* slash = "\\";
#if !defined(WIN32)
        slash = "/";
#endif

        size_t capacity = 2048;
        size_t objs_len = 0;
        size_t len = 0;

        /* combine all object files into a single string */
        objs = malloc(sizeof(char) * capacity);
        for (i = 0; i < target->source_count; i++) {
            const char* source_name = filesystem_find_filename(target->sources[i], true, NULL);

            len = snprintf(
                tmp, 1024 * 4, "%s%s%s%s ", target_dir, slash, source_name, config.obj_ext);
            if (len <= 0) {
                printf("- snprintf failed\n");
                return false;
            }
            if (len == 1024 * 4) {
                printf("- snprintf reached tmp buffer limit\n");
                return false;
            }

            if (objs_len + len + 1 > capacity) {
                char* o;
                capacity += 2048;
                o = malloc(sizeof(char) * capacity);
                memcpy(o, objs, sizeof(char) * capacity);
                free(objs);
                objs = o;
            }

            memcpy(objs + objs_len, tmp, sizeof(char) * len);
            objs_len += len;
            objs[objs_len] = 0;
        }

        cmd = create_link_cmd(ldflags, objs, target_dir, target);
        if (cmd == NULL) {
            return false;
        }

        if (config.verbose) {
            printf("- [%s] %s\n", target->name, cmd);
        } else {
            printf("- [%s] linking the code...\n", target->name);
        }
        if (system_cmd(cmd) != 0) {
            char* cache_file_path;

            printf("- [%s] link failed, full link command:\n%s\n", target->name, cmd);

            /* kill cache to rebuild later */
            cache_file_path = filesystem_append_path(
                target_dir, target_dir_len, CACHED_BUILD_CONFIG_FILE, 0, NULL);
            filesystem_remove_file(cache_file_path);
            free(cache_file_path);
            return false;
        }

        free(cmd);
    }

    if (!target->recompile_all_sources && !ldflags_changed && source_count_to_recompile == 0) {
        printf("- nothing to rebuild for target %s, already up-to-date\n", target->name);
    } else {
        target->was_outdated = true;
        printf("- done building target %s\n", target->name);
    }

    target->built = true;

    for (i = 0; i < source_count_to_recompile; i++) {
        free(sources_to_recompile[i]);
    }
    free(sources_to_recompile);

    free(objs);
    free(ccflags);
    free(ldflags);
    free(target_dir);
    return true;
}

#if defined(WIN32)
void
print_win32_last_error(DWORD code) {
    LPSTR msg = NULL;
    DWORD size;

    size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);

    if (size > 0 && msg != NULL) {
        printf("- %s\n", msg);
        LocalFree(msg);
    } else {
        printf("- failed to get error msg from code %lu", code);
    }
}

void
prefix_cmd(const char* prefix, char** cmd, size_t cmd_len) {
    char* cmd2;
    size_t prefix_len;

    prefix_len = strlen(prefix);

    cmd2 = malloc(sizeof(char) * (prefix_len + 5 + cmd_len + 1 + 1));
    memcpy(cmd2, prefix, sizeof(char) * prefix_len);
    memcpy(cmd2 + prefix_len, " /C \"", sizeof(char) * 5);
    memcpy(cmd2 + prefix_len + 5, *cmd, sizeof(char) * cmd_len);
    cmd2[prefix_len + 5 + cmd_len] = '\"';
    cmd2[prefix_len + 5 + cmd_len + 1] = 0;

    free(*cmd);
    *cmd = cmd2;
}
#endif

static int
system_cmd(const char* cmd) {
#if defined(WIN32)
    /* spawn a subprocess (instead of just calling `system`) because 2 reasons: */
    /* 1. cl.exe prints source file names if called using `system` */
    /* 2. on Windows 98 COMMAND.COM has a limitation of 126 characters */
    SECURITY_ATTRIBUTES sa;
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    size_t cmd_len;
    char* cmd_output = NULL;
    DWORD cmd_output_capacity = 0;
    DWORD cmd_output_len = 0;
    DWORD bytes_read;
    DWORD exit_code = 0;

    /* we need to create a modifiable cmd for CreateProcessA */
    cmd_len = strlen(cmd);
    char* cmd_copy = malloc(sizeof(char) * (cmd_len + 1));
    memcpy(cmd_copy, cmd, sizeof(char) * cmd_len);
    cmd_copy[cmd_len] = 0;

    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        printf("- failed to create a pipe for child process, error:\n");
        print_win32_last_error(GetLastError());
        free(cmd_copy);
        return -1;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmd_copy, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        if (err == 2) {
            /* not found, try adding a cmd prefix */
            char buf[MAX_PATH];
            DWORD len = GetEnvironmentVariableA("COMSPEC", buf, sizeof(buf));
            if (len == 0) {
                printf("- error: expected to find COMSPEC environment variable\n");
            } else if (len >= sizeof(buf)) {
                printf("- error: buffer for COMSPEC was not big enough\n");
            } else {
                prefix_cmd(buf, &cmd_copy, cmd_len);
                if (!CreateProcessA(
                        NULL, cmd_copy, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
                    err = GetLastError();
                } else {
                    err = 0;
                }
            }
        }
        if (err != 0) {
            printf("- failed to spawn a child process, error:\n");
            print_win32_last_error(err);
            printf("- used command:\n%s\n", cmd_copy);
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            free(cmd_copy);
            return -1;
        }
    }

    CloseHandle(write_pipe);

    cmd_output_capacity = 1024 * 2;
    cmd_output = malloc(sizeof(char) * cmd_output_capacity);
    memset(cmd_output, 0, sizeof(char) * cmd_output_capacity);

    while (
        ReadFile(
            read_pipe, cmd_output, cmd_output_capacity - cmd_output_len - 1, &bytes_read, NULL)
        && bytes_read > 0) {
        cmd_output[bytes_read] = 0;
        cmd_output_len += bytes_read;
        if (cmd_output_capacity - cmd_output_len - 1 < 1024) {
            cmd_output_capacity += 1024 * 2;
            char* s = malloc(sizeof(char*) * cmd_output_capacity);
            memset(s, 0, sizeof(char) * cmd_output_capacity);
            memcpy(s, cmd_output, sizeof(char) * cmd_output_len);
            free(cmd_output);
            cmd_output = s;
        }
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(read_pipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        unsigned int i = 0;
        unsigned int start_idx = 0;
        for (i; i < cmd_output_len; i++) {
            if (cmd_output[i] == 0 || i + 1 == cmd_output_len) {
                printf("%s\n", cmd_output + start_idx);
                start_idx = i + 1;
                if (i + 1 < cmd_output_len && cmd_output[i + 1] == 0) {
                    break;
                }
            }
        }
        free(cmd_output);
        free(cmd_copy);
        return (int)exit_code;
    }

    free(cmd_output);
    free(cmd_copy);
    return 0;
#elif defined(__linux__)
    int ret = system(cmd);
    if (ret == -1 || WIFSIGNALED(ret)) {
        return -1;
    }
    return WEXITSTATUS(ret);
#else
#error "unsupported OS"
#endif
}

static void
cache_target_build_config(
    tb_target_info* target, const char* cache_file_path, const char* ldflags) {
    FILE* fp = fopen(cache_file_path, "w");
    if (fp == NULL) {
        printf("- failed to write to file \"%s\" to cache build config\n", cache_file_path);
        return;
    }

    /* compiler */
    fprintf(fp, "%s\n", config.cc);

    /* build mode */
    fprintf(fp, "%s\n", config.is_release ? "release" : "debug");

    /* target type */
    fprintf(fp, "%i\n", (int)target->type);

    /* ld flags */
    fprintf(fp, "%s\n", ldflags);

    /* private includes */
    fprintf(fp, "%s\n", target->private_includes == NULL ? " " : target->private_includes);

    /* public includes */
    fprintf(fp, "%s\n", target->public_includes == NULL ? " " : target->public_includes);

    /* macros */
    fprintf(fp, "%s\n", target->macros == NULL ? " " : target->macros);

    /* header count */
    fprintf(fp, "%u\n", target->header_count);

    /* dependency count */
    fprintf(fp, "%u\n", target->dependency_count);

    fclose(fp);
}

static void
check_cached_build_outdated(
    tb_target_info* target, const char* target_dir, unsigned int target_dir_len,
    const char* ldflags, bool* recompile_all_sources, bool* ldflags_changed,
    bool* headers_changed) {
    FILE* fp;
    char* cache_file_path =
        filesystem_append_path(target_dir, target_dir_len, CACHED_BUILD_CONFIG_FILE, 0, NULL);

    fp = fopen(cache_file_path, "r");
    if (fp == NULL) {
        cache_target_build_config(target, cache_file_path, ldflags);
        (*recompile_all_sources) = true;
        (*ldflags_changed) = true;
        (*headers_changed) = true;
    } else {
        char buf32[32];
        char* line = NULL;
        size_t line_len;
        (*recompile_all_sources) = false;
        (*ldflags_changed) = true;
        (*headers_changed) = true;

#define COMPARE_CACHE_STR(str)                                                                \
    if (!read_line(&line, &line_len, fp)) {                                                   \
        (*recompile_all_sources) = true;                                                      \
        goto cache_check_end;                                                                 \
    }                                                                                         \
    /* exclude \n */                                                                          \
    line[line_len - 1] = 0;                                                                   \
    if (strcmp(line, str) != 0) {                                                             \
        (*recompile_all_sources) = true;                                                      \
        goto cache_check_end;                                                                 \
    }

        /* compiler */
        COMPARE_CACHE_STR(config.cc)

        /* build mode */
        COMPARE_CACHE_STR(config.is_release ? "release" : "debug")

        /* target type */
        snprintf(buf32, 32, "%i", target->type);
        COMPARE_CACHE_STR(buf32);

        /* ld flags */
        COMPARE_CACHE_STR(ldflags);
        (*ldflags_changed) = false;

        /* private includes */
        COMPARE_CACHE_STR(target->private_includes == NULL ? " " : target->private_includes);

        /* public includes */
        COMPARE_CACHE_STR(target->public_includes == NULL ? " " : target->public_includes);

        /* macros */
        COMPARE_CACHE_STR(target->macros == NULL ? " " : target->macros);

        /* header count */
        snprintf(buf32, 32, "%u", target->header_count);
        COMPARE_CACHE_STR(buf32)
        (*headers_changed) = false;

        /* dependency count */
        snprintf(buf32, 32, "%u", target->dependency_count);
        COMPARE_CACHE_STR(buf32)

    cache_check_end:
        free(line);
        fclose(fp);

        if ((*recompile_all_sources) || (*ldflags_changed)) {
            cache_target_build_config(target, cache_file_path, ldflags);
        }
    }

    free(cache_file_path);
}

static bool
is_cached_build_str_changed(
    const char* filepath, const char* str_name, const char* new_str,
    unsigned int new_str_len) {
    FILE* file;
    unsigned long file_size;

    file = fopen(filepath, "rb");
    if (file == NULL) {
        /* save new str */
        file = fopen(filepath, "wb");
        if (file == NULL) {
            printf("- failed to create file at: %s", filepath);
            return false;
        }
        fwrite(new_str, sizeof(char), new_str_len, file);
        fclose(file);
        return true;
    } else {
        /* see if changed */
        char* old_str;

        /* get file size */
        file_size = 0;
        fseek(file, 0, SEEK_END);
        {
            long size = ftell(file);
            if (size < 0) {
                printf("- failed to get file size %s\n", filepath);
                return false;
            }
            file_size = (unsigned long)size;
        }
        rewind(file);

        old_str = malloc(sizeof(char) * (file_size + 1));
        fread(old_str, sizeof(char), file_size, file);
        old_str[file_size] = 0;
        fclose(file);

        if (strcmp(old_str, new_str) == 0) {
            return false;
        } else {
            printf("- %s changed from the previous compilation", str_name);
        }

        free(old_str);

        /* save new str */
        {
            file = fopen(filepath, "wb");
            if (file == NULL) {
                printf("- failed to overwrite the file at: %s", filepath);
                return false;
            }
            fwrite(new_str, sizeof(char), new_str_len, file);
            fclose(file);
        }

        return true;
    }
}

static bool
have_variable(const char* name) {
    size_t namelen = strlen(name);
    unsigned int i;

    for (i = 0; i < variable_count; i++) {
        if (namelen != variables[i].name_len) {
            continue;
        }
        if (strcmp(variables[i].name, name) != 0) {
            continue;
        }
        return true;
    }

    return false;
}

static void
add_variable(const char* name, const char* value) {
    tb_variable* vars;
    unsigned int i;
    size_t namelen = strlen(name);

    /* check if we already have a variable with this name */
    for (i = 0; i < variable_count; i++) {
        if (namelen != variables[i].name_len) {
            continue;
        }
        if (strcmp(variables[i].name, name) != 0) {
            continue;
        }
        printf(
            "warning: a variable with the name \"%s\" already exists, skipping "
            "redefinition\n",
            name);
        return;
    }

    vars = (tb_variable*)malloc(sizeof(tb_variable) * (variable_count + 1));
    memcpy(vars, variables, sizeof(tb_variable) * variable_count);

    free(variables);
    variables = vars;

    {
        tb_variable* var = &variables[variable_count];

        var->name = (char*)malloc(sizeof(char) * (namelen + 1));
        memcpy(var->name, name, sizeof(char) * namelen);
        var->name[namelen] = 0;

        var->name_len = (unsigned int)namelen;

        if (value == NULL) {
            var->value = NULL;
        }
    }
    variable_count += 1;
}

static void
add_function(const char* name, bool (*process)(char* args, unsigned int args_len)) {
    tb_function* funcs;
    unsigned int i;
    size_t namelen = strlen(name);

    /* check if we already have a function with this name */
    for (i = 0; i < function_count; i++) {
        if (namelen != functions[i].name_len) {
            continue;
        }
        if (strcmp(functions[i].name, name) != 0) {
            continue;
        }
        printf(
            "warning: a function with the name \"%s\" already exists, skipping "
            "redefinition",
            name);
        return;
    }

    funcs = (tb_function*)malloc(sizeof(tb_function) * (function_count + 1));
    memcpy(funcs, functions, sizeof(tb_function) * function_count);

    free(functions);
    functions = funcs;

    {
        tb_function* func = &functions[function_count];

        func->name = (char*)malloc(sizeof(char) * (namelen + 1));
        memcpy(func->name, name, sizeof(char) * namelen);
        func->name[namelen] = 0;

        func->name_len = (unsigned int)namelen;
        func->process = process;
    }
    function_count += 1;
}

static tb_target_info*
get_target_info(const char* name, bool create_new_if_didnt_exist) {
    tb_target_info** infos;
    tb_target_info* target;
    unsigned int i;
    size_t namelen = strlen(name);

    for (i = 0; i < target_count; i++) {
        if (targets[i]->name_len != namelen) {
            continue;
        }
        if (strcmp(targets[i]->name, name) != 0) {
            continue;
        }
        return targets[i];
    }

    if (!create_new_if_didnt_exist) {
        return NULL;
    }

    infos = (tb_target_info**)malloc(sizeof(tb_target_info*) * (target_count + 1));
    memcpy(infos, targets, sizeof(tb_target_info*) * target_count);

    free(targets);
    targets = infos;

    targets[target_count] = (tb_target_info*)malloc(sizeof(tb_target_info));
    target = targets[target_count];
    target_count += 1;

    {
        target->name = (char*)malloc(sizeof(char) * (namelen + 1));
        memcpy(target->name, name, sizeof(char) * namelen);
        target->name[namelen] = 0;

        target->name_len = (unsigned int)namelen;

        target->macros = NULL;
        target->private_includes = NULL;
        target->public_includes = NULL;
        target->additional_ld_flags = NULL;

        target->sources = NULL;
        target->source_count = 0;
        target->sources_capacity = 0;

        target->headers = NULL;
        target->header_count = 0;
        target->headers_capacity = 0;

        target->dependencies = NULL;
        target->dependency_count = 0;

        target->type = TB_TT_EXE;

        target->recompile_all_sources = true;
        target->built = false;
        target->was_outdated = false;
    }

    return target;
}

static bool
read_line_with_user_func(char** lineptr, size_t* len, FILE* stream) {
    tb_user_function* f = config.inside_user_function;
    unsigned int start_idx;

    if (f == NULL || f->pos >= f->code_count) {
        config.inside_user_function = NULL;
        return read_line(lineptr, len, stream);
    }

    /* search for 0 (our end of line marker) */
    start_idx = f->pos;
    for (; f->pos < f->code_count; f->pos++) {
        if (f->code[f->pos] == 0) {
            f->pos += 1;
            break;
        }
    }
    (*len) = f->pos - start_idx;

    if ((*len) == 0) {
        return true;
    } else {
        free(*lineptr);
        (*lineptr) = malloc(sizeof(char) * ((*len) + 1));
        memcpy((*lineptr), f->code + start_idx, sizeof(char) * (*len));
        (*lineptr)[*len] = 0;
    }

    return true;
}

static bool
read_line(char** lineptr, size_t* len, FILE* stream) {
    const size_t min_buffer_size = 256;
    char* new_ptr = NULL;
    size_t buf_size = 0;
    size_t pos = 0;
    int c;

    c = getc(stream);
    if (c == EOF) {
        return false;
    }

    if (*lineptr == NULL) {
        *lineptr = (char*)malloc(min_buffer_size);
        if (*lineptr == NULL) {
            return false;
        }
        buf_size = min_buffer_size;
    }

    pos = 0;
    while (c != EOF) {
        if (pos + 1 >= buf_size) {
            size_t new_size = buf_size + (buf_size >> 2);

            if (new_size < min_buffer_size) {
                new_size = min_buffer_size;
            }

            if (new_size <= buf_size) {
                return false;
            }

            new_ptr = (char*)realloc(*lineptr, new_size);
            if (new_ptr == NULL) {
                return false;
            }
            buf_size = new_size;
            *lineptr = new_ptr;
        }

        ((unsigned char*)(*lineptr))[pos++] = (unsigned char)c;
        if (c == '\n') {
            break;
        }
        c = getc(stream);
    }

    (*lineptr)[pos] = 0;

    if (c == EOF && !feof(stream)) {
        *len = pos;
        return true;
    }

    *len = pos;
    return true;
}

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)

#if !defined(HAVE_VSNPRINTF)
int
vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    int retval = -1;

#if defined(_MSC_VER)
    if (size != 0 && str != NULL) {
        retval = _vsnprintf(str, size, format, ap);

        /* guarantee null-termination even if truncated */
        str[size - 1] = '\0';
    }

    if (retval < 0) {
        FILE* nul = fopen("NUL", "wb");
        if (nul != NULL) {
            retval = vfprintf(nul, format, ap);
            fclose(nul);
        }
    }
#else
    FILE* fp;

    fp = tmpfile();
    if (fp == NULL) {
        return -1;
    }

    retval = vfprintf(fp, format, ap);

    if (retval >= 0 && size > 0 && str != NULL) {
        size_t to_read = (size_t)retval < (size - 1) ? (size_t)retval : (size - 1);
        rewind(fp);
        if (to_read > 0) {
            to_read = fread(str, 1, to_read, fp);
        }
        str[to_read] = '\0';
    }

    fclose(fp);
#endif

    return retval;
}
#endif

#if !defined(HAVE_SNPRINTF)
int
snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    int retval;

    va_start(ap, format);
    retval = vsnprintf(str, size, format, ap);
    va_end(ap);

    return retval;
}
#endif

#endif
