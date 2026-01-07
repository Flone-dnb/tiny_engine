#if defined(WIN32)
// Hide console on Windows.
#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#endif

#include "editor.h"

int
main(void) {
    editor_run();
    return 0;
}
