#include <game/scene_animation.h>

#include <string.h>

// Animation of a variable (of some object). Inside a variable keyframes are sorted in time increasing order.
#define SCENE_ANIM_OBJ_VARIABLE_TYPE(type)                                                    \
    typedef struct te_scene_animation_obj_variable_##type {                                   \
        char* name;                                                                           \
        te_scene_animation_keyframe_##type* keyframes;                                        \
        unsigned int keyframe_count;                                                          \
    } te_scene_animation_obj_variable_##type;
SCENE_ANIM_OBJ_VARIABLE_TYPE(bool);
SCENE_ANIM_OBJ_VARIABLE_TYPE(unsigned);
SCENE_ANIM_OBJ_VARIABLE_TYPE(float);
SCENE_ANIM_OBJ_VARIABLE_TYPE(vec2);
SCENE_ANIM_OBJ_VARIABLE_TYPE(vec3);
SCENE_ANIM_OBJ_VARIABLE_TYPE(vec4);

// Animated variables of some object.
typedef struct te_scene_animation_obj {
    // Name of an object in the world (object name is supposed to be unique).
    char* name;

    te_scene_animation_obj_variable_bool* bools;
    te_scene_animation_obj_variable_unsigned* uints;
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
    te_scene_animation_obj* animated_objects;
    unsigned int animated_object_count;
};

void
scene_animation_obj_init(te_scene_animation_obj* obj, const char* name) {
    size_t len = strlen(name);
    obj->name = malloc(sizeof(char) * (len + 1));
    memcpy(obj->name, name, sizeof(char) * len);
    obj->name[len] = 0;

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

    for (unsigned int i = 0; i < obj->bool_count; i++) {
        free(obj->bools[i].keyframes);
    }
    free(obj->bools);

    for (unsigned int i = 0; i < obj->uint_count; i++) {
        free(obj->uints[i].keyframes);
    }
    free(obj->uints);

    for (unsigned int i = 0; i < obj->float_count; i++) {
        free(obj->floats[i].keyframes);
    }
    free(obj->floats);

    for (unsigned int i = 0; i < obj->vec2_count; i++) {
        free(obj->vec2s[i].keyframes);
    }
    free(obj->vec2s);

    for (unsigned int i = 0; i < obj->vec3_count; i++) {
        free(obj->vec3s[i].keyframes);
    }
    free(obj->vec3s);

    for (unsigned int i = 0; i < obj->vec4_count; i++) {
        free(obj->vec4s[i].keyframes);
    }
    free(obj->vec4s);
}

te_scene_animation*
scene_animation_create() {
    te_scene_animation* scene_animation = malloc(sizeof(te_scene_animation));

    scene_animation->animated_objects = NULL;
    scene_animation->animated_object_count = 0;

    return scene_animation;
}

void
scene_animation_destroy(te_scene_animation* scene_animation) {
    for (unsigned int i = 0; i < scene_animation->animated_object_count; i++) {
        te_scene_animation_obj* obj = &scene_animation->animated_objects[i];
        scene_animation_obj_deinit(obj);
    }

    free(scene_animation->animated_objects);

    free(scene_animation);
}

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

#define SCENE_ANIM_CREATE_GET_VAR_FUNC(var_name, var_type)                                    \
    te_scene_animation_obj_variable_##var_type* get_var_##var_name(                           \
        te_scene_animation_obj* obj, const char* variable_name) {                             \
        for (unsigned int i = 0; i < obj->var_name##_count; i++) {                            \
            if (strcmp(obj->var_name##s[i].name, variable_name) == 0) {                       \
                return &obj->var_name##s[i];                                                  \
            }                                                                                 \
        }                                                                                     \
        te_scene_animation_obj_variable_##var_type* new_vars = malloc(                        \
            sizeof(te_scene_animation_obj_variable_##var_type)                                \
            * (obj->var_name##_count + 1));                                                   \
        memcpy(                                                                               \
            new_vars, obj->var_name##s,                                                       \
            sizeof(te_scene_animation_obj_variable_##var_type) * obj->var_name##_count);      \
                                                                                              \
        free(obj->var_name##s);                                                               \
        obj->var_name##s = new_vars;                                                          \
                                                                                              \
        obj->var_name##_count += 1;                                                           \
                                                                                              \
        te_scene_animation_obj_variable_##var_type* var =                                     \
            &obj->var_name##s[obj->var_name##_count - 1];                                     \
        var->keyframe_count = 0;                                                              \
        var->keyframes = NULL;                                                                \
                                                                                              \
        size_t len = strlen(variable_name);                                                   \
        var->name = malloc(sizeof(char) * (len + 1));                                         \
        memcpy(var->name, variable_name, sizeof(char) * len);                                 \
        var->name[len] = 0;                                                                   \
                                                                                              \
        return var;                                                                           \
    }
SCENE_ANIM_CREATE_GET_VAR_FUNC(bool, bool)
SCENE_ANIM_CREATE_GET_VAR_FUNC(uint, unsigned)
SCENE_ANIM_CREATE_GET_VAR_FUNC(float, float)
SCENE_ANIM_CREATE_GET_VAR_FUNC(vec2, vec2)
SCENE_ANIM_CREATE_GET_VAR_FUNC(vec3, vec3)
SCENE_ANIM_CREATE_GET_VAR_FUNC(vec4, vec4)

#define SCENE_ANIM_CREATE_NEW_KEYFRAME_FUNC(type_name)                                        \
    te_scene_animation_keyframe_##type_name* get_keyframe_##type_name(                        \
        te_scene_animation_obj_variable_##type_name* var, float time) {                       \
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
            te_scene_animation_keyframe_##type_name* new_frames = malloc(                     \
                sizeof(te_scene_animation_keyframe_##type_name) * (var->keyframe_count + 1)); \
            memcpy(                                                                           \
                new_frames, var->keyframes,                                                   \
                sizeof(te_scene_animation_keyframe_##type_name) * var->keyframe_count);       \
                                                                                              \
            free(var->keyframes);                                                             \
            var->keyframes = new_frames;                                                      \
                                                                                              \
            var->keyframe_count += 1;                                                         \
            return &var->keyframes[var->keyframe_count - 1];                                  \
        } else {                                                                              \
            return &var->keyframes[left_idx];                                                 \
        }                                                                                     \
    }
SCENE_ANIM_CREATE_NEW_KEYFRAME_FUNC(bool)
SCENE_ANIM_CREATE_NEW_KEYFRAME_FUNC(unsigned)
SCENE_ANIM_CREATE_NEW_KEYFRAME_FUNC(float)
SCENE_ANIM_CREATE_NEW_KEYFRAME_FUNC(vec2)
SCENE_ANIM_CREATE_NEW_KEYFRAME_FUNC(vec3)
SCENE_ANIM_CREATE_NEW_KEYFRAME_FUNC(vec4)

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

te_scene_animation_keyframe_bool*
scene_animation_get_keyframes_bool(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    unsigned int* out_keyframe_count) {
    te_scene_animation_obj* obj = get_obj(scene_animation, object_name);
    te_scene_animation_obj_variable_bool* var = get_var_bool(obj, variable_name);
    (*out_keyframe_count) = var->keyframe_count;
    return var->keyframes;
}
