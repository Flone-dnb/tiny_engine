#pragma once
#if defined(ENGINE_DEBUG_TOOLS)

#include <stdbool.h>
#include <input/keyboard_button.h>

// Can be displayed in the game using the tilde (~) button.
// Allows for creating and executing custom commands during the game.
typedef struct te_debug_console te_debug_console;

struct te_game_manager;

// Structure of a registered command in the debug console.
typedef struct te_debug_console_command {
    // Unique name of the command.
    const char* name;

    // Only 1 of these callbacks is non-NULL depending on how much arguments the command requires.
    void (*no_args)(struct te_game_manager* game_manager);
    void (*arg_uint)(struct te_game_manager* game_manager, unsigned int arg);
} te_debug_console_command;

void debug_console_register_command(te_debug_console_command command);

// Function to programmatically toggle "show_stats" command.
void debug_console_show_stats(void);
void debug_console_hide_stats(void);
bool debug_console_is_stats_shown(void);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Must be called before game is started and after game is finished.
void prv_debug_console_init(struct te_game_manager* game_manager);
void prv_debug_console_deinit(void);

void prv_debug_console_show(void);
void prv_debug_console_hide(void);

bool prv_debug_console_is_shown(void);

void prv_debug_console_on_keyboard_input(struct te_game_manager* game_manager, enum te_keyboard_button button);
void prv_debug_console_on_keyboard_input_text(const char* text);

void prv_debug_console_draw(float delta_time_sec);

// Groups various statistics that can be displayed using the debug console's command "show_stats".
typedef struct te_debug_stats {
    unsigned int fps;

    // in MB
    unsigned int process_mem;
    unsigned int total_mem;
    unsigned int total_used_mem;

    unsigned int rendered_model_count;
    unsigned int cpu_ahead_gpu_frame_count;

    float cpu_time_frame_ms;
    float cpu_time_submit_models_ms;
    float cpu_time_submit_widgets_ms;
    float cpu_time_submit_debug_ms;
    float cpu_time_swap_ms;

    float gpu_time_frame_ms;
    float gpu_time_draw_models_ms;
    float gpu_time_draw_widgets_ms;
    float gpu_time_draw_debug_ms;
} te_debug_stats;

// Returns always valid pointer to update debug stats.
te_debug_stats* prv_debug_console_get_stats(void);

#endif
