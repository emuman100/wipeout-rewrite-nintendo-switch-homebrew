/*
 * render_gles2_compat.h — Switch GLES2 compatibility header
 *
 * Force-included into every translation unit via -include flag in CMakeLists.
 * This means it runs before render_gl.c's own #include <GL/glew.h>, so our
 * GLES2 headers are already included and the glew stub's #pragma once prevents
 * the real glew from being included.
 */

#pragma once

#ifdef PLATFORM_SWITCH

/* Pull in Switch mesa GLES2 and EGL headers */
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stddef.h>  /* NULL */

/* Stub out glew — render_gl.c calls glewInit() and sets glewExperimental
 * on the non-Apple unix path. We no-op them here so the code compiles.
 * __attribute__((unused)) suppresses the "defined but not used" warning
 * in every translation unit that doesn't reference glewExperimental. */
static unsigned int glewExperimental __attribute__((unused));
static inline unsigned int glewInit(void) { return 0; }

/* VAOs — render_gl.c calls glGenVertexArrays/glBindVertexArray unconditionally.
 * GLES2 core doesn't have them. On switch-mesa they are available via the
 * GL_OES_vertex_array_object extension but NOT as directly linked symbols.
 * We load them at runtime via eglGetProcAddress and provide thin wrappers. */

typedef void (*PFNGLGENVERTEXARRAYSOESPROC)(GLsizei n, GLuint *arrays);
typedef void (*PFNGLBINDVERTEXARRAYOESPROC)(GLuint array);
typedef void (*PFNGLDELETEVERTEXARRAYSOESPROC)(GLsizei n, const GLuint *arrays);

static PFNGLGENVERTEXARRAYSOESPROC    _glGenVertexArrays_fn    = NULL;
static PFNGLBINDVERTEXARRAYOESPROC    _glBindVertexArray_fn    = NULL;
static PFNGLDELETEVERTEXARRAYSOESPROC _glDeleteVertexArrays_fn = NULL;

static inline void _switch_vao_init(void) {
    if (!_glGenVertexArrays_fn) {
        _glGenVertexArrays_fn    = (PFNGLGENVERTEXARRAYSOESPROC)
            eglGetProcAddress("glGenVertexArraysOES");
        _glBindVertexArray_fn    = (PFNGLBINDVERTEXARRAYOESPROC)
            eglGetProcAddress("glBindVertexArrayOES");
        _glDeleteVertexArrays_fn = (PFNGLDELETEVERTEXARRAYSOESPROC)
            eglGetProcAddress("glDeleteVertexArraysOES");
    }
}

static inline void glGenVertexArrays(GLsizei n, GLuint *arrays) {
    _switch_vao_init();
    if (_glGenVertexArrays_fn) _glGenVertexArrays_fn(n, arrays);
    else if (arrays) *arrays = 1;  /* no-op fallback */
}
static inline void glBindVertexArray(GLuint array) {
    _switch_vao_init();
    if (_glBindVertexArray_fn) _glBindVertexArray_fn(array);
}
static inline void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    _switch_vao_init();
    if (_glDeleteVertexArrays_fn) _glDeleteVertexArrays_fn(n, arrays);
}

/* GLvoid is used in render_gl.c macros but not defined by GLES2 */
#ifndef GLvoid
#  define GLvoid void
#endif

/* GL_TRUE/GL_FALSE may not be defined by all GLES2 headers */
#ifndef GL_TRUE
#  define GL_TRUE  1
#endif
#ifndef GL_FALSE
#  define GL_FALSE 0
#endif

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

/* VAOs — used in render_gl.c but not in GLES2 core.
 * switch-mesa provides them via OES extension but the GLES2 path
 * in render_gl.c guards the VAO calls with USE_GLES2, so these
 * are just safety stubs. */
#ifndef GL_VERTEX_ARRAY_BINDING
#  define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif

static inline void glPolygonMode(unsigned int face, unsigned int mode) {
    (void)face; (void)mode;
}

/* glGetTexImage is desktop GL only — not used in GLES2 path but
 * the function prototype exists in render_gl.c */
static inline void glGetTexImage(unsigned int target, int level,
    unsigned int format, unsigned int type, void *pixels) {
    (void)target; (void)level; (void)format; (void)type; (void)pixels;
}

#endif /* PLATFORM_SWITCH */
