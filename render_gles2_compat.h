/*
 * render_gles2_compat.h — Switch GLES2 compatibility header
 *
 * Force-included into every translation unit via -include flag in CMakeLists.
 * This means it runs before render_gl.c's own #include <GL/glew.h>, so our
 * GLES2 headers are already included and the glew stub's #pragma once prevents
 * the real glew from being included.
 *
 * GL function resolution:
 *   Both ppsspp (m4xw) and gzdoom (fgsfdsfgs) Switch ports avoid statically
 *   linking libGLESv2 for GL calls. Instead they resolve all GL function
 *   pointers dynamically after eglMakeCurrent via eglGetProcAddress / dlsym.
 *   Without this, mesa's dispatch table is uninitialised when GL calls hit it,
 *   causing crashes deep in mesa internals (nv50_blitter, FramebufferRenderbuffer
 *   etc). We follow the same pattern here.
 *
 *   render_init_gl_funcs() must be called once from platform_switch.c after
 *   eglMakeCurrent succeeds.
 */

#pragma once

#ifdef PLATFORM_SWITCH

/* Pull in Switch mesa GLES2 and EGL headers */
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stddef.h>  /* NULL */
#include <string.h>  /* memcpy */

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

/* glGetTexImage is desktop GL only */
static inline void glGetTexImage(unsigned int target, int level,
    unsigned int format, unsigned int type, void *pixels) {
    (void)target; (void)level; (void)format; (void)type; (void)pixels;
}


/* -------------------------------------------------------------------------
 * Dynamic GL function resolution
 *
 * Both ppsspp (m4xw) and gzdoom (fgsfdsfgs) Switch ports resolve all GL
 * function pointers dynamically via eglGetProcAddress after eglMakeCurrent.
 * Statically linked libGLESv2 symbols point into an uninitialised mesa
 * dispatch table and cause crashes deep in mesa internals.
 *
 * We declare function pointer variables for every GL function used by
 * render_gl.c, then redefine each function name as a macro pointing to
 * the pointer. render_init_gl_funcs() must be called once after
 * eglMakeCurrent in platform_switch.c.
 * ---------------------------------------------------------------------- */

/* Declare function pointer types for every GL call in render_gl.c.
 * Skip functions already handled as macros above (VAOs, glGenerateMipmap,
 * glGetTexImage, glPolygonMode, glDebugMessageCallback/Control). */

typedef void      (*_pfn_glAttachShader)(GLuint, GLuint);
typedef void      (*_pfn_glBindBuffer)(GLenum, GLuint);
typedef void      (*_pfn_glBindFramebuffer)(GLenum, GLuint);
typedef void      (*_pfn_glBindRenderbuffer)(GLenum, GLuint);
typedef void      (*_pfn_glBindTexture)(GLenum, GLuint);
typedef void      (*_pfn_glBlendFunc)(GLenum, GLenum);
typedef void      (*_pfn_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void      (*_pfn_glClear)(GLbitfield);
typedef void      (*_pfn_glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void      (*_pfn_glCompileShader)(GLuint);
typedef GLuint    (*_pfn_glCreateProgram)(void);
typedef GLuint    (*_pfn_glCreateShader)(GLenum);
typedef void      (*_pfn_glDepthMask)(GLboolean);
typedef void      (*_pfn_glDisable)(GLenum);
typedef void      (*_pfn_glDrawArrays)(GLenum, GLint, GLsizei);
typedef void      (*_pfn_glEnable)(GLenum);
typedef void      (*_pfn_glEnableVertexAttribArray)(GLuint);
typedef void      (*_pfn_glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
typedef void      (*_pfn_glGenBuffers)(GLsizei, GLuint*);
typedef void      (*_pfn_glGenFramebuffers)(GLsizei, GLuint*);
typedef void      (*_pfn_glGenRenderbuffers)(GLsizei, GLuint*);
typedef void      (*_pfn_glGenTextures)(GLsizei, GLuint*);
typedef GLint     (*_pfn_glGetAttribLocation)(GLuint, const GLchar*);
typedef void      (*_pfn_glGetFloatv)(GLenum, GLfloat*);
typedef void      (*_pfn_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void      (*_pfn_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef GLint     (*_pfn_glGetUniformLocation)(GLuint, const GLchar*);
typedef void      (*_pfn_glLinkProgram)(GLuint);
typedef void      (*_pfn_glPolygonOffset)(GLfloat, GLfloat);
typedef void      (*_pfn_glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
typedef void      (*_pfn_glShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
typedef void      (*_pfn_glTexParameterf)(GLenum, GLenum, GLfloat);
typedef void      (*_pfn_glTexParameteri)(GLenum, GLenum, GLint);
typedef void      (*_pfn_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void      (*_pfn_glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
typedef void      (*_pfn_glUniform1f)(GLint, GLfloat);
typedef void      (*_pfn_glUniform1i)(GLint, GLint);
typedef void      (*_pfn_glUniform2f)(GLint, GLfloat, GLfloat);
typedef void      (*_pfn_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void      (*_pfn_glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void      (*_pfn_glUseProgram)(GLuint);
typedef void      (*_pfn_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void      (*_pfn_glViewport)(GLint, GLint, GLsizei, GLsizei);
typedef void      (*_pfn_glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);

/* Function pointer variables — defined in one TU via render_init_gl_funcs(),
 * declared extern in all others. The #ifdef guard ensures the definition
 * only appears once (in platform_switch.c which includes this header first
 * in the force-include chain with RENDER_GL_FUNC_IMPL defined). All other
 * TUs get extern declarations. */
#ifdef RENDER_GL_FUNC_IMPL
#  define GL_FUNC_DECL(type, name) type _gl_##name = NULL;
#else
#  define GL_FUNC_DECL(type, name) extern type _gl_##name;
#endif

GL_FUNC_DECL(_pfn_glAttachShader,          glAttachShader)
GL_FUNC_DECL(_pfn_glBindBuffer,            glBindBuffer)
GL_FUNC_DECL(_pfn_glBindFramebuffer,       glBindFramebuffer)
GL_FUNC_DECL(_pfn_glBindRenderbuffer,      glBindRenderbuffer)
GL_FUNC_DECL(_pfn_glBindTexture,           glBindTexture)
GL_FUNC_DECL(_pfn_glBlendFunc,             glBlendFunc)
GL_FUNC_DECL(_pfn_glBufferData,            glBufferData)
GL_FUNC_DECL(_pfn_glClear,                 glClear)
GL_FUNC_DECL(_pfn_glClearColor,            glClearColor)
GL_FUNC_DECL(_pfn_glCompileShader,         glCompileShader)
GL_FUNC_DECL(_pfn_glCreateProgram,         glCreateProgram)
GL_FUNC_DECL(_pfn_glCreateShader,          glCreateShader)
GL_FUNC_DECL(_pfn_glDepthMask,             glDepthMask)
GL_FUNC_DECL(_pfn_glDisable,               glDisable)
GL_FUNC_DECL(_pfn_glDrawArrays,            glDrawArrays)
GL_FUNC_DECL(_pfn_glEnable,                glEnable)
GL_FUNC_DECL(_pfn_glEnableVertexAttribArray, glEnableVertexAttribArray)
GL_FUNC_DECL(_pfn_glFramebufferRenderbuffer, glFramebufferRenderbuffer)
GL_FUNC_DECL(_pfn_glFramebufferTexture2D,  glFramebufferTexture2D)
GL_FUNC_DECL(_pfn_glGenBuffers,            glGenBuffers)
GL_FUNC_DECL(_pfn_glGenFramebuffers,       glGenFramebuffers)
GL_FUNC_DECL(_pfn_glGenRenderbuffers,      glGenRenderbuffers)
GL_FUNC_DECL(_pfn_glGenTextures,           glGenTextures)
GL_FUNC_DECL(_pfn_glGetAttribLocation,     glGetAttribLocation)
GL_FUNC_DECL(_pfn_glGetFloatv,             glGetFloatv)
GL_FUNC_DECL(_pfn_glGetShaderInfoLog,      glGetShaderInfoLog)
GL_FUNC_DECL(_pfn_glGetShaderiv,           glGetShaderiv)
GL_FUNC_DECL(_pfn_glGetUniformLocation,    glGetUniformLocation)
GL_FUNC_DECL(_pfn_glLinkProgram,           glLinkProgram)
GL_FUNC_DECL(_pfn_glPolygonOffset,         glPolygonOffset)
GL_FUNC_DECL(_pfn_glRenderbufferStorage,   glRenderbufferStorage)
GL_FUNC_DECL(_pfn_glShaderSource,          glShaderSource)
GL_FUNC_DECL(_pfn_glTexParameterf,         glTexParameterf)
GL_FUNC_DECL(_pfn_glTexParameteri,         glTexParameteri)
GL_FUNC_DECL(_pfn_glTexImage2D,            glTexImage2D)
GL_FUNC_DECL(_pfn_glTexSubImage2D,         glTexSubImage2D)
GL_FUNC_DECL(_pfn_glUniform1f,             glUniform1f)
GL_FUNC_DECL(_pfn_glUniform1i,             glUniform1i)
GL_FUNC_DECL(_pfn_glUniform2f,             glUniform2f)
GL_FUNC_DECL(_pfn_glUniform3f,             glUniform3f)
GL_FUNC_DECL(_pfn_glUniformMatrix4fv,      glUniformMatrix4fv)
GL_FUNC_DECL(_pfn_glUseProgram,            glUseProgram)
GL_FUNC_DECL(_pfn_glVertexAttribPointer,   glVertexAttribPointer)
GL_FUNC_DECL(_pfn_glViewport,              glViewport)

#undef GL_FUNC_DECL

/* Redefine every GL function name as a macro pointing to the function pointer.
 * This intercepts all call sites in render_gl.c and routes them through the
 * dynamically resolved pointers instead of the static libGLESv2 symbols. */
#define glAttachShader           _gl_glAttachShader
#define glBindBuffer             _gl_glBindBuffer
#define glBindFramebuffer        _gl_glBindFramebuffer
#define glBindRenderbuffer       _gl_glBindRenderbuffer
#define glBindTexture            _gl_glBindTexture
#define glBlendFunc              _gl_glBlendFunc
#define glBufferData             _gl_glBufferData
#define glClear                  _gl_glClear
#define glClearColor             _gl_glClearColor
#define glCompileShader          _gl_glCompileShader
#define glCreateProgram          _gl_glCreateProgram
#define glCreateShader           _gl_glCreateShader
#define glDepthMask              _gl_glDepthMask
#define glDisable                _gl_glDisable
#define glDrawArrays             _gl_glDrawArrays
#define glEnable                 _gl_glEnable
#define glEnableVertexAttribArray _gl_glEnableVertexAttribArray
#define glFramebufferRenderbuffer _gl_glFramebufferRenderbuffer
#define glFramebufferTexture2D   _gl_glFramebufferTexture2D
#define glGenBuffers             _gl_glGenBuffers
#define glGenFramebuffers        _gl_glGenFramebuffers
#define glGenRenderbuffers       _gl_glGenRenderbuffers
#define glGenTextures            _gl_glGenTextures
#define glGetAttribLocation      _gl_glGetAttribLocation
#define glGetFloatv              _gl_glGetFloatv
#define glGetShaderInfoLog       _gl_glGetShaderInfoLog
#define glGetShaderiv            _gl_glGetShaderiv
#define glGetUniformLocation     _gl_glGetUniformLocation
#define glLinkProgram            _gl_glLinkProgram
#define glPolygonOffset          _gl_glPolygonOffset
#define glRenderbufferStorage    _gl_glRenderbufferStorage
#define glShaderSource           _gl_glShaderSource
#define glTexParameterf          _gl_glTexParameterf
#define glTexParameteri          _gl_glTexParameteri
#define glTexImage2D             _gl_glTexImage2D
#define glTexSubImage2D          _gl_glTexSubImage2D
#define glUniform1f              _gl_glUniform1f
#define glUniform1i              _gl_glUniform1i
#define glUniform2f              _gl_glUniform2f
#define glUniform3f              _gl_glUniform3f
#define glUniformMatrix4fv       _gl_glUniformMatrix4fv
#define glUseProgram             _gl_glUseProgram
#define glVertexAttribPointer    _gl_glVertexAttribPointer
#define glViewport               _gl_glViewport

/* Resolver — call once from platform_switch.c after eglMakeCurrent.
 * Returns the number of functions that failed to resolve (0 = all good). */
#ifdef RENDER_GL_FUNC_IMPL
static inline int render_init_gl_funcs(void) {
    int failed = 0;
    /* Use memcpy to copy the raw pointer bits from eglGetProcAddress into the
     * function pointer variable. This matches how ppsspp/rglgen does it on
     * Switch — a direct cast can go through an extra indirection in mesa-switch's
     * dispatch table, producing a valid-looking but broken function pointer. */
    #define LOAD(name) do { \
        void *_p = (void*)eglGetProcAddress(#name); \
        if (_p) { memcpy(&_gl_##name, &_p, sizeof(_p)); } \
        else { failed++; } \
    } while(0);

    LOAD(glAttachShader)
    LOAD(glBindBuffer)
    LOAD(glBindFramebuffer)
    LOAD(glBindRenderbuffer)
    LOAD(glBindTexture)
    LOAD(glBlendFunc)
    LOAD(glBufferData)
    LOAD(glClear)
    LOAD(glClearColor)
    LOAD(glCompileShader)
    LOAD(glCreateProgram)
    LOAD(glCreateShader)
    LOAD(glDepthMask)
    LOAD(glDisable)
    LOAD(glDrawArrays)
    LOAD(glEnable)
    LOAD(glEnableVertexAttribArray)
    LOAD(glFramebufferRenderbuffer)
    LOAD(glFramebufferTexture2D)
    LOAD(glGenBuffers)
    LOAD(glGenFramebuffers)
    LOAD(glGenRenderbuffers)
    LOAD(glGenTextures)
    LOAD(glGetAttribLocation)
    LOAD(glGetFloatv)
    LOAD(glGetShaderInfoLog)
    LOAD(glGetShaderiv)
    LOAD(glGetUniformLocation)
    LOAD(glLinkProgram)
    LOAD(glPolygonOffset)
    LOAD(glRenderbufferStorage)
    LOAD(glShaderSource)
    LOAD(glTexParameterf)
    LOAD(glTexParameteri)
    LOAD(glTexImage2D)
    LOAD(glTexSubImage2D)
    LOAD(glUniform1f)
    LOAD(glUniform1i)
    LOAD(glUniform2f)
    LOAD(glUniform3f)
    LOAD(glUniformMatrix4fv)
    LOAD(glUseProgram)
    LOAD(glVertexAttribPointer)
    LOAD(glViewport)

    #undef LOAD
    return failed;
}
#endif /* RENDER_GL_FUNC_IMPL */

#endif /* PLATFORM_SWITCH */
