/*
 * render_gles2_compat.h — Switch GLES2 compatibility header
 *
 * Force-included into every translation unit via -include flag in CMakeLists.
 * This means it runs before render_gl.c's own #include <GL/glew.h>, so our
 * GLES2 headers are already included and the glew stub's #pragma once prevents
 * the real glew from being included.
 *
 * GL function resolution:
 *   ppsspp Switch port (m4xw) links libGLESv2 statically and calls GL
 *   functions directly — eglGetProcAddress returns dispatch table stubs for
 *   core GLES2 functions that crash because mesa's nouveau shader compiler
 *   initialises lazily and the stub path bypasses that init.
 *
 *   We link libGLESv2 statically (CMakeLists.txt) and call GL functions
 *   directly. render_init_gl_funcs() is a no-op kept for call-site compat.
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

/* VAOs — GL_OES_vertex_array_object is present in mesa-switch's static
 * libGLESv2 (confirmed by strings on the linked binary). The earlier VAO
 * crashes were caused by the eglGetProcAddress dispatch stub path, which
 * is now avoided by linking libGLESv2 statically. Real VAO functions work.
 *
 * The core names (glGenVertexArrays, glBindVertexArray) are not declared
 * in <GLES2/gl2.h> — they are OES extensions. Declare them here as inline
 * wrappers around the OES symbols which ARE present in static libGLESv2.
 *
 * render_gl.c also references glGenVertexArraysAPPLE/glBindVertexArrayAPPLE
 * on the Apple path — redirect those to the OES functions too. */
static inline void glGenVertexArrays(GLsizei n, GLuint *arrays) {
    glGenVertexArraysOES(n, arrays);
}
static inline void glBindVertexArray(GLuint array) {
    glBindVertexArrayOES(array);
}
static inline void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    glDeleteVertexArraysOES(n, arrays);
}

#define glGenVertexArraysAPPLE(n, arrays)    glGenVertexArraysOES(n, arrays)
#define glBindVertexArrayAPPLE(id)           glBindVertexArrayOES(id)
#define glDeleteVertexArraysAPPLE(n, arrays) glDeleteVertexArraysOES(n, arrays)

/* Debug callbacks — not available in GLES2 */
#define glDebugMessageCallback(cb, userp)              ((void)(cb))
#define glDebugMessageControl(src,type,sev,c,ids,ena)  ((void)(src))

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

/* Anisotropic filtering extension constants */
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

static inline void glPolygonMode(unsigned int face, unsigned int mode) {
    (void)face; (void)mode;
}

/* mesa-switch requires GL_RGBA for FBO color attachments.
 * GL_RGB is not color-renderable in GLES2. */
#ifdef GL_RGB
#undef GL_RGB
#define GL_RGB GL_RGBA
#endif

/* glGenerateMipmap triggers the nv50 blitter which crashes on Switch */
#define glGenerateMipmap(target) ((void)(target))

/* Anisotropic filtering — not in core GLES2.
 *
 * render_gl.c makes exactly two anisotropy calls (confirmed by source audit):
 *   glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy) → GL_INVALID_VALUE
 *   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy)
 *
 * glTexParameterf is the only glTexParameterf call in the codebase — no-op it.
 * glGetFloatv wrapper suppresses the invalid enum, passes everything else through.
 *
 * IMPORTANT: wrapper defined BEFORE the macro so the body sees the real symbol. */
static inline void _switch_glGetFloatv(GLenum pname, GLfloat *params) {
    if (pname == GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT) {
        if (params) *params = 1.0f;
        return;
    }
    glGetFloatv(pname, params);  /* real symbol — macro not yet defined */
}
#define glGetFloatv _switch_glGetFloatv
#define glTexParameterf(target, pname, param) ((void)(target))

/* glGetTexImage is desktop GL only */
static inline void glGetTexImage(unsigned int target, int level,
    unsigned int format, unsigned int type, void *pixels) {
    (void)target; (void)level; (void)format; (void)type; (void)pixels;
}

/* -------------------------------------------------------------------------
 * GL function calls — direct static link via libGLESv2
 * ---------------------------------------------------------------------- */

#ifdef RENDER_GL_FUNC_IMPL
static inline int render_init_gl_funcs(void) {
    return 0; /* GL functions resolved via static libGLESv2 link */
}
#endif /* RENDER_GL_FUNC_IMPL */

#endif /* PLATFORM_SWITCH */
