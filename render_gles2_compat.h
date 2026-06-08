/*
 * render_gles2_compat.h
 * Compatibility shim for wipeout-rewrite's GL renderer when targeting GLES2.
 *
 * The upstream renderer (render_gl.c) was written for desktop OpenGL 2.x /
 * OpenGL ES 2 with the RENDERER_GLES2 flag.  This header is included when
 * building for the Switch to map any remaining desktop-only symbols to their
 * GLES2 equivalents or to stub them out.
 *
 * Include this file *before* any OpenGL header in platform_switch.c and in
 * render_gl.c (guarded by __SWITCH__ or PLATFORM_SWITCH).
 *
 * Most of the heavy lifting is already done by the upstream RENDERER_GLES2
 * path; this file just handles edge cases.
 */

#pragma once

#ifdef PLATFORM_SWITCH

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

/* GLES2 does not expose these desktop-GL types/macros */
#ifndef GL_RGBA8
#  define GL_RGBA8  GL_RGBA
#endif

#ifndef GL_RGB8
#  define GL_RGB8   GL_RGB
#endif

/*
 * Desktop GL has glGenVertexArrays / glBindVertexArray, GLES2 doesn't.
 * The upstream code guards these with RENDERER_GLES2, but just in case:
 */
#ifndef RENDERER_GLES2
#  error "RENDERER_GLES2 must be defined for the Switch build"
#endif

/*
 * GLES2 doesn't have GL_CLAMP_TO_BORDER.
 * Substitute GL_CLAMP_TO_EDGE (closest meaningful fallback).
 */
#ifndef GL_CLAMP_TO_BORDER
#  define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
#endif

/*
 * GLES2 doesn't expose glPolygonMode.
 * Wireframe mode isn't used in release builds; stub it to prevent link errors.
 */
#ifndef GL_FILL
#  define GL_FILL 0x1B02
#endif
#ifndef GL_LINE
#  define GL_LINE 0x1B01
#endif
static inline void glPolygonMode(GLenum face, GLenum mode) {
    (void)face; (void)mode;
    /* No-op on GLES2 */
}

/*
 * GLES2 uses highp/mediump precision qualifiers in shaders.
 * The upstream GLSL shaders include a "#ifdef GL_ES" guard that adds
 *   precision mediump float;
 * to the fragment shader preamble.  That guard is already in the source.
 * Nothing extra needed here, but the reminder is useful.
 */

#endif /* PLATFORM_SWITCH */
