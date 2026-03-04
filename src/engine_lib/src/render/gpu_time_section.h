#pragma once

// Macros to measure GPU time.
#if defined(ENGINE_DEBUG_TOOLS)
#include <glad/glad.h>
#define GPU_TIME_SECTION_BEGIN(gl_query_id)                                                   \
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {                                              \
        glBeginQueryEXT(GL_TIME_ELAPSED_EXT, gl_query_id);                                    \
    }
#define GPU_TIME_SECTION_END                                                                  \
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {                                              \
        glEndQueryEXT(GL_TIME_ELAPSED_EXT);                                                   \
    }
#else
#define GPU_TIME_SECTION_BEGIN(gl_query_id)
#define GPU_TIME_SECTION_END
#endif
