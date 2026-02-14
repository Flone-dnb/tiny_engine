//
// This file provides various helper functions for working with char16_t* strings.
//

#pragma once

#include <uchar.h>

unsigned int char16_strlen(const char16_t* text);

// You must free returned pointer when you no longer need it.
char* char16_to_char(const char16_t* src, unsigned int src_strlen, unsigned int* dst_strlen);
char16_t* char16_from_char(const char* src, unsigned int src_strlen, unsigned int* dst_strlen);
