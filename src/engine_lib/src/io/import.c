#include <io/import.h>

#include <snprintf.h>
#include <game_manager.h>
#include <world.h>
#include <game/model.h>
#include <game/skeleton.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <math/math_funcs.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stb/stb_image.h>
#include <cglm/affine.h>
#include <cglm/euler.h>
#include <cglm/quat.h>

/* Returns absolute path to the destination texture (located in the "res" directory).
 * You must free returned pointer. */
static char*
save_texture(
    size_t node_idx, size_t prim_idx, const char* path_to_gltf_dir,
    unsigned int path_to_gltf_dir_len, cgltf_image* image, const char* path_to_tex_dir,
    unsigned int path_to_tex_dir_len) {
    char* path_to_src_image;
    char* tex_name;
    char* absolute_tex_path;
    stbi_uc* image_data;
    unsigned int i;
    int tex_name_len;
    int width;
    int height;
    int channel_count;

    /* check image URI */
    if (image->uri != NULL && image->uri[0] != '\0') {
        /* don't handle slashes in URI (for now) */
        const unsigned int uri_len = (unsigned int)strlen(image->uri);
        for (i = 0; i < uri_len; i++) {
            if (image->uri[i] == '/' || image->uri[i] == '\\') {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found path in the image path (paths in images not supported): %s",
                    image->uri);
                abort();
            }
        }

        path_to_src_image = filesystem_append_path(
            path_to_gltf_dir, path_to_gltf_dir_len, image->uri, uri_len, NULL);
        if (filesystem_does_path_exists(path_to_src_image)) {
            char* path_to_dst_image = filesystem_append_path(
                path_to_tex_dir, path_to_tex_dir_len, image->uri, uri_len, NULL);

            if (filesystem_does_path_exists(path_to_dst_image)) {
                /* multiple primitives using the same texture that we already saved */
                free(path_to_src_image);
                return path_to_dst_image;
            }

            filesystem_copy_file(path_to_src_image, path_to_dst_image);

            free(path_to_src_image);
            return path_to_dst_image;
        }

        free(path_to_src_image);
    }

    /* prepare a new texture name */
    tex_name_len = snprintf(NULL, 0, "tex_%zu_%zu", node_idx, prim_idx);
    if (tex_name_len < 0) {
        log_error(__FILE__, __LINE__, "snprintf error");
        abort();
    }
    tex_name = malloc(sizeof(char) * ((unsigned int)tex_name_len + 1));
    snprintf(tex_name, (unsigned int)tex_name_len + 1, "tex_%zu_%zu", node_idx, prim_idx);

    /* load texture */
    image_data = stbi_load_from_memory(
        (const stbi_uc*)image->buffer_view->buffer->data + image->buffer_view->offset,
        (int)image->buffer_view->size, &width, &height, &channel_count, 0);
    if (image_data == NULL) {
        log_error(__FILE__, __LINE__, "failed to load image data");
        abort();
    }

    /* write texture to disk */
    absolute_tex_path = NULL;
    if (strcmp(image->mime_type, "image/jpeg") == 0) {
        absolute_tex_path = filesystem_append_path_ext(
            path_to_tex_dir, path_to_tex_dir_len, tex_name, (unsigned int)tex_name_len, ".jpg",
            4, NULL);
        if (stbi_write_jpg(absolute_tex_path, width, height, channel_count, image_data, 100)
            != 1) {
            log_error(__FILE__, __LINE__, "failed to write image");
            abort();
        }
    } else if (strcmp(image->mime_type, "image/png") == 0) {
        absolute_tex_path = filesystem_append_path_ext(
            path_to_tex_dir, path_to_tex_dir_len, tex_name, (unsigned int)tex_name_len, ".png",
            4, NULL);
        if (stbi_write_png(
                absolute_tex_path, width, height, channel_count, image_data,
                width * channel_count)
            != 1) {
            log_error(__FILE__, __LINE__, "failed to write image");
            abort();
        }
    } else {
        stbi_image_free(image_data);
        free(tex_name);
        log_error_fmt(__FILE__, __LINE__, "unknown image type %s", image->mime_type);
        abort();
    }

    stbi_image_free(image_data);

    free(tex_name);
    return absolute_tex_path;
}

static void
save_skeleton_node(cgltf_node* node, FILE* fp, cgltf_skin* skin) {
    mat4 transform;
    te_vec3 rot;
    te_vec4 loc;
    mat4 rot_mat;
    te_vec3 scale;
    size_t i;
    unsigned int child_count;
    unsigned int name_len;

    name_len = 0;
    if (node->name != NULL) {
        name_len = (unsigned int)strlen(node->name);
    }

    /* save name */
    fwrite(&name_len, sizeof(name_len), 1, fp);
    if (node->name != NULL) {
        fwrite(node->name, sizeof(char), name_len, fp);
    }

    /* prepare local transform */
    glm_mat4_identity(transform);
    if (node->has_matrix) {
        vec4_copy(&node->matrix[0], transform[0]);
        vec4_copy(&node->matrix[4], transform[1]);
        vec4_copy(&node->matrix[8], transform[2]);
        vec4_copy(&node->matrix[12], transform[3]);
    } else if (node->has_translation || node->has_rotation || node->has_scale) {
        mat4 translate_mat;
        mat4 rot_mat;
        mat4 scale_mat;

        glm_mat4_identity(translate_mat);
        glm_mat4_identity(rot_mat);
        glm_mat4_identity(scale_mat);

        if (node->has_translation) {
            glm_translate_make(translate_mat, node->translation);
        }
        if (node->has_rotation) {
            mat4 mat;
            glm_quat_mat4(node->rotation, mat);

            glm_euler_angles(mat, rot);
            rot[0] = glm_deg(rot[0]);
            rot[1] = glm_deg(rot[1]);
            rot[2] = glm_deg(rot[2]);

            math_make_rotation_mat(rot, rot_mat);
        }
        if (node->has_scale) {
            glm_scale_make(scale_mat, node->scale);
        }

        /* scale, rotate and then translate */
        glm_mat4_mul(rot_mat, scale_mat, transform);
        glm_mat4_mul(translate_mat, transform, transform);
    }

    glm_decompose(transform, loc, rot_mat, scale);
    glm_euler_angles(rot_mat, rot);
    rot[0] = glm_deg(rot[0]);
    rot[1] = glm_deg(rot[1]);
    rot[2] = glm_deg(rot[2]);

    /* save local transform */
    fwrite(&loc, sizeof(float), 3, fp);
    fwrite(&rot, sizeof(float), 3, fp);
    fwrite(&scale, sizeof(float), 3, fp);

    /* save inverse bind pose matrix */
    glm_mat4_identity(transform);
    if (skin->inverse_bind_matrices != NULL) {
        cgltf_accessor* accessor = skin->inverse_bind_matrices;
        cgltf_buffer_view* buffer_view = accessor->buffer_view;

        mat4* data = (mat4*)((char*)buffer_view->buffer->data
                             + (buffer_view->offset + accessor->offset));

        for (i = 0; i < skin->joints_count; i++) {
            if (node != skin->joints[i]) {
                continue;
            }
            data += i;
            glm_mat4_copy(*data, transform);
            break;
        }
    }
    fwrite(&transform, sizeof(float), 4 * 4, fp);

    /* save child count */
    child_count = (unsigned int)node->children_count;
    fwrite(&child_count, sizeof(child_count), 1, fp);

    for (i = 0; i < node->children_count; i++) {
        save_skeleton_node(node->children[i], fp, skin);
    }
}

static char*
import_skeleton(
    cgltf_node* node, const char* path_to_anim_dir, unsigned int path_to_anim_dir_len,
    cgltf_skin* skin) {
    char* path_to_skeleton;
    FILE* fp;
    unsigned int total_bone_count;

    if (skin->joints_count > TE_MAX_BONE_COUNT) {
        log_error_fmt(
            __FILE__, __LINE__,
            "found skeleton with %zu bones while max allowed bone count is %i, "
            "you can increase the bone count limit in the C code",
            skin->joints_count, TE_MAX_BONE_COUNT);
        abort();
    }

    if (!filesystem_does_path_exists(path_to_anim_dir)) {
        filesystem_create_directory(path_to_anim_dir);
    }

    path_to_skeleton = filesystem_append_path(
        path_to_anim_dir, path_to_anim_dir_len, "skeleton.bin", 12, NULL);

    fp = fopen(path_to_skeleton, "wb");
    if (fp == NULL) {
        log_error_fmt(__FILE__, __LINE__, "failed to create file at %s", path_to_skeleton);
        abort();
    }

    total_bone_count = (unsigned int)skin->joints_count;
    fwrite(&total_bone_count, sizeof(total_bone_count), 1, fp);

    save_skeleton_node(node, fp, skin);

    fclose(fp);

    return path_to_skeleton;
}

static bool
keyframe_value_eps_cmp(te_vec4 a, te_vec4 b) {
    const float eps = 0.002f;
    return math_abs(a[0] - b[0]) <= eps && math_abs(a[1] - b[1]) <= eps && math_abs(a[2] - b[2]) <= eps
           && math_abs(a[3] - b[3]) <= eps;
}

static void
keyframe_value_to_vec4(enum te_animation_channel_type channel_type, float* values, te_vec4 dst) {
    if (channel_type == TE_ACT_ROTATION) {
        glm_quat_copy(values, dst);
        glm_quat_normalize(dst);
    } else {
        vec3_copy(values, dst);
        dst[3] = 0.0f;
    }
}

static void
count_anim_bone_count(
    unsigned int* bone_idx, cgltf_node* skin_node, cgltf_animation* anim,
    unsigned int* anim_bone_count) {
    size_t node_idx;
    unsigned int channel_idx;

    for (channel_idx = 0; channel_idx < anim->channels_count; channel_idx++) {
        if (anim->channels[channel_idx].target_node != skin_node) {
            continue;
        }

        (*anim_bone_count) += 1;
        break;
    }

    (*bone_idx) += 1;
    for (node_idx = 0; node_idx < skin_node->children_count; node_idx++) {
        count_anim_bone_count(bone_idx, skin_node->children[node_idx], anim, anim_bone_count);
    }
}

static bool
save_anim_channel(
    unsigned int* bone_idx, cgltf_node* skin_node, cgltf_animation* anim,
    float* anim_duration_sec, FILE* fp) {
    cgltf_animation_channel* channel;
    cgltf_accessor* input;
    cgltf_accessor* output;
    cgltf_buffer_view* time_buffer_view;
    cgltf_buffer_view* value_buffer_view;
    te_vec4 first_value;
    te_vec4 prev_value;
    te_vec4 step;
    float* values;
    float* timestamps;
    float duration_sec;
    float prev_timestamp;
    float end_mark;
    size_t i;
    unsigned char channel_type;
    unsigned char interpolation_type;
    unsigned int value_comp_count;
    unsigned int channel_idx;
    bool step_found;
    bool write_keyframe;

    /* find all channels that animate this skeleton bone
     * in GLTF 1 channel animates 1 property (such as translation) */
    bool found_channel = false;
    for (channel_idx = 0; channel_idx < anim->channels_count; channel_idx++) {
        if (anim->channels[channel_idx].target_node != skin_node) {
            continue;
        }

        channel = &anim->channels[channel_idx];

        if (channel->target_path != cgltf_animation_path_type_translation
            && channel->target_path != cgltf_animation_path_type_rotation
            && channel->target_path != cgltf_animation_path_type_scale) {
            continue;
        }

        input = channel->sampler->input;
        output = channel->sampler->output;

        if (!input->has_max) {
            log_error(
                __FILE__, __LINE__, "found an animation channel without the \"max\" field");
            abort();
        }
        duration_sec = input->max[0];
        if (duration_sec > *anim_duration_sec) {
            (*anim_duration_sec) = duration_sec;
        }

        if (input->type != cgltf_type_scalar) {
            log_error(
                __FILE__, __LINE__,
                "found an animation channel with an unexpected input type");
            abort();
        }
        if (output->type != cgltf_type_te_vec3 && output->type != cgltf_type_vec4) {
            log_error(
                __FILE__, __LINE__,
                "found an animation channel with an unexpected output type");
            abort();
        }

        if (!found_channel) {
            /* write bone index before writing all channels that affect it */
            fwrite(bone_idx, sizeof(*bone_idx), 1, fp);
        } else {
            unsigned int same_bone_mark = 0xFFFFFFFF - 1;
            fwrite(&same_bone_mark, sizeof(same_bone_mark), 1, fp);
        }
        found_channel = true;

        /* write channel type */
        channel_type = TE_ACT_POSITION;
        if (channel->target_path == cgltf_animation_path_type_rotation) {
            channel_type = TE_ACT_ROTATION;
        } else if (channel->target_path == cgltf_animation_path_type_scale) {
            channel_type = TE_ACT_SCALE;
        }
        fwrite(&channel_type, sizeof(channel_type), 1, fp);

        /* write interpolation type */
        interpolation_type = 0;
        if (channel->sampler->interpolation == cgltf_interpolation_type_step) {
            interpolation_type = TE_KIT_STEP;
        } else if (channel->sampler->interpolation == cgltf_interpolation_type_linear) {
            interpolation_type = TE_KIT_LINEAR;
        } else if (channel->sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
            /* interpolation_type = TE_KIT_CUBIC_SPLINE; */
            /* Usually GLTF stores linear but if found a file with cubic spline test how our
             * keyframe skipping algorithm works with it. */
            log_error(__FILE__, __LINE__, "unsupported interpolation type - TODO?");
            abort();
        } else {
            log_error(
                __FILE__, __LINE__,
                "found an animation channel with an unexpected channel "
                "interpolation type");
            abort();
        }
        fwrite(&interpolation_type, sizeof(interpolation_type), 1, fp);

        /* get timestamps */
        if (input->count == 0) {
            log_error(__FILE__, __LINE__, "found an animation channel with 0 timestamps");
            abort();
        }
        time_buffer_view = input->buffer_view;
        timestamps = (float*)((char*)time_buffer_view->buffer->data
                              + (time_buffer_view->offset + input->offset));

        /* get values */
        value_buffer_view = output->buffer_view;
        values = (float*)((char*)value_buffer_view->buffer->data
                          + (value_buffer_view->offset + output->offset));

        value_comp_count = channel_type == TE_ACT_ROTATION ? 4 : 3;

        /* process all keyframes but remove redundant values */
        keyframe_value_to_vec4(channel_type, values, first_value);

        prev_timestamp = *timestamps;
        vec4_copy(first_value, prev_value);

        if (*timestamps < 0.0f) {
            log_error_fmt(
                __FILE__, __LINE__, "found negative timestamp %f at index 0", *timestamps);
            abort();
        }
        fwrite(timestamps, sizeof(float), 1, fp);
        fwrite(first_value, sizeof(vec4), 1, fp);

        timestamps += 1;
        values += value_comp_count;

        step_found = false;
        write_keyframe = true;
        for (i = 1; i < output->count; i++) {
            te_vec4 value;
            keyframe_value_to_vec4(channel_type, values, value);

            if (i + 1 < output->count) {
                if (!step_found) {
                    te_vec4 diff;
                    vec4_sub(value, prev_value, diff);
                    vec4_copy(diff, step);

                    step_found = true;
                    write_keyframe = false;
                } else {
                    te_vec4 expected;
                    vec4_copy(prev_value, expected);
                    glm_vec4_add(expected, step, expected);

                    if (keyframe_value_eps_cmp(value, expected)) {
                        write_keyframe = false;
                    } else {
                        te_vec4 diff;

                        fwrite(&prev_timestamp, sizeof(float), 1, fp);
                        fwrite(prev_value, sizeof(vec4), 1, fp);

                        vec4_sub(value, prev_value, diff);
                        vec4_copy(diff, step);

                        write_keyframe = false;
                    }
                }
            }

            if (write_keyframe) {
                fwrite(timestamps, sizeof(float), 1, fp);
                fwrite(value, sizeof(vec4), 1, fp);
            }

            if (*timestamps < 0.0f) {
                log_error_fmt(
                    __FILE__, __LINE__, "found negative timestamp %f at index %u", *timestamps,
                    i);
                abort();
            }

            vec4_copy(value, prev_value);
            prev_timestamp = *timestamps;

            timestamps += 1;
            values += value_comp_count;

            write_keyframe = true;
        }

        end_mark = -1.0f;
        fwrite(&end_mark, sizeof(end_mark), 1, fp);
    }

    (*bone_idx) += 1;
    for (i = 0; i < skin_node->children_count; i++) {
        found_channel |=
            save_anim_channel(bone_idx, skin_node->children[i], anim, anim_duration_sec, fp);
    }

    return found_channel;
}

static void
import_animation(
    cgltf_animation* anim, const char* path_to_anim_dir, unsigned int path_to_anim_dir_len,
    cgltf_node* skin_root_node) {
    char* path_to_anim;
    FILE* fp;
    float anim_duration;
    unsigned int bone_idx;
    unsigned int anim_bone_count;

    if (!filesystem_does_path_exists(path_to_anim_dir)) {
        log_error(__FILE__, __LINE__, "expected a skeleton to be already imported");
        abort();
    }
    if (anim->name == NULL) {
        log_error(__FILE__, __LINE__, "found animation without a name");
        abort();
    }

    path_to_anim = filesystem_append_path_ext(
        path_to_anim_dir, path_to_anim_dir_len, anim->name, 0, ".anim", 5, NULL);

    fp = fopen(path_to_anim, "wb");
    if (fp == NULL) {
        log_error_fmt(__FILE__, __LINE__, "failed to create file at %s", path_to_anim);
        abort();
    }

    /* write animated bone count */
    bone_idx = 0;
    anim_bone_count = 0;
    count_anim_bone_count(&bone_idx, skin_root_node, anim, &anim_bone_count);
    fwrite(&anim_bone_count, sizeof(anim_bone_count), 1, fp);

    /* write bone animations */
    bone_idx = 0;
    anim_duration = 0.0f;
    if (!save_anim_channel(&bone_idx, skin_root_node, anim, &anim_duration, fp)) {
        log_error(
            __FILE__, __LINE__,
            "unable to find a single animation channel that animates skeleton bones");
        abort();
    }

    /* mark bone info end */
    bone_idx = 0xFFFFFFFF;
    fwrite(&bone_idx, sizeof(bone_idx), 1, fp);

    /* write anim duration */
    fwrite(&anim_duration, sizeof(anim_duration), 1, fp);

    fclose(fp);
    free(path_to_anim);
}

static void
save_primitive(
    const char* path_to_gltf_dir, unsigned int path_to_gltf_dir_len,
    cgltf_primitive* primitive, size_t node_idx, size_t prim_idx, const char* path_to_file,
    te_model* model, const char* path_to_tex_dir, unsigned int path_to_tex_dir_len,
    bool* found_skin) {
    te_vertex_pack* vertices;
    unsigned short* indices;
    FILE* fp;
    size_t pos_attribute_idx = 0xFFFFFFFF;
    size_t normal_attribute_idx = 0xFFFFFFFF;
    size_t texcoord_attribute_idx = 0xFFFFFFFF;
    size_t joints_attribute_idx = 0xFFFFFFFF;
    size_t weights_attribute_idx = 0xFFFFFFFF;
    size_t i;
    unsigned int vertex_count;
    unsigned int index_count;
    unsigned char id;
    bool is_skinned;
    (*found_skin) = false;

    /* check index type */
    if (primitive->indices->component_type != cgltf_component_type_r_16u) {
        log_error_fmt(
            __FILE__, __LINE__,
            "found GLTF mesh with unsupported indices type %d, indices type must be "
            "UNSIGNED_SHORT",
            primitive->indices->component_type);
        abort();
    }

    /* check index count */
    if (primitive->indices->count
        > 0xFFFFFFFFu) { // because we store index count as unsigned int
        log_error_fmt(
            __FILE__, __LINE__, "GLTF mesh index count exceeds limit of %u", 0xFFFFFFFFu);
        abort();
    }

    /* check attribute types */
    for (i = 0; i < primitive->attributes_count; i++) {
        if (primitive->attributes[i].type == cgltf_attribute_type_position) {
            pos_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec3) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported position type %d, expected vec3",
                    primitive->attributes[i].data->type);
                abort();
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_32f) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported position component type %d, "
                    "expected "
                    "float",
                    primitive->attributes[i].data->type);
                abort();
            }
        } else if (primitive->attributes[i].type == cgltf_attribute_type_normal) {
            normal_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec3) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported normal type %d, expected vec3",
                    primitive->attributes[i].data->type);
                abort();
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_32f) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported normal component type %d, expected "
                    "float",
                    primitive->attributes[i].data->type);
                abort();
            }
        } else if (primitive->attributes[i].type == cgltf_attribute_type_texcoord) {
            texcoord_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec2) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported texcoord type %d, expected vec2",
                    primitive->attributes[i].data->type);
                abort();
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_32f) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported texcoord component type %d, "
                    "expected "
                    "float",
                    primitive->attributes[i].data->type);
                abort();
            }
        } else if (primitive->attributes[i].type == cgltf_attribute_type_joints) {
            joints_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec4) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported joints type %d, expected vec4",
                    primitive->attributes[i].data->type);
                abort();
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_8u) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported joints component type %d, expected "
                    "unsigned byte",
                    primitive->attributes[i].data->type);
                abort();
            }
        } else if (primitive->attributes[i].type == cgltf_attribute_type_weights) {
            weights_attribute_idx = i;

            if (primitive->attributes[i].data->type != cgltf_type_vec4) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported weights type %d, expected vec4",
                    primitive->attributes[i].data->type);
                abort();
            }
            if (primitive->attributes[i].data->component_type != cgltf_component_type_r_32f) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "found GLTF mesh with unsupported weights component type %d, expected "
                    "float",
                    primitive->attributes[i].data->type);
                abort();
            }
        }
    }
    if (pos_attribute_idx == 0xFFFFFFFF) {
        log_error(__FILE__, __LINE__, "found GLTF mesh without a POSITION attribute");
        abort();
    } else if (normal_attribute_idx == 0xFFFFFFFF) {
        log_error(__FILE__, __LINE__, "found GLTF mesh without a NORMAL attribute");
        abort();
    } else if (texcoord_attribute_idx == 0xFFFFFFFF) {
        log_error(__FILE__, __LINE__, "found GLTF mesh without a TEXCOORD attribute");
        abort();
    }

    if (joints_attribute_idx != 0xFFFFFFFF || weights_attribute_idx != 0xFFFFFFFF) {
        if (joints_attribute_idx == 0xFFFFFFFF || weights_attribute_idx == 0xFFFFFFFF) {
            log_error(
                __FILE__, __LINE__, "found skin joints or weights but expected to find both");
            abort();
        }
        (*found_skin) = true;
    }

    /* check vertex count */
    if (primitive->attributes[pos_attribute_idx].data->count
        > 0xFFFFFFFFu) { /* because we store vertex count as unsigned int */
        log_error_fmt(
            __FILE__, __LINE__, "GLTF mesh vertex count exceeds limit of %u", 0xFFFFFFFFu);
        abort();
    }
    if (primitive->attributes[pos_attribute_idx].data->count == 0) {
        log_error(__FILE__, __LINE__, "GLTF mesh vertex count is zero");
        abort();
    }

    /* load indices */
    index_count = (unsigned int)primitive->indices->count;
    indices = malloc(sizeof(unsigned short) * index_count);
    {
        cgltf_buffer_view* index_buffer_view = primitive->indices->buffer_view;
        const size_t index_stride = index_buffer_view->stride == 0 ? sizeof(unsigned short)
                                                                   : index_buffer_view->stride;

        char* index_data = (char*)index_buffer_view->buffer->data
                           + (index_buffer_view->offset + primitive->indices->offset);

        for (i = 0; i < primitive->indices->count; i++) {
            indices[i] = *(unsigned short*)index_data;
            index_data += index_stride;
        }
    }

    /* load vertices */
    vertex_count = (unsigned int)primitive->attributes[pos_attribute_idx].data->count;
    is_skinned = joints_attribute_idx != 0xFFFFFFFF;
    vertices = vertex_pack_create(vertex_count, is_skinned);
    {
        /* load position */
        {
            cgltf_accessor* accessor = primitive->attributes[pos_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(vec3) : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                          [vertices->vertex_sizeof * i
                                           + vertices->attribute_offsets[TE_VA_POSITION]];
                vec3_copy(*(vec3*)data, (float*)dst);
                data += stride;
            }
        }

        /* load normal */
        {
            cgltf_accessor* accessor = primitive->attributes[normal_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(vec3) : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                          [vertices->vertex_sizeof * i
                                           + vertices->attribute_offsets[TE_VA_NORMAL]];
                vec3_copy(*(vec3*)data, (float*)dst);
                data += stride;
            }
        }

        /* load UV */
        {
            cgltf_accessor* accessor = primitive->attributes[texcoord_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            const size_t stride =
                buffer_view->stride == 0 ? sizeof(vec2) : buffer_view->stride;
            char* data =
                (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (i = 0; i < vertex_count; i++) {
                unsigned char* dst =
                    &vertices->data
                         [vertices->vertex_sizeof * i + vertices->attribute_offsets[TE_VA_UV]];
                vec2_copy(*(vec2*)data, (float*)dst);
                data += stride;
            }
        }

        /* load bone indices */
        if (joints_attribute_idx != 0xFFFFFFFF) {
            char* data;
            size_t stride;
            cgltf_accessor* accessor = primitive->attributes[joints_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            assert(sizeof(unsigned char) == sizeof(te_bone_index_t));

            stride =
                buffer_view->stride == 0 ? sizeof(unsigned char) * 4 : buffer_view->stride;
            data = (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                          [vertices->vertex_sizeof * i
                                           + vertices->attribute_offsets[TE_VA_BONE_INDICES]];
                memcpy(dst, data, sizeof(unsigned char) * 4);
                data += stride;
            }
        }

        /* load bone weights */
        if (weights_attribute_idx != 0xFFFFFFFF) {
            size_t stride;
            char* data;
            cgltf_accessor* accessor = primitive->attributes[weights_attribute_idx].data;
            cgltf_buffer_view* buffer_view = accessor->buffer_view;

            stride = buffer_view->stride == 0 ? sizeof(vec4) : buffer_view->stride;
            data = (char*)buffer_view->buffer->data + (buffer_view->offset + accessor->offset);

            for (i = 0; i < vertex_count; i++) {
                unsigned char* dst = &vertices->data
                                          [vertices->vertex_sizeof * i
                                           + vertices->attribute_offsets[TE_VA_BONE_WEIGHTS]];
                vec4_copy(*(vec4*)data, (float*)dst);
                data += stride;
            }
        }
    }

    fp = fopen(path_to_file, "wb");
    if (fp == NULL) {
        log_error_fmt(__FILE__, __LINE__, "failed to open file for writing %s", path_to_file);
        abort();
    }

    id = is_skinned ? 100 : 0;
    fwrite(&id, sizeof(id), 1, fp);

    fwrite(&vertex_count, sizeof(vertex_count), 1, fp);
    fwrite(vertices->data, vertices->vertex_sizeof, vertex_count, fp);

    fwrite(&index_count, sizeof(index_count), 1, fp);
    fwrite(indices, sizeof(unsigned short), index_count, fp);

    fclose(fp);
    vertex_pack_destroy(vertices);
    free(indices);

    /* save material */
    if (primitive->material != NULL) {
        /* color */
        te_vec4 rgba;
        vec3_copy(primitive->material->pbr_metallic_roughness.base_color_factor, rgba);
        rgba[3] = 1.0f;
        model_set_color(model, rgba);

        /* texture */
        if (primitive->material->pbr_metallic_roughness.base_color_texture.texture != NULL) {
            char* absolute_tex_path;
            char* relative_path;
            cgltf_texture* tex;

            tex = primitive->material->pbr_metallic_roughness.base_color_texture.texture;

            if (!filesystem_does_path_exists(path_to_tex_dir)) {
                filesystem_create_directory(path_to_tex_dir);
            }

            absolute_tex_path = save_texture(
                node_idx, prim_idx, path_to_gltf_dir, path_to_gltf_dir_len, tex->image,
                path_to_tex_dir, path_to_tex_dir_len);
            relative_path = filesystem_convert_path_to_relative(absolute_tex_path);

            model_set_texture(model, relative_path);

            free(absolute_tex_path);
            free(relative_path);
        }
    }
}

bool
import_file_as_world(
    te_game_manager* game_manager, const char* path_to_file,
    const char* relative_path_to_dir) {
    cgltf_options options = {0};
    cgltf_data* data;
    te_world* world;
    cgltf_result result;
    char* res_path;
    char* relative_skel_path;
    cgltf_node* skin_root;
    cgltf_skin* skin;
    const char* filename;
    char* path_to_gltf_dir;
    char* geo_dir;
    char* res_out_dir;
    char* tex_dir;
    char* anim_dir;
    size_t i;
    unsigned int filename_len;
    unsigned int res_out_dir_len;
    unsigned int res_path_len;
    unsigned int path_to_gltf_dir_len;
    unsigned int geo_dir_len;
    unsigned int tex_dir_len;
    unsigned int anim_dir_len;

    res_path = filesystem_prepend_res_to_path(relative_path_to_dir, &res_path_len);
    if (!filesystem_does_path_exists(res_path)) {
        log_error_fmt(__FILE__, __LINE__, "the specified path %s does not exist", res_path);
        free(res_path);
        return false;
    }

    filename = filesystem_find_filename(path_to_file, false, &filename_len);
    if (filename == NULL) {
        log_error_fmt(__FILE__, __LINE__, "unable to parse filename from %s", path_to_file);
        free(res_path);
        return false;
    }

    /* because we will create a bunch of directories for imported objects (named after the GLTF file)
     * we need to make sure directory names don't have special characters that some OSes forbid */
    for (i = 0; i < filename_len; i++) {
        if (!((filename[i] >= 'A' && filename[i] <= 'Z')
              || (filename[i] >= 'a' && filename[i] <= 'z')
              || (filename[i] >= '0' && filename[i] <= '9') || filename[i] == '-'
              || filename[i] == '_')) {
            log_error_fmt(
                __FILE__, __LINE__,
                "file %s contains forbidden characters in the name, allowed characters: "
                "A-z, "
                "0-9, _, -",
                path_to_file);
            free(res_path);
            return false;
        }
    }

    /* prepare output directory path */
    res_out_dir = filesystem_append_path(
        res_path, res_path_len, filename, filename_len, &res_out_dir_len);

    free(res_path);
    res_path = NULL;

    /* make sure the output directory does not exist yet */
    if (filesystem_does_path_exists(res_out_dir)) {
        log_error_fmt(
            __FILE__, __LINE__,
            "attempted to create a new directory to import GLTF data but that directory "
            "already exists %s",
            res_out_dir);
        free(res_out_dir);
        return false;
    }

    /* parse GLTF */
    data = NULL;
    result = cgltf_parse_file(&options, path_to_file, &data);
    if (result != cgltf_result_success) {
        log_error_fmt(__FILE__, __LINE__, "failed to parse GLTF file %s", path_to_file);
        free(res_out_dir);
        return false;
    }
    if (data->scene == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "parsed GLTF file has invalid scene %s", path_to_file);
        free(res_out_dir);
        return false;
    }
    result = cgltf_load_buffers(&options, data, path_to_file);
    if (result != cgltf_result_success) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to load buffers of parsed GLTF file %s", path_to_file);
        free(res_out_dir);
        return false;
    }

    world = game_manager_create_world(game_manager, "import_gltf_world");
    if (world == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to create a new world to import %s", path_to_file);
        free(res_out_dir);
        return false;
    }

    /* get GLTF parent dir for later use */
    path_to_gltf_dir = filesystem_get_parent_path(path_to_file, 0, &path_to_gltf_dir_len);
    if (path_to_gltf_dir == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to get parent directory of %s", path_to_file);
        free(res_out_dir);
        return false;
    }

    filesystem_create_directory(res_out_dir);

    /* prepare path for newly imported geometry files */
    geo_dir = filesystem_append_path(res_out_dir, res_out_dir_len, "geo", 3, &geo_dir_len);
    filesystem_create_directory(geo_dir);

    /* prepare path for newly imported texture files */
    tex_dir = filesystem_append_path(res_out_dir, res_out_dir_len, "tex", 3, &tex_dir_len);
    /* note: don't create tex directory, we will create it if textures found */

    /* repare path for newly imported skeleton/animation files */
    anim_dir = filesystem_append_path(res_out_dir, res_out_dir_len, "anim", 4, &anim_dir_len);
    /* note: don't create anim directory, we will create it if skeleton/animation are found */

    /* first import skeletons to assign them to models later */
    relative_skel_path = NULL;
    skin_root = NULL;
    skin = NULL;
    if (data->skins_count > 0) {
        char* path_to_skeleton;

        if (data->skins_count > 1) {
            log_error(__FILE__, __LINE__, "found multiple skeletons, expected 0 or 1");
            abort();
        }

        skin = &data->skins[0];
        if (skin->joints_count == 0) {
            log_error(__FILE__, __LINE__, "found skeleton with 0 bones");
            abort();
        }

        skin_root = skin->joints[0];
        path_to_skeleton = import_skeleton(skin_root, anim_dir, anim_dir_len, skin);

        relative_skel_path = filesystem_convert_path_to_relative(path_to_skeleton);
        free(path_to_skeleton);
    }

    /* import models */
    for (i = 0; i < data->meshes_count; i++) {
        size_t prim_idx;

        cgltf_mesh* mesh = &data->meshes[i];

        for (prim_idx = 0; prim_idx < mesh->primitives_count; prim_idx++) {
            char* geo_path;
            char* geo_relative;
            char* geo_name;
            unsigned int geo_name_len;
            bool found_skin;

            cgltf_primitive* primitive = &mesh->primitives[prim_idx];
            te_model* model = model_create();

            /* prepare geometry bin file name */
            int len = -1;
            if (mesh->name == NULL) {
                len = snprintf(NULL, 0, "%zu_%zu", i, prim_idx);
            } else {
                len = snprintf(NULL, 0, "%s_%zu", mesh->name, prim_idx);
            }
            if (len < 0) {
                log_error(__FILE__, __LINE__, "snprintf error");
                abort();
            }
            geo_name_len = (unsigned int)len;

            geo_name = malloc(sizeof(char) * (geo_name_len + 1));
            if (mesh->name == NULL) {
                snprintf(geo_name, geo_name_len + 1, "%zu_%zu", i, prim_idx);
            } else {
                snprintf(geo_name, geo_name_len + 1, "%s_%zu", mesh->name, prim_idx);
            }

            geo_path = filesystem_append_path_ext(
                geo_dir, geo_dir_len, geo_name, geo_name_len, ".geo", 4, NULL);

            found_skin = false;
            save_primitive(
                path_to_gltf_dir, path_to_gltf_dir_len, primitive, i, prim_idx, geo_path,
                model, tex_dir, tex_dir_len, &found_skin);

            /* set geometry */
            geo_relative = filesystem_convert_path_to_relative(geo_path);
            model_set_geometry(model, geo_relative);

            /* set skeleton */
            if (relative_skel_path != NULL && found_skin) {
                model_set_skeleton_path(model, relative_skel_path);
            }

            /* spawn */
            world_spawn_game_object(world, model, model_get_game_object_info());

            free(geo_path);
            free(geo_relative);
            free(geo_name);
        }
    }

    /* import animations */
    for (i = 0; i < data->animations_count; i++) {
        cgltf_animation* anim = &data->animations[i];
        import_animation(anim, anim_dir, anim_dir_len, skin_root);
    }

    /* save new world */
    {
        if (data->meshes_count > 0) {
            char* world_path = filesystem_append_path_ext(
                res_out_dir, res_out_dir_len, filename, filename_len, ".txt", 4, NULL);

            char* relative_path = filesystem_convert_path_to_relative(world_path);
            if (relative_path == NULL) {
                log_error_fmt(
                    __FILE__, __LINE__,
                    "failed to convert path %s to be relative to the \"res\" directory",
                    world_path);
                abort();
            }

            world_save_to_file(world, relative_path, false);

            free(relative_path);
            free(world_path);
        }
        game_manager_destroy_world(game_manager, world);
    }

    cgltf_free(data);
    free(res_out_dir);
    free(geo_dir);
    free(tex_dir);
    free(anim_dir);
    free(relative_skel_path);
    free(path_to_gltf_dir);

    return true;
}
