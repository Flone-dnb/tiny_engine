#include "render/shader_manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad/glad.h"
#include "io/log.h"
#include "io/paths.h"
#include "misc/error.h"

/** Groups information about a shader program. */
typedef struct te_shader_program {
    /** Non-NULL path (relative to the `res` directory) to the vertex shader. */
    char* vert_relative_path;

    /** Non-NULL path (relative to the `res` directory) to the fragment shader. */
    char* frag_relative_path;

    /** strlen of @ref vert_relative_path. */
    unsigned int vert_relative_path_len;

    /** strlen of @ref frag_relative_path. */
    unsigned int frag_relative_path_len;

    /** OpenGL ID of the shader program. */
    unsigned int id;

    /** The number of places this program is currently used in. */
    unsigned int usage_count;
} te_shader_program;

/** Loads, compiles and caches shader programs. */
struct te_shader_manager {
    /** Compiled shaders. Size of this array is @ref shader_count. */
    te_shader_program* shaders;

    /** Size of @ref shaders. */
    unsigned int shader_count;
};

te_shader_manager*
prv_shader_manager_create() {
    te_shader_manager* manager = malloc(sizeof(te_shader_manager));
    manager->shader_count = 0;
    manager->shaders = NULL;

    return manager;
}

void
prv_shader_manager_destroy(te_shader_manager* manager) {
    if (manager->shader_count > 0) {
        show_error_and_abort("shader manager is being destroyed but there are still some shaders in use");
    }

    free(manager);
}

unsigned int
prv_shader_manager_compile_shader(const char* path, bool is_frag) {
    const char* prefix = "#version 100\n"
                         "precision highp float;\n"
                         "precision highp int;\n\n";
    const unsigned long prefix_len = strlen(prefix);

    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        show_error_and_abort("unable read the specified shader file (file exist?)");
    }

    // Get file size.
    unsigned long file_size = 0;
    fseek(f, 0, SEEK_END);
    {
        long size = ftell(f);
        if (size < 0) {
            show_error_and_abort("failed to get shader file size");
        }
        file_size = (unsigned long)size;
    }
    rewind(f);

    // Read content.
    char* file_content = malloc(sizeof(char) * (file_size + 1));
    const unsigned long bytes_read = fread(file_content, 1, file_size, f);
    file_content[file_size] = 0;
    if (bytes_read != file_size) {
        show_error_and_abort("failed to read the shader file");
    }
    fclose(f);

    // Construct the final code.
    char* shader_code = malloc(sizeof(char) * (prefix_len + file_size + 1));
    memcpy(shader_code, prefix, sizeof(char) * prefix_len);
    memcpy(shader_code + prefix_len, file_content, file_size);
    shader_code[prefix_len + file_size] = 0;
    const unsigned long code_len = prefix_len + file_size;

    // Compile shader.
    const unsigned int shader_id = glCreateShader(is_frag ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
    glShaderSource(shader_id, 1, (const char**)&shader_code, NULL);
    glCompileShader(shader_id);

    // Check errors.
    int success = 0;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        char comp_error_msg[4096] = {0};
        glGetShaderInfoLog(shader_id, 4096, NULL, comp_error_msg);

        // Format output (log source code with line numbers).
        const unsigned long fmt_code_len = code_len + 4096;
        char* fmt_code = malloc(sizeof(char) * fmt_code_len);
        memset(fmt_code, 0, sizeof(char) * fmt_code_len);

        char line_text[16] = {0};
        unsigned int line_num = 2;

        fmt_code[0] = '\n';
        fmt_code[1] = '1';
        fmt_code[2] = '.';
        fmt_code[3] = ' ';
        for (unsigned int src = 0, dst = 4; src < code_len; src++) {
            fmt_code[dst] = shader_code[src];
            dst += 1;
            if (shader_code[src] == 0) {
                break;
            }

            if (shader_code[src] == '\n') {
                // Append line number.
                snprintf(line_text, 16, "%u. ", line_num);
                const unsigned long line_len = strlen(line_text);
                memcpy(fmt_code + dst, line_text, line_len);
                dst += line_len;
            }
        }

        log_info_fmt("failed to compile shader \"%s\", error: %s, see full source code below:", path,
                     comp_error_msg);
        log_info(fmt_code);
        free(fmt_code);

        show_error_and_abort("failed to compile shader, see log for more details");
    }

    free(shader_code);

    return shader_id;
}

unsigned int
shader_manager_request_shader(te_shader_manager* manager, const char* vert_relative_path,
                              const char* frag_relative_path) {
    const unsigned long vert_relative_path_len = strlen(vert_relative_path);
    const unsigned long frag_relative_path_len = strlen(frag_relative_path);

    // Check cache.
    unsigned int index = 0;
    bool found = false;
    for (unsigned int i = 0; i < manager->shader_count; i++) {
        if (manager->shaders[i].frag_relative_path_len != frag_relative_path_len
            || manager->shaders[i].vert_relative_path_len != vert_relative_path_len) {
            continue;
        }
        if (strcmp(manager->shaders[i].frag_relative_path, frag_relative_path) != 0) {
            continue;
        }
        if (strcmp(manager->shaders[i].vert_relative_path, vert_relative_path) != 0) {
            continue;
        }
        found = true;
        index = i;
        break;
    }
    if (!found) {
        // Compile new program.
        char* vert_path = paths_prepend_res_to_path(vert_relative_path);
        char* frag_path = paths_prepend_res_to_path(frag_relative_path);

        const unsigned int vert_id = prv_shader_manager_compile_shader(vert_path, false);
        const unsigned int frag_id = prv_shader_manager_compile_shader(frag_path, true);

        unsigned int prog_id = glCreateProgram();
        glAttachShader(prog_id, vert_id);
        glAttachShader(prog_id, frag_id);
        glLinkProgram(prog_id);
        int success = 0;
        glGetProgramiv(prog_id, GL_LINK_STATUS, &success);
        if (success == 0) {
            int log_len = 0;
            glGetProgramiv(prog_id, GL_INFO_LOG_LENGTH, &log_len);

            char* msg = malloc(sizeof(char) * (unsigned long)log_len);
            glGetProgramInfoLog(prog_id, log_len, NULL, msg);

            log_info_fmt("failed to link shaders \"%s\" and \"%s\", error: %s", vert_path, frag_path, msg);
            free(msg);

            show_error_and_abort("failed to link shader program, see log for more details");
        }

        glDetachShader(prog_id, vert_id);
        glDeleteShader(vert_id);

        glDetachShader(prog_id, frag_id);
        glDeleteShader(frag_id);

        // Cache results.
        te_shader_program* new_shaders = malloc(sizeof(te_shader_program) * (manager->shader_count + 1));
        memcpy(new_shaders, manager->shaders, sizeof(te_shader_program) * manager->shader_count);

        free(manager->shaders);
        manager->shaders = new_shaders;

        index = manager->shader_count;
        manager->shader_count += 1;

        // Init data.
        te_shader_program* prog = &manager->shaders[index];

        prog->usage_count = 0; // will increment below
        prog->id = prog_id;

        prog->vert_relative_path = malloc(sizeof(char) * (vert_relative_path_len + 1));
        memcpy(prog->vert_relative_path, vert_relative_path, sizeof(char) * vert_relative_path_len);
        prog->vert_relative_path[vert_relative_path_len] = 0;
        prog->vert_relative_path_len = (unsigned int)vert_relative_path_len;

        prog->frag_relative_path = malloc(sizeof(char) * (frag_relative_path_len + 1));
        memcpy(prog->frag_relative_path, frag_relative_path, sizeof(char) * frag_relative_path_len);
        prog->frag_relative_path[frag_relative_path_len] = 0;
        prog->frag_relative_path_len = (unsigned int)frag_relative_path_len;
    }

    manager->shaders[index].usage_count += 1;
    return manager->shaders[index].id;
}

void
shader_manager_mark_unused_shader(te_shader_manager* manager, unsigned int prog_id) {
    unsigned int index = 0;
    bool found = false;
    for (unsigned int i = 0; i < manager->shader_count; i++) {
        if (manager->shaders[i].id != prog_id) {
            continue;
        }
        found = true;
        index = i;
        break;
    }
    if (!found) {
        show_error_and_abort("unable to find the specified shader program");
    }
    if (manager->shaders[index].usage_count == 0) {
        show_error_and_abort(
            "the specified shader program id already has usage count of 0 (this is a shader manager bug)");
    }

    manager->shaders[index].usage_count -= 1;
    if (manager->shaders[index].usage_count > 0) {
        return;
    }

    glDeleteProgram(manager->shaders[index].id);

    // Remove from cache.
    if (manager->shader_count == 1) {
        free(manager->shaders);
        manager->shaders = NULL;
        manager->shader_count = 0;
    } else {
        te_shader_program* new_shaders = malloc(sizeof(te_shader_program) * (manager->shader_count - 1));
        memcpy(new_shaders, manager->shaders, sizeof(te_shader_program) * index);
        memcpy(new_shaders + index, manager->shaders + (index + 1),
               sizeof(te_shader_program) * (manager->shader_count - index - 1));

        free(manager->shaders);
        manager->shaders = new_shaders;
        manager->shader_count -= 1;
    }
}
