#pragma once

// Macros to measure GPU time.
#if defined(ENGINE_DEBUG_TOOLS)
#include <glad/glad.h>
#define GPU_TIME_SECTION_BEGIN(gl_query_id) glBeginQuery(GL_TIME_ELAPSED, gl_query_id);
#define GPU_TIME_SECTION_END glEndQuery(GL_TIME_ELAPSED);
#else
#define GPU_TIME_SECTION_BEGIN(gl_query_id)
#define GPU_TIME_SECTION_END
#endif
