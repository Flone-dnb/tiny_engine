#include <world.h>

#include <stdlib.h>
#include <string.h>

#include <game/model.h>
#include <game/camera.h>
#include <game/game_object_info.h>
#include <game_manager.h>
#include <io/log.h>
#include <io/config.h>
#include <type_database.h>
#include <render/model_renderer.h>
#include <render/widget_renderer.h>
#include <render/renderer.h>
#include <widget/widget.h>
#include <window.h>
#if defined(ENGINE_DEBUG_TOOLS)
#include <glad/glad.h>
#include <render/gpu_time_section.h>
#endif

#define CONFIG_VAR_NAME_HAS_CHILD_MODEL "has_child_model"
#define CONFIG_VAR_NAME_HAS_ATTACHED_CAMERA "has_attached_camera"
#define CONFIG_VAR_NAME_CHILD_WIDGET_COUNT "child_widget_count"

// World represents several objects: audio system, cameras, game objects and etc.
struct te_world {
    // Always valid pointer. Game manager that owns this world. You should not free/destroy this pointer.
    te_game_manager* game_manager;

    // NULL if no active camera. Do not free/destroy this pointer. The camera will register/unregister itself.
    te_camera* active_camera;

    // Always valid pointer, size of this array is @ref spawned_root_game_object_array_size but the actually
    // used number of elements is @ref spawned_root_game_object_count. If some game object despawned some pointers
    // will be shifted to keep the array valid without any "holes". This array does not shrink
    // but the number of used (valid) elements may decrease.
    // Just like @ref spawned_widgets stores only root game objects.
    te_game_object_info** spawned_root_game_objects;

    // NULL if nothing spawned, size of this array is @ref spawned_widget_count.
    // Each widget here can have child widgets, this array only stores root widgets.
    te_widget** spawned_widgets;

    // Spawned widgets (from @ref spawned_widgets) that receive input (for example buttons).
    // Size of this array is @ref interactable_widget_count.
    te_widget** interactable_widgets;

    // Renders models of the world.
    te_model_renderer* opaque_model_renderer;
    te_model_renderer* transparent_model_renderer;

    // Renders widgets of the world.
    te_widget_renderer* widget_renderer;

    // May be NULL. Item from array @ref interactable_widgets that currently hovered.
    te_widget* hovered_interactable_widget;

    // World name.
    char* name;

    // Number of spawned game objects (valid elements) in @ref spawned_root_game_objects.
    unsigned int spawned_root_game_object_count;

    // Total number of elements that @ref spawned_root_game_objects can hold.
    unsigned int spawned_root_game_object_array_size;

    // Size of the array @ref spawned_widgets.
    unsigned int spawned_widget_count;

    // Size of the array @ref interactable_widgets.
    unsigned int interactable_widget_count;

    // `true` if the world is currently being destroyed.
    bool is_being_destroyed;

#if defined(ENGINE_DEBUG_TOOLS)
    // GPU time query IDs.
    unsigned int gl_query_draw_models;
    unsigned int gl_query_draw_widgets;
#endif
};

te_world*
prv_world_create(struct te_game_manager* game_manager, const char* name) {
    if (name == NULL) {
        log_error("world name must not be NULL");
        abort();
    }

    te_world* world = malloc(sizeof(te_world));

    world->game_manager = game_manager;

    world->active_camera = NULL;

    world->spawned_widgets = NULL;
    world->spawned_widget_count = 0;

    world->interactable_widgets = NULL;
    world->hovered_interactable_widget = NULL;
    world->interactable_widget_count = 0;

    world->spawned_root_game_object_count = 0;
    world->spawned_root_game_object_array_size = 128;
    world->spawned_root_game_objects = malloc(sizeof(te_game_object_info*) * world->spawned_root_game_object_array_size);

    world->opaque_model_renderer = model_renderer_create(128, 128);
    world->transparent_model_renderer = model_renderer_create(4, 4);
    world->widget_renderer = widget_renderer_create(game_manager_get_renderer(game_manager));
    world->is_being_destroyed = false;

    // Copy name.
    const size_t name_len = strlen(name);
    world->name = malloc(sizeof(char) * (name_len + 1));
    memcpy(world->name, name, name_len);
    world->name[name_len] = 0;

#if defined(ENGINE_DEBUG_TOOLS)
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {
        glGenQueriesEXT(1, &world->gl_query_draw_models);
        glGenQueriesEXT(1, &world->gl_query_draw_widgets);

        // Init timers.
        GPU_TIME_SECTION_BEGIN(world->gl_query_draw_models);
        GPU_TIME_SECTION_END;
        GPU_TIME_SECTION_BEGIN(world->gl_query_draw_widgets);
        GPU_TIME_SECTION_END;
    }
#endif

    return world;
}

void
prv_world_destroy(te_world* world) {
    world->is_being_destroyed = true;

    // Despawn and destroy world objects.
    {
        // Game objects.
        for (unsigned int i = 0; i < world->spawned_root_game_object_count; i++) {
            te_game_object_info* info = world->spawned_root_game_objects[i];
            info->on_despawned(info->game_object);
            info->destroy(info->game_object);
        }
        free(world->spawned_root_game_objects);

        // Widgets.
        while (world->spawned_widget_count > 0) {
            te_widget* widget = world->spawned_widgets[world->spawned_widget_count - 1];
            world->spawned_widget_count -= 1;
            prv_widget_on_despawned(widget);
            widget_destroy(widget);
        }
        free(world->spawned_widgets);
        if (world->interactable_widget_count > 0) {
            log_error("all widgets of a world were destroyed but there are still some "
                      "interactable widgets registered");
            abort();
        }
        free(world->interactable_widgets);
    }

    free(world->name);
    model_renderer_destroy(world->opaque_model_renderer);
    model_renderer_destroy(world->transparent_model_renderer);
    widget_renderer_destroy(world->widget_renderer);

#if defined(ENGINE_DEBUG_TOOLS)
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {
        glDeleteQueriesEXT(1, &world->gl_query_draw_models);
        glDeleteQueriesEXT(1, &world->gl_query_draw_widgets);
    }
#endif

    free(world);
}

bool
prv_world_is_being_destroyed(te_world* world) {
    return world->is_being_destroyed;
}

void
prv_world_on_window_size_changed(te_world* world) {
    if (world->active_camera == NULL) {
        return;
    }

    for (unsigned int i = 0; i < world->spawned_widget_count; i++) {
        prv_widget_on_window_size_changed(world->spawned_widgets[i]);
    }
}

void
prv_world_add_root_game_object_no_notify(
    te_world* world, struct te_game_object_info* info, bool ignore_if_already_added) {
    if (ignore_if_already_added) {
        for (unsigned int i = 0; i < world->spawned_root_game_object_count; i++) {
            if (world->spawned_root_game_objects[i] == info) {
                return;
            }
        }
    }

    if (world->spawned_root_game_object_count == world->spawned_root_game_object_array_size) {
        // Expand array.
        const unsigned int grow_size = 128;
        te_game_object_info** new_items = malloc(
            sizeof(te_game_object_info*)
            * (world->spawned_root_game_object_array_size + grow_size));
        memcpy(
            new_items, world->spawned_root_game_objects,
            sizeof(te_game_object_info*) * world->spawned_root_game_object_count);

        free(world->spawned_root_game_objects);
        world->spawned_root_game_objects = new_items;
        world->spawned_root_game_object_array_size += grow_size;
    }

    world->spawned_root_game_objects[world->spawned_root_game_object_count] = info;
    world->spawned_root_game_object_count += 1;
}

void
prv_world_remove_root_game_object_no_notify(
    te_world* world, struct te_game_object_info* info, bool must_exist_in_array) {
    // Find model.
    unsigned int idx = 0;
    bool found = false;
    for (unsigned int i = 0; i < world->spawned_root_game_object_count; i++) {
        if (world->spawned_root_game_objects[i] != info) {
            continue;
        }

        idx = i;
        found = true;
    }
    if (!found) {
        if (must_exist_in_array) {
            log_error("unable to despawn the specified game object: the object was not spawned "
                      "previously or is a child object (despawn root "
                      "object to despawn child objects or detach child object from parent first, "
                      "then despawn the game object)");
            abort();
        } else {
            return;
        }
    }

    // Remove from array (shift other elements).
    if (world->spawned_root_game_object_count > 1) {
        memmove(
            world->spawned_root_game_objects + idx,
            world->spawned_root_game_objects + (idx + 1),
            sizeof(te_game_object_info*) * (world->spawned_root_game_object_count - idx - 1));
    }
    world->spawned_root_game_object_count -= 1;
}

void
prv_world_add_root_widget_no_notify(
    te_world* world, struct te_widget* widget, bool check_if_already_added) {
    if (check_if_already_added) {
        for (unsigned int i = 0; i < world->spawned_widget_count; i++) {
            if (world->spawned_widgets[i] == widget) {
                return;
            }
        }
    }

    te_widget** new_widgets = malloc(sizeof(te_widget*) * (world->spawned_widget_count + 1));
    memcpy(
        new_widgets, world->spawned_widgets, sizeof(te_widget*) * world->spawned_widget_count);

    free(world->spawned_widgets);
    world->spawned_widgets = new_widgets;

    world->spawned_widgets[world->spawned_widget_count] = widget;
    world->spawned_widget_count += 1;
}

void
prv_world_remove_root_widget_no_notify(
    te_world* world, struct te_widget* widget, bool must_exist_in_array) {
    if (world->spawned_widget_count == 1) {
        if (world->spawned_widgets[0] != widget) {
            if (must_exist_in_array) {
                log_error("expected the widget to be spawned in this world");
                abort();
            } else {
                return;
            }
        }
        free(world->spawned_widgets);
        world->spawned_widgets = NULL;
        world->spawned_widget_count = 0;
    } else {
        unsigned int i = 0;
        bool found = false;
        for (; i < world->spawned_widget_count; i++) {
            if (world->spawned_widgets[i] != widget) {
                continue;
            }

            found = true;
            break;
        }
        if (!found) {
            if (must_exist_in_array) {
                log_error("unable to despawn the specified widget: the widget was not spawned "
                          "previously or is a child widget "
                          "(despawn root widget to despawn child widgets or detach child "
                          "widget from parent first, then despawn "
                          "the widget)");
                abort();
            } else {
                return;
            }
        }

        te_widget** new_widgets =
            malloc(sizeof(te_widget*) * (world->spawned_widget_count - 1));
        memcpy(new_widgets, world->spawned_widgets, sizeof(te_widget*) * i);
        memcpy(
            new_widgets + i, world->spawned_widgets + (i + 1),
            sizeof(te_widget*) * (world->spawned_widget_count - i - 1));

        free(world->spawned_widgets);
        world->spawned_widgets = new_widgets;
        world->spawned_widget_count -= 1;
    }
}

const char*
world_get_name(te_world* world) {
    return world->name;
}

void
world_set_active_camera(te_world* world, te_camera* camera) {
    if (world->active_camera == camera) {
        return;
    }

    if (camera_get_world(camera) != world) {
        log_error("in order to make a camera active in the world you first need to spawn the "
                  "camera in the world");
        abort();
    }

    world->active_camera = camera;

    prv_camera_on_active(world->active_camera);
}

static void
prv_save_widget_recursive(te_config* config, te_widget* widget) {
    if (!widget_is_serialization_allowed(widget)) {
        return;
    }

    const te_type_info* type_info =
        type_database_get_type_info(widget_get_owner_type_id(widget));
    const unsigned int section_idx =
        type_info_save_to_config(type_info, config, widget_get_owner(widget));

    unsigned int count;
    te_widget** child_widgets = widget_get_child_widgets(widget, &count);
    if (count == 0) {
        return;
    }

    // Don't count non serializable children.
    for (unsigned int i = 0; i < count; i++) {
        if (!widget_is_serialization_allowed(child_widgets[i])) {
            count -= 1;
        }
    }

    config_section_set_uint(config, section_idx, CONFIG_VAR_NAME_CHILD_WIDGET_COUNT, count);

    for (unsigned int i = 0; i < count; i++) {
        te_widget* widget = child_widgets[i];
        prv_save_widget_recursive(config, widget);
    }
    free(child_widgets);
}

void
world_save_to_file(
    te_world* world, const char* relative_path) {
    te_config* config = config_create(NULL);

    // Save lighting data.
    te_lighting_data* lighting_data =
        renderer_get_lighting_data(game_manager_get_renderer(world->game_manager));
    const unsigned int section_idx = config_create_section(config, "lighting_data");
    config_section_set_float_array(
        config, section_idx, "directional_light_color", lighting_data->directional_light_color,
        4);
    config_section_set_float_array(
        config, section_idx, "directional_light_direction",
        lighting_data->directional_light_direction, 3);
    config_section_set_float_array(
        config, section_idx, "point_light_color", lighting_data->point_light_color,
        4);
    config_section_set_float_array(
        config, section_idx, "point_light_pos_and_dist",
        lighting_data->point_light_pos_and_dist,
        4);
    config_section_set_float_array(
        config, section_idx, "ambient_light_color", lighting_data->ambient_light_color, 3);
    config_section_set_float_array(
        config, section_idx, "clear_color", lighting_data->clear_color, 3);
    config_section_set_float_array(
        config, section_idx, "distance_fog_color", lighting_data->distance_fog_color, 3);
    config_section_set_float_array(
        config, section_idx, "distance_fog_range", lighting_data->distance_fog_range, 2);

    // Save game objects.
    if (world->spawned_root_game_object_count > 0) {
        for (unsigned int idx = 0; idx < world->spawned_root_game_object_count; idx++) {
            te_game_object_info* info = world->spawned_root_game_objects[idx];

            const te_type_info* type_info = type_database_get_type_info(info->type_id);
            if (type_info == NULL) {
                continue;
            }

            if (!type_info->is_serialization_allowed(info->game_object)) {
                continue;
            }

            const unsigned int section_idx =
                type_info_save_to_config(type_info, config, info->game_object);

            if (info->type == TE_GOT_MODEL) {
                // Special case for models.
                te_model* model = info->game_object;
                te_model* child_model = model_get_child_model(model);
                te_camera* attached_camera = model_get_attached_camera(model);
                if (child_model != NULL) {
                    config_section_set_bool(
                        config, section_idx, CONFIG_VAR_NAME_HAS_CHILD_MODEL, true);
                    (void)type_info_save_to_config(type_info, config, child_model);
                }
                if (attached_camera != NULL) {
                    config_section_set_bool(
                        config, section_idx, CONFIG_VAR_NAME_HAS_ATTACHED_CAMERA, true);
                    (void)type_info_save_to_config(type_info, config, attached_camera);
                }
            }
        }
    }

    // Spawn widgets.
    if (world->spawned_widget_count > 0) {
        for (unsigned int widget_idx = 0; widget_idx < world->spawned_widget_count;
             widget_idx++) {
            te_widget* widget = world->spawned_widgets[widget_idx];
            prv_save_widget_recursive(config, widget);
        }
    }

    config_save(config, relative_path, false);
    config_destroy(config);
}

static void
prv_load_child_widgets_recursive(
    const char* relative_path, te_config* config, unsigned int section_count,
    te_widget* parent_widget, unsigned int parent_child_count, unsigned int* section_idx) {
    for (unsigned int child_idx = 0; child_idx < parent_child_count; child_idx++) {
        if ((*section_idx) >= section_count) {
            log_error_fmt(
                "unexpected end of file \"%s\", have %u sections while expected to have "
                "more",
                relative_path, section_count);
            abort();
        }

        const char* id = config_section_get_name(config, (*section_idx));
        const te_type_info* type_info = type_database_get_type_info(id);
        void* widget_owner = type_info->create();

        type_info_load_from_config(type_info, config, (*section_idx), widget_owner);
        if (type_info->get_widget == NULL) {
            log_error("expected a child object to be a widget");
            abort();
        }
        te_widget* child_widget = type_info->get_widget(widget_owner);
        widget_set_parent(child_widget, parent_widget);

        const unsigned int count = config_section_get_uint(
            config, (*section_idx), CONFIG_VAR_NAME_CHILD_WIDGET_COUNT, 0);

        (*section_idx) += 1;

        if (count > 0) {
            prv_load_child_widgets_recursive(
                relative_path, config, section_count, child_widget, count, section_idx);
        }
    }
}

void
world_add_from_file(te_world* world, const char* relative_path, bool load_lighting_data) {
    world_add_from_file_with_offset(
        world, relative_path, load_lighting_data, (vec3){0.0f, 0.0f, 0.0f});
}

static void load_vec_from_config(te_config* config, unsigned int section_idx, const char* key, unsigned int comp_count, float* target) {
    unsigned int count;
    float* array = config_section_get_float_array(config, section_idx, key, &count);
    if (count == 0) {
        log_error_fmt("expected to find the value \"%s\" in the config", key);
        abort();
    } else if (count != comp_count) {
        log_error_fmt(
            "unexpected array size found in the config, expected %u but found %u", comp_count,
            count);
        abort();
    }

    for (unsigned int i = 0; i < comp_count; i++) {
        target[i] = array[i];
    }
}

void
world_add_from_file_with_offset(
    te_world* world, const char* relative_path, bool load_lighting_data,
    vec3 location_offset) {
    const te_type_info* model_type_info = type_database_get_type_info(model_get_type_id());
    const te_type_info* camera_type_info = type_database_get_type_info(camera_get_type_id());

    te_config* config = config_create(relative_path);

    const unsigned int section_count = config_get_section_count(config);
    unsigned int section_idx = 0;

    if (section_count == 0) {
        log_error("expected world file to have at least 1 section");
        abort();
    }

    // Load lighting data.
    if (strcmp(config_section_get_name(config, section_idx), "lighting_data") == 0) {
        if (load_lighting_data) {
            te_lighting_data* lighting_data =
                renderer_get_lighting_data(game_manager_get_renderer(world->game_manager));

            load_vec_from_config(
                config, section_idx, "directional_light_color", 4,
                lighting_data->directional_light_color);
            load_vec_from_config(
                config, section_idx, "directional_light_direction", 3,
                lighting_data->directional_light_direction);
            load_vec_from_config(
                config, section_idx, "point_light_color", 4, lighting_data->point_light_color);
            load_vec_from_config(
                config, section_idx, "point_light_pos_and_dist", 4,
                lighting_data->point_light_pos_and_dist);
            load_vec_from_config(
                config, section_idx, "ambient_light_color", 3,
                lighting_data->ambient_light_color);
            load_vec_from_config(
                config, section_idx, "clear_color", 3, lighting_data->clear_color);
            load_vec_from_config(
                config, section_idx, "distance_fog_color", 3,
                lighting_data->distance_fog_color);
            load_vec_from_config(
                config, section_idx, "distance_fog_range", 2,
                lighting_data->distance_fog_range);
        }
        section_idx += 1;
    }

    // Load world objects.
    for (; section_idx < section_count;) {
        const char* id = config_section_get_name(config, section_idx);
        const te_type_info* type_info = type_database_get_type_info(id);

        void* obj = type_info->create();
        type_info_load_from_config(type_info, config, section_idx, obj);

        const bool has_child_model = config_section_get_bool(
            config, section_idx, CONFIG_VAR_NAME_HAS_CHILD_MODEL, false);
        const bool has_attached_camera = config_section_get_bool(
            config, section_idx, CONFIG_VAR_NAME_HAS_ATTACHED_CAMERA, false);
        const unsigned int child_widget_count = config_section_get_uint(
            config, section_idx, CONFIG_VAR_NAME_CHILD_WIDGET_COUNT, 0);
        section_idx += 1;

        // Apply offset (only apply to root objects, child/attached objects will be affected).
        if (type_info->get_game_object_info != NULL) {
            te_game_object_info* game_obj_info = type_info->get_game_object_info(obj);
            switch (game_obj_info->type) {
                case (TE_GOT_MODEL): {
                    te_model* model = obj;

                    vec3 pos;
                    model_get_position(model, pos);
                    glm_vec3_add(pos, location_offset, pos);
                    model_set_position(model, pos);
                    break;
                }
                case (TE_GOT_CAMERA): {
                    te_camera* camera = obj;

                    vec3 pos;
                    camera_get_position(camera, pos);
                    glm_vec3_add(pos, location_offset, pos);
                    camera_set_position(camera, pos);
                    break;
                }
            }
        }

        if (type_info->get_game_object_info != NULL && type_info->get_game_object_info(obj)->type == TE_GOT_MODEL) {
            te_model* model = obj;

            if (has_child_model) {
                if (section_idx >= section_count) {
                    log_error_fmt(
                        "unexpected end of file \"%s\", have %u sections while expected to "
                        "have more",
                        relative_path, section_count);
                    abort();
                }
                te_model* child_model = model_create();
                type_info_load_from_config(model_type_info, config, section_idx, child_model);
                model_set_parent(child_model, model);
                section_idx += 1;
            }
            if (has_attached_camera) {
                if (section_idx >= section_count) {
                    log_error_fmt(
                        "unexpected end of file \"%s\", have %u sections while expected to "
                        "have more",
                        relative_path, section_count);
                    abort();
                }
                te_camera* camera = camera_create();
                type_info_load_from_config(camera_type_info, config, section_idx, camera);
                model_attach_camera(model, camera);
                section_idx += 1;
            }
        } else if (child_widget_count > 0) {
            if (type_info->get_widget == NULL) {
                log_error("found widget section that specified child count but the type does "
                          "not have widget conversion function set");
                abort();
            }
            te_widget* widget = type_info->get_widget(obj);
            prv_load_child_widgets_recursive(
                relative_path, config, section_count, widget, child_widget_count,
                &section_idx);
        }

        type_info->spawn(world, obj);
    }

    config_destroy(config);
}

te_camera*
world_get_active_camera(te_world* world) {
    return world->active_camera;
}

te_game_object_info**
world_get_root_game_objects(te_world* world, unsigned int* count) {
    (*count) = world->spawned_root_game_object_count;
    te_game_object_info** out = malloc(sizeof(te_game_object_info*) * (*count));
    memcpy(out, world->spawned_root_game_objects, sizeof(te_game_object_info*) * (*count));
    return out;
}

te_widget**
world_get_widgets(te_world* world, unsigned int* count) {
    (*count) = world->spawned_widget_count;
    te_widget** out = malloc(sizeof(te_widget*) * (*count));
    memcpy(out, world->spawned_widgets, sizeof(te_widget*) * (*count));
    return out;
}

te_model_renderer*
world_get_opaque_model_renderer(te_world* world) {
    return world->opaque_model_renderer;
}

struct te_model_renderer*
world_get_transparent_model_renderer(te_world* world) {
    return world->transparent_model_renderer;
}

te_widget_renderer*
world_get_widget_renderer(te_world* world) {
    return world->widget_renderer;
}

struct te_game_manager*
world_get_game_manager(te_world* world) {
    return world->game_manager;
}

void
world_spawn_game_object(te_world* world, te_game_object_info* info) {
    if (world->is_being_destroyed) {
        return;
    }

    te_world* old_obj_world = info->get_world(info->game_object);
    if (old_obj_world != NULL) {
        if (old_obj_world == world) {
            log_error("the game object is already spawned in this world");
            abort();
        } else {
            log_error("the specified game object cannot be spawned in this world because the game object "
                      "must be first despawned from the world it currently resides in");
            abort();
        }
    }

#if defined(DEBUG)
    prv_world_add_root_game_object_no_notify(world, info, true);
#else
    prv_world_add_root_game_object_no_notify(world, info, false);
#endif

    info->on_spawned(info->game_object, world);
}

void
world_despawn_game_object(te_world* world, te_game_object_info* info) {
    te_world* obj_world = info->get_world(info->game_object);
    if (obj_world != world) {
        log_error("the specified model cannot be despawned from this world as it's not "
                  "spawned in this world");
        abort();
    }

    prv_world_remove_root_game_object_no_notify(world, info, true);

    info->on_despawned(info->game_object);
}

void
world_spawn_widget(te_world* world, struct te_widget* widget) {
    if (world->is_being_destroyed) {
        return;
    }

    te_world* old_widget_world = widget_get_world(widget);
    if (old_widget_world != NULL) {
        if (old_widget_world == world) {
            log_error("the widget is already spawned in this world");
            abort();
        } else {
            log_error(
                "the specified widget cannot be spawned in this world because the widget "
                "must be first despawned from the world it currently resides in");
            abort();
        }
    }

#if defined(DEBUG)
    prv_world_add_root_widget_no_notify(world, widget, true);
#else
    prv_world_add_root_widget_no_notify(world, widget, false);
#endif
    prv_widget_on_spawned(widget, world);
}

void
world_despawn_widget(te_world* world, te_widget* widget) {
    if (widget_get_world(widget) != world) {
        log_error("the specified widget cannot be despawned from this world as it's not "
                  "spawned in this world");
        abort();
    }

    prv_world_remove_root_widget_no_notify(world, widget, true);
    prv_widget_on_despawned(widget);
}

void
prv_world_add_interactable_widget(te_world* world, te_widget* widget) {
    te_widget** new_widgets =
        malloc(sizeof(te_widget*) * (world->interactable_widget_count + 1));
    memcpy(
        new_widgets, world->interactable_widgets,
        sizeof(te_widget*) * world->interactable_widget_count);

    free(world->interactable_widgets);
    world->interactable_widgets = new_widgets;

    world->interactable_widgets[world->interactable_widget_count] = widget;
    world->interactable_widget_count += 1;
}

void
prv_world_remove_interactable_widget(te_world* world, te_widget* widget) {
    if (widget == world->hovered_interactable_widget) {
        world->hovered_interactable_widget = NULL;
    }

    if (world->interactable_widget_count == 1) {
        free(world->interactable_widgets);
        world->interactable_widgets = NULL;
        world->interactable_widget_count = 0;
    } else {
        unsigned int i = 0;
        bool found = false;
        for (; i < world->interactable_widget_count; i++) {
            if (world->interactable_widgets[i] != widget) {
                continue;
            }

            found = true;
            break;
        }
        if (!found) {
            log_error("unable to find the specified widget");
            abort();
        }

        te_widget** new_widgets =
            malloc(sizeof(te_widget*) * (world->interactable_widget_count - 1));
        memcpy(new_widgets, world->interactable_widgets, sizeof(te_widget*) * i);
        memcpy(
            new_widgets + i, world->interactable_widgets + (i + 1),
            sizeof(te_widget*) * (world->interactable_widget_count - i - 1));

        free(world->interactable_widgets);
        world->interactable_widgets = new_widgets;
        world->interactable_widget_count -= 1;
    }
}

void
prv_world_interactable_widget_pos_size_changed(te_world* world) {
    te_window* window = game_manager_get_window(world->game_manager);

    vec2 cursor_pos;
    window_get_cursor_position(window, &cursor_pos[0], &cursor_pos[1]);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);

    glm_vec2_div(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

    prv_world_on_mouse_moved(world, cursor_pos);
}

void
prv_world_on_mouse_cursor_captured(te_world* world, bool captured, float cursor_pos[2]) {
    if (captured) {
        if (world->hovered_interactable_widget != NULL) {
            prv_widget_on_cursor_left(world->hovered_interactable_widget, cursor_pos);
            world->hovered_interactable_widget = NULL;
        }
    } else {
        prv_world_on_mouse_moved(world, cursor_pos);
    }
}

void
prv_world_on_mouse_moved(te_world* world, float cursor_pos[2]) {
    // Notify widgets.
    te_window* window = game_manager_get_window(world->game_manager);
    if (window_is_mouse_captured(window)) {
        return;
    }

    vec2 pos;
    vec2 size;
    for (unsigned int i = 0; i < world->interactable_widget_count; i++) {
        widget_get_screen_position(world->interactable_widgets[i], pos);
        if (pos[0] > cursor_pos[0] || pos[1] > cursor_pos[1]) {
            continue;
        }

        widget_get_screen_size(world->interactable_widgets[i], size);
        if (cursor_pos[0] > pos[0] + size[0] || cursor_pos[1] > pos[1] + size[1]) {
            continue;
        }

        if (world->hovered_interactable_widget == NULL) {
            world->hovered_interactable_widget = world->interactable_widgets[i];
            prv_widget_on_cursor_entered(world->hovered_interactable_widget, cursor_pos);
        } else {
            if (world->hovered_interactable_widget != world->interactable_widgets[i]) {
                prv_widget_on_cursor_left(world->hovered_interactable_widget, cursor_pos);

                world->hovered_interactable_widget = world->interactable_widgets[i];
                prv_widget_on_cursor_entered(world->hovered_interactable_widget, cursor_pos);
            } else {
                prv_widget_on_hovered_cursor_moved(
                    world->hovered_interactable_widget, cursor_pos);
            }
        }

        return;
    }

    if (world->hovered_interactable_widget != NULL) {
        prv_widget_on_cursor_left(world->hovered_interactable_widget, cursor_pos);
        world->hovered_interactable_widget = NULL;
    }
}

bool
prv_world_on_mouse_button_pressed(
    te_world* world, enum te_mouse_button button, float cursor_pos[2]) {
    // Notify widgets.
    te_window* window = game_manager_get_window(world->game_manager);
    if (window_is_mouse_captured(window)) {
        return false;
    }

    vec2 pos;
    vec2 size;
    for (unsigned int i = 0; i < world->interactable_widget_count; i++) {
        widget_get_screen_position(world->interactable_widgets[i], pos);
        if (pos[0] > cursor_pos[0] || pos[1] > cursor_pos[1]) {
            continue;
        }

        widget_get_screen_size(world->interactable_widgets[i], size);
        if (cursor_pos[0] > pos[0] + size[0] || cursor_pos[1] > pos[1] + size[1]) {
            continue;
        }

        prv_widget_on_mouse_button_pressed(world->interactable_widgets[i], button, cursor_pos);
        return true;
    }

    return false;
}

bool
prv_world_on_mouse_button_released(
    te_world* world, enum te_mouse_button button, float cursor_pos[2]) {
    // Notify widgets.
    te_window* window = game_manager_get_window(world->game_manager);
    if (window_is_mouse_captured(window)) {
        return false;
    }

    vec2 pos;
    vec2 size;
    for (unsigned int i = 0; i < world->interactable_widget_count; i++) {
        widget_get_screen_position(world->interactable_widgets[i], pos);
        if (pos[0] > cursor_pos[0] || pos[1] > cursor_pos[1]) {
            continue;
        }

        widget_get_screen_size(world->interactable_widgets[i], size);
        if (cursor_pos[0] > pos[0] + size[0] || cursor_pos[1] > pos[1] + size[1]) {
            continue;
        }

        prv_widget_on_mouse_button_released(
            world->interactable_widgets[i], button, cursor_pos);
        return true;
    }

    return false;
}

void
prv_world_on_keyboard_input_text(te_world* world, const char* text) {
    if (world->hovered_interactable_widget == NULL) {
        return;
    }

    prv_widget_on_keyboard_input_text(world->hovered_interactable_widget, text);
}

void
prv_world_on_keyboard_input(te_world* world, enum te_keyboard_button button, bool is_repeat) {
    // Don't care if repeat or not for UI.
    (void)is_repeat;

    if (world->hovered_interactable_widget == NULL) {
        return;
    }

    prv_widget_on_keyboard_input(world->hovered_interactable_widget, button);
}

void
prv_world_on_input_source_changed(te_world* world) {
    if (world->hovered_interactable_widget != NULL) {
        te_window* window = game_manager_get_window(world->game_manager);

        vec2 cursor_pos;
        window_get_cursor_position(window, &cursor_pos[0], &cursor_pos[1]);

        unsigned int window_width;
        unsigned int window_height;
        window_get_size(window, &window_width, &window_height);

        glm_vec2_div(
            cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

        prv_widget_on_cursor_left(world->hovered_interactable_widget, cursor_pos);
        world->hovered_interactable_widget = NULL;
    }
}

#if defined(ENGINE_DEBUG_TOOLS)
unsigned int
prv_world_get_gl_query_draw_models(te_world* world) {
    return world->gl_query_draw_models;
}

unsigned int
prv_world_get_gl_query_draw_widgets(te_world* world) {
    return world->gl_query_draw_widgets;
}
#endif
