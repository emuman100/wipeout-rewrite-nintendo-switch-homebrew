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
 * GLES2 core doesn't have them, and calling eglGetProcAddress to load the OES
 * extension versions crashes inside mesa's extension dispatch table setup on
 * Switch firmware 19.0.1 with switch-mesa 20.1.0.
 *
 * Safe alternative: use no-op stubs. Both shaders (game and post) use the
 * same VBO and vertex_t layout. The vertex attribute setup
 * (glEnableVertexAttribArray + glVertexAttribPointer) executes once at shader
 * init and sets global GL state. Since neither shader reads attributes the
 * other enables, having extra enabled attributes from the game shader active
 * during the post blit is harmless — the post shader simply ignores them.
 *
 * If switch_vao_load() is called it is now a no-op. */

static inline void switch_vao_load(void) { /* no-op — VAOs disabled */ }

static GLuint _vao_counter = 0;
static inline void glGenVertexArrays(GLsizei n, GLuint *arrays) {
    if (arrays) {
        for (GLsizei i = 0; i < n; i++)
            arrays[i] = ++_vao_counter;  /* assign unique fake IDs */
    }
}
static inline void glBindVertexArray(GLuint array) { (void)array; }
static inline void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    (void)n; (void)arrays;
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

/* Anisotropic filtering extension constants.
 * render_gl.c queries and sets these unconditionally. Without the defines
 * glGetFloatv/glTexParameterf would receive an unknown enum, generating
 * GL_INVALID_ENUM and leaving anisotropy uninitialised.
 * switch-mesa/Nouveau does support EXT_texture_filter_anisotropic,
 * so these will be honoured at runtime. */
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
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
