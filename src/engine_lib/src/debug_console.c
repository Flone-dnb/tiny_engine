#include "debug_console.h"
#include <stdio.h>
#include "misc/error.h"
#if defined(ENGINE_DEBUG_TOOLS)

#include <stdint.h>
#include <string.h>
#include "game_manager.h"
#include "hashmap.c/hashmap.h"
#include "misc/memory_usage.h"
#include "render/debug_drawer.h"
#include "render/renderer.h"

// Command hash for hashmap.
uint64_t
debug_console_command_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const te_debug_console_command* command = item;
    return hashmap_sip(command->name, strlen(command->name), seed0, seed1);
}

// Command compare for hashmap.
int
debug_console_command_compare(const void* a, const void* b, void* udata) {
    (void)udata;
    const te_debug_console_command* command1 = a;
    const te_debug_console_command* command2 = b;
    return strcmp(command1->name, command2->name);
}

// Groups debug console data.
struct te_debug_console {
    te_game_manager* game_manager;

    struct hashmap* commands;

    // Current user input. Non-NULL because preallocated.
    // Actually valid char count is @ref input_valid_len.
    // Size of this array is @ref input_total_len.
    char* input;

    // Result of the user input, displayed if @ref message_sec_left is > 0.
    const char* message;

    te_debug_stats stats;

    // Copy of @ref stats displayed for @ref time_sec_to_update_stats.
    te_debug_stats displayed_stats;

    vec2 screen_pos;

    // Time (in seconds) left to display @ref message.
    float message_sec_left;

    // Time (in seconds) until @ref displayed_stats is updated.
    float time_sec_to_update_stats;

    // Number of valid elements in @ref input.
    unsigned int input_valid_len;

    // Total len of the array @ref input (excluding the NULL terminated character).
    unsigned int input_total_len;

    bool is_shown;

    // For @ref stats. Can be drawn even if the console is hidden.
    bool show_stats;
};

// Static to allow using debug console easily from various places.
static te_debug_console console;

void
prv_debug_console_show_stats() {
    console.show_stats = true;
    console.displayed_stats = console.stats;
}

void
prv_debug_console_hide_stats() {
    console.show_stats = false;
}

void
prv_debug_console_init(te_game_manager* game_manager) {
    console.game_manager = game_manager;
    console.commands = hashmap_new(sizeof(te_debug_console_command), 4, 0, 0, debug_console_command_hash,
                                   debug_console_command_compare, NULL, NULL);
    console.input_total_len = 65;
    console.input = malloc(sizeof(char) * console.input_total_len);
    console.input_valid_len = 0;
    console.is_shown = false;
    console.show_stats = false;
    console.message = NULL;
    console.message_sec_left = 0.0f;
    console.time_sec_to_update_stats = 0.0f;
    memset(&console.stats, 0, sizeof(te_debug_stats));
    memset(&console.displayed_stats, 0, sizeof(te_debug_stats));

    glm_vec2_copy((vec2){0.01f, 0.95f}, console.screen_pos);

    {
        te_debug_console_command command = {0};
        command.name = "show_stats";
        command.no_args = prv_debug_console_show_stats;
        debug_console_register_command(command);
    }
    {
        te_debug_console_command command = {0};
        command.name = "hide_stats";
        command.no_args = prv_debug_console_hide_stats;
        debug_console_register_command(command);
    }
}

void
prv_debug_console_deinit(void) {
    hashmap_free(console.commands);
    console.commands = NULL;

    free(console.input);
    console.input = NULL;

    console.input_total_len = 0;
    console.input_valid_len = 0;
}

void
debug_console_register_command(te_debug_console_command command) {
    hashmap_set(console.commands, &command);
}

void
debug_console_show_stats(void) {
    prv_debug_console_show_stats();
}

void
debug_console_hide_stats(void) {
    prv_debug_console_hide_stats();
}

bool
debug_console_is_stats_shown(void) {
    return console.show_stats;
}

void
prv_debug_console_show(void) {
    console.is_shown = true;
}

void
prv_debug_console_hide(void) {
    console.is_shown = false;
    console.input_valid_len = 0;
    console.message_sec_left = 0.0f;
}

bool
prv_debug_console_is_shown(void) {
    return console.is_shown;
}

void
prv_debug_console_on_keyboard_input(struct te_game_manager* game_manager, enum te_keyboard_button button) {
    if (button == TE_KB_BACKSPACE && console.input_valid_len > 0) {
        console.input_valid_len -= 1;
        console.input[console.input_valid_len] = 0;
        return;
    }

    if (button == TE_KB_ENTER && console.input_valid_len > 0) {
        // Check if arguments are specified.
        unsigned int arg_pos = 0;
        bool found_arg = false;
        for (unsigned int i = 0; i < console.input_valid_len; i++) {
            if (console.input[i] == ' ') {
                found_arg = true;
                arg_pos = i;
                break;
            }
        }

        const unsigned int command_len = found_arg ? arg_pos : console.input_valid_len;
        te_debug_console_command target_command;
        target_command.name = malloc(sizeof(char) * (command_len + 1));
        memcpy((char*)target_command.name, console.input, sizeof(char) * command_len);
        ((char*)target_command.name)[command_len] = 0;

        const te_debug_console_command* found_command = hashmap_get(console.commands, &target_command);
        if (found_command == NULL) {
            console.message = "command not found";
            console.message_sec_left = 1.0f;
        } else {
            if (found_command->arg_uint != NULL && (!found_arg || (arg_pos + 1) >= console.input_valid_len)) {
                console.message = "the command requires arguments";
                console.message_sec_left = 2.0f;
            } else if (found_command->arg_uint != NULL && found_arg) {
                unsigned int num = (unsigned int)strtoul(console.input + arg_pos, NULL, 10);
                found_command->arg_uint(game_manager, num);

                console.is_shown = false;
            } else {
                found_command->no_args(game_manager);

                console.is_shown = false;
            }
        }

        free((char*)target_command.name);

        console.input_valid_len = 0;
    }
}

void
prv_debug_console_on_keyboard_input_text(const char* text) {
    const unsigned int text_len = (unsigned int)strlen(text);
    if (console.input_valid_len + text_len > console.input_total_len) {
        console.input_total_len = console.input_valid_len + text_len + 64;

        char* new_input = malloc(sizeof(char) * (console.input_total_len + 1));
        memcpy(new_input, console.input, sizeof(char) * console.input_valid_len);

        free(console.input);
        console.input = new_input;
    }

    memcpy(console.input + console.input_valid_len, text, text_len);
    console.input_valid_len += text_len;
    console.input[console.input_valid_len] = 0;
}

void
prv_debug_console_draw_stat(vec2 screen_pos, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_start(args_copy, fmt);

    int len = vsnprintf(NULL, 0, fmt, args);
    if (CGLM_UNLIKELY(len <= 0)) {
        va_end(args);
        va_end(args_copy);
        show_error_and_abort("snprintf error");
    }
    len += 1;

    char* text = malloc(sizeof(char) * ((unsigned int)len + 1));
    vsnprintf(text, (unsigned int)len, fmt, args_copy);
    text[len] = 0;

    debug_drawer_draw_text_at_pos(text, 0.0f, (vec3){1.0f, 1.0f, 1.0f}, screen_pos);
    screen_pos[1] += debug_drawer_get_default_text_height();

    free(text);
    va_end(args);
    va_end(args_copy);
}

void
prv_debug_console_draw(float delta_time_sec) {
    console.time_sec_to_update_stats -= delta_time_sec;
    if (console.time_sec_to_update_stats <= 0.0f) {
        console.displayed_stats = console.stats;
        console.time_sec_to_update_stats = 1.0f;

        te_debug_stats* stats = &console.displayed_stats;
        stats->process_mem = (unsigned int)(memory_usage_get_process_used_memory() / 1024 / 1024);
        stats->total_used_mem = (unsigned int)(memory_usage_get_total_used_memory() / 1024 / 1024);
        stats->total_mem = (unsigned int)(memory_usage_get_total_memory() / 1024 / 1024);
    }

    if (console.show_stats) {
        te_debug_stats* stats = &console.displayed_stats;
        vec2 screen_pos;
        glm_vec2_copy((vec2){0.01f, 0.7f}, screen_pos);

        const unsigned int fps_limit =
            renderer_get_fps_limit(game_manager_get_renderer(console.game_manager));
        prv_debug_console_draw_stat(screen_pos, "FPS: %u (limit: %u)", stats->fps, fps_limit);

#if defined(ENGINE_ASAN_ENABLED)
        prv_debug_console_draw_stat(screen_pos, "RAM used (MB): %zu (%zu/%zu) (ASan enabled)",
                                    stats->process_mem, stats->total_used_mem, stats->total_mem);
#else
        prv_debug_console_draw_stat(screen_pos, "RAM used (MB): %zu (%zu/%zu)", stats->process_mem,
                                    stats->total_used_mem, stats->total_mem);
#endif
    }

    if (!console.is_shown) {
        return;
    }

    if (console.message_sec_left > 0.0f) {
        console.message_sec_left -= delta_time_sec;
        debug_drawer_draw_text_at_pos(console.message, 0.0f, (vec3){1.0f, 1.0f, 1.0f}, console.screen_pos);
        return;
    }

    if (console.input_valid_len == 0) {
        debug_drawer_draw_text_at_pos("type a command...", 0.0f, (vec3){1.0f, 1.0f, 1.0f},
                                      console.screen_pos);
    } else {
        debug_drawer_draw_text_at_pos(console.input, 0.0f, (vec3){1.0f, 1.0f, 1.0f}, console.screen_pos);
    }
}

te_debug_stats*
prv_debug_console_get_stats(void) {
    return &console.stats;
}

#endif
