#pragma once

// Macros to measure GPU time.
#if defined(ENGINE_DEBUG_TOOLS)
#include <glad/glad.h>

#if defined(ENGINE_GLES)
#define GPU_TIME_SECTION_BEGIN(gl_query_id)                                                   \
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {                                              \
        glBeginQueryEXT(GL_TIME_ELAPSED_EXT, gl_query_id);                                    \
    }
#else
#define GPU_TIME_SECTION_BEGIN(gl_query_id) glBeginQuery(GL_TIME_ELAPSED, gl_query_id);
#endif

#if defined(ENGINE_GLES)
#define GPU_TIME_SECTION_END                                                                  \
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {                                              \
        glEndQueryEXT(GL_TIME_ELAPSED_EXT);                                                   \
    }
#else
#define GPU_TIME_SECTION_END glEndQuery(GL_TIME_ELAPSED);
#endif

#else

#define GPU_TIME_SECTION_BEGIN(gl_query_id)
#define GPU_TIME_SECTION_END

#endif
