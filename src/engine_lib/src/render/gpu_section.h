#pragma once

// Macros for creating GPU debug markers (groups GPU commands in RenderDoc).
#if defined(DEBUG)
#include "glad/glad.h"
#define GPU_SECTION_BEGIN(name)                                                                                        \
    if (GLAD_GL_KHR_debug == 1) {                                                                                      \
        glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);                                                 \
    }
#define GPU_SECTION_END                                                                                                \
    if (GLAD_GL_KHR_debug == 1) {                                                                                      \
        glPopDebugGroupKHR();                                                                                          \
    }
#else
#define GPU_SECTION_BEGIN(name)
#define GPU_SECTION_END
#endif
