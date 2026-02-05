#pragma once
#if defined(ENGINE_DEBUG_TOOLS)

#include <stdbool.h>
#include "input/keyboard_button.h"

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

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Must be called before game is started and after game is finished.
void prv_debug_console_init();
void prv_debug_console_deinit();

void prv_debug_console_show();
void prv_debug_console_hide();

bool prv_debug_console_is_shown();

void prv_debug_console_on_keyboard_input(struct te_game_manager* game_manager,
                                         enum te_keyboard_button button);
void prv_debug_console_on_keyboard_input_text(const char* text);

void prv_debug_console_draw(float delta_time_sec);

#endif
