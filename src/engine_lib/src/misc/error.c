#include "misc/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL3/SDL_messagebox.h"
#include "glad/glad.h"
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
show_gl_error_and_abort(unsigned int gl_erorr) {
    switch (gl_erorr) {
        case GL_INVALID_ENUM: show_error_and_abort("GL error: INVALID_ENUM"); break;
        case GL_INVALID_VALUE: show_error_and_abort("GL error: INVALID_VALUE"); break;
        case GL_INVALID_OPERATION: show_error_and_abort("GL error: INVALID_OPERATION"); break;
        case GL_OUT_OF_MEMORY: show_error_and_abort("GL error: OUT_OF_MEMORY"); break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: show_error_and_abort("GL error: INVALID_FRAMEBUFFER_OPERATION"); break;
        default: {
            char error_msg[128] = {0};
            snprintf(&error_msg[0], 128, "GL error: %u", gl_erorr);
            show_error_and_abort(&error_msg[0]);
        } break;
    }
}

void
error_destroy(te_error* err) {
    free(err->message);
    free(err);
}
