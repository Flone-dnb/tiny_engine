#include <game/scene_animation.h>

#include <string.h>
#include <world.h>
#include <game_manager.h>
#include <game/model.h>
#include <game/camera.h>
#include <math_funcs.h>
#include <io/log.h>
#include <cglm/quat.h>
#include <cglm/euler.h>

// Animation of a variable (of some object). Inside a variable keyframes are sorted in time increasing order.
#define SCENE_ANIM_OBJ_VARIABLE_TYPE(type_name)                                               \
    typedef struct te_scene_animation_obj_variable_##type_name {                              \
        char* name;                                                                           \
        /* Sort of like a cached obj ptr to not look for it every frame. Not saved. */        \
        te_variable_info* transient_var_info;                                                 \
        te_scene_animation_keyframe_##type_name* keyframes;                                   \
        unsigned int keyframe_count;                                                          \
    } te_scene_animation_obj_variable_##type_name;
SCENE_ANIM_OBJ_VARIABLE_TYPE(bool);
SCENE_ANIM_OBJ_VARIABLE_TYPE(uint);
SCENE_ANIM_OBJ_VARIABLE_TYPE(float);
SCENE_ANIM_OBJ_VARIABLE_TYPE(vec2);
SCENE_ANIM_OBJ_VARIABLE_TYPE(vec3);
SCENE_ANIM_OBJ_VARIABLE_TYPE(vec4);

// Animated variables of some object.
typedef struct te_scene_animation_obj {
    // Name of an object in the world (object name is supposed to be unique).
    char* name;

    // Sort of like a cached obj ptr to not look for it every frame.
    // Not saved.
    void* transient_obj;
    const te_type_info* transient_obj_type_info;

    te_scene_animation_obj_variable_bool* bools;
    te_scene_animation_obj_variable_uint* uints;
    te_scene_animation_obj_variable_float* floats;
    te_scene_animation_obj_variable_vec2* vec2s;
    te_scene_animation_obj_variable_vec3* vec3s;
    te_scene_animation_obj_variable_vec4* vec4s;

    unsigned int bool_count;
    unsigned int uint_count;
    unsigned int float_count;
    unsigned int vec2_count;
    unsigned int vec3_count;
    unsigned int vec4_count;
} te_scene_animation_obj;

struct te_scene_animation {
    // Always valid.
    te_world* world;

    // NULL if nothing animated.
    te_scene_animation_obj* animated_objects;

    unsigned int animated_object_count;
    float current_time_sec;

    // Duration of the animation. Not saved, calculated at runtime.
    float transient_duration_sec;

    // ID used to unregister tick callback. 0xFFFFFFFF if not registered.
    unsigned int tick_callback_id;

    bool loop;
};

#define SCENE_ANIM_VAR_INIT(var_type)                                                         \
    void scene_animation_var_##var_type##_init(                                               \
        te_scene_animation_obj_variable_##var_type* var, const char* variable_name) {         \
        var->transient_var_info = NULL;                                                       \
        var->keyframe_count = 0;                                                              \
        var->keyframes = NULL;                                                                \
                                                                                              \
        size_t len = strlen(variable_name);                                                   \
        var->name = malloc(sizeof(char) * (len + 1));                                         \
        memcpy(var->name, variable_name, sizeof(char) * len);                                 \
        var->name[len] = 0;                                                                   \
    }
SCENE_ANIM_VAR_INIT(bool)
SCENE_ANIM_VAR_INIT(uint)
SCENE_ANIM_VAR_INIT(float)
SCENE_ANIM_VAR_INIT(vec2)
SCENE_ANIM_VAR_INIT(vec3)
SCENE_ANIM_VAR_INIT(vec4);

#define SCENE_ANIM_VAR_DEINIT(var_type)                                                       \
    void scene_animation_var_##var_type##_deinit(                                             \
        te_scene_animation_obj_variable_##var_type* var) {                                    \
        free(var->keyframes);                                                                 \
        free(var->name);                                                                      \
    }
SCENE_ANIM_VAR_DEINIT(bool)
SCENE_ANIM_VAR_DEINIT(uint)
SCENE_ANIM_VAR_DEINIT(float)
SCENE_ANIM_VAR_DEINIT(vec2)
SCENE_ANIM_VAR_DEINIT(vec3)
SCENE_ANIM_VAR_DEINIT(vec4)

void
scene_animation_obj_init(te_scene_animation_obj* obj, const char* name) {
    size_t len = strlen(name);
    obj->name = malloc(sizeof(char) * (len + 1));
    memcpy(obj->name, name, sizeof(char) * len);
    obj->name[len] = 0;

    obj->transient_obj = NULL;

    obj->bool_count = 0;
    obj->bools = NULL;

    obj->uint_count = 0;
    obj->uints = NULL;

    obj->float_count = 0;
    obj->floats = NULL;

    obj->vec2_count = 0;
    obj->vec2s = NULL;

    obj->vec3_count = 0;
    obj->vec3s = NULL;

    obj->vec4_count = 0;
    obj->vec4s = NULL;
}

void
scene_animation_obj_deinit(te_scene_animation_obj* obj) {
    free(obj->name);

#define DEINIT_VARIABLES(var_type)                                                            \
    for (unsigned int i = 0; i < obj->var_type##_count; i++) {                                \
        scene_animation_var_##var_type##_deinit(&obj->var_type##s[i]);                        \
    }                                                                                         \
    free(obj->var_type##s);

    DEINIT_VARIABLES(bool)
    DEINIT_VARIABLES(uint)
    DEINIT_VARIABLES(float)
    DEINIT_VARIABLES(vec2)
    DEINIT_VARIABLES(vec3)
    DEINIT_VARIABLES(vec4)
}

te_scene_animation*
prv_scene_animation_create(te_world* world) {
    te_scene_animation* scene_animation = malloc(sizeof(te_scene_animation));

    scene_animation->animated_objects = NULL;
    scene_animation->world = world;
    scene_animation->animated_object_count = 0;
    scene_animation->current_time_sec = 0.0f;
    scene_animation->transient_duration_sec = 0.0f;
    scene_animation->tick_callback_id = 0xFFFFFFFF;
    scene_animation->loop = true;

    return scene_animation;
}

void
prv_scene_animation_destroy(te_scene_animation* scene_animation) {
    scene_animation_stop(scene_animation);

    for (unsigned int i = 0; i < scene_animation->animated_object_count; i++) {
        scene_animation_obj_deinit(&scene_animation->animated_objects[i]);
    }
    free(scene_animation->animated_objects);

    free(scene_animation);
}

static void
find_game_obj_recursive(te_game_object_info* info, te_scene_animation_obj* target) {
    const char* obj_name = info->get_name(info->game_object);
    if (obj_name != NULL && strcmp(obj_name, target->name) == 0) {
        target->transient_obj = info->game_object;
        target->transient_obj_type_info = type_database_get_type_info(info->type_id);
        if (target->transient_obj_type_info == NULL) {
            log_error_fmt("failed to get type info for type \"%s\"", info->type_id);
        }
        return;
    }

    // Special case for models.
    if (info->type == TE_GOT_MODEL) {
        unsigned int child_count = model_get_child_model_count(info->game_object);
        for (unsigned int i = 0; i < child_count; i++) {
            te_model* child = model_get_child_model(info->game_object, i);
            if (child != NULL && model_is_serialization_allowed(child)) {
                obj_name = model_get_name(child);
                if (obj_name != NULL && strcmp(obj_name, target->name) == 0) {
                    target->transient_obj = child;
                    target->transient_obj_type_info =
                        type_database_get_type_info(model_get_type_id());
                    return;
                }
            }
        }

        te_camera* camera = model_get_attached_camera(info->game_object);
        if (camera != NULL && camera_is_serialization_allowed(camera)) {
            obj_name = camera_get_name(camera);
            if (obj_name != NULL && strcmp(obj_name, target->name) == 0) {
                target->transient_obj = camera;
                target->transient_obj_type_info =
                    type_database_get_type_info(camera_get_type_id());
                return;
            }
        }
    }
}

static void
cache_obj_and_var(te_scene_animation* anim) {
    unsigned int root_game_obj_count;
    te_game_object_info** root_game_objs =
        world_get_root_game_objects(anim->world, &root_game_obj_count);

    anim->transient_duration_sec = 0.0f;

    for (unsigned int obj_idx = 0; obj_idx < anim->animated_object_count; obj_idx++) {
        te_scene_animation_obj* obj = &anim->animated_objects[obj_idx];

        obj->transient_obj = NULL;
        obj->transient_obj_type_info = NULL;
        for (unsigned int go_idx = 0; go_idx < root_game_obj_count; go_idx++) {
            find_game_obj_recursive(root_game_objs[go_idx], obj);
            if (obj->transient_obj != NULL) {
                break;
            }
        }
        if (obj->transient_obj == NULL || obj->transient_obj_type_info == NULL) {
            log_error_fmt(
                "failed to find a world object with name \"%s\" to animate", obj->name);
            abort();
        }

#define CACHE_TRANSIENT_VAR_INFO(var_type)                                                    \
    for (unsigned int anim_var_idx = 0; anim_var_idx < obj->var_type##_count;                 \
         anim_var_idx++) {                                                                    \
        te_scene_animation_obj_variable_##var_type* var = &obj->var_type##s[anim_var_idx];    \
        var->transient_var_info = NULL;                                                       \
        for (unsigned int var_idx = 0;                                                        \
             var_idx < obj->transient_obj_type_info->variable_count; var_idx++) {             \
            if (strcmp(var->name, obj->transient_obj_type_info->variables[var_idx].name)      \
                != 0) {                                                                       \
                continue;                                                                     \
            }                                                                                 \
            var->transient_var_info = &obj->transient_obj_type_info->variables[var_idx];      \
            break;                                                                            \
        }                                                                                     \
        if (var->transient_var_info == NULL) {                                                \
            log_error_fmt(                                                                    \
                "failed to find variable with name \"%s\" of object \"%s\" to animate",       \
                var->name, obj->name);                                                        \
            abort();                                                                          \
        }                                                                                     \
        if (var->keyframes[var->keyframe_count - 1].time > anim->transient_duration_sec) {    \
            anim->transient_duration_sec = var->keyframes[var->keyframe_count - 1].time;      \
        }                                                                                     \
    }
        CACHE_TRANSIENT_VAR_INFO(bool)
        CACHE_TRANSIENT_VAR_INFO(float)
        CACHE_TRANSIENT_VAR_INFO(uint)
        CACHE_TRANSIENT_VAR_INFO(vec2)
        CACHE_TRANSIENT_VAR_INFO(vec3)
        CACHE_TRANSIENT_VAR_INFO(vec4)
    }

    free(root_game_objs);
}

static void
clear_obj_and_var_cache(te_scene_animation* anim) {
#define CLEAR_TRANSIENT_VAR_INFO(var_type)                                                    \
    for (unsigned int var_idx = 0; var_idx < obj->var_type##_count; var_idx++) {              \
        obj->var_type##s[var_idx].transient_var_info = NULL;                                  \
    }

    for (unsigned int obj_idx = 0; obj_idx < anim->animated_object_count; obj_idx++) {
        te_scene_animation_obj* obj = &anim->animated_objects[obj_idx];
        obj->transient_obj = NULL;
        obj->transient_obj_type_info = NULL;

        CLEAR_TRANSIENT_VAR_INFO(bool)
        CLEAR_TRANSIENT_VAR_INFO(float)
        CLEAR_TRANSIENT_VAR_INFO(uint)
        CLEAR_TRANSIENT_VAR_INFO(vec2)
        CLEAR_TRANSIENT_VAR_INFO(vec3)
        CLEAR_TRANSIENT_VAR_INFO(vec4)
    }

    anim->transient_duration_sec = 0.0f;
}

void
scene_animation_set_is_looping(te_scene_animation* scene_animation, bool loop) {
    scene_animation->loop = loop;
}

static void scene_animation_tick(te_scene_animation* anim, float delta_time_sec);

void
scene_animation_pause(te_scene_animation* scene_animation) {
    if (scene_animation->tick_callback_id == 0xFFFFFFFF) {
        return;
    }

    game_manager_remove_tick_callback(
        world_get_game_manager(scene_animation->world), scene_animation->tick_callback_id);
    scene_animation->tick_callback_id = 0xFFFFFFFF;

    clear_obj_and_var_cache(scene_animation);
}

void
scene_animation_stop(te_scene_animation* scene_animation) {
    if (scene_animation->tick_callback_id == 0xFFFFFFFF) {
        return;
    }

    game_manager_remove_tick_callback(
        world_get_game_manager(scene_animation->world), scene_animation->tick_callback_id);
    scene_animation->tick_callback_id = 0xFFFFFFFF;

    scene_animation->current_time_sec = 0.0f;

    // Reset animated objects.
    scene_animation_tick(scene_animation, 0.0f);

    clear_obj_and_var_cache(scene_animation);
}

bool
scene_animation_is_playing(te_scene_animation* scene_animation) {
    return scene_animation->tick_callback_id != 0xFFFFFFFF;
}

float
scene_animation_get_current_time(te_scene_animation* scene_animation) {
    return scene_animation->current_time_sec;
}

char**
scene_animation_get_object_names(
    te_scene_animation* scene_animation, unsigned int* out_count) {
    (*out_count) = scene_animation->animated_object_count;

    char** names = malloc(sizeof(char*) * scene_animation->animated_object_count);
    for (unsigned int i = 0; i < scene_animation->animated_object_count; i++) {
        names[i] = scene_animation->animated_objects[i].name;
    }

    return names;
}

#define SCENE_ANIM_GET_VAR_NAMES_IMPL(type)                                                   \
    char** scene_animation_get_##type##_variable_names(                                       \
        te_scene_animation* scene_animation, const char* object_name,                         \
        unsigned int* out_count) {                                                            \
        for (unsigned int obj_idx = 0; obj_idx < scene_animation->animated_object_count;      \
             obj_idx++) {                                                                     \
            te_scene_animation_obj* obj = &scene_animation->animated_objects[obj_idx];        \
            if (strcmp(object_name, obj->name) != 0) {                                        \
                continue;                                                                     \
            }                                                                                 \
                                                                                              \
            char** names = malloc(sizeof(char*) * obj->type##_count);                         \
            for (unsigned int i = 0; i < obj->type##_count; i++) {                            \
                names[i] = obj->type##s[i].name;                                              \
            }                                                                                 \
                                                                                              \
            (*out_count) = obj->type##_count;                                                 \
            return names;                                                                     \
        }                                                                                     \
                                                                                              \
        (*out_count) = 0;                                                                     \
        return NULL;                                                                          \
    }
SCENE_ANIM_GET_VAR_NAMES_IMPL(bool)
SCENE_ANIM_GET_VAR_NAMES_IMPL(uint)
SCENE_ANIM_GET_VAR_NAMES_IMPL(float)
SCENE_ANIM_GET_VAR_NAMES_IMPL(vec2)
SCENE_ANIM_GET_VAR_NAMES_IMPL(vec3)
SCENE_ANIM_GET_VAR_NAMES_IMPL(vec4)

static te_scene_animation_obj*
get_obj(te_scene_animation* scene_animation, const char* object_name) {
    for (unsigned int i = 0; i < scene_animation->animated_object_count; i++) {
        if (strcmp(scene_animation->animated_objects[i].name, object_name) == 0) {
            return &scene_animation->animated_objects[i];
        }
    }

    te_scene_animation_obj* new_objs =
        malloc(sizeof(te_scene_animation_obj) * (scene_animation->animated_object_count + 1));
    memcpy(
        new_objs, scene_animation->animated_objects,
        sizeof(te_scene_animation_obj) * scene_animation->animated_object_count);

    free(scene_animation->animated_objects);
    scene_animation->animated_objects = new_objs;

    scene_animation->animated_object_count += 1;

    te_scene_animation_obj* obj =
        &scene_animation->animated_objects[scene_animation->animated_object_count - 1];
    scene_animation_obj_init(obj, object_name);

    return obj;
}

#define SCENE_ANIM_GET_VAR_FUNC_IMPL(var_name)                                                \
    te_scene_animation_obj_variable_##var_name* get_var_##var_name(                           \
        te_scene_animation_obj* obj, const char* variable_name) {                             \
        for (unsigned int i = 0; i < obj->var_name##_count; i++) {                            \
            if (strcmp(obj->var_name##s[i].name, variable_name) == 0) {                       \
                return &obj->var_name##s[i];                                                  \
            }                                                                                 \
        }                                                                                     \
        te_scene_animation_obj_variable_##var_name* new_vars = malloc(                        \
            sizeof(te_scene_animation_obj_variable_##var_name)                                \
            * (obj->var_name##_count + 1));                                                   \
        memcpy(                                                                               \
            new_vars, obj->var_name##s,                                                       \
            sizeof(te_scene_animation_obj_variable_##var_name) * obj->var_name##_count);      \
                                                                                              \
        free(obj->var_name##s);                                                               \
        obj->var_name##s = new_vars;                                                          \
                                                                                              \
        obj->var_name##_count += 1;                                                           \
                                                                                              \
        te_scene_animation_obj_variable_##var_name* var =                                     \
            &obj->var_name##s[obj->var_name##_count - 1];                                     \
        scene_animation_var_##var_name##_init(var, variable_name);                            \
                                                                                              \
        return var;                                                                           \
    }
SCENE_ANIM_GET_VAR_FUNC_IMPL(bool)
SCENE_ANIM_GET_VAR_FUNC_IMPL(uint)
SCENE_ANIM_GET_VAR_FUNC_IMPL(float)
SCENE_ANIM_GET_VAR_FUNC_IMPL(vec2)
SCENE_ANIM_GET_VAR_FUNC_IMPL(vec3)
SCENE_ANIM_GET_VAR_FUNC_IMPL(vec4)

#define SCENE_ANIM_GET_KEYFRAME_FUNC_IMPL(type_name)                                          \
    te_scene_animation_keyframe_##type_name* get_keyframe_##type_name(                        \
        te_scene_animation_obj_variable_##type_name* var, float time) {                       \
        const size_t keyframe_size = sizeof(te_scene_animation_keyframe_##type_name);         \
        unsigned int left_idx = 0;                                                            \
        for (; left_idx < var->keyframe_count;) {                                             \
            if (time > var->keyframes[left_idx].time) {                                       \
                left_idx += 1;                                                                \
                continue;                                                                     \
            }                                                                                 \
            if (left_idx > 0) {                                                               \
                left_idx -= 1;                                                                \
            }                                                                                 \
            break;                                                                            \
        }                                                                                     \
        if (left_idx >= var->keyframe_count) {                                                \
            /* add a new keyframe (last) */                                                   \
            te_scene_animation_keyframe_##type_name* new_frames =                             \
                malloc(keyframe_size * (var->keyframe_count + 1));                            \
            memcpy(new_frames, var->keyframes, keyframe_size * var->keyframe_count);          \
                                                                                              \
            free(var->keyframes);                                                             \
            var->keyframes = new_frames;                                                      \
                                                                                              \
            var->keyframe_count += 1;                                                         \
            return &var->keyframes[var->keyframe_count - 1];                                  \
        } else {                                                                              \
            /* insert a new keyframe */                                                       \
            te_scene_animation_keyframe_##type_name* new_frames =                             \
                malloc(keyframe_size * (var->keyframe_count + 1));                            \
            memcpy(new_frames, var->keyframes, keyframe_size*(left_idx + 1));                 \
            memcpy(                                                                           \
                new_frames + (left_idx + 2), var->keyframes + (left_idx + 1),                 \
                keyframe_size * (var->keyframe_count - (left_idx + 1)));                      \
                                                                                              \
            free(var->keyframes);                                                             \
            var->keyframes = new_frames;                                                      \
                                                                                              \
            var->keyframe_count += 1;                                                         \
            return &var->keyframes[left_idx + 1];                                             \
        }                                                                                     \
    }
SCENE_ANIM_GET_KEYFRAME_FUNC_IMPL(bool)
SCENE_ANIM_GET_KEYFRAME_FUNC_IMPL(uint)
SCENE_ANIM_GET_KEYFRAME_FUNC_IMPL(float)
SCENE_ANIM_GET_KEYFRAME_FUNC_IMPL(vec2)
SCENE_ANIM_GET_KEYFRAME_FUNC_IMPL(vec3)
SCENE_ANIM_GET_KEYFRAME_FUNC_IMPL(vec4);

#define SCENE_ANIM_REMOVE_ALL_KEYFRAMES_IMPL(var_type)                                        \
    void scene_animation_remove_all_keyframes_##var_type(                                     \
        te_scene_animation* scene_animation, const char* object_name,                         \
        const char* variable_name) {                                                          \
        te_scene_animation_obj* obj = get_obj(scene_animation, object_name);                  \
        for (unsigned int var_idx = 0; var_idx < obj->var_type##_count; var_idx++) {          \
            if (strcmp(obj->var_type##s[var_idx].name, variable_name) != 0) {                 \
                continue;                                                                     \
            }                                                                                 \
                                                                                              \
            if (obj->var_type##_count == 1) {                                                 \
                scene_animation_var_##var_type##_deinit(&obj->var_type##s[var_idx]);          \
                free(obj->var_type##s);                                                       \
                obj->var_type##s = NULL;                                                      \
            } else {                                                                          \
                te_scene_animation_obj_variable_##var_type* new_vars = malloc(                \
                    sizeof(te_scene_animation_obj_variable_##var_type)                        \
                    * (obj->var_type##_count - 1));                                           \
                memcpy(                                                                       \
                    new_vars, obj->var_type##s,                                               \
                    sizeof(te_scene_animation_obj_variable_##var_type) * var_idx);            \
                memcpy(                                                                       \
                    new_vars + var_idx, obj->var_type##s + (var_idx + 1),                     \
                    sizeof(te_scene_animation_obj_variable_##var_type)                        \
                        * (obj->var_type##_count - var_idx - 1));                             \
                                                                                              \
                free(obj->var_type##s);                                                       \
                obj->var_type##s = new_vars;                                                  \
            }                                                                                 \
                                                                                              \
            obj->var_type##_count -= 1;                                                       \
            break;                                                                            \
        }                                                                                     \
    }
SCENE_ANIM_REMOVE_ALL_KEYFRAMES_IMPL(bool)
SCENE_ANIM_REMOVE_ALL_KEYFRAMES_IMPL(uint)
SCENE_ANIM_REMOVE_ALL_KEYFRAMES_IMPL(float)
SCENE_ANIM_REMOVE_ALL_KEYFRAMES_IMPL(vec2)
SCENE_ANIM_REMOVE_ALL_KEYFRAMES_IMPL(vec3)
SCENE_ANIM_REMOVE_ALL_KEYFRAMES_IMPL(vec4)

void
scene_animation_remove_keyframe(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    void* keyframe) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);

#define FIND_AND_REMOVE_KEYFRAME(var_type)                                                    \
    for (unsigned int i = 0; i < obj->var_type##_count; i++) {                                \
        if (strcmp(obj->var_type##s[i].name, variable_name) == 0) {                           \
            scene_animation_remove_keyframe_##var_type(                                       \
                scene_animation, object_name, variable_name, keyframe);                       \
            return;                                                                           \
        }                                                                                     \
    }
    FIND_AND_REMOVE_KEYFRAME(bool)
    FIND_AND_REMOVE_KEYFRAME(uint)
    FIND_AND_REMOVE_KEYFRAME(float)
    FIND_AND_REMOVE_KEYFRAME(vec2)
    FIND_AND_REMOVE_KEYFRAME(vec3)
    FIND_AND_REMOVE_KEYFRAME(vec4)
}

#define SCENE_ANIM_REMOVE_KEYFRAME_IMPL(var_type)                                             \
    void scene_animation_remove_keyframe_##var_type(                                          \
        te_scene_animation* scene_animation, const char* object_name,                         \
        const char* variable_name, te_scene_animation_keyframe_##var_type* target_keyframe) { \
        te_scene_animation_obj* obj = get_obj(scene_animation, object_name);                  \
        const size_t keyframe_size = sizeof(te_scene_animation_keyframe_##var_type);          \
        for (unsigned int var_idx = 0; var_idx < obj->var_type##_count; var_idx++) {          \
            te_scene_animation_obj_variable_##var_type* var = &obj->var_type##s[var_idx];     \
            if (strcmp(var->name, variable_name) != 0) {                                      \
                continue;                                                                     \
            }                                                                                 \
                                                                                              \
            for (unsigned int keyframe_idx = 0; keyframe_idx < var->keyframe_count;           \
                 keyframe_idx++) {                                                            \
                te_scene_animation_keyframe_##var_type* keyframe =                            \
                    &var->keyframes[keyframe_idx];                                            \
                if (keyframe != target_keyframe) {                                            \
                    continue;                                                                 \
                }                                                                             \
                                                                                              \
                if (var->keyframe_count == 1) {                                               \
                    scene_animation_remove_all_keyframes_##var_type(                          \
                        scene_animation, object_name, variable_name);                         \
                } else {                                                                      \
                    te_scene_animation_keyframe_##var_type* new_keyframes =                   \
                        malloc(keyframe_size * (var->keyframe_count - 1));                    \
                    memcpy(new_keyframes, var->keyframes, keyframe_size* keyframe_idx);       \
                    memcpy(                                                                   \
                        new_keyframes + keyframe_idx, var->keyframes + (keyframe_idx + 1),    \
                        keyframe_size * (var->keyframe_count - keyframe_idx - 1));            \
                                                                                              \
                    free(var->keyframes);                                                     \
                    var->keyframes = new_keyframes;                                           \
                    var->keyframe_count -= 1;                                                 \
                }                                                                             \
                break;                                                                        \
            }                                                                                 \
                                                                                              \
            break;                                                                            \
        }                                                                                     \
    }
SCENE_ANIM_REMOVE_KEYFRAME_IMPL(bool)
SCENE_ANIM_REMOVE_KEYFRAME_IMPL(uint)
SCENE_ANIM_REMOVE_KEYFRAME_IMPL(float)
SCENE_ANIM_REMOVE_KEYFRAME_IMPL(vec2)
SCENE_ANIM_REMOVE_KEYFRAME_IMPL(vec3)
SCENE_ANIM_REMOVE_KEYFRAME_IMPL(vec4)

void
scene_animation_add_keyframe_bool(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    float time, bool value) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);
    te_scene_animation_obj_variable_bool* var = get_var_bool(obj, variable_name);
    te_scene_animation_keyframe_bool* keyframe = get_keyframe_bool(var, time);

    keyframe->time = time;
    keyframe->value = value;
    keyframe->interpolation = TE_SAIT_STEP;
}

void
scene_animation_add_keyframe_uint(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    float time, unsigned int value) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);
    te_scene_animation_obj_variable_uint* var = get_var_uint(obj, variable_name);
    te_scene_animation_keyframe_uint* keyframe = get_keyframe_uint(var, time);

    keyframe->time = time;
    keyframe->value = value;
    keyframe->interpolation = TE_SAIT_CUBIC_SPLINE;
}

void
scene_animation_add_keyframe_float(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    float time, float value) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);
    te_scene_animation_obj_variable_float* var = get_var_float(obj, variable_name);
    te_scene_animation_keyframe_float* keyframe = get_keyframe_float(var, time);

    keyframe->time = time;
    keyframe->value = value;
    keyframe->interpolation = TE_SAIT_CUBIC_SPLINE;
}

void
scene_animation_add_keyframe_vec2(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    float time, vec2 value) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);
    te_scene_animation_obj_variable_vec2* var = get_var_vec2(obj, variable_name);
    te_scene_animation_keyframe_vec2* keyframe = get_keyframe_vec2(var, time);

    keyframe->time = time;
    glm_vec2_copy(value, keyframe->value);
    keyframe->interpolation = TE_SAIT_CUBIC_SPLINE;
}

void
scene_animation_add_keyframe_vec3(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    float time, vec3 value) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);
    te_scene_animation_obj_variable_vec3* var = get_var_vec3(obj, variable_name);
    te_scene_animation_keyframe_vec3* keyframe = get_keyframe_vec3(var, time);

    keyframe->time = time;
    glm_vec3_copy(value, keyframe->value);
    keyframe->interpolation = TE_SAIT_CUBIC_SPLINE;
}

void
scene_animation_add_keyframe_vec4(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    float time, vec4 value) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);
    te_scene_animation_obj_variable_vec4* var = get_var_vec4(obj, variable_name);
    te_scene_animation_keyframe_vec4* keyframe = get_keyframe_vec4(var, time);

    keyframe->time = time;
    glm_vec4_copy(value, keyframe->value);
    keyframe->interpolation = TE_SAIT_CUBIC_SPLINE;
}

#define SCENE_ANIM_GET_KEYFRAMES_IMPL(var_type)                                               \
    te_scene_animation_keyframe_##var_type* scene_animation_get_keyframes_##var_type(         \
        te_scene_animation* scene_animation, const char* object_name,                         \
        const char* variable_name, unsigned int* out_keyframe_count) {                        \
        te_scene_animation_obj* obj = get_obj(scene_animation, object_name);                  \
        te_scene_animation_obj_variable_##var_type* var =                                     \
            get_var_##var_type(obj, variable_name);                                           \
        (*out_keyframe_count) = var->keyframe_count;                                          \
        return var->keyframes;                                                                \
    }
SCENE_ANIM_GET_KEYFRAMES_IMPL(bool)
SCENE_ANIM_GET_KEYFRAMES_IMPL(uint)
SCENE_ANIM_GET_KEYFRAMES_IMPL(float)
SCENE_ANIM_GET_KEYFRAMES_IMPL(vec2)
SCENE_ANIM_GET_KEYFRAMES_IMPL(vec3)
SCENE_ANIM_GET_KEYFRAMES_IMPL(vec4);

static void
interpolate_bool(
    te_scene_animation* anim, te_scene_animation_obj* obj,
    te_scene_animation_obj_variable_bool* var, unsigned int left_idx) {
    (void)anim;
    obj->transient_obj_type_info->bool_setters[var->transient_var_info->set_get_index](
        obj->transient_obj, var->keyframes[left_idx].value);
}

static void
interpolate_uint(
    te_scene_animation* anim, te_scene_animation_obj* obj,
    te_scene_animation_obj_variable_uint* var, unsigned int left_idx) {
    if (left_idx == var->keyframe_count - 1
        || var->keyframes[left_idx].interpolation == TE_SAIT_STEP) {
        obj->transient_obj_type_info->uint_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, var->keyframes[left_idx].value);
    } else {
        const float factor =
            (anim->current_time_sec - var->keyframes[left_idx].time)
            / (var->keyframes[left_idx + 1].time - var->keyframes[left_idx].time);

        unsigned int from = var->keyframes[left_idx].value;
        unsigned int to = var->keyframes[left_idx + 1].value;
        unsigned int out_value;

        if (var->keyframes[left_idx].interpolation == TE_SAIT_LINEAR) {
            out_value = from + (unsigned int)roundf(factor * (float)(to - from));
        } else {
            float s = glm_smoothstep(0.0f, 1.0f, factor);
            out_value = from + (unsigned int)roundf(s * (float)(to - from));
        }

        obj->transient_obj_type_info->uint_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, out_value);
    }
}

static void
interpolate_float(
    te_scene_animation* anim, te_scene_animation_obj* obj,
    te_scene_animation_obj_variable_float* var, unsigned int left_idx) {
    if (left_idx == var->keyframe_count - 1
        || var->keyframes[left_idx].interpolation == TE_SAIT_STEP) {
        obj->transient_obj_type_info->float_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, var->keyframes[left_idx].value);
    } else {
        const float factor =
            (anim->current_time_sec - var->keyframes[left_idx].time)
            / (var->keyframes[left_idx + 1].time - var->keyframes[left_idx].time);

        float from = var->keyframes[left_idx].value;
        float to = var->keyframes[left_idx + 1].value;
        float out_value;

        if (var->keyframes[left_idx].interpolation == TE_SAIT_LINEAR) {
            out_value = from + factor * (to - from);
        } else {
            float s = glm_smoothstep(0.0f, 1.0f, factor);
            out_value = from + s * (to - from);
        }

        obj->transient_obj_type_info->float_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, out_value);
    }
}

static void
interpolate_vec2(
    te_scene_animation* anim, te_scene_animation_obj* obj,
    te_scene_animation_obj_variable_vec2* var, unsigned int left_idx) {
    if (left_idx == var->keyframe_count - 1
        || var->keyframes[left_idx].interpolation == TE_SAIT_STEP) {
        obj->transient_obj_type_info->vec2_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, var->keyframes[left_idx].value);
    } else {
        const float factor =
            (anim->current_time_sec - var->keyframes[left_idx].time)
            / (var->keyframes[left_idx + 1].time - var->keyframes[left_idx].time);

        vec2 out_value;

        if (var->keyframes[left_idx].interpolation == TE_SAIT_LINEAR) {
            glm_vec2_lerp(
                var->keyframes[left_idx].value, var->keyframes[left_idx + 1].value, factor,
                out_value);
        } else {
            float s = glm_smoothstep(0.0f, 1.0f, factor);
            glm_vec2_lerp(
                var->keyframes[left_idx].value, var->keyframes[left_idx + 1].value, s,
                out_value);
        }

        obj->transient_obj_type_info->vec2_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, out_value);
    }
}

static void
interpolate_vec3(
    te_scene_animation* anim, te_scene_animation_obj* obj,
    te_scene_animation_obj_variable_vec3* var, unsigned int left_idx) {
    if (left_idx == var->keyframe_count - 1
        || var->keyframes[left_idx].interpolation == TE_SAIT_STEP) {
        obj->transient_obj_type_info->vec3_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, var->keyframes[left_idx].value);
    } else {
        const float factor =
            (anim->current_time_sec - var->keyframes[left_idx].time)
            / (var->keyframes[left_idx + 1].time - var->keyframes[left_idx].time);

        vec3 out_value;

        if (strcmp(var->name, "rotation") == 0) {
            mat4 rot_mat_from;
            math_make_rotation_mat(var->keyframes[left_idx].value, rot_mat_from);

            mat4 rot_mat_to;
            math_make_rotation_mat(var->keyframes[left_idx + 1].value, rot_mat_to);

            vec4 from;
            vec4 to;
            glm_mat4_quat(rot_mat_from, from);
            glm_mat4_quat(rot_mat_to, to);

            vec4 result;

            if (var->keyframes[left_idx].interpolation == TE_SAIT_LINEAR) {
                glm_quat_slerp(from, to, factor, result);
            } else {
                float s = glm_smoothstep(0.0f, 1.0f, factor);
                glm_quat_slerp(from, to, s, result);
            }

            glm_quat_mat4(result, rot_mat_to);

            glm_euler_angles(rot_mat_to, out_value);
            out_value[0] = glm_deg(out_value[0]);
            out_value[1] = glm_deg(out_value[1]);
            out_value[2] = glm_deg(out_value[2]);
        } else {
            if (var->keyframes[left_idx].interpolation == TE_SAIT_LINEAR) {
                glm_vec3_lerp(
                    var->keyframes[left_idx].value, var->keyframes[left_idx + 1].value, factor,
                    out_value);
            } else {
                float s = glm_smoothstep(0.0f, 1.0f, factor);
                glm_vec3_lerp(
                    var->keyframes[left_idx].value, var->keyframes[left_idx + 1].value, s,
                    out_value);
            }
        }

        obj->transient_obj_type_info->vec3_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, out_value);
    }
}

static void
interpolate_vec4(
    te_scene_animation* anim, te_scene_animation_obj* obj,
    te_scene_animation_obj_variable_vec4* var, unsigned int left_idx) {
    if (left_idx == var->keyframe_count - 1
        || var->keyframes[left_idx].interpolation == TE_SAIT_STEP) {
        obj->transient_obj_type_info->vec4_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, var->keyframes[left_idx].value);
    } else {
        const float factor =
            (anim->current_time_sec - var->keyframes[left_idx].time)
            / (var->keyframes[left_idx + 1].time - var->keyframes[left_idx].time);

        vec4 out_value;

        if (var->keyframes[left_idx].interpolation == TE_SAIT_LINEAR) {
            glm_vec4_lerp(
                var->keyframes[left_idx].value, var->keyframes[left_idx + 1].value, factor,
                out_value);
        } else {
            float s = glm_smoothstep(0.0f, 1.0f, factor);
            glm_vec4_lerp(
                var->keyframes[left_idx].value, var->keyframes[left_idx + 1].value, s,
                out_value);
        }

        obj->transient_obj_type_info->vec4_setters[var->transient_var_info->set_get_index](
            obj->transient_obj, out_value);
    }
}

static void
scene_animation_tick(te_scene_animation* anim, float delta_time_sec) {
    anim->current_time_sec += delta_time_sec;
    if (anim->loop) {
        anim->current_time_sec = fmodf(anim->current_time_sec, anim->transient_duration_sec);
    } else {
        anim->current_time_sec =
            glm_clamp(anim->current_time_sec, 0.0f, anim->transient_duration_sec);
    }

    for (unsigned int obj_idx = 0; obj_idx < anim->animated_object_count; obj_idx++) {
        te_scene_animation_obj* obj = &anim->animated_objects[obj_idx];

#define INTERPOLATE_VARIABLES(var_type)                                                       \
    for (unsigned int var_idx = 0; var_idx < obj->var_type##_count; var_idx++) {              \
        te_scene_animation_obj_variable_##var_type* var = &obj->var_type##s[var_idx];         \
                                                                                              \
        for (unsigned int keyframe_idx = 0; keyframe_idx < var->keyframe_count;               \
             keyframe_idx++) {                                                                \
            unsigned int left_idx = 0;                                                        \
            for (; left_idx < var->keyframe_count;) {                                         \
                if (anim->current_time_sec > var->keyframes[left_idx].time) {                 \
                    left_idx += 1;                                                            \
                    continue;                                                                 \
                }                                                                             \
                if (left_idx > 0) {                                                           \
                    left_idx -= 1;                                                            \
                }                                                                             \
                break;                                                                        \
            }                                                                                 \
            if (left_idx >= var->keyframe_count) {                                            \
                left_idx = var->keyframe_count - 1;                                           \
            }                                                                                 \
            interpolate_##var_type(anim, obj, var, left_idx);                                 \
        }                                                                                     \
    }
        INTERPOLATE_VARIABLES(bool)
        INTERPOLATE_VARIABLES(uint)
        INTERPOLATE_VARIABLES(float)
        INTERPOLATE_VARIABLES(vec2)
        INTERPOLATE_VARIABLES(vec3)
        INTERPOLATE_VARIABLES(vec4)
    }
}

void
scene_animation_play(te_scene_animation* scene_animation) {
    if (scene_animation->tick_callback_id != 0xFFFFFFFF) {
        // Already playing.
        return;
    }

    cache_obj_and_var(scene_animation);

    scene_animation->current_time_sec = 0.0f;

    // Init animated objects.
    scene_animation_tick(scene_animation, 0.0f);

    if (scene_animation->tick_callback_id == 0xFFFFFFFF) {
        // Register tick callback to update animation.
        scene_animation->tick_callback_id = game_manager_add_tick_callback(
            world_get_game_manager(scene_animation->world), scene_animation,
            scene_animation_tick);
    }
}
