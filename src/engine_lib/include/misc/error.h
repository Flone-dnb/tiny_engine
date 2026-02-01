#pragma once

// Error object groups an error message and a file/line where the error occurred.
typedef struct te_error te_error;

// Creates and destroys an error object.
#define error_create(message) prv_error_create(message, __FILE__, __LINE__)
void error_destroy(te_error* err);

// Shows the error message and aborts the program.
void error_show_and_abort(te_error* err);
void show_error_and_abort(const char* message);

// Converts GL error to text, shows it and aborts the program.
void show_gl_error_and_abort(unsigned int gl_erorr);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Creates a new error object that stores the specified message.
te_error* prv_error_create(const char* message, char* file, int line);
