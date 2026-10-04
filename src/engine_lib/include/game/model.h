#pragma once

#include <stdbool.h>
#include <math/mat4.h>
#include <math/vec2.h>

typedef struct te_model te_model;
struct te_world;
struct te_camera;
struct te_model_renderer;
struct te_game_object_info;
struct te_skeleton;
struct te_sound;
struct te_aabb_shape;

/* ------------------------------------------------------------------------------------------------
 *                                      VERTEX API
 * ------------------------------------------------------------------------------------------------ */

/* components of a vertex */
enum te_vertex_attribute {
    TE_VA_POSITION = 0,
    TE_VA_NORMAL,
    TE_VA_UV,
    TE_VA_BONE_INDICES,
    TE_VA_BONE_WEIGHTS,

    TE_VA_COUNT /* marks the size of this enum */
};

/* manages vertex access */
typedef struct te_vertex_pack {
#if defined(ENGINE_GLES)
    /* binds vertex attribute names to a shader program, used after creating a model's shader */
    void (*bind_gl_vertex_attributes)(unsigned int shader_prog_id);
#endif

    /* sets pointers to vertex attributes, used during the rendering */
    void (*set_attribute_pointers)(void);

    /* actual vertex data */
    unsigned char* data;

    /* number of vertices in @ref data */
    unsigned int vertex_count;

    /* sizeof a single vertex in @ref data */
    unsigned int vertex_sizeof;

    /* for each attribute stores an offset (in bytes) from the vertex start position
     * stores 255 if no such attribute */
    unsigned char attribute_offsets[TE_VA_COUNT];
} te_vertex_pack;

/* allocates vertices array and initializes all internal variables of the returned vertex pack */
te_vertex_pack* vertex_pack_create(unsigned int vertex_count, bool is_skinned);
void vertex_pack_destroy(te_vertex_pack* pack);

typedef struct te_model_vertex {
    /* NOTE: if changing this struct also update gl vertex attribute description and offsets */
    te_vec3 pos;
    te_vec3 normal;
    te_vec2 uv;
    /* NOTE: if changing this struct also update gl vertex attribute description and offsets */
} te_model_vertex;

typedef unsigned char te_bone_index_t;
typedef struct te_model_vertex_skinned {
    /* NOTE: if changing this struct also update gl vertex attribute description and offsets */
    te_vec3 pos;
    te_vec3 normal;
    te_vec2 uv;
    te_bone_index_t bone_indices[4];
    float boneWeights[4];
    /* NOTE: if changing this struct also update gl vertex attribute description and offsets */
} te_model_vertex_skinned;

/* ------------------------------------------------------------------------------------------------
 *                                      MODEL API
 * ------------------------------------------------------------------------------------------------ */

te_model* model_create(void);
/* creates a new model by loading the first model from the specified file
 * retuns NULL if no model found */
te_model* model_create_from_file(const char* relative_path);
void model_destroy(te_model* model);

/* returns always valid game object info */
struct te_game_object_info* model_get_game_object_info(void);

/* specify path (relative to the `res` directory) to the file that stores mesh geometry
 * specify NULL to use default model instead */
void model_set_geometry(te_model* model, const char* relative_path);
const char* model_get_geometry(te_model* model);

/* this geometry provider callback is useful for procedural CPU-generated meshes,
 * if you have static geometry on the disk use @ref model_set_geometry instead
 *
 * this callback function will be triggered when the model is being spawned,
 * in this case this callback has to provide vertices and indices for the model
 * 
 * if you specify `free_geometry` as `true` the model will handle freeing the provided geometry */
void model_set_custom_geometry_provider(
    te_model* model, void (*custom_get_geometry)(
                         te_model* model, te_vertex_pack** vertices, unsigned short** indices,
                         unsigned int* index_count, bool* free_custom_geometry));

/* optionally you can set a name of the model, the string will be copied
 * returns NULL if was not set previously */
void model_set_name(te_model* model, const char* name);
const char* model_get_name(te_model* model);

void model_set_position(te_model* model, te_vec3 position);
void model_set_rotation(te_model* model, te_vec3 rotation); /* in degrees */
void model_set_scale(te_model* model, te_vec3 scale);

void model_get_position(te_model* model, te_vec3 out);
void model_get_rotation(te_model* model, te_vec3 out); /* in degrees */
void model_get_scale(te_model* model, te_vec3 out);

/* unlike @ref model_get_position this function considers possible parent models */
void model_get_world_position(te_model* model, te_vec3 out);

/* sets path (relative to the `res` directory) to skeleton to use */
void model_set_skeleton_path(te_model* model, const char* relative_path);
const char* model_get_skeleton_path(te_model* model);
struct te_skeleton* model_get_skeleton(te_model* model);

/* sets color of the model in the RGBA format in range [0.0; 1.0]
 * note that alpha will be ignored if @ref model_enable_transparency is disabled */
void model_set_color(te_model* model, te_vec4 color);
void model_get_color(te_model* model, te_vec4 out);

/* sets path (relative to the `res` directory) to texture to use
 * the path string will be copied and stored in the model, specify NULL to remove texture */
void model_set_texture(te_model* model, const char* relative_path);

/* returns NULL if texture is not set
 * do not free returned string, valid while the model exists */
const char* model_get_texture(te_model* model);

/* sets texture tiling multiplier */
void model_set_texture_tiling(te_model* model, te_vec2 tex_tiling);
void model_get_texture_tiling(te_model* model, te_vec2 tex_tiling);

/* sets offset for UV coordinates */
void model_set_uv_offset(te_model* model, te_vec2 uv_offset);
void model_get_uv_offset(te_model* model, te_vec2 uv_offset);

/* disables backface culling */
void model_set_disable_backface_culling(te_model* model, bool disable_backface_culling);
bool model_get_disable_backface_culling(te_model* model);

/* child model's location/rotation/scale will then be treated as relative to the parents location/rotation/scale
 * 
 * if you want to attach to parent model's skeleton bone specify skeleton bone index or 0xFFFFFFFF to just attach to model
 * if the child model is not spawned but the parent is spawned will make the child model spawned
 * when parent is despawned/destroyed will also make the attached child despawn/destroy
 *
 * after attached do not attempt to despawn the child model using the world because the world only operates on "root" models
 * in order to despawn such model first detach it from the parent to make it "root" model and then despawn using the world
 *
 * specify NULL as parent to detach */
void model_set_parent(te_model* model, te_model* new_parent, unsigned int parent_bone_idx);
te_model* model_get_parent(te_model* model);
te_model* model_get_child_model(
    te_model* model,
    unsigned int index); /* if returns NULL for 0 index then has no children */
unsigned int model_get_child_model_count(te_model* model);
unsigned int model_get_parent_bone_idx(te_model* model);

/* same as @ref model_set_parent but attaches a camera, specify NULL to detach the camera
 * after attached do not attempt to despawn this camera using the world because the world only operates on "root" cameras
 * in order to despawn such camera first detach it from the parent to make it "root" camera and then despawn using the world */
void model_attach_camera(te_model* model, struct te_camera* camera);
struct te_camera* model_get_attached_camera(te_model* model);

/* the model takes the ownership of the sound, multiple sounds can be attached
 * spawns the sound if needed, the model does not autoplay the sound you need to do it yourself
 * the sound will not be destroyed until the model is destroyed but the sounds are stopped after the model is despawned */
void model_attach_sound(te_model* model, struct te_sound* sound);
struct te_sound* model_get_attached_sound(te_model* model, unsigned int idx);

/* optionally you can set a custom pointer to be stored in the model */
void model_set_custom_ptr(te_model* model, void* ptr);
void* model_get_custom_ptr(te_model* model);

/* optionally you can set a custom value which will also be saved/loaded along with the model */
void model_set_custom_value(te_model* model, unsigned int value);
unsigned int model_get_custom_value(te_model* model);

/* optionally you can set a custom callback that will be called after the model was spawned
 * or before it was despawned */
void
model_set_custom_on_after_spawned(te_model* model, void (*custom_on_after_spawned)(te_model*));
void model_set_custom_on_before_despawned(
    te_model* model, void (*custom_on_before_despawned)(te_model*));

/* optionally you can set a custom callback that will be called before the model is destroyed */
void model_set_custom_on_before_destroyed(
    te_model* model, void (*custom_on_before_destroyed)(te_model*));

/* specify path to the shader file (relative to the `res` directory), the string will be copied
 * to the model's object, specify NULL to remove */
void model_set_custom_vert_shader(te_model* model, const char* vert_relative_path);
const char* model_get_custom_vert_shader(te_model* model);

/* specify path to the shader file (relative to the `res` directory), the string will be copied
 * to the model's object, specify NULL to remove */
void model_set_custom_frag_shader(te_model* model, const char* frag_relative_path);
const char* model_get_custom_frag_shader(te_model* model);

/* transparency is disabled by default
 * note that this option is not required to draw something like grass blades
 * (for such case just import grass texture with alpha channel), this option is
 * used for cases where you need something with alpha blending (semi-trasparent objects)
 * note that there's no sorting for transparent geometry */
void model_enable_transparency(te_model* model, bool enable);
bool model_is_transparency_enabled(te_model* model);

/* allows disabling serialization of the model (enabled by default) */
void model_set_is_serialization_allowed(te_model* model, bool enable);
bool model_is_serialization_allowed(te_model* model);

/* returns NULL if the model is not spawned
 * do not free/destroy returned pointer, valid while the model exists */
struct te_world* model_get_world(te_model* model);

/* returns unique ID of this type in the type database */
const char* model_get_type_id(void);
/* registers the type in the type database */
void model_register_type(void);

/* ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------ */

/* returns model's world matrix (includes parent if has any) */
te_mat4* prv_model_get_world_mat_tmp(te_model* model);

/* returns NULL if not spawned */
struct te_aabb_shape* prv_model_get_world_aabb(te_model* model);

void prv_model_on_after_skeleton_updated(te_model* model);

/* returns 0xFFFFFFFF if the model is not being rendered,
 * otherwise handle into the model renderer's data array */
unsigned int prv_model_get_render_data_handle(te_model* model);

/* returns NULL if the model is not being rendered,
 * otherwise returns model renderer used to render the model */
struct te_model_renderer* prv_model_get_model_renderer(te_model* model);

/* used during the rendering */
void prv_model_set_attribute_pointers_model_vertex(void);
void prv_model_set_attribute_pointers_model_vertex_skinned(void);
