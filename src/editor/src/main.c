#if defined(WIN32)
// Hide console on Windows.
#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#endif

#include "editor.h"
#include "window.h"

#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#endif

int
main(void) {
    // Enable run-time memory checks for debug builds (on Windows).
#if defined(WIN32) && defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    te_window* window = window_create("tiny engine editor");

    te_game_window_callbacks callbacks;
    callbacks.on_game_started = &editor_on_game_started;
    callbacks.on_game_tick = &editor_on_game_tick;
    callbacks.on_window_close = &editor_on_window_close;
    window_process_events(window, &callbacks);

    window_destroy(window);

    return 0;
}
