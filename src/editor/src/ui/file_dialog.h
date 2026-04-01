#pragma once

// Optional widget used to ask the user for a filepath.
typedef struct te_file_dialog te_file_dialog;
struct te_world;

enum te_file_dialog_mode {
    TE_FDM_SELECT_DIR,
    TE_FDM_SELECT_EXISTING_FILE,
    TE_FDM_SELECT_NEW_FILE,
};

// Displays a file explorer to select a file/directory.
// Do not free the path variable passed to you in the callback.
// It's safe to destroy file dialog in callbacks.
te_file_dialog* file_dialog_create(
    struct te_world* world, void* custom, void (*on_selected)(void* custom, const char* path),
    void (*on_cancel)(void* custom), enum te_file_dialog_mode mode);
void file_dialog_destroy(te_file_dialog* file_dialog);
