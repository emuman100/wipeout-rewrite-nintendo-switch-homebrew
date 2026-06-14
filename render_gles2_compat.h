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

/* Stub out glew */
static unsigned int glewExperimental __attribute__((unused));
static inline unsigned int glewInit(void) { return 0; }

/* -------------------------------------------------------------------------
 * VAO emulation — Option 3
 *
 * VAO core names (glGenVertexArrays/glBindVertexArray) are not declared in
 * the Switch SDK GLES2 headers, and the OES extension names are also absent.
 * Real VAO hardware support cannot be used via the headers.
 *
 * render_gl.c uses VAOs solely to save and restore vertex attribute layout
 * when switching between the game shader and the post shader via use_program().
 * Without VAO restore, the post shader inherits the game shader's attribute
 * layout — wrong indices/offsets — and the fullscreen blit produces a black
 * screen.
 *
 * Solution: fake VAO IDs + replay the known vertex_t layout on bind.
 *
 * vertex_t layout (from types.h, confirmed by ELF analysis, never changes):
 *   pos:   vec3_t  @ offset  0, stride 24, 3 floats
 *   uv:    vec2_t  @ offset 12, stride 24, 2 floats
 *   color: rgba_t  @ offset 20, stride 24, 4 unsigned bytes normalized
 *
 * Per-VAO state stored: the attribute indices assigned by glGetAttribLocation
 * at shader init time, recorded via our glEnableVertexAttribArray intercept.
 * On glBindVertexArray, we re-call glEnableVertexAttribArray +
 * glVertexAttribPointer with those stored indices and the known vertex_t layout.
 *
 * Max 4 VAOs (game + post_default + post_crt + 1 spare).
 * Max 4 attrib slots per VAO (game uses 3, post uses 2).
 *
 * Storage defined once in platform_switch.c (RENDER_GL_FUNC_IMPL TU).
 * ---------------------------------------------------------------------- */

#define _SW_VAO_MAX    4
#define _SW_ATTR_MAX   4

typedef struct {
    GLuint indices[_SW_ATTR_MAX];  /* attribute indices from glGetAttribLocation */
    int    count;                  /* number of active attributes */
} _sw_vao_t;

#ifdef RENDER_GL_FUNC_IMPL
_sw_vao_t _sw_vaos[_SW_VAO_MAX + 1];  /* slot 0 unused */
GLuint    _sw_vao_next  = 1;
GLuint    _sw_vao_bound = 0;
#else
extern _sw_vao_t _sw_vaos[_SW_VAO_MAX + 1];
extern GLuint    _sw_vao_next;
extern GLuint    _sw_vao_bound;
#endif

/* vertex_t offsets — must match types.h exactly */
#define _SW_OFFSET_POS   0
#define _SW_OFFSET_UV    12
#define _SW_OFFSET_COLOR 20
#define _SW_STRIDE       24

/* Replay the vertex_t attribute layout for the given VAO.
 * Called by glBindVertexArray after setting _sw_vao_bound.
 * The VBO is already bound by render_flush before drawing. */
static inline void _sw_replay_attribs(GLuint id) {
    if (id == 0 || id > _SW_VAO_MAX) return;
    _sw_vao_t *v = &_sw_vaos[id];
    for (int i = 0; i < v->count; i++) {
        GLuint idx = v->indices[i];
        glEnableVertexAttribArray(idx);
        if (i == 0) {
            /* pos: 3 floats */
            glVertexAttribPointer(idx, 3, GL_FLOAT, GL_FALSE,
                                  _SW_STRIDE, (const void *)_SW_OFFSET_POS);
        } else if (i == 1) {
            /* uv: 2 floats */
            glVertexAttribPointer(idx, 2, GL_FLOAT, GL_FALSE,
                                  _SW_STRIDE, (const void *)_SW_OFFSET_UV);
        } else {
            /* color: 4 unsigned bytes, normalized */
            glVertexAttribPointer(idx, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                                  _SW_STRIDE, (const void *)_SW_OFFSET_COLOR);
        }
    }
}

static inline void glGenVertexArrays(GLsizei n, GLuint *arrays) {
    for (GLsizei i = 0; i < n; i++)
        arrays[i] = (_sw_vao_next <= _SW_VAO_MAX) ? _sw_vao_next++ : 0;
}

static inline void glBindVertexArray(GLuint id) {
    _sw_vao_bound = id;
    _sw_replay_attribs(id);
}

static inline void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    for (GLsizei i = 0; i < n; i++) {
        GLuint id = arrays[i];
        if (id >= 1 && id <= _SW_VAO_MAX)
            _sw_vaos[id] = (_sw_vao_t){0};
    }
}

/* Record attribute index into the currently bound VAO.
 * render_gl.c calls glEnableVertexAttribArray then glVertexAttribPointer
 * during shader init — we only need the index; layout is known statically. */
static inline void _sw_glEnableVertexAttribArray(GLuint index) {
    glEnableVertexAttribArray(index);  /* real call — macro not yet defined */
    if (_sw_vao_bound >= 1 && _sw_vao_bound <= _SW_VAO_MAX) {
        _sw_vao_t *v = &_sw_vaos[_sw_vao_bound];
        if (v->count < _SW_ATTR_MAX)
            v->indices[v->count++] = index;
    }
}

#define glEnableVertexAttribArray(index)  _sw_glEnableVertexAttribArray(index)


/* Debug callbacks — not available in GLES2 */
#define glDebugMessageCallback(cb, userp)              ((void)(cb))
#define glDebugMessageControl(src,type,sev,c,ids,ena)  ((void)(src))

/* GLvoid is used in render_gl.c macros but not defined by GLES2 */
#ifndef GLvoid
#  define GLvoid void
#endif

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

/* mesa-switch requires GL_RGBA for FBO color attachments */
#ifdef GL_RGB
#undef GL_RGB
#define GL_RGB GL_RGBA
#endif

/* glGenerateMipmap triggers the nv50 blitter which crashes on Switch */
#define glGenerateMipmap(target) ((void)(target))

/* Anisotropic filtering — not in core GLES2.
 * glGetFloatv wrapper suppresses the invalid enum query.
 * glTexParameterf is no-opped — only called once in codebase for anisotropy.
 * Wrapper defined BEFORE macro to avoid recursive expansion. */
static inline void _switch_glGetFloatv(GLenum pname, GLfloat *params) {
    if (pname == GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT) {
        if (params) *params = 1.0f;
        return;
    }
    glGetFloatv(pname, params);  /* real symbol — macro not yet defined */
}
#define glGetFloatv _switch_glGetFloatv
#define glTexParameterf(target, pname, param) ((void)(target))

/* glTexParameteri — intercept mipmap min filter modes.
 * RENDER_USE_MIPMAPS=1 causes render_gl.c to set GL_LINEAR_MIPMAP_LINEAR
 * as the atlas min filter, but glGenerateMipmap is a no-op on Switch.
 * A texture with a mipmap filter but no mipmap data is mipmap-incomplete
 * in GLES2 — it samples as black and may generate GL_INVALID_VALUE.
 * Replace any mipmap min filter with GL_LINEAR on Switch.
 * Wrapper defined BEFORE macro to avoid recursive expansion. */
static inline void _switch_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (pname == GL_TEXTURE_MIN_FILTER) {
        if (param == GL_LINEAR_MIPMAP_LINEAR ||
            param == GL_LINEAR_MIPMAP_NEAREST ||
            param == GL_NEAREST_MIPMAP_LINEAR ||
            param == GL_NEAREST_MIPMAP_NEAREST) {
            param = GL_LINEAR;
        }
    }
    glTexParameteri(target, pname, param);  /* real symbol — macro not yet defined */
}
#define glTexParameteri _switch_glTexParameteri

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
    return 0;
}
#endif

#endif /* PLATFORM_SWITCH */
