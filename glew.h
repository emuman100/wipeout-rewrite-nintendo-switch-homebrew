/*
 * GL/glew.h — Switch build stub
 *
 * render_gl.c includes <GL/glew.h> on __unix__. On the Switch we redirect
 * to GLES2 headers and provide no-op stubs for glew-specific symbols.
 */
#pragma once

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

/* glewExperimental is assigned as an lvalue in render_gl.c.
 * Declare it as a static variable so the assignment compiles and is
 * silently discarded. */
static unsigned int glewExperimental;
static inline unsigned int glewInit(void) { return 0; }

/* GLES2 compat defines */
#ifndef GL_RGBA8
#  define GL_RGBA8 GL_RGBA
#endif
#ifndef GL_RGB8
#  define GL_RGB8  GL_RGB
#endif
#ifndef GL_CLAMP_TO_BORDER
#  define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
#endif
#ifndef GL_FILL
#  define GL_FILL 0x1B02
#endif
#ifndef GL_LINE
#  define GL_LINE 0x1B01
#endif
static inline void glPolygonMode(unsigned int face, unsigned int mode) {
    (void)face; (void)mode;
}
