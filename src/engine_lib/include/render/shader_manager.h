#pragma once

typedef struct te_shader_manager te_shader_manager;

// Looks if the specified shader program is already used (loaded) and if so returns OpenGL ID
// of the shader program (otherwise compiles it).
//
// You must use @ref shader_manager_mark_unused_shader when you no longer need the shader program.
//
// Specify a non-NULL path (relative to the `res` directory) to the shader files.
unsigned int shader_manager_request_shader(te_shader_manager* manager, const char* vert_relative_path,
                                           const char* frag_relative_path);

// Decrements shader usage counter.
void shader_manager_mark_unused_shader(te_shader_manager* manager, unsigned int prog_id);

// Helper function that uses glGetUniformLocation and shows an error if the specified uniform is not found.
int get_uniform_location(unsigned int prog_id, const char* name);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Creates shader manager. Renderer is expected to create this manager.
te_shader_manager* prv_shader_manager_create();

// Destroys shader manager.
void prv_shader_manager_destroy(te_shader_manager* manager);
