#include "debug_console.h"
#if defined(ENGINE_DEBUG_TOOLS)

#include <stdint.h>
#include <string.h>
#include "hashmap.c/hashmap.h"
#include "render/debug_drawer.h"

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
    struct hashmap* commands;

    // Current user input. Non-NULL because preallocated. Size of this array is @ref input_len.
    char* input;

    // Result of the user input, displayed if @ref message_sec_left is > 0.
    const char* message;

    vec2 screen_pos;

    // Time (in seconds) left to display @ref message.
    float message_sec_left;

    // Number of valid elements in @ref input.
    unsigned int input_valid_len;

    // Total len of the array @ref input (excluding the NULL terminated character).
    unsigned int input_total_len;

    bool is_shown;
};

// Static to allow using debug console easily from various places.
static te_debug_console console;

void
prv_debug_console_init() {
    console.commands = hashmap_new(sizeof(te_debug_console_command), 4, 0, 0, debug_console_command_hash,
                                   debug_console_command_compare, NULL, NULL);
    console.input_total_len = 65;
    console.input = malloc(sizeof(char) * console.input_total_len);
    console.input_valid_len = 0;
    console.is_shown = false;
    console.message = NULL;
    console.message_sec_left = 0.0f;

    glm_vec2_copy((vec2){0.01f, 0.95f}, console.screen_pos);
}

void
prv_debug_console_deinit() {
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
prv_debug_console_show() {
    console.is_shown = true;
}

void
prv_debug_console_hide() {
    console.is_shown = false;
    console.input_valid_len = 0;
    console.message_sec_left = 0.0f;
}

bool
prv_debug_console_is_shown() {
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
prv_debug_console_draw(float delta_time_sec) {
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

#endif
