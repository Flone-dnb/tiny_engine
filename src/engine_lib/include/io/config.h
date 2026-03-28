// This file provides functionality for saving and loading config files.
// The config file has a custom file format which is similar to INI format but much simpler:
// - section names are not required to be unique (but key names should be unique within a section)
// - supported a few primitive value types and arrays (example: "key = [..., ...]")

#pragma once

#include <stdbool.h>

typedef struct te_config te_config;

// Creates a new empty config. Specify NULL as path to create a new config, otherwise
// specify a path to the file (relative to the "res" directory) to load.
// Returned pointer must be later destroyed using @ref config_destroy.
te_config* config_create(const char* opt_relative_path_to_load);
void config_destroy(te_config* config);

// Creates a new section in the specified config file.
// The name string will be copied to the section's data.
// Section name must only contain characters A-Z, a-z, 0-9, '.' and '_'.
// Returns index of the section.
unsigned int config_create_section(te_config* config, const char* name);

// Sets a value with a unique name (unique within the section) to a config's section.
// Key names must only contain characters A-Z, a-z, 0-9, '.' and '_'.
void config_section_set_bool(
    te_config* config, unsigned int section_idx, const char* key, bool value);
void config_section_set_uint(
    te_config* config, unsigned int section_idx, const char* key, unsigned int value);
void config_section_set_float(
    te_config* config, unsigned int section_idx, const char* key, float value);
void config_section_set_string(
    te_config* config, unsigned int section_idx, const char* key, const char* value);

void config_section_set_bool_array(
    te_config* config, unsigned int section_idx, const char* key, bool* values,
    unsigned int value_count);
void config_section_set_uint_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* values,
    unsigned int value_count);
void config_section_set_float_array(
    te_config* config, unsigned int section_idx, const char* key, float* values,
    unsigned int value_count);
void config_section_set_string_array(
    te_config* config, unsigned int section_idx, const char* key, char** values,
    unsigned int value_count);

unsigned int config_get_section_count(te_config* config);
const char* config_section_get_name(te_config* config, unsigned int section_idx);

bool config_section_get_bool(
    te_config* config, unsigned int section_idx, const char* key, bool if_not_found);
unsigned int config_section_get_uint(
    te_config* config, unsigned int section_idx, const char* key, unsigned int if_not_found);
float config_section_get_float(
    te_config* config, unsigned int section_idx, const char* key, float if_not_found);
// Do not free returned pointer. The pointer may become invalid after new strings are added.
char* config_section_get_string(
    te_config* config, unsigned int section_idx, const char* key, char* if_not_found);

// Do not free returned pointer. The pointer may become invalid after new values are added.
// Value count is set to 0 if not found.
bool* config_section_get_bool_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count);
unsigned int* config_section_get_uint_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count);
float* config_section_get_float_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count);
char** config_section_get_string_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count);

// Serializes the config to the specified path relative to the "res" directory.
void config_save(te_config* config, const char* relative_path, bool create_backup);
