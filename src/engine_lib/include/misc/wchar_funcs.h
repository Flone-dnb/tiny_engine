//
// This file provides various helper functions for working with wchar_t* strings.
//

#pragma once

#include <wchar.h>

// You must free returned pointer when you no longer need it.
char* wchar_to_char(const wchar_t* src, unsigned int* dst_strlen);
wchar_t* wchar_from_char(const char* src, unsigned int* dst_strlen);
