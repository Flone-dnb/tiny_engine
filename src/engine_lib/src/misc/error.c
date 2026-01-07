#include "misc/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL3/SDL_messagebox.h"
#include "io/log.h"

te_error*
prv_error_create(const char* message, char* file, int line) {
    char location_info[512] = {0};
    snprintf(&location_info[0], 512, " (%s:%d)", file, line);

    const unsigned long message_len = strlen(message);
    const unsigned long location_info_len = strlen(location_info);
    const unsigned long full_message_len = message_len + location_info_len + 1ul;

    // Init error.
    te_error* err = malloc(sizeof(te_error));
    err->message = malloc(full_message_len);
    memset(err->message, 0, full_message_len);

    // Prepare full message.
    strcpy(err->message, message);
    strcpy(err->message + message_len, location_info);

    return err;
}

void
error_show_and_abort(te_error* err) {
    log_error(err->message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", err->message, NULL);
    abort();
}

void
show_error_and_abort(const char* message) {
    te_error* err = error_create(message);
    error_show_and_abort(err);
    error_destroy(err);
}

void
error_destroy(te_error* err) {
    free(err->message);
    free(err);
}
