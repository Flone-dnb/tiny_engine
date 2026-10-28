#pragma once

/* macros for creating GPU debug markers (groups GPU commands in RenderDoc) */
#if defined(DEBUG)
#include <glad/gl.h>

#define GPU_SECTION_BEGIN(name)                                                               \
    if (GLAD_GL_KHR_debug == 1) {                                                             \
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);                           \
    }

#define GPU_SECTION_END                                                                       \
    if (GLAD_GL_KHR_debug == 1) {                                                             \
        glPopDebugGroup();                                                                    \
    }

#else
#define GPU_SECTION_BEGIN(name)
#define GPU_SECTION_END
#endif
