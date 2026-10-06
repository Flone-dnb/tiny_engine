#include <io/config.h>

#include <snprintf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io/filesystem.h>
#include <io/log.h>
#include <misc/globals.h>

static const unsigned int init_section_item_array_size = 16;
static const unsigned int init_section_array_size = 8;
static const unsigned int init_value_per_type_array_size = 32;

static const unsigned int section_item_expand_count = 16;
static const unsigned int section_expand_count = 32;
static const unsigned int value_per_type_expand_count = 64;

static bool
prv_config_getline(char** lineptr, size_t* len, FILE* stream) {
    const size_t min_buffer_size = 256;
    char* new_ptr = NULL;
    size_t buf_size = 0;
    size_t pos = 0;
    int c;

    c = getc(stream);
    if (c == EOF) {
        return false;
    }

    if (*lineptr == NULL) {
        *lineptr = malloc(min_buffer_size);
        if (*lineptr == NULL) {
            return false;
        }
        buf_size = min_buffer_size;
    }

    pos = 0;
    while (c != EOF) {
        if (pos + 1 >= buf_size) {
            size_t new_size = buf_size + (buf_size >> 2);

            if (new_size < min_buffer_size) {
                new_size = min_buffer_size;
            }

            if (new_size <= buf_size) {
                return false;
            }

            new_ptr = realloc(*lineptr, new_size);
            if (new_ptr == NULL) {
                return false;
            }
            buf_size = new_size;
            *lineptr = new_ptr;
        }

        ((unsigned char*)(*lineptr))[pos++] = (unsigned char)c;
        if (c == '\n') {
            break;
        }
        c = getc(stream);
    }

    (*lineptr)[pos] = 0;

    if (c == EOF && !feof(stream)) {
        *len = pos;
        return true;
    }

    *len = pos;
    return true;
}

enum te_config_value_type { TE_CVT_BOOL, TE_CVT_UINT, TE_CVT_FLOAT, TE_CVT_STRING };

/* stores information about a "key"-"value" pair from the config */
typedef struct te_config_item {
    /* name of the value (key) */
    char* name;

    /* starting index into the global array values */
    unsigned int value_start_index;

    /* number of elements starting from @ref value_start_index */
    unsigned int value_count;

    /* type of the value */
    enum te_config_value_type type;

    /* strlen of @ref name */
    unsigned int name_len;
} te_config_item;

/* a group of "key"-"value" pairs */
typedef struct te_config_section {
    char* name;
    te_config_item* items;

    unsigned int item_count;
    unsigned int item_array_size;
} te_config_section;

/* a group of sections */
struct te_config {
    te_config_section* sections;

    char** strings;
    unsigned int* uints;
    float* floats;
    bool* bools;

    /* actual (used) item count */
    unsigned int section_count;
    unsigned int uint_count;
    unsigned int float_count;
    unsigned int string_count;
    unsigned int bool_count;

    /* total number of items that arrays can fit */
    unsigned int section_array_size;
    unsigned int uint_array_size;
    unsigned int float_array_size;
    unsigned int string_array_size;
    unsigned int bool_array_size;
};

static void prv_config_load(te_config* config, const char* relative_path);

te_config*
config_create(const char* opt_relative_path_to_load) {
    te_config* config = malloc(sizeof(te_config));

    config->section_count = 0;
    config->uint_count = 0;
    config->float_count = 0;
    config->string_count = 0;
    config->bool_count = 0;

    config->section_array_size = init_section_array_size;
    config->uint_array_size = init_value_per_type_array_size;
    config->float_array_size = init_value_per_type_array_size;
    config->string_array_size = init_value_per_type_array_size;
    config->bool_array_size = init_value_per_type_array_size;

    config->sections = malloc(sizeof(te_config_section) * config->section_array_size);
    config->uints = malloc(sizeof(unsigned int) * config->uint_array_size);
    config->floats = malloc(sizeof(float) * config->float_array_size);
    config->strings = malloc(sizeof(char*) * config->string_array_size);
    config->bools = malloc(sizeof(bool) * config->bool_array_size);

    if (opt_relative_path_to_load != NULL) {
        prv_config_load(config, opt_relative_path_to_load);
    }

    return config;
}

void
config_destroy(te_config* config) {
    unsigned int i, j;

    if (config->section_count > 0) {
        for (i = 0; i < config->section_count; i++) {
            te_config_section* section = &config->sections[i];

            free(section->name);

            for (j = 0; j < section->item_count; j++) {
                free(section->items[j].name);
            }
            free(section->items);
        }
    }

    free(config->sections);
    free(config->uints);
    free(config->floats);
    free(config->bools);
    for (i = 0; i < config->string_count; i++) {
        free(config->strings[i]);
    }
    free(config->strings);

    free(config);
}

unsigned int
config_create_section(te_config* config, const char* name) {
    te_config_section* new_sections;
    te_config_section* section;
    size_t name_len;
    size_t i;
    unsigned int section_idx;

    if (name == NULL) {
        log_error(__FILE__, __LINE__, "NULL section name specified");
        abort();
    }

    /* check name */
    name_len = strlen(name);
    for (i = 0; i < name_len; i++) {
        if ((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z')
            || (name[i] >= '0' && name[i] <= '9') || name[i] == '.' || name[i] == '_') {
            continue;
        }

        log_error_fmt(
            __FILE__, __LINE__, "section name \"%s\" contains forbidden characters", name);
        abort();
    }

    if (config->section_count == config->section_array_size) {
        /* expand array */
        config->section_array_size += section_expand_count;
        new_sections = malloc(sizeof(te_config_section) * config->section_array_size);
        memcpy(
            new_sections, config->sections, sizeof(te_config_section) * config->section_count);

        free(config->sections);
        config->sections = new_sections;
    }

    /* init section. */
    section_idx = config->section_count;
    section = &config->sections[section_idx];
    config->section_count += 1;

    section->item_count = 0;
    section->item_array_size = init_section_item_array_size;
    section->items = malloc(sizeof(te_config_item) * section->item_array_size);

    section->name = malloc(sizeof(char) * (name_len + 1));
    memcpy(section->name, name, sizeof(char) * name_len);
    section->name[name_len] = 0;

    return section_idx;
}

static void
prv_config_section_expand_items(te_config_section* section) {
    te_config_item* new_items;

    section->item_array_size += section_item_expand_count;

    new_items = malloc(sizeof(te_config_item) * section->item_array_size);
    memcpy(new_items, section->items, sizeof(te_config_item) * section->item_count);

    free(section->items);
    section->items = new_items;
}

static void
prv_config_check_item_name(const char* key, size_t key_len) {
    size_t i;
    for (i = 0; i < key_len; i++) {
        if ((key[i] >= 'A' && key[i] <= 'Z') || (key[i] >= 'a' && key[i] <= 'z')
            || (key[i] >= '0' && key[i] <= '9') || key[i] == '.' || key[i] == '_') {
            continue;
        }

        log_error_fmt(
            __FILE__, __LINE__, "key name \"%s\" contains forbidden characters", key);
        abort();
    }
}

void
config_section_set_bool(
    te_config* config, unsigned int section_idx, const char* key, bool value) {
    config_section_set_bool_array(config, section_idx, key, &value, 1);
}

void
config_section_set_uint(
    te_config* config, unsigned int section_idx, const char* key, unsigned int value) {
    config_section_set_uint_array(config, section_idx, key, &value, 1);
}

void
config_section_set_float(
    te_config* config, unsigned int section_idx, const char* key, float value) {
    config_section_set_float_array(config, section_idx, key, &value, 1);
}

void
config_section_set_string(
    te_config* config, unsigned int section_idx, const char* key, const char* value) {
    config_section_set_string_array(config, section_idx, key, (char**)&value, 1);
}

void
config_section_set_bool_array(
    te_config* config, unsigned int section_idx, const char* key, bool* values,
    unsigned int value_count) {
    te_config_section* section;
    te_config_item* new_item;
    bool* new_bools;
    size_t key_len;
    unsigned int i;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif

    section = &config->sections[section_idx];

    /* check key name */
    key_len = strlen(key);
    prv_config_check_item_name(key, key_len);

#if defined(DEBUG)
    /* check that section name is unique */
    for (i = 0; i < section->item_count; i++) {
        if (strcmp(section->name, key) == 0) {
            log_error_fmt(
                __FILE__, __LINE__,
                "the section \"%s\" already has a key with the name \"%s\"", section->name,
                key);
            abort();
        }
    }
#endif

    if (section->item_count == section->item_array_size) {
        prv_config_section_expand_items(section);
    }

    if (config->bool_count + value_count > config->bool_array_size) {
        /* expand array */
        config->bool_array_size += value_count + value_per_type_expand_count;

        new_bools = malloc(sizeof(bool) * config->bool_array_size);
        memcpy(new_bools, config->bools, sizeof(bool) * config->bool_count);

        free(config->bools);
        config->bools = new_bools;
    }

    new_item = &section->items[section->item_count];
    section->item_count += 1;

    new_item->type = TE_CVT_BOOL;

    /* copy name */
    new_item->name = malloc(sizeof(char) * (key_len + 1));
    memcpy(new_item->name, key, sizeof(char) * key_len);
    new_item->name[key_len] = 0;
    new_item->name_len = (unsigned int)key_len;

    /* copy values */
    new_item->value_start_index = config->bool_count;
    new_item->value_count = value_count;
    memcpy(config->bools + new_item->value_start_index, values, sizeof(bool) * value_count);

    config->bool_count += value_count;
}

void
config_section_set_uint_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* values,
    unsigned int value_count) {
    unsigned int* new_uints;
    te_config_section* section;
    te_config_item* new_item;
    size_t key_len;
    unsigned int i;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif
    section = &config->sections[section_idx];

    /* check key name */
    key_len = strlen(key);
    prv_config_check_item_name(key, key_len);

#if defined(DEBUG)
    /* check that section name is unique */
    for (i = 0; i < section->item_count; i++) {
        if (strcmp(section->name, key) == 0) {
            log_error_fmt(
                __FILE__, __LINE__,
                "the section \"%s\" already has a key with the name \"%s\"", section->name,
                key);
            abort();
        }
    }
#endif

    if (section->item_count == section->item_array_size) {
        prv_config_section_expand_items(section);
    }

    if (config->uint_count + value_count > config->uint_array_size) {
        /* expand array */
        config->uint_array_size += value_count + value_per_type_expand_count;

        new_uints = malloc(sizeof(unsigned int) * config->uint_array_size);
        memcpy(new_uints, config->uints, sizeof(unsigned int) * config->uint_count);

        free(config->uints);
        config->uints = new_uints;
    }

    new_item = &section->items[section->item_count];
    section->item_count += 1;

    new_item->type = TE_CVT_UINT;

    /* copy name */
    new_item->name = malloc(sizeof(char) * (key_len + 1));
    memcpy(new_item->name, key, sizeof(char) * key_len);
    new_item->name[key_len] = 0;
    new_item->name_len = (unsigned int)key_len;

    /* copy values */
    new_item->value_start_index = config->uint_count;
    new_item->value_count = value_count;
    memcpy(
        config->uints + new_item->value_start_index, values,
        sizeof(unsigned int) * value_count);

    config->uint_count += value_count;
}

void
config_section_set_float_array(
    te_config* config, unsigned int section_idx, const char* key, float* values,
    unsigned int value_count) {
    float* new_floats;
    te_config_section* section;
    te_config_item* new_item;
    size_t key_len;
    unsigned int i;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif
    section = &config->sections[section_idx];

    /* check key name */
    key_len = strlen(key);
    prv_config_check_item_name(key, key_len);

#if defined(DEBUG)
    /* check that section name is unique */
    for (i = 0; i < section->item_count; i++) {
        if (strcmp(section->name, key) == 0) {
            log_error_fmt(
                __FILE__, __LINE__,
                "the section \"%s\" already has a key with the name \"%s\"", section->name,
                key);
            abort();
        }
    }
#endif

    if (section->item_count == section->item_array_size) {
        prv_config_section_expand_items(section);
    }

    if (config->float_count + value_count > config->float_array_size) {
        /* expand array */
        config->float_array_size += value_count + value_per_type_expand_count;

        new_floats = malloc(sizeof(float) * config->float_array_size);
        memcpy(new_floats, config->floats, sizeof(float) * config->float_count);

        free(config->floats);
        config->floats = new_floats;
    }

    new_item = &section->items[section->item_count];
    section->item_count += 1;

    new_item->type = TE_CVT_FLOAT;

    /* copy name */
    new_item->name = malloc(sizeof(char) * (key_len + 1));
    memcpy(new_item->name, key, sizeof(char) * key_len);
    new_item->name[key_len] = 0;
    new_item->name_len = (unsigned int)key_len;

    /* copy values */
    new_item->value_start_index = config->float_count;
    new_item->value_count = value_count;
    memcpy(config->floats + new_item->value_start_index, values, sizeof(float) * value_count);

    config->float_count += value_count;
}

void
config_section_set_string_array(
    te_config* config, unsigned int section_idx, const char* key, char** values,
    unsigned int value_count) {
    te_config_section* section;
    te_config_item* new_item;
    char** new_strings;
    size_t key_len;
    size_t value_len;
    size_t i;
    size_t j;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif
    section = &config->sections[section_idx];

    /* check key name */
    key_len = strlen(key);
    prv_config_check_item_name(key, key_len);

    /* check that value does not contain forbidden characters */
    for (i = 0; i < value_count; i++) {
        const char* value = values[i];
        value_len = strlen(value);
        for (j = 0; j < value_len; j++) {
            if (value[j] == '\n' || value[j] == '\r' || value[j] == '"') {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "string value \"%s\" contains forbidden characters, note: string values "
                    "can't contain \" character "
                    "because it's used internally",
                    value);
                abort();
            }
        }
    }

#if defined(DEBUG)
    /* check that section name is unique */
    for (i = 0; i < section->item_count; i++) {
        if (strcmp(section->name, key) == 0) {
            log_error_fmt(
                __FILE__, __LINE__,
                "the section \"%s\" already has a key with the name \"%s\"", section->name,
                key);
            abort();
        }
    }
#endif

    if (section->item_count == section->item_array_size) {
        prv_config_section_expand_items(section);
    }

    if (config->string_count + value_count > config->string_array_size) {
        /* expand array */
        config->string_array_size += value_count + value_per_type_expand_count;

        new_strings = malloc(sizeof(char*) * config->string_array_size);
        memcpy(new_strings, config->strings, sizeof(char*) * config->string_count);

        free(config->strings);
        config->strings = new_strings;
    }

    new_item = &section->items[section->item_count];
    section->item_count += 1;

    new_item->type = TE_CVT_STRING;

    /* copy name */
    new_item->name = malloc(sizeof(char) * (key_len + 1));
    memcpy(new_item->name, key, sizeof(char) * key_len);
    new_item->name[key_len] = 0;
    new_item->name_len = (unsigned int)key_len;

    /* copy values */
    new_item->value_start_index = config->string_count;
    new_item->value_count = value_count;
    for (i = 0; i < value_count; i++) {
        char** new_value = &config->strings[new_item->value_start_index + i];

        value_len = strlen(values[i]);
        (*new_value) = malloc(sizeof(char) * (value_len + 1));

        memcpy((*new_value), values[i], sizeof(char) * value_len);
        (*new_value)[value_len] = 0;
    }

    config->string_count += value_count;
}

unsigned int
config_get_section_count(te_config* config) {
    return config->section_count;
}

const char*
config_section_get_name(te_config* config, unsigned int section_idx) {
#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif
    return config->sections[section_idx].name;
}

bool
config_section_get_bool(
    te_config* config, unsigned int section_idx, const char* key, bool if_not_found) {
    unsigned int count;
    bool* array = config_section_get_bool_array(config, section_idx, key, &count);
    if (count == 0) {
        return if_not_found;
    }
    return array[0];
}

unsigned int
config_section_get_uint(
    te_config* config, unsigned int section_idx, const char* key, unsigned int if_not_found) {
    unsigned int count;
    unsigned int* array = config_section_get_uint_array(config, section_idx, key, &count);
    if (count == 0) {
        return if_not_found;
    }
    return array[0];
}

float
config_section_get_float(
    te_config* config, unsigned int section_idx, const char* key, float if_not_found) {
    unsigned int count;
    float* array = config_section_get_float_array(config, section_idx, key, &count);
    if (count == 0) {
        return if_not_found;
    }
    return array[0];
}

char*
config_section_get_string(
    te_config* config, unsigned int section_idx, const char* key, char* if_not_found) {
    unsigned int count;
    char** array = config_section_get_string_array(config, section_idx, key, &count);
    if (count == 0) {
        return if_not_found;
    }
    return array[0];
}

bool*
config_section_get_bool_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count) {
    unsigned int key_len;
    unsigned int i;
    te_config_section* section;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif

    key_len = (unsigned int)strlen(key);
    section = &config->sections[section_idx];

    for (i = 0; i < section->item_count; i++) {
        if (section->items[i].name_len != key_len) {
            continue;
        }

        if (strcmp(section->items[i].name, key) != 0) {
            continue;
        }

#if defined(DEBUG)
        if (section->items[i].type != TE_CVT_BOOL) {
            log_error_fmt(
                __FILE__, __LINE__, "expected section \"%s\" item \"%s\" to have a bool value",
                section->name, section->items[i].name);
            abort();
        }
#endif

        (*value_count) = section->items[i].value_count;
        return &config->bools[section->items[i].value_start_index];
    }

    (*value_count) = 0;
    return NULL;
}

unsigned int*
config_section_get_uint_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count) {
    unsigned int key_len;
    unsigned int i;
    te_config_section* section;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif

    key_len = (unsigned int)strlen(key);
    section = &config->sections[section_idx];

    for (i = 0; i < section->item_count; i++) {
        if (section->items[i].name_len != key_len) {
            continue;
        }

        if (strcmp(section->items[i].name, key) != 0) {
            continue;
        }

#if defined(DEBUG)
        if (section->items[i].type != TE_CVT_UINT) {
            log_error_fmt(
                __FILE__, __LINE__, "expected section \"%s\" item \"%s\" to have a uint value",
                section->name, section->items[i].name);
            abort();
        }
#endif

        (*value_count) = section->items[i].value_count;
        return &config->uints[section->items[i].value_start_index];
    }

    (*value_count) = 0;
    return NULL;
}

float*
config_section_get_float_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count) {
    unsigned int key_len;
    unsigned int i;
    te_config_section* section;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif

    key_len = (unsigned int)strlen(key);
    section = &config->sections[section_idx];

    for (i = 0; i < section->item_count; i++) {
        if (section->items[i].name_len != key_len) {
            continue;
        }

        if (strcmp(section->items[i].name, key) != 0) {
            continue;
        }

#if defined(DEBUG)
        if (section->items[i].type != TE_CVT_FLOAT) {
            log_error_fmt(
                __FILE__, __LINE__,
                "expected section \"%s\" item \"%s\" to have a float value", section->name,
                section->items[i].name);
            abort();
        }
#endif

        (*value_count) = section->items[i].value_count;
        return &config->floats[section->items[i].value_start_index];
    }

    (*value_count) = 0;
    return NULL;
}

char**
config_section_get_string_array(
    te_config* config, unsigned int section_idx, const char* key, unsigned int* value_count) {
    unsigned int key_len;
    unsigned int i;
    te_config_section* section;

#if defined(DEBUG)
    if (section_idx >= config->section_count) {
        log_error(__FILE__, __LINE__, "the specified config section index is out of range");
        abort();
    }
#endif

    key_len = (unsigned int)strlen(key);
    section = &config->sections[section_idx];

    for (i = 0; i < section->item_count; i++) {
        if (section->items[i].name_len != key_len) {
            continue;
        }

        if (strcmp(section->items[i].name, key) != 0) {
            continue;
        }

#if defined(DEBUG)
        if (section->items[i].type != TE_CVT_STRING) {
            log_error_fmt(
                __FILE__, __LINE__,
                "expected section \"%s\" item \"%s\" to have a string value", section->name,
                section->items[i].name);
            abort();
        }
#endif

        (*value_count) = section->items[i].value_count;
        return &config->strings[section->items[i].value_start_index];
    }

    (*value_count) = 0;
    return NULL;
}

void
config_save(te_config* config, const char* relative_path, bool create_backup) {
    unsigned int path_len;
    char* path_to_config;
    char* path_to_bak;
    FILE* fp;
    te_config_section* section;
    te_config_item* item;
    char temp[256] = {0};
    unsigned int section_idx;
    unsigned int item_idx;
    unsigned int v_idx;
    int i;
    int len;

    if (config->section_count == 0) {
        log_warn(__FILE__, __LINE__, "config has no sections - nothing to save");
        return;
    }

    path_to_config = filesystem_prepend_res_to_path(relative_path, &path_len);

    path_to_bak = malloc(sizeof(char) * (path_len + 4 + 1));
    memcpy(path_to_bak, path_to_config, sizeof(char) * path_len);
    memcpy(path_to_bak + path_len, ".bak", 4);
    path_to_bak[path_len + 4] = 0;

    if (create_backup && filesystem_does_path_exists(path_to_config)) {
        if (filesystem_does_path_exists(path_to_bak)) {
            filesystem_remove_file(path_to_bak);
        }
        filesystem_rename_file(path_to_config, path_to_bak);
    }

    fp = fopen(path_to_config, "w");
    if (fp == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to open the path \"%s\" for writing", path_to_config);
        free(path_to_config);
        abort();
    }

    for (section_idx = 0; section_idx < config->section_count; section_idx++) {
        section = &config->sections[section_idx];

        fprintf(fp, "[%s]\n", section->name);

        for (item_idx = 0; item_idx < section->item_count; item_idx++) {
            item = &section->items[item_idx];

            fprintf(fp, "%s = ", item->name);

            if (item->value_count > 1) {
                fprintf(fp, "%s", "[");
            }

            for (v_idx = 0; v_idx < item->value_count; v_idx++) {
                const unsigned int value_index = item->value_start_index + v_idx;

                switch (item->type) {
                    case (TE_CVT_BOOL): {
                        fprintf(fp, "%s", config->bools[value_index] ? "true" : "false");
                        break;
                    }
                    case (TE_CVT_UINT): {
                        fprintf(fp, "%u", config->uints[value_index]);
                        break;
                    }
                    case (TE_CVT_FLOAT): {
                        len = snprintf(temp, 256, "%.4f", config->floats[value_index]);
#if defined(DEBUG)
                        if (len <= 0) {
                            log_error(__FILE__, __LINE__, "snprintf error");
                            abort();
                        }
#endif
                        for (i = 0; i < len; i++) {
                            if (temp[i] == '.') {
                                break;
                            }
                            if (temp[i] == ',') {
                                temp[i] = '.';
                                break;
                            }
                        }
                        for (i = len - 1; i > 0; i--) { /* remove trailing zeroes */
                            if (temp[i] == '0') {
                                temp[i] = 0;
                            } else if (temp[i] == '.') {
                                if (i + 2 >= len) {
                                    log_error_fmt(
                                        __FILE__, __LINE__,
                                        "unexpected float text \"%s\", found dot at %d with "
                                        "len %d",
                                        temp, i, len);
                                }
                                temp[i + 1] = '0';
                                temp[i + 2] = 0;
                                break;
                            } else {
                                break;
                            }
                        }
                        fprintf(fp, "%s", temp);
                        break;
                    }
                    case (TE_CVT_STRING): {
                        fprintf(fp, "\"%s\"", config->strings[value_index]);
                        break;
                    }
                }

                if (item->value_count > 1 && ((unsigned int)(i + 1) != item->value_count)) {
                    fprintf(fp, "%s", ", ");
                }
            }

            if (item->value_count > 1) {
                fprintf(fp, "%s", "]\n");
            } else {
                fprintf(fp, "%s", "\n");
            }
        }

        if (section_idx + 1 != config->section_count) {
            fprintf(fp, "\n");
        }
    }

    fclose(fp);

    if (create_backup && !filesystem_does_path_exists(path_to_bak)) {
        filesystem_copy_file(path_to_config, path_to_bak);
    }

    free(path_to_config);
    free(path_to_bak);
}

static void
prv_config_load(te_config* config, const char* relative_path) {
    char* line = NULL;
    char** strings;
    float* floats;
    unsigned int* uints;
    char* start;
    char* end;
    size_t line_len;
    size_t key_len;
    size_t str_len;
    size_t value_len;
    size_t i;
    size_t value_start;
    unsigned int item_count;
    unsigned int path_len;
    unsigned int item_idx;
    unsigned int line_num = 0;
    unsigned int section_idx = 0xffffffff;
    FILE* fp;
    char* path_to_config;
    bool is_array;
    bool is_float;

    path_to_config = filesystem_prepend_res_to_path(relative_path, &path_len);

    fp = fopen(path_to_config, "r");
    if (fp == NULL) {
        /* check if backup file exists */
        char* path_to_bak = malloc(sizeof(char) * (path_len + 4 + 1));
        memcpy(path_to_bak, path_to_config, sizeof(char) * path_len);
        memcpy(path_to_bak + path_len, ".bak", 4);
        path_to_bak[path_len + 4] = 0;

        fp = fopen(path_to_bak, "r");
        if (fp == NULL) {
            log_error_fmt(
                __FILE__, __LINE__, "failed to open file at \"%s\" (does file exist?)",
                path_to_config);
            abort();
        }

        /* restore the original file */
        filesystem_copy_file(path_to_bak, path_to_config);

        free(path_to_bak);
    }

    while (prv_config_getline(&line, &line_len, fp)) {
        line_num += 1;

        if (line_len == 0) {
            continue;
        }

        if (line[0] == '[') {
            /* section line */
            unsigned int erase_count = 1; /* '\n' */
            if (line[line_len - 2] == '\r') {
                erase_count += 2; /* '\r' and ']' */
            } else {
                erase_count += 1; /* ']' */
            }

            line[line_len - erase_count] = 0; /* replace ']' with 0 */
            section_idx = config_create_section(config, line + 1);
            continue;
        }

        /* key-value line */
        key_len = 0;
        for (i = 0; i < line_len; i++) {
            if (line[i] == ' ') {
                key_len = i;
                break;
            }
        }
        if (key_len == 0) {
            /* empty line */
            continue;
        }
        line[key_len] = 0; /* null terminated for string */

        if (section_idx == 0xffffffff) {
            log_error_fmt(
                __FILE__, __LINE__,
                "config \"%s\", line %u: found key-value pair without a section",
                path_to_config, line_num);
            abort();
        }

        value_start = key_len + 3;
        item_count = 1;
        is_array = line[value_start] == '[';
        if (is_array) {
            value_start += 1; /* skip '[' */

            for (i = value_start; i < line_len; i++) {
                if (line[i] == ',') {
                    item_count += 1;
                }
            }
        }

        item_idx = 0;

        /* bool value */
        if (line[value_start] == 't' || line[value_start] == 'f') {
            bool* bools = malloc(sizeof(bool) * item_count);

            do {
                if (line[value_start] == 't') {
                    bools[item_idx] = true;
                    value_start += 4;
                } else {
                    bools[item_idx] = false;
                    value_start += 5;
                }

                item_idx += 1;
                value_start += 2; /* skip ", " */
            } while (item_idx < item_count);

            config_section_set_bool_array(config, section_idx, line, bools, item_count);
            free(bools);
            continue;
        }

        /* string value */
        if (line[value_start] == '"') {
            value_start += 1;
            strings = malloc(sizeof(char*) * item_count);

            do {
                str_len = 0;
                for (i = value_start; i < line_len; i++) {
                    if (line[i] == '"') {
                        line[i] = 0; /* null terminated for string */
                        break;
                    }
                    str_len += 1;
                }

                strings[item_idx] = malloc(sizeof(char) * (str_len + 1));
                memcpy(strings[item_idx], line + value_start, sizeof(char) * str_len);
                strings[item_idx][str_len] = 0;

                value_start += str_len + 1; /* plus skip '"' */
                item_idx += 1;
                value_start += 3; /* skip ", "" */
            } while (item_idx < item_count);

            config_section_set_string_array(config, section_idx, line, strings, item_count);
            for (i = 0; i < item_count; i++) {
                free(strings[i]);
            }
            free(strings);
            continue;
        }

        /* determine if float or uint */
        is_float = false;
        if (line[value_start] == '-') {
            is_float = true;
        } else {
            for (i = value_start + 1; i < line_len; i++) {
                if (line[i] == '.') {
                    is_float = true;
                    break;
                }
                if (line[i] < '0' || line[i] > '9') {
                    break;
                }
            }
        }

        if (is_float) {
            floats = malloc(sizeof(float) * item_count);

            do {
                start = line + value_start;
                end = NULL;
                floats[item_idx] = globals_convert_string_to_float(start, &end);
                if (start == end) {
                    log_error_fmt(
                        __FILE__, __LINE__,
                        "config \"%s\", line %u: failed to convert value to float",
                        path_to_config, line_num);
                    abort();
                }

                value_len = (size_t)(end - start);

                item_idx += 1;
                value_start += value_len + 2; /* skip ", " */
            } while (item_idx < item_count);

            config_section_set_float_array(config, section_idx, line, floats, item_count);
            free(floats);
            continue;
        } else {
            uints = malloc(sizeof(unsigned int) * item_count);

            do {
                start = line + value_start;
                end = NULL;
                uints[item_idx] = (unsigned int)strtoul(start, &end, 10);
                if (start == end) {
                    log_error_fmt(
                        __FILE__, __LINE__,
                        "config \"%s\", line %u: failed to convert value to unsigned int",
                        path_to_config, line_num);
                    abort();
                }

                value_len = (size_t)(end - start);

                item_idx += 1;
                value_start += value_len + 2; /* skip ", " */
            } while (item_idx < item_count);

            config_section_set_uint_array(config, section_idx, line, uints, item_count);
            free(uints);
            continue;
        }
    }
    free(line);

    fclose(fp);
    free(path_to_config);
}
