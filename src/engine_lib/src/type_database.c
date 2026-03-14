#include <type_database.h>

#include <stdlib.h>
#include <string.h>
#include <game/camera.h>
#include <game/model.h>
#include <hashmap.c/hashmap.h>
#include <io/log.h>
#include <io/config.h>
#include <misc/wchar_funcs.h>
#include <widget/button_widget.h>
#include <widget/checkbox_widget.h>
#include <widget/progress_widget.h>
#include <widget/rect_widget.h>
#include <widget/slider_widget.h>
#include <widget/text_edit_widget.h>
#include <widget/text_widget.h>
#include <widget/vbox_widget.h>

typedef struct te_type_database {
    struct hashmap* types;
} te_type_database;

// Static for ease of access.
static te_type_database type_database;

// Command hash for hashmap.
static uint64_t
prv_type_info_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const te_type_info* const* info = item;
    return hashmap_sip((*info)->id, strlen((*info)->id), seed0, seed1);
}

// Command compare for hashmap.
static int
prv_type_info_compare(const void* a, const void* b, void* udata) {
    (void)udata;
    const te_type_info* const* info1 = a;
    const te_type_info* const* info2 = b;
    return strcmp((*info1)->id, (*info2)->id);
}

void
prv_type_database_init(void) {
    type_database.types = hashmap_new(
        sizeof(te_type_info*), 4, 0, 0, prv_type_info_hash, prv_type_info_compare, NULL, NULL);

    // Register engine types.
    model_register_type();
    camera_register_type();
    button_widget_register_type();
    checkbox_widget_register_type();
    progress_widget_register_type();
    rect_widget_register_type();
    slider_widget_register_type();
    text_edit_widget_register_type();
    text_widget_register_type();
    vbox_widget_register_type();
}

te_type_info*
type_info_create(
    const char* id, void* (*create)(void), void (*spawn)(struct te_world* world, void* obj),
    struct te_widget* (*get_widget)(void*)) {
    te_type_info* info = malloc(sizeof(te_type_info));

    info->create = create;
    info->spawn = spawn;
    info->get_widget = get_widget;

    info->id = id;
    info->variables = NULL;
    info->variable_count = 0;

    info->bool_setters = NULL;
    info->bool_getters = NULL;
    info->bool_count = 0;

    info->uint_setters = NULL;
    info->uint_getters = NULL;
    info->uint_count = 0;

    info->float_setters = NULL;
    info->float_getters = NULL;
    info->float_count = 0;

    info->vec2_setters = NULL;
    info->vec2_getters = NULL;
    info->vec2_count = 0;

    info->vec3_setters = NULL;
    info->vec3_getters = NULL;
    info->vec3_count = 0;

    info->vec4_setters = NULL;
    info->vec4_getters = NULL;
    info->vec4_count = 0;

    info->string_setters = NULL;
    info->string_getters = NULL;
    info->string_count = 0;

    info->wstring_setters = NULL;
    info->wstring_getters = NULL;
    info->wstring_count = 0;

    // NOTE: add new variables to type_info_save_to_config and type_info_load_from_config

    return info;
}

void
prv_type_database_deinit(void) {
    size_t iter = 0;
    void* item;
    while (hashmap_iter(type_database.types, &iter, &item)) {
        const te_type_info** ptr = item;
        te_type_info* info = (te_type_info*)*ptr;
        free(info->variables);
        free(info->bool_setters);
        free(info->bool_getters);
        free(info->uint_setters);
        free(info->uint_getters);
        free(info->float_setters);
        free(info->float_getters);
        free(info->vec2_setters);
        free(info->vec2_getters);
        free(info->vec3_setters);
        free(info->vec3_getters);
        free(info->vec4_setters);
        free(info->vec4_getters);
        free(info->string_setters);
        free(info->string_getters);
        free(info->wstring_setters);
        free(info->wstring_getters);

        free(info);
    }
    hashmap_clear(type_database.types, true);

    hashmap_free(type_database.types);
    type_database.types = NULL;
}

#define TYPE_INFO_ALLOC_VARIABLE(                                                             \
    info, var_name, var_type, type_var_count, setters, getters, new_setter, new_getter)       \
    if (info->variable_count == 0xffff) {                                                     \
        log_error("reached variable limit");                                                  \
        abort();                                                                              \
    }                                                                                         \
    te_variable_info* new_variables =                                                         \
        malloc(sizeof(te_variable_info) * (info->variable_count + 1));                        \
    memcpy(new_variables, info->variables, sizeof(te_variable_info) * info->variable_count);  \
                                                                                              \
    free(info->variables);                                                                    \
    info->variables = new_variables;                                                          \
                                                                                              \
    info->variable_count += 1;                                                                \
                                                                                              \
    void* new_setters = malloc(sizeof(void*) * (type_var_count + 1));                         \
    memcpy(new_setters, setters, sizeof(void*) * type_var_count);                             \
                                                                                              \
    free(setters);                                                                            \
    setters = new_setters;                                                                    \
    setters[type_var_count] = new_setter;                                                     \
                                                                                              \
    void* new_getters = malloc(sizeof(void*) * (type_var_count + 1));                         \
    memcpy(new_getters, getters, sizeof(void*) * type_var_count);                             \
                                                                                              \
    free(getters);                                                                            \
    getters = new_getters;                                                                    \
    getters[type_var_count] = new_getter;                                                     \
                                                                                              \
    type_var_count += 1;                                                                      \
                                                                                              \
    te_variable_info* var_info = &info->variables[info->variable_count - 1];                  \
    var_info->name = name;                                                                    \
    var_info->type = var_type;                                                                \
    var_info->set_get_index = type_var_count - 1;

void
type_info_add_bool_variable(
    te_type_info* info, const char* name, te_bool_setter setter, te_bool_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_BOOL, info->bool_count, info->bool_setters, info->bool_getters,
        setter, getter);
}

void
type_info_add_uint_variable(
    te_type_info* info, const char* name, te_uint_setter setter, te_uint_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_UINT, info->uint_count, info->uint_setters, info->uint_getters,
        setter, getter);
}

void
type_info_add_float_variable(
    te_type_info* info, const char* name, te_float_setter setter, te_float_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_FLOAT, info->float_count, info->float_setters, info->float_getters,
        setter, getter);
}

void
type_info_add_vec2_variable(
    te_type_info* info, const char* name, te_vec2_setter setter, te_vec2_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_VEC2, info->vec2_count, info->vec2_setters, info->vec2_getters,
        setter, getter);
}

void
type_info_add_vec3_variable(
    te_type_info* info, const char* name, te_vec3_setter setter, te_vec3_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_VEC3, info->vec3_count, info->vec3_setters, info->vec3_getters,
        setter, getter);
}

void
type_info_add_vec4_variable(
    te_type_info* info, const char* name, te_vec4_setter setter, te_vec4_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_VEC4, info->vec4_count, info->vec4_setters, info->vec4_getters,
        setter, getter);
}

void
type_info_add_string_variable(
    te_type_info* info, const char* name, te_string_setter setter, te_string_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_STRING, info->string_count, info->string_setters,
        info->string_getters, setter, getter);
}

void
type_info_add_wstring_variable(
    te_type_info* info, const char* name, te_wstring_setter setter, te_wstring_getter getter) {
    TYPE_INFO_ALLOC_VARIABLE(
        info, name, TE_VT_WSTRING, info->wstring_count, info->wstring_setters,
        info->wstring_getters, setter, getter);
}

unsigned int
type_info_save_to_config(const te_type_info* type_info, te_config* config, void* obj) {
    const unsigned int section_idx = config_create_section(config, type_info->id);

    for (unsigned int var_idx = 0; var_idx < type_info->variable_count; var_idx++) {
        te_variable_info* var_info = &type_info->variables[var_idx];
        switch (var_info->type) {
            case (TE_VT_BOOL): {
                config_section_set_bool(
                    config, section_idx, var_info->name,
                    type_info->bool_getters[var_info->set_get_index](obj));
                break;
            }
            case (TE_VT_UINT): {
                config_section_set_uint(
                    config, section_idx, var_info->name,
                    type_info->uint_getters[var_info->set_get_index](obj));
                break;
            }
            case (TE_VT_FLOAT): {
                config_section_set_float(
                    config, section_idx, var_info->name,
                    type_info->float_getters[var_info->set_get_index](obj));
                break;
            }
            case (TE_VT_STRING): {
                const char* text = type_info->string_getters[var_info->set_get_index](obj);
                if (text != NULL) {
                    config_section_set_string(config, section_idx, var_info->name, text);
                }
                break;
            }
            case (TE_VT_WSTRING): {
                const wchar_t* src_text =
                    type_info->wstring_getters[var_info->set_get_index](obj);
                if (src_text != NULL) {
                    unsigned int text_len;
                    char* text = wchar_to_char(src_text, &text_len);
                    config_section_set_string(config, section_idx, var_info->name, text);
                    free(text);
                }
                break;
            }
            case (TE_VT_VEC2): {
                vec2 val;
                type_info->vec2_getters[var_info->set_get_index](obj, val);
                config_section_set_float_array(config, section_idx, var_info->name, val, 2);
                break;
            }
            case (TE_VT_VEC3): {
                vec3 val;
                type_info->vec3_getters[var_info->set_get_index](obj, val);
                config_section_set_float_array(config, section_idx, var_info->name, val, 3);
                break;
            }
            case (TE_VT_VEC4): {
                vec4 val;
                type_info->vec4_getters[var_info->set_get_index](obj, val);
                config_section_set_float_array(config, section_idx, var_info->name, val, 4);
                break;
            }
        }
    }

    return section_idx;
}

void
type_info_load_from_config(
    const te_type_info* type_info, te_config* config, unsigned int section_idx, void* obj) {
    for (unsigned int var_idx = 0; var_idx < type_info->variable_count; var_idx++) {
        te_variable_info* var_info = &type_info->variables[var_idx];
        switch (var_info->type) {
            case (TE_VT_BOOL): {
                type_info->bool_setters[var_info->set_get_index](
                    obj, config_section_get_bool(
                             config, section_idx, var_info->name,
                             type_info->bool_getters[var_info->set_get_index](obj)));
                break;
            }
            case (TE_VT_UINT): {
                type_info->uint_setters[var_info->set_get_index](
                    obj, config_section_get_uint(
                             config, section_idx, var_info->name,
                             type_info->uint_getters[var_info->set_get_index](obj)));
                break;
            }
            case (TE_VT_FLOAT): {
                type_info->float_setters[var_info->set_get_index](
                    obj, config_section_get_float(
                             config, section_idx, var_info->name,
                             type_info->float_getters[var_info->set_get_index](obj)));
                break;
            }
            case (TE_VT_STRING): {
                char* val =
                    config_section_get_string(config, section_idx, var_info->name, NULL);
                if (val != NULL) {
                    type_info->string_setters[var_info->set_get_index](obj, val);
                }
                break;
            }
            case (TE_VT_WSTRING): {
                char* val =
                    config_section_get_string(config, section_idx, var_info->name, NULL);
                if (val != NULL) {
                    unsigned int len;
                    wchar_t* text = wchar_from_char(val, &len);
                    type_info->wstring_setters[var_info->set_get_index](obj, text);
                    free(text);
                }
                break;
            }
            case (TE_VT_VEC2): {
                unsigned int count;
                float* val = config_section_get_float_array(
                    config, section_idx, var_info->name, &count);
                if (count != 2) {
                    log_warn_fmt(
                        "variable \"%s\" of section with index %u has unexpected array size "
                        "in the config, expected 2 got %u, "
                        "ignoring this variable",
                        var_info->name, section_idx, count);
                    continue;
                }
                type_info->vec2_setters[var_info->set_get_index](obj, val);
                break;
            }
            case (TE_VT_VEC3): {
                unsigned int count;
                float* val = config_section_get_float_array(
                    config, section_idx, var_info->name, &count);
                if (count != 3) {
                    log_warn_fmt(
                        "variable \"%s\" of section with index %u has unexpected array size "
                        "in the config, expected 3 got %u, "
                        "ignoring this variable",
                        var_info->name, section_idx, count);
                    continue;
                }
                type_info->vec3_setters[var_info->set_get_index](obj, val);
                break;
            }
            case (TE_VT_VEC4): {
                unsigned int count;
                float* val = config_section_get_float_array(
                    config, section_idx, var_info->name, &count);
                if (count != 4) {
                    log_warn_fmt(
                        "variable \"%s\" of section with index %u has unexpected array size "
                        "in the config, expected 4 got %u, "
                        "ignoring this variable",
                        var_info->name, section_idx, count);
                    continue;
                }
                type_info->vec4_setters[var_info->set_get_index](obj, val);
                break;
            }
        }
    }
}

void
type_database_register_type(te_type_info* info) {
    if (type_database.types == NULL) {
        log_error("type database is not initialized yet or was already deinitialized");
        abort();
    }

    const te_type_info* const* found = hashmap_get(type_database.types, &info);
    if (found != NULL) {
        log_error("a type with the specified ID is already registered");
        abort();
    }

    hashmap_set(type_database.types, &info);
}

const te_type_info*
type_database_get_type_info(const char* id) {
    if (type_database.types == NULL) {
        log_error("type database is not initialized yet or was already deinitialized");
        abort();
    }

    te_type_info* test = type_info_create(id, NULL, NULL, NULL);
    const te_type_info* const* found = hashmap_get(type_database.types, &test);
    free(test);

    if (found == NULL) {
        return NULL;
    }

    return *found;
}

const char**
type_database_get_all_type_ids(unsigned int* count) {
    (*count) = (unsigned int)hashmap_count(type_database.types);
    if ((*count) == 0) {
        return NULL;
    }

    const char** array = malloc(sizeof(const char*) * (*count));

    size_t iter = 0;
    size_t item_idx = 0;
    void* item;
    while (hashmap_iter(type_database.types, &iter, &item)) {
        const te_type_info** ptr = item;
        te_type_info* info = (te_type_info*)*ptr;

        array[item_idx] = info->id;
        item_idx += 1;
    }

    return array;
}
