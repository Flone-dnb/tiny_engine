#include <game/skeleton.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <game_manager.h>
#include <game/model.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <math/math_funcs.h>
#include <hashmap.c/hashmap.h>

struct te_skeleton_bone;
typedef struct te_skeleton_bone {
    te_mat4 inverse_bind_pose_mat;

    char* name;

    /* transform of the node relative to its parent */
    te_vec3 position;
    te_vec3 rotation; /* in degrees */
    te_vec3 scale;

    unsigned int child_count;
    unsigned int parent_idx; /* if has no parent then 0xFFFFFFFF */
} te_skeleton_bone;

struct te_skeleton {
    te_game_manager* game_manager;

    /* do not free/destroy, model that uses this skeleton */
    te_model* model;

    struct hashmap* preloaded_anims;

    /* the number of items in this array is @ref bone_count
     * stores root bone at index 0, then child nodes, example:
     * 0 - root (child_count = 2)
     *   1 - child1 (child_count = 1)
     *     2 - child1_child
     *   3 - child2 (child_count = 0) */
    te_skeleton_bone* bones;

    /* the number of matrices in this array is @ref bone_count
     * each matrix stores the final transform of a bone from @ref bones
     * so that matrix at index 0 stores transform for a bone at index 0 from @ref bones
     * these matrices contain the current state of the skeleton (which may be animated)
     * stored outside of the bones array to be passed to the shader */
    te_mat4* skinning_mats;

    /* NULL if not playing an animation, otherwise reference to the animation
     * do not destroy/free this pointer, references an animation from @ref preloaded_anims */
    te_skeleton_animation* playing_anim;

    /* not NULL if blending old animation (this one) with a new one (@ref playing_anim)
     * blend time is @ref anim_blend_time_sec and the current progress is @ref curr_anim_blend_time_sec */
    te_skeleton_animation* prev_anim;

    /* time (in seconds) to blend @ref prev_anim and @ref playing_anim */
    float anim_blend_time_sec;

    /* value in range [0.0; @ref anim_blend_time_sec] that determines the blend state */
    float curr_anim_blend_time_sec;

    unsigned int bone_count;

    /* ID used to unregister tick callback. 0xFFFFFFFF if not registered */
    unsigned int tick_callback_id;
};

typedef struct te_skeleton_animation_keyframe {
    enum te_keyframe_interpolation_type interpolation_type;
    float time;
    te_vec4 value; /* for rotation quaternion, for others 4th component is zero */
} te_skeleton_animation_keyframe;

/* animates transform of a single skeleton bone */
typedef struct te_skeleton_bone_animation {
    te_skeleton_animation_keyframe* keyframes[TE_ACT_COUNT];
    unsigned int keyframe_count[TE_ACT_COUNT];
} te_skeleton_bone_animation;

struct te_skeleton_animation {
    /* not NULL, unique name of the animation within a skeleton */
    char* name;

    /* the number of elements in this array is equal to the skeleton's bone count
     * for each skeleton bone (even for bones that were not animated) there's an item in this array
     * item at index 0 corresponds to the bone with index 0 from the skeleton's "bones" array
     * if a bone was not animated the item stores 0xFFFFFFFF, otherwise it stores
     * an index into the @ref bone_anims array. */
    unsigned int* bone_anim_indices;

    /* each item stores animation data of a certain skeleton bone
     * to determine which item (from this array) animated which bone
     * use @ref bone_anim_indices */
    te_skeleton_bone_animation* bone_anims;

    /* the number of elements in the array @ref bone_anims */
    unsigned int bone_anim_count;

    float duration_sec;

    /* ---------------------------------- runtime params */

    float current_time_sec;
    bool loop;
};

/* command hash for hashmap */
static uint64_t
anim_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const te_skeleton_animation* const* info = item;
    return hashmap_sip((*info)->name, strlen((*info)->name), seed0, seed1);
}

/* command compare for hashmap */
static int
anim_compare(const void* a, const void* b, void* udata) {
    const te_skeleton_animation* const* info1 = a;
    const te_skeleton_animation* const* info2 = b;
    (void)udata;
    return strcmp((*info1)->name, (*info2)->name);
}

static void load_skeleton_bone(
    te_skeleton* skeleton, FILE* fp, unsigned int* bone_idx, unsigned int parent_bone_idx);

te_skeleton*
prv_skeleton_create(
    const char* relative_path, te_model* model, te_game_manager* game_manager) {
    char* res_path;
    FILE* fp;
    unsigned int bone_idx;

    te_skeleton* skeleton = malloc(sizeof(te_skeleton));
    skeleton->game_manager = game_manager;
    skeleton->model = model;
    skeleton->playing_anim = NULL;
    skeleton->prev_anim = NULL;
    skeleton->anim_blend_time_sec = 0.0f;
    skeleton->curr_anim_blend_time_sec = 0.0f;
    skeleton->tick_callback_id = 0xFFFFFFFF;

    skeleton->preloaded_anims = hashmap_new(
        sizeof(te_skeleton_animation*), 8, 0, 0, anim_hash, anim_compare, NULL, NULL);

    res_path = filesystem_prepend_res_to_path(relative_path, NULL);
    fp = fopen(res_path, "rb");

    /* read bone count */
    fread(&skeleton->bone_count, sizeof(skeleton->bone_count), 1, fp);
    skeleton->bones = malloc(sizeof(te_skeleton_bone) * skeleton->bone_count);

    /* read bones */
    bone_idx = 0;
    load_skeleton_bone(skeleton, fp, &bone_idx, 0xFFFFFFFF);

    fclose(fp);
    free(res_path);

    /* allocate skinning matrices for each bone */
    skeleton->skinning_mats = malloc(sizeof(te_mat4) * skeleton->bone_count);

    /* init bone transforms (first update) */
    prv_skeleton_update(skeleton, 0.0f);

    return skeleton;
}

static void
skeleton_animation_destroy(te_skeleton_animation* anim) {
    unsigned int i;
    unsigned int channel_type;

    free(anim->name);

    free(anim->bone_anim_indices);

    for (i = 0; i < anim->bone_anim_count; i++) {
        te_skeleton_bone_animation* bone_anim = &anim->bone_anims[i];

        for (channel_type = 0; channel_type < TE_ACT_COUNT; channel_type++) {
            if (bone_anim->keyframe_count[channel_type] == 0) {
                continue;
            }
            free(bone_anim->keyframes[channel_type]);
        }
    }
    free(anim->bone_anims);

    free(anim);
}

void
prv_skeleton_destroy(te_skeleton* skeleton) {
    void* item;
    unsigned int i;
    size_t iter;

    skeleton_stop_animation(skeleton);

    iter = 0;
    while (hashmap_iter(skeleton->preloaded_anims, &iter, &item)) {
        const te_skeleton_animation** ptr = item;
        te_skeleton_animation* anim = (te_skeleton_animation*)*ptr;
        skeleton_animation_destroy(anim);
    }
    hashmap_free(skeleton->preloaded_anims);

    free(skeleton->skinning_mats);

    for (i = 0; i < skeleton->bone_count; i++) {
        free(skeleton->bones[i].name);
    }
    free(skeleton->bones);

    free(skeleton);
}

static void
load_skeleton_bone(
    te_skeleton* skeleton, FILE* fp, unsigned int* bone_idx, unsigned int parent_bone_idx) {
    unsigned int name_len;
    unsigned int i;

    const unsigned int this_bone_idx = *bone_idx;
    te_skeleton_bone* bone = &skeleton->bones[this_bone_idx];
    (*bone_idx) += 1;

    bone->parent_idx = parent_bone_idx;

    /* read name */
    bone->name = NULL;
    fread(&name_len, sizeof(name_len), 1, fp);
    if (name_len > 0) {
        bone->name = malloc(sizeof(char) * (name_len + 1));
        fread(bone->name, sizeof(char), name_len, fp);
        bone->name[name_len] = 0;
    }

    /* read local transform */
    fread(bone->position, sizeof(float), 3, fp);
    fread(bone->rotation, sizeof(float), 3, fp);
    fread(bone->scale, sizeof(float), 3, fp);

    fread(&bone->inverse_bind_pose_mat[0], sizeof(te_mat4), 1, fp);

    /* read child count */
    fread(&bone->child_count, sizeof(bone->child_count), 1, fp);

    for (i = 0; i < bone->child_count; i++) {
        load_skeleton_bone(skeleton, fp, bone_idx, this_bone_idx);
    }
}

static void
skeleton_bone_interpolate_pos_scale(
    float curr_time_sec, te_skeleton_bone_animation* bone_anim,
    enum te_animation_channel_type channel_type, te_mat4 out) {
    te_vec3 out_value;
    unsigned int left_idx;

    const unsigned int keyframe_count = bone_anim->keyframe_count[channel_type];
    te_skeleton_animation_keyframe* keyframes = bone_anim->keyframes[channel_type];

#if defined(DEBUG)
    if (keyframe_count == 0) {
        log_error(__FILE__, __LINE__, "expected to have at least 1 keyframe");
        abort();
    }
#endif

    left_idx = 0;
    for (; left_idx < keyframe_count;) {
        if (curr_time_sec > keyframes[left_idx].time) {
            left_idx += 1;
            continue;
        }
        if (left_idx > 0) {
            left_idx -= 1;
        }
        break;
    }
    if (left_idx >= keyframe_count) {
        left_idx = keyframe_count - 1;
    }

    if (left_idx == keyframe_count - 1
        || keyframes[left_idx].interpolation_type == TE_KIT_STEP) {
        vec3_copy(keyframes[left_idx].value, out_value);
    } else {
        const float factor = (curr_time_sec - keyframes[left_idx].time)
                             / (keyframes[left_idx + 1].time - keyframes[left_idx].time);

        if (keyframes[left_idx].interpolation_type == TE_KIT_LINEAR) {
            vec3_lerp(
                keyframes[left_idx].value, keyframes[left_idx + 1].value, factor, out_value);
        } else {
            float s = math_smoothstep(0.0f, 1.0f, factor);
            vec3_lerp(keyframes[left_idx].value, keyframes[left_idx + 1].value, s, out_value);
        }
    }

    mat4_identity(out);
    if (channel_type == TE_ACT_SCALE) {
        mat4_set_scaling_part(out, out_value);
    } else {
        mat4_set_translation_part(out, out_value);
    }
}

static void
skeleton_bone_interpolate_rotation(
    float curr_time_sec, te_skeleton_bone_animation* bone_anim, te_mat4 out) {
    const unsigned int keyframe_count = bone_anim->keyframe_count[TE_ACT_ROTATION];
    te_skeleton_animation_keyframe* keyframes = bone_anim->keyframes[TE_ACT_ROTATION];
    te_mat4 mat;
    te_vec4 from;
    te_vec4 to;
    te_vec4 result;
    te_vec3 rot;
    float factor;
    unsigned int left_idx;

#if defined(DEBUG)
    if (keyframe_count == 0) {
        log_error(__FILE__, __LINE__, "expected to have at least 1 keyframe");
        abort();
    }
#endif

    left_idx = 0;
    for (; left_idx < keyframe_count;) {
        if (curr_time_sec > keyframes[left_idx].time) {
            left_idx += 1;
            continue;
        }
        if (left_idx > 0) {
            left_idx -= 1;
        }
        break;
    }
    if (left_idx >= keyframe_count) {
        left_idx = keyframe_count - 1;
    }

    vec4_copy(keyframes[left_idx].value, from);

    if (left_idx == keyframe_count - 1
        || keyframes[left_idx].interpolation_type == TE_KIT_STEP) {
        te_mat4 mat;
        te_vec3 rot;

        mat4_from_quat(from, mat);

        mat4_extract_euler_angles_rad(mat, rot);
        rot[0] = math_deg(rot[0]);
        rot[1] = math_deg(rot[1]);
        rot[2] = math_deg(rot[2]);

        math_make_rotation_mat(rot, out);
        return;
    }

    factor = (curr_time_sec - keyframes[left_idx].time)
             / (keyframes[left_idx + 1].time - keyframes[left_idx].time);

    vec4_copy(keyframes[left_idx + 1].value, to);

    if (keyframes[left_idx].interpolation_type == TE_KIT_LINEAR) {
        vec4_slerp(from, to, factor, result);
    } else {
        float s = math_smoothstep(0.0f, 1.0f, factor);
        vec4_slerp(from, to, s, result);
    }

    mat4_from_quat(result, mat);

    mat4_extract_euler_angles_rad(mat, rot);
    rot[0] = math_deg(rot[0]);
    rot[1] = math_deg(rot[1]);
    rot[2] = math_deg(rot[2]);

    math_make_rotation_mat(rot, out);
}

static void
update_skeleton_bone_skinning_mat(
    te_skeleton* skeleton, unsigned int* bone_idx, te_mat4 parent_transform) {
    te_mat4 global_transform;
    te_mat4 mat1;
    te_mat4 mat2;
    unsigned int i;

    te_skeleton_bone* bone = &skeleton->bones[*bone_idx];
    mat4_identity(global_transform);

    if (skeleton->playing_anim != NULL
        && skeleton->playing_anim->bone_anim_indices[*bone_idx] != 0xFFFFFFFF) {
        te_skeleton_bone_animation* bone_anim;

        /* apply base scale and rotation (in case they were changed/adjusted using the code) */
        mat4_identity(mat1);
        mat4_set_scaling_part(mat1, bone->scale);

        math_make_rotation_mat(bone->rotation, mat2);

        mat4_mul(mat2, mat1, global_transform);

        bone_anim = &skeleton->playing_anim
                         ->bone_anims[skeleton->playing_anim->bone_anim_indices[*bone_idx]];

        skeleton_bone_interpolate_pos_scale(
            skeleton->playing_anim->current_time_sec, bone_anim, TE_ACT_SCALE, mat1);

        skeleton_bone_interpolate_rotation(
            skeleton->playing_anim->current_time_sec, bone_anim, mat2);

        /* scale, rotate */
        mat4_mul(mat2, mat1, mat1);

        /* translate */
        skeleton_bone_interpolate_pos_scale(
            skeleton->playing_anim->current_time_sec, bone_anim, TE_ACT_POSITION, mat2);
        mat4_mul(mat2, mat1, mat1);

        mat4_mul(mat1, global_transform, global_transform);
    } else {
        mat4_identity(mat1);
        mat4_set_scaling_part(mat1, bone->scale);

        math_make_rotation_mat(bone->rotation, mat2);

        /* scale, rotate and then translate */
        mat4_mul(mat2, mat1, global_transform);
        mat4_identity(mat2);
        mat4_set_translation_part(mat2, bone->position);
        mat4_mul(mat2, global_transform, global_transform);
    }

    mat4_mul(parent_transform, global_transform, global_transform);
    if (skeleton->prev_anim == NULL) {
        mat4_mul(
            global_transform, bone->inverse_bind_pose_mat, skeleton->skinning_mats[*bone_idx]);
    } else {
        float factor;
        mat4_mul(global_transform, bone->inverse_bind_pose_mat, mat1);

        /* skinning_mats already has prev_anim matrices for this frame, we need to blend animations now */
        factor = skeleton->curr_anim_blend_time_sec / skeleton->anim_blend_time_sec;
        for (i = 0; i < 4; i++) {
            vec4_lerp(
                skeleton->skinning_mats[*bone_idx][i], mat1[i], factor,
                skeleton->skinning_mats[*bone_idx][i]);
        }
    }

    (*bone_idx) += 1;
    for (i = 0; i < bone->child_count; i++) {
        update_skeleton_bone_skinning_mat(skeleton, bone_idx, global_transform);
    }
}

void
prv_skeleton_update(te_skeleton* skeleton, float delta_time_sec) {
    te_mat4 identity;
    unsigned int bone_idx;

    if (skeleton->playing_anim != NULL) {
        te_skeleton_animation* anim = skeleton->playing_anim;

        anim->current_time_sec += delta_time_sec;

        if (anim->loop) {
            anim->current_time_sec = (float)fmod(anim->current_time_sec, anim->duration_sec);
        } else {
            anim->current_time_sec =
                math_clamp(anim->current_time_sec, 0.0f, anim->duration_sec);
        }

        if (skeleton->prev_anim != NULL) {
            skeleton->curr_anim_blend_time_sec += delta_time_sec;
            if (skeleton->curr_anim_blend_time_sec >= skeleton->anim_blend_time_sec) {
                skeleton->prev_anim = NULL;
                skeleton->curr_anim_blend_time_sec = 0.0f;
                skeleton->anim_blend_time_sec = 0.0f;
            } else {
                te_skeleton_animation* temp;

                mat4_identity(identity);

                /* fully sample previous animation to later blend with the current one */
                temp = skeleton->playing_anim;
                skeleton->playing_anim = skeleton->prev_anim;
                skeleton->prev_anim = NULL;

                bone_idx = 0;
                update_skeleton_bone_skinning_mat(skeleton, &bone_idx, identity);

                skeleton->prev_anim = skeleton->playing_anim;
                skeleton->playing_anim = temp;
            }
        }
    }

    mat4_identity(identity);

    bone_idx = 0;
    update_skeleton_bone_skinning_mat(skeleton, &bone_idx, identity);

    prv_model_on_after_skeleton_updated(skeleton->model);
}

unsigned int
skeleton_get_bone_count(te_skeleton* skeleton) {
    return skeleton->bone_count;
}

te_mat4*
skeleton_get_skinning_mats(te_skeleton* skeleton) {
    return skeleton->skinning_mats;
}

static bool
load_bone_anim(te_skeleton_animation* skel_anim, unsigned int bone_anim_idx, FILE* fp) {
    te_skeleton_bone_animation* bone_anim;
    unsigned int bone_idx;
    unsigned int i;

    fread(&bone_idx, sizeof(bone_idx), 1, fp);

    if (bone_idx == 0xFFFFFFFF) {
        /* bone info ended */
        return false;
    }

    /* init bone anim */
    skel_anim->bone_anim_indices[bone_idx] = bone_anim_idx;
    bone_anim = &skel_anim->bone_anims[bone_anim_idx];
    for (i = 0; i < TE_ACT_COUNT; i++) {
        bone_anim->keyframe_count[i] = 0;
        bone_anim->keyframes[i] = NULL;
    }

    while (1) {
        te_skeleton_animation_keyframe* keyframes;
        te_vec4 value;
        unsigned char channel_type;
        unsigned char interpolation_type;
        unsigned int keyframe_count;
        unsigned int end_mark;
        float timestamp;

        fread(&channel_type, sizeof(channel_type), 1, fp);
        fread(&interpolation_type, sizeof(interpolation_type), 1, fp);

        /* count keyframes */
        keyframe_count = 0;
        timestamp = 0.0f;
        while (1) {
            fread(&timestamp, sizeof(timestamp), 1, fp);
            if (timestamp < -0.5f) {
                /* get back to the first keyframe */
                fseek(
                    fp,
                    -(long int)((
                        sizeof(timestamp)
                        + keyframe_count * (sizeof(timestamp) + sizeof(value)))),
                    SEEK_CUR);
                break;
            }

            fread(value, sizeof(value), 1, fp);
            keyframe_count += 1;
        }
        if (keyframe_count == 0) {
            log_error_fmt(
                __FILE__, __LINE__,
                "expected to find at least a single keyframe for a bone with index %u",
                bone_idx);
            abort();
        }

        bone_anim->keyframe_count[channel_type] = keyframe_count;
        keyframes = malloc(sizeof(te_skeleton_animation_keyframe) * keyframe_count);
        if (bone_anim->keyframes[channel_type] != NULL) {
            log_error_fmt(
                __FILE__, __LINE__,
                "found duplicate animation channels on the bone %u, channel %u", bone_idx,
                channel_type);
            abort();
        }
        bone_anim->keyframes[channel_type] = keyframes;

        for (i = 0; i < keyframe_count; i++) {
            keyframes[i].interpolation_type = interpolation_type;

            fread(&keyframes[i].time, sizeof(keyframes[i].time), 1, fp);
            fread(keyframes[i].value, sizeof(value), 1, fp);
        }

        /* read keyframe end mark (-1.0f) */
        fread(&timestamp, sizeof(timestamp), 1, fp);

        fread(&end_mark, sizeof(end_mark), 1, fp);
        if (end_mark == 0xFFFFFFFF) {
            /* bone info ended */
            return true;
        } else if (end_mark == 0xFFFFFFFF - 1) {
            /* same bone next but different channel */
            continue;
        } else {
            /* next bone index */
            fseek(fp, -(long int)(sizeof(end_mark)), SEEK_CUR);
            return true;
        }
    }

    return true;
}

static void
prv_skeleton_preload_animation_file(
    te_skeleton* skeleton, const char* path_to_anim_file, const char* name,
    unsigned int name_len) {
    FILE* fp;
    te_skeleton_animation* anim;
    unsigned int i;

    /* check if already loaded */
    {
        te_skeleton_animation* lookup_ptr;
        const te_skeleton_animation* const* found;

        te_skeleton_animation lookup;
        lookup.name = (char*)name; /* only for lookup */
        lookup_ptr = &lookup;
        found = hashmap_get(skeleton->preloaded_anims, &lookup_ptr);
        if (found != NULL) {
            return;
        }
    }

    fp = fopen(path_to_anim_file, "rb");
    if (fp == NULL) {
        log_error_fmt(__FILE__, __LINE__, "failed to open file %s", path_to_anim_file);
        abort();
    }

    anim = malloc(sizeof(te_skeleton_animation));
    anim->current_time_sec = 0.0f;
    anim->loop = false;

    anim->name = malloc(sizeof(char) * (name_len + 1));
    memcpy(anim->name, name, sizeof(char) * name_len);
    anim->name[name_len] = 0;

    anim->bone_anim_indices = malloc(sizeof(unsigned int) * skeleton->bone_count);
    for (i = 0; i < skeleton->bone_count; i++) {
        anim->bone_anim_indices[i] = 0xFFFFFFFF;
    }

    /* read animated bone count */
    fread(&anim->bone_anim_count, sizeof(anim->bone_anim_count), 1, fp);
    anim->bone_anims = malloc(sizeof(te_skeleton_bone_animation) * anim->bone_anim_count);

    for (i = 0; i < anim->bone_anim_count; i++) {
        if (!load_bone_anim(anim, i, fp)) {
            log_error_fmt(
                __FILE__, __LINE__, "unable to find a single bone animation info in file %s",
                path_to_anim_file);
            abort();
        }
    }

    fread(&anim->duration_sec, sizeof(anim->duration_sec), 1, fp);

    fclose(fp);

    /* add to cache */
    hashmap_set(skeleton->preloaded_anims, &anim);
}

void
skeleton_load_animations(te_skeleton* skeleton, const char* relative_path) {
    unsigned int path_to_anim_dir_len;
    char* anim_path = filesystem_prepend_res_to_path(relative_path, &path_to_anim_dir_len);
    if (!filesystem_does_path_exists(anim_path)) {
        log_error_fmt(
            __FILE__, __LINE__, "the specified path does not exist \"%s\"", anim_path);
        abort();
    }

    if (filesystem_path_is_directory(anim_path)) {
        unsigned int entry_idx;
        unsigned int entry_count;

        /* load all anim files from this directory */
        te_filesystem_entry* entries = filesystem_list_directory(anim_path, &entry_count);

        for (entry_idx = 0; entry_idx < entry_count; entry_idx++) {
            te_filesystem_entry* entry = &entries[entry_idx];
            if (!entry->is_dir) {
                if (entry->name_len >= 6
                    && strncmp(entry->name + entry->name_len - 5, ".anim", 5) == 0) {
                    char* path_to_anim = filesystem_append_path(
                        anim_path, path_to_anim_dir_len, entry->name, entry->name_len, NULL);

                    prv_skeleton_preload_animation_file(
                        skeleton, path_to_anim, entry->name, entry->name_len);

                    free(path_to_anim);
                }
            }

            free(entry->name);
        }
        free(entries);
    } else {
        /* load anim file */
        unsigned int filename_len;
        const char* filename = filesystem_find_filename(anim_path, true, &filename_len);
        if (filename_len < 6) {
            log_error_fmt(
                __FILE__, __LINE__, "invalid anim file path specified: %s", anim_path);
            abort();
        }
        if (strncmp(filename + filename_len - 5, ".anim", 5) != 0) {
            log_error_fmt(__FILE__, __LINE__, "expected .anim extension: %s", anim_path);
            abort();
        }
        prv_skeleton_preload_animation_file(skeleton, anim_path, filename, filename_len);
    }

    free(anim_path);
}

void
skeleton_play_animation(
    te_skeleton* skeleton, const char* anim_name, bool loop, float blend_time_sec) {
    te_skeleton_animation* lookup_ptr;
    te_skeleton_animation* const* found;

    te_skeleton_animation lookup;
    lookup.name = (char*)anim_name; /* only for lookup */
    lookup_ptr = &lookup;
    found = hashmap_get(skeleton->preloaded_anims, &lookup_ptr);
    if (found == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "unable to find animation %s (was it loaded previously?)",
            anim_name);
        abort();
    }

    if (blend_time_sec >= 0.01f) {
        skeleton->prev_anim = skeleton->playing_anim;
        skeleton->anim_blend_time_sec = blend_time_sec;
        skeleton->curr_anim_blend_time_sec = 0.0f;
    }

    skeleton->playing_anim = *found;
    skeleton->playing_anim->loop = loop;
    skeleton->playing_anim->current_time_sec = 0.0f;

    prv_skeleton_update(skeleton, 0.0f);

    if (skeleton->tick_callback_id == 0xFFFFFFFF) {
        /* register tick callback to update animation */
        skeleton->tick_callback_id = game_manager_add_tick_callback(
            skeleton->game_manager, skeleton, prv_skeleton_update);
    }
}

void
skeleton_stop_animation(te_skeleton* skeleton) {
    if (skeleton->playing_anim == NULL) {
        return;
    }

    skeleton->playing_anim->loop = false;
    skeleton->playing_anim->current_time_sec = 0.0f;
    skeleton->playing_anim = NULL;

    skeleton->prev_anim = NULL;
    skeleton->anim_blend_time_sec = 0.0f;
    skeleton->curr_anim_blend_time_sec = 0.0f;

    prv_skeleton_update(skeleton, 0.0f);

    game_manager_remove_tick_callback(skeleton->game_manager, skeleton->tick_callback_id);
    skeleton->tick_callback_id = 0xFFFFFFFF;
}

void
skeleton_unload_animations(te_skeleton* skeleton) {
    void* item;
    size_t iter;

    skeleton_stop_animation(skeleton);

    iter = 0;
    while (hashmap_iter(skeleton->preloaded_anims, &iter, &item)) {
        const te_skeleton_animation** ptr = item;
        te_skeleton_animation* anim = (te_skeleton_animation*)*ptr;
        skeleton_animation_destroy(anim);
    }
    hashmap_clear(skeleton->preloaded_anims, false);
}
