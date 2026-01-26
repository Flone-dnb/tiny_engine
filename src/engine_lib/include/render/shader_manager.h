#pragma once

typedef struct te_shader_manager te_shader_manager;

/**
 * Looks if the specified shader program was previously compiled and if so returns OpenGL ID
 * of the shader program (otherwise compiles it).
 *
 * @remark You must use @ref shader_manager_mark_unused_shader when you no longer need the shader.
 *
 * @param manager            Shader manager.
 * @param vert_relative_path Non-NULL path (relative to the `res` directory) to the vertex shader.
 * @param frag_relative_path Non-NULL path (relative to the `res` directory) to the fragment shader.
 *
 * @return OpenGL ID of the shader program.
 */
unsigned int shader_manager_request_shader(te_shader_manager* manager, const char* vert_relative_path,
                                           const char* frag_relative_path);

/**
 * Decrements shader usage counter.
 *
 * @param manager Shader manager.
 * @param prog_id OpenGL ID of the shader program received in @ref shader_manager_request_shader.
 */
void shader_manager_mark_unused_shader(te_shader_manager* manager, unsigned int prog_id);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Creates shader manager. Renderer is expected to create this manager.
 *
 * @return Shader manager.
 */
te_shader_manager* prv_shader_manager_create();

/**
 * Destroys shader manager..
 *
 * @param manager Shader manager.
 */
void prv_shader_manager_destroy(te_shader_manager* manager);
