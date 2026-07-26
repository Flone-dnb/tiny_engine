//
// This file provides various helper functions for working with wchar_t* strings.
//

#pragma once

#include <wchar.h>

// dst_strlen will store strlen() or the returned string.
// Specify NULL as `dst_strlen` if you don't need it.
// You must free returned pointer when you no longer need it.
char* wchar_to_char(const wchar_t* src, unsigned int* dst_strlen);

// dst_strlen will store wcslen() of the returned string.
// Specify NULL as `dst_strlen` if you don't need it.
// You must free returned pointer when you no longer need it.
wchar_t* wchar_from_char(const char* src, unsigned int* dst_strlen);
