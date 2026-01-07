#pragma once

/** Groups error-related information. */
typedef struct {
    /** Error message. */
    char* message;
} te_error;

/** The "public" version of the function. */
#define error_create(message) prv_error_create(message, __FILE__, __LINE__)

/**
 * Shows the error message and aborts the program.
 *
 * @param err Error to show.
 */
void error_show_and_abort(te_error* err);

/**
 * Destroys an error object.
 *
 * @param err Error object.
 */
void error_destroy(te_error* err);

/**
 * Shows the error message and aborts the program.
 *
 * @param message Message to show.
 */
void show_error_and_abort(const char* message);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Creates a new error object that stores the specified message.
 *
 * @param message Message that will be copied to the error object.
 * @param file    Caller filename.
 * @param line    Caller line.
 *
 * @return Created error object.
 */
te_error* prv_error_create(const char* message, char* file, int line);
