#pragma once

/* macros to measure GPU time */

#if defined(ENGINE_DEBUG_TOOLS)

#include <glad/gl.h>

#define GPU_TIME_SECTION_BEGIN(gl_query_id)                                                   \
    if (GLAD_GL_ARB_timer_query == 1 && GLAD_GL_ARB_occlusion_query == 1) {                   \
        glBeginQueryARB(GL_TIME_ELAPSED, gl_query_id);                                        \
    }

#define GPU_TIME_SECTION_END                                                                  \
    if (GLAD_GL_ARB_timer_query == 1 && GLAD_GL_ARB_occlusion_query == 1) {                   \
        glEndQueryARB(GL_TIME_ELAPSED);                                                       \
    }

#else

#define GPU_TIME_SECTION_BEGIN(gl_query_id)
#define GPU_TIME_SECTION_END

#endif
