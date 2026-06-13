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

/* VAOs — glGenVertexArrays/glBindVertexArray crash on Switch firmware 19.0.1
 * with mesa 20.1.0 because mesa's dispatch table setup calls eglGetProcAddress
 * internally which triggers a fatal crash in _mesa_CompressedTextureSubImage3D.
 *
 * --wrap linker flags only intercept some calls; mesa's internal dispatch
 * routing bypasses them. The only reliable fix is to define these as
 * MACROS so the preprocessor replaces every call site before the compiler
 * or linker can route them to mesa.
 *
 * Safety: both shaders use the same vertex_t layout (pos@0 uv@12 color@20
 * stride=24). Attribute state from shader init persists as global GL state.
 * The post shader ignores the color attribute the game shader enables. */
#define glGenVertexArrays(n, arrays) \
    do { GLsizei _i; if (arrays) for (_i = 0; _i < (n); _i++) (arrays)[_i] = 1; } while(0)
#define glBindVertexArray(id)              ((void)(id))
#define glDeleteVertexArrays(n, arrays)    ((void)(n))

/* Apple VAO variants — not present on Switch but referenced in render_gl.c */
#define glGenVertexArraysAPPLE(n, arrays)    ((void)(n))
#define glBindVertexArrayAPPLE(id)           ((void)(id))
#define glDeleteVertexArraysAPPLE(n, arrays) ((void)(n))

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

/* Anisotropic filtering extension constants. */
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
 * GL_RGB is not color-renderable in GLES2, so FBO completeness fails
 * and mesa crashes if GL_RGB is used as internalformat for a render target. */
#ifdef GL_RGB
#undef GL_RGB
#define GL_RGB GL_RGBA
#endif

/* glGenerateMipmap on a render-target texture triggers the nv50 blitter
 * in mesa-switch, which crashes on firmware 19.0.1 with mesa 20.1.0. */
#define glGenerateMipmap(target) ((void)(target))

/* Anisotropic filtering extension — not in core GLES2.
 * glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT) with an undefined enum
 * generates GL_INVALID_VALUE and leaves anisotropy=0. Then
 * glTexParameterf(..., GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.0) is invalid
 * (minimum is 1.0) and corrupts the atlas texture sampler state,
 * causing all texture lookups to return black.
 * No-op the anisotropy parameter on Switch by wrapping glTexParameterf.
 *
 * IMPORTANT: define the wrapper BEFORE defining the macro so that the
 * wrapper body calls the real libGLESv2 glTexParameterf symbol, not itself. */
static inline void _switch_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT) return;
    glTexParameterf(target, pname, param);  /* real symbol — macro not yet defined */
}
#define glTexParameterf _switch_glTexParameterf

/* glGetTexImage is desktop GL only */
static inline void glGetTexImage(unsigned int target, int level,
    unsigned int format, unsigned int type, void *pixels) {
    (void)target; (void)level; (void)format; (void)type; (void)pixels;
}


/* -------------------------------------------------------------------------
 * GL function calls — direct static link via libGLESv2
 *
 * ppsspp confirms: on Switch, link libGLESv2 statically and call GL functions
 * directly. eglGetProcAddress returns dispatch table stubs for core GLES2
 * functions that crash on first call because the mesa nouveau driver initializes
 * its shader compiler lazily and the stub path bypasses that init.
 *
 * render_init_gl_funcs() is kept as a no-op for compatibility with the call
 * site in platform_switch.c.
 * ---------------------------------------------------------------------- */

#ifdef RENDER_GL_FUNC_IMPL
static inline int render_init_gl_funcs(void) {
    return 0; /* GL functions resolved via static libGLESv2 link */
}
#endif /* RENDER_GL_FUNC_IMPL */

#endif /* PLATFORM_SWITCH */
