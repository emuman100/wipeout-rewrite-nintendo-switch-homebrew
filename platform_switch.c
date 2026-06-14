/* GL function pointers defined in this TU via -DRENDER_GL_FUNC_IMPL (set per-file in CMakeLists.txt). */

/*
 * platform_switch.c
 * Nintendo Switch platform backend for wipeout-rewrite
 *
 * Implements the same platform API as platform_sdl.c / platform_sokol.c,
 * but targets the Nintendo Switch via devkitA64 + libnx + EGL/OpenGL ES 2.
 *
 * Build requirements:
 *   devkitPro devkitA64 r19+
 *   libnx 4.x
 *   switch-mesa / switch-libdrm (for EGL + OpenGL ES 2)
 *   switch-sdl2  (optional – see AUDIO section; audren path used by default)
 *   deko3d is NOT used; we rely on OpenGL ES 2 via EGL (mesa-switch)
 *
 * The wipeout-rewrite CMake flags to pair with this file:
 *   -DPLATFORM=SWITCH -DRENDERER=GLES2
 * CMakeLists.txt maps RENDERER=GLES2 internally to -DRENDERER_GL=1 -DUSE_GLES2=1.
 *
 * Controller mapping (Joy-Con / Pro Controller):
 *   Accelerate      → A  (HidNpadButton_A)
 *   Fire weapon     → X  (HidNpadButton_X)
 *   Change view     → Y  (HidNpadButton_Y)
 *   Left airbrake   → L  (HidNpadButton_L)
 *   Right airbrake  → R  (HidNpadButton_R)
 *   ZR / ZL         → unbound by default (remap in-game: Options → Controls)
 *   Navigate Up     → Up or Left-stick up
 *   Navigate Down   → Down or Left-stick down
 *   Navigate Left   → Left or Left-stick left
 *   Navigate Right  → Right or Left-stick right
 *   Confirm         → A
 *   Back            → B or X
 *   Pause           → + (Plus)
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>  /* memalign */

/* Debug logging — writes to sdmc:/switch/wipegame/debug.log */
static FILE *s_log = NULL;

static void log_open(void) {
    s_log = fopen("sdmc:/switch/wipegame/debug.log", "w");
}

static void log_close(void) {
    if (s_log) { fflush(s_log); fclose(s_log); s_log = NULL; }
}

#ifndef TRACE
#  define TRACE(fmt, ...) do { \
        if (s_log) { fprintf(s_log, fmt "\n", ##__VA_ARGS__); fflush(s_log); } \
    } while(0)
#endif

/* Redirect printf to the debug log so die() messages are captured.
 * die() in utils.h calls printf() then exit(1); without this the error
 * message is lost and we only see the crash, not the cause. */
int printf(const char *fmt, ...) {
    if (!s_log) return 0;
    va_list args;
    va_start(args, fmt);
    int r = vfprintf(s_log, fmt, args);
    va_end(args);
    fflush(s_log);
    return r;
}

/* devkitPro / libnx */
#include <switch.h>

/* EGL + GLES2 via mesa-switch */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

/* wipeout-rewrite internal headers (paths relative to project src/) */
#include "platform.h"
#include "input.h"
#include "system.h"
#include "mem.h"

/*
 * Override the libnx applet type.
 *
 * mesa-switch calls viCreateManagedLayer() during eglInitialize() which
 * requires SystemApplication-level display layer access rights.
 *
 * AppletType_SystemApplication is correct for both launch paths:
 *   - "hold R" title override (AppletType_Application context)
 *   - Normal hbmenu NRO launch
 *
 * AppletType_LibraryApplet was tried during development and caused
 * 2168-0002 from the VI service regardless of launch method.
 * AppletType_SystemApplication resolved this on hardware (session 6).
 *
 * Note: appletLockExit() is deliberately NOT called — it also caused
 * 2168-0002 in testing and is unnecessary for clean exit behaviour.
 */
u32 __nx_applet_type = AppletType_SystemApplication;

static EGLDisplay s_egl_display = EGL_NO_DISPLAY;
static EGLContext s_egl_context = EGL_NO_CONTEXT;
static EGLSurface s_egl_surface = EGL_NO_SURFACE;

static NWindow *s_nwindow = NULL;

static bool s_wants_exit = false;

/* Last known EGL surface dimensions — used to detect dock/undock transitions.
 * appletGetOperationMode() is unreliable for NROs launched from hbmenu
 * (returns Handheld regardless of dock state). eglQuerySurface reports the
 * actual framebuffer dimensions from the compositor, which is ground truth. */
static int s_surface_w = SWITCH_W_HANDHELD;
static int s_surface_h = SWITCH_H_HANDHELD;

/* Audio */
static AudioOutBuffer s_audio_buffers[2];
static u8 *s_audio_data[2];
static u32 s_audio_buffer_size = 0;
static u32 s_audio_sample_rate = 0;    /* cached at audio_init() */
static u32 s_audio_channel_count = 0;  /* cached at audio_init() */
static void (*s_audio_mix_cb)(float *buffer, uint32_t len) = NULL;
static bool s_audio_ok = false;   /* true only if audio_init() succeeded */
static float *s_float_buf = NULL; /* pre-allocated resample buffer (avoids per-frame malloc) */

/* Controller – initialised once in main(), updated each frame */
static PadState s_pad;

/* Screen dimensions — docked mode uses 1920×1080, handheld uses 1280×720.
 * The Switch hardware compositor handles the actual scaling to TV output;
 * we track the EGL surface dimensions and tell the game the logical render
 * size. eglQuerySurface gives us the ground truth regardless of applet type. */
#define SWITCH_W_HANDHELD 1280
#define SWITCH_H_HANDHELD  720
#define SWITCH_W_DOCKED   1920
#define SWITCH_H_DOCKED   1080

/* Audio buffer count — two DMA buffers alternated each frame */
#define AUDIO_BUFFER_FRAMES 2

/* -------------------------------------------------------------------------
 * EGL / OpenGL ES 2 initialisation
 * ---------------------------------------------------------------------- */

static bool egl_init(void) {
    /* Grab the default NWindow (framebuffer window) */
    s_nwindow = nwindowGetDefault();
    if (!s_nwindow) {
        TRACE("egl_init: nwindowGetDefault failed");
        return false;
    }
    TRACE("egl_init: nwindow OK");

    vec2i_t initial_size = { SWITCH_W_HANDHELD, SWITCH_H_HANDHELD };
    nwindowSetDimensions(s_nwindow, initial_size.x, initial_size.y);

    s_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_egl_display == EGL_NO_DISPLAY) {
        TRACE("egl_init: eglGetDisplay failed");
        return false;
    }
    TRACE("egl_init: eglGetDisplay OK");

    EGLint major, minor;
    if (!eglInitialize(s_egl_display, &major, &minor)) {
        TRACE("egl_init: eglInitialize failed");
        return false;
    }
    TRACE("egl_init: eglInitialize OK (%d.%d)", major, minor);

    /* Request OpenGL ES 2 */
    eglBindAPI(EGL_OPENGL_ES_API);

    static const EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      0,   /* ppsspp uses 0 alpha for GLES2 */
        EGL_DEPTH_SIZE,      16,  /* ppsspp uses 16, not 24 */
        EGL_STENCIL_SIZE,    8,   /* ppsspp requests stencil */
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(s_egl_display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
        TRACE("egl_init: eglChooseConfig failed");
        return false;
    }
    TRACE("egl_init: eglChooseConfig OK");

    s_egl_surface = eglCreateWindowSurface(s_egl_display, config,
                                           (EGLNativeWindowType)s_nwindow, NULL);
    if (s_egl_surface == EGL_NO_SURFACE) {
        TRACE("egl_init: eglCreateWindowSurface failed (0x%x)", eglGetError());
        return false;
    }
    TRACE("egl_init: eglCreateWindowSurface OK");

    static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    s_egl_context = eglCreateContext(s_egl_display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (s_egl_context == EGL_NO_CONTEXT) {
        TRACE("egl_init: eglCreateContext failed");
        return false;
    }
    TRACE("egl_init: eglCreateContext OK");

    if (!eglMakeCurrent(s_egl_display, s_egl_surface, s_egl_surface, s_egl_context)) {
        TRACE("egl_init: eglMakeCurrent failed");
        return false;
    }
    TRACE("egl_init: eglMakeCurrent OK");

    /* Query actual surface dimensions from the compositor — this is the
     * ground truth for initial resolution. appletGetOperationMode() is
     * unreliable for hbmenu-launched NROs; eglQuerySurface is not. */
    EGLint surf_w = SWITCH_W_HANDHELD, surf_h = SWITCH_H_HANDHELD;
    eglQuerySurface(s_egl_display, s_egl_surface, EGL_WIDTH,  &surf_w);
    eglQuerySurface(s_egl_display, s_egl_surface, EGL_HEIGHT, &surf_h);
    s_surface_w = (int)surf_w;
    s_surface_h = (int)surf_h;
    TRACE("egl_init: surface dimensions %dx%d (%s)",
          s_surface_w, s_surface_h,
          (s_surface_w == SWITCH_W_DOCKED) ? "docked 1080p" : "handheld 720p");

    /* GL functions are resolved via static libGLESv2 link (confirmed correct
     * approach by ppsspp Switch port). render_init_gl_funcs() is a no-op. */
    render_init_gl_funcs();
    TRACE("egl_init: GL via static libGLESv2 link OK");


    /* Disable vsync so the game can manage its own timing */
    eglSwapInterval(s_egl_display, 0);

    /* Drain any stale GL errors generated during mesa initialisation.
     * ppsspp explicitly does this after glewInit() with the comment
     * "glew will generate an invalid enum error, ignore."
     * Mesa's nouveau driver can leave errors in the queue during its own
     * init sequence; draining here prevents them from appearing as false
     * positives in our per-frame glGetError logging. */
    { GLenum _e; int _n = 0;
      while ((_e = glGetError()) != GL_NO_ERROR) {
          TRACE("egl_init: draining stale GL error 0x%x", _e);
          if (++_n > 16) break;  /* safety cap */
      }
      if (_n) TRACE("egl_init: drained %d stale error(s)", _n);
    }

    TRACE("egl_init: done");

    return true;
}

static void egl_cleanup(void) {
    if (s_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(s_egl_display, s_egl_context);
        if (s_egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(s_egl_display, s_egl_surface);
        eglTerminate(s_egl_display);
    }
    s_egl_display = EGL_NO_DISPLAY;
    s_egl_context = EGL_NO_CONTEXT;
    s_egl_surface = EGL_NO_SURFACE;
}

/* -------------------------------------------------------------------------
 * Audio (libnx AudioOut)
 *
 * The Switch AudioOut API is callback-less; we push audio each frame from
 * platform_end_frame().  We allocate two DMA-aligned buffers and alternate.
 * ---------------------------------------------------------------------- */

static bool audio_init(void) {
    Result rc = audoutInitialize();
    if (R_FAILED(rc)) {
        TRACE("platform_switch: audoutInitialize failed: 0x%x", rc);
        return false;
    }

    /* Cache these – they are constant for the lifetime of the app */
    s_audio_sample_rate   = audoutGetSampleRate();    /* 48000 Hz */
    TRACE("audio_init: sample_rate=%u channels=%u", s_audio_sample_rate, audoutGetChannelCount());
    s_audio_channel_count = audoutGetChannelCount();  /* 2 (stereo) */

    /*
     * Each buffer holds one frame worth of audio at the HW rate.
     * Size must be aligned to 0x1000 for DMA.
     */
    s_audio_buffer_size = (s_audio_sample_rate / 30) * s_audio_channel_count * sizeof(s16);
    s_audio_buffer_size = (s_audio_buffer_size + 0xFFF) & ~0xFFF;

    /* Pre-allocate the float conversion buffer once (avoids per-frame malloc).
     * Sized for total floats: max source frames * channels.
     * src_frames = ceil(hw_frames * 44100/48000) + 2; multiply by channels
     * for the total float count passed to the mix callback. */
    u32 max_hw_samples = s_audio_buffer_size / (s_audio_channel_count * sizeof(s16));
    u32 max_src_frames = (u32)((double)max_hw_samples * 44100.0 / s_audio_sample_rate) + 2;
    u32 max_src_floats = max_src_frames * s_audio_channel_count;
    s_float_buf = (float *)malloc(max_src_floats * sizeof(float));
    if (!s_float_buf) {
        TRACE("platform_switch: float_buf alloc failed");
        audoutExit();
        return false;
    }

    for (int i = 0; i < AUDIO_BUFFER_FRAMES; i++) {
        s_audio_data[i] = (u8 *)memalign(0x1000, s_audio_buffer_size);
        if (!s_audio_data[i]) {
            TRACE("platform_switch: audio buffer alloc failed at index %d", i);
            /* Free any DMA buffers already allocated and the float buffer */
            for (int j = 0; j < i; j++) {
                free(s_audio_data[j]);
                s_audio_data[j] = NULL;
            }
            free(s_float_buf);
            s_float_buf = NULL;
            audoutExit();
            return false;
        }
        memset(s_audio_data[i], 0, s_audio_buffer_size);

        s_audio_buffers[i].next        = NULL;
        s_audio_buffers[i].buffer      = s_audio_data[i];
        s_audio_buffers[i].buffer_size = s_audio_buffer_size;
        s_audio_buffers[i].data_size   = s_audio_buffer_size;
        s_audio_buffers[i].data_offset = 0;
    }

    TRACE("audio_init: buffer_size=%u hw_samples=%u src_frames=%u src_floats=%u", s_audio_buffer_size, max_hw_samples, max_src_frames, max_src_floats);
    rc = audoutStartAudioOut();
    if (R_FAILED(rc)) {
        TRACE("platform_switch: audoutStartAudioOut failed: 0x%x", rc);
        for (int i = 0; i < AUDIO_BUFFER_FRAMES; i++) {
            free(s_audio_data[i]);
            s_audio_data[i] = NULL;
        }
        free(s_float_buf);
        s_float_buf = NULL;
        audoutExit();
        return false;
    }

    /* Pre-queue BOTH buffers so the hardware always has one ready */
    audoutAppendAudioOutBuffer(&s_audio_buffers[0]);
    audoutAppendAudioOutBuffer(&s_audio_buffers[1]);

    s_audio_ok = true;
    return true;
}

static void audio_cleanup(void) {
    if (!s_audio_ok) return;
    audoutStopAudioOut();
    audoutExit();
    for (int i = 0; i < AUDIO_BUFFER_FRAMES; i++) {
        if (s_audio_data[i]) {
            free(s_audio_data[i]);
            s_audio_data[i] = NULL;
        }
    }
    if (s_float_buf) {
        free(s_float_buf);
        s_float_buf = NULL;
    }
    s_audio_ok = false;
}

/*
 * Called once per frame (from platform_end_frame).
 * Waits for a released buffer, fills it via the game's mix callback,
 * then re-queues it.
 */
static void audio_update(void) {
    if (!s_audio_ok || !s_audio_mix_cb) return;

    AudioOutBuffer *released = NULL;
    u32 released_count = 0;

    /* Non-blocking poll for a released buffer.
     * audoutWaitPlayFinish with UINT64_MAX blocks for the full buffer duration
     * (~42ms at 48000Hz/2048 samples), starving the game loop at 30fps.
     * Instead poll with zero timeout and skip if no buffer is ready — the
     * game loop timing is driven by vsync/eglSwapBuffers, not audio. */
    Result rc = audoutGetReleasedAudioOutBuffer(&released, &released_count);
    if (R_FAILED(rc) || released_count == 0) return;

    /* Use cached HW parameters (constant after audio_init) */
    u32 hw_num_samples = s_audio_buffer_size / (s_audio_channel_count * sizeof(s16));

    /* Ask the game mixer for 44100 Hz samples, then resample to 48000 Hz.
     * The sfx mixer assumes 44100 Hz output — requesting at the wrong rate
     * would shift pitch of all SFX and music.
     *
     * IMPORTANT: the mix callback's `len` parameter is total float count
     * (frames * channels), matching the upstream SDL platform which passes
     * `len_bytes / sizeof(float)`.  We must pass src_frames * channels,
     * NOT src_frames alone — passing only src_frames starves the mixer,
     * producing garbled/slow audio because it fills only the L channel. */
    u32 src_frames  = (u32)((double)hw_num_samples * 44100.0 / s_audio_sample_rate) + 2;
    u32 src_samples = src_frames * s_audio_channel_count;  /* total floats */

    memset(s_float_buf, 0, src_samples * sizeof(float));
    s_audio_mix_cb(s_float_buf, src_samples);

    /* Resample 44100 → 48000 with linear interpolation, then convert to s16le.
     * Position math is in frames; buffer indexing uses frames * channels. */
    s16 *out = (s16 *)released->buffer;
    for (u32 i = 0; i < hw_num_samples; i++) {
        double src_pos = (double)i * 44100.0 / s_audio_sample_rate;
        u32 src_i = (u32)src_pos;
        float frac = (float)(src_pos - src_i);
        if (src_i + 1 >= src_frames) src_i = src_frames - 2;

        for (u32 ch = 0; ch < s_audio_channel_count; ch++) {
            float s0 = s_float_buf[src_i * s_audio_channel_count + ch];
            float s1 = s_float_buf[(src_i + 1) * s_audio_channel_count + ch];
            float s  = s0 + (s1 - s0) * frac;
            if      (s >  1.0f) s =  1.0f;
            else if (s < -1.0f) s = -1.0f;
            out[i * s_audio_channel_count + ch] = (s16)(s * 32767.0f);
        }
    }

    released->data_size = hw_num_samples * s_audio_channel_count * sizeof(s16);
    audoutAppendAudioOutBuffer(released);
}

/* -------------------------------------------------------------------------
 * Input
 *
 * Maps libnx HidNpadButton values to wipeout-rewrite INPUT_* constants.
 * s_pad is initialised once in main() via padInitializeDefault().
 * input_clear() resets per-frame edge states (pressed/released) before
 * re-polling, matching the pattern used by platform_sdl.c.
 * ---------------------------------------------------------------------- */

static void input_update(void) {
    /*
     * Note: input_clear() is NOT called here. It is called by system_update()
     * after game_update(), so that pressed/released states are visible to game
     * logic for the full frame — matching the DC port's system_update() pattern.
     */
    padUpdate(&s_pad);

    u64 held    = padGetButtons(&s_pad);
    u64 pressed = padGetButtonsDown(&s_pad);
    u64 released_buttons = padGetButtonsUp(&s_pad);
    HidAnalogStickState ls = padGetStickPos(&s_pad, 0);
    HidAnalogStickState rs = padGetStickPos(&s_pad, 1);

    /* ---- Digital buttons — passed as float 0.0/1.0 per input_set_button_state contract ---- */

    /* Accelerate */
    input_set_button_state(INPUT_GAMEPAD_A,       (held & HidNpadButton_A)     ? 1.0f : 0.0f);
    /* Brake */
    input_set_button_state(INPUT_GAMEPAD_B,       (held & HidNpadButton_B)     ? 1.0f : 0.0f);
    /* Fire / Use weapon — ZR is right trigger */
    input_set_button_state(INPUT_GAMEPAD_R_TRIGGER,  (held & HidNpadButton_ZR)    ? 1.0f : 0.0f);
    /* Absorb — ZL is left trigger */
    input_set_button_state(INPUT_GAMEPAD_L_TRIGGER,  (held & HidNpadButton_ZL)    ? 1.0f : 0.0f);
    /* Left airbrake — L is left shoulder */
    input_set_button_state(INPUT_GAMEPAD_L_SHOULDER, (held & HidNpadButton_L)     ? 1.0f : 0.0f);
    /* Right airbrake — R is right shoulder */
    input_set_button_state(INPUT_GAMEPAD_R_SHOULDER, (held & HidNpadButton_R)     ? 1.0f : 0.0f);

    /* D-pad */
    input_set_button_state(INPUT_GAMEPAD_DPAD_UP,    (held & HidNpadButton_Up)    ? 1.0f : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_DPAD_DOWN,  (held & HidNpadButton_Down)  ? 1.0f : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT,  (held & HidNpadButton_Left)  ? 1.0f : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_DPAD_RIGHT, (held & HidNpadButton_Right) ? 1.0f : 0.0f);

    /* Menu navigation */
    input_set_button_state(INPUT_GAMEPAD_START,   (held & HidNpadButton_Plus)  ? 1.0f : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_SELECT,  (held & HidNpadButton_Minus) ? 1.0f : 0.0f);

    /* X / Y */
    input_set_button_state(INPUT_GAMEPAD_X,       (held & HidNpadButton_X)     ? 1.0f : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_Y,       (held & HidNpadButton_Y)     ? 1.0f : 0.0f);

    /* Stick click buttons */
    input_set_button_state(INPUT_GAMEPAD_L_STICK_PRESS, (held & HidNpadButton_StickL) ? 1.0f : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_R_STICK_PRESS, (held & HidNpadButton_StickR) ? 1.0f : 0.0f);

    /* INPUT_GAMEPAD_HOME (124) is intentionally not mapped.
     * The Switch Home button is OS-reserved and cannot be read by applications.
     * The game never binds INPUT_GAMEPAD_HOME to any action in game.c — it only
     * appears in the button name table (input.c) and the SDL platform's gamepad
     * map. Omitting it here has no effect on gameplay or the controls menu. */

    /* ---- Analog stick → axis values ---- */

    /* Left stick X → steering (range -1..1) */
    float axis_x = (float)ls.x / 32767.0f;
    float axis_y = (float)ls.y / 32767.0f;

    input_set_button_state(INPUT_GAMEPAD_L_STICK_LEFT,  axis_x < 0 ? -axis_x : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_L_STICK_RIGHT, axis_x > 0 ?  axis_x : 0.0f);
    /*
     * libnx Y axis: positive = stick physically UP.
     * SDL/game convention: positive = stick DOWN (screen-space Y).
     * Negate so L_STICK_UP fires when the stick is pushed up.
     */
    input_set_button_state(INPUT_GAMEPAD_L_STICK_UP,    axis_y > 0 ?  axis_y : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_L_STICK_DOWN,  axis_y < 0 ? -axis_y : 0.0f);

    /* Right stick → camera look (same Y-axis convention as left stick) */
    float raxis_x = (float)rs.x / 32767.0f;
    float raxis_y = (float)rs.y / 32767.0f;

    input_set_button_state(INPUT_GAMEPAD_R_STICK_LEFT,  raxis_x < 0 ? -raxis_x : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_R_STICK_RIGHT, raxis_x > 0 ?  raxis_x : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_R_STICK_UP,    raxis_y > 0 ?  raxis_y : 0.0f);
    input_set_button_state(INPUT_GAMEPAD_R_STICK_DOWN,  raxis_y < 0 ? -raxis_y : 0.0f);

    /* Exit when Home is held for 5 seconds – or just Plus in menus via the game */
    (void)pressed;
    (void)released_buttons;
}

/* -------------------------------------------------------------------------
 * Public platform API (matches platform.h contract)
 * ---------------------------------------------------------------------- */

void platform_exit(void) {
    s_wants_exit = true;
}

vec2i_t platform_screen_size(void) {
    return (vec2i_t){ s_surface_w, s_surface_h };
}

double platform_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

bool platform_get_fullscreen(void) {
    /* Switch is always full-screen; this option is a no-op */
    return true;
}

void platform_set_fullscreen(bool fullscreen) {
    (void)fullscreen;
    /* Nothing to do; the Switch has no windowed mode */
}

void platform_set_audio_mix_cb(void (*cb)(float *buffer, uint32_t len)) {
    s_audio_mix_cb = cb;
}

/* ---- Video ---- */

void platform_video_init(void) {
    /* EGL is already up by the time the game calls this */
}

void platform_video_cleanup(void) {
    egl_cleanup();
}

void platform_prepare_frame(void) {
    static int prep_count = 0;
    if (prep_count < 3) {
        /* Query whatever FBO is currently bound (set by render_frame_prepare) */
        GLint current_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        TRACE("prepare_frame %d: bound_fbo=%d fbo_status=0x%x (%s)", prep_count,
            current_fbo, status,
            status == 0x8CD5 ? "COMPLETE" :
            status == 0x8CD6 ? "INCOMPLETE_ATTACHMENT" :
            status == 0x8CD7 ? "INCOMPLETE_MISSING_ATTACHMENT" :
            status == 0x8CD9 ? "INCOMPLETE_DIMENSIONS" :
            status == 0x8CDD ? "UNSUPPORTED" : "UNKNOWN");
        prep_count++;
    }
}

void platform_end_frame(void) {
    /* Log GL errors and swap result for first few frames */
    static int frame_count = 0;
    if (frame_count < 5) {
        GLenum gl_err = glGetError();
        TRACE("frame %d: pre-swap glGetError=0x%x", frame_count, gl_err);
        EGLBoolean swap_ok = eglSwapBuffers(s_egl_display, s_egl_surface);
        GLenum gl_err2 = glGetError();
        EGLint egl_err = eglGetError();
        TRACE("frame %d: post-swap glGetError=0x%x eglSwapBuffers=%d eglGetError=0x%x",
            frame_count, gl_err2, (int)swap_ok, (int)egl_err);
        frame_count++;
    } else {
        eglSwapBuffers(s_egl_display, s_egl_surface);
    }

    /* Push audio for this frame */
    audio_update();
}

/* ---- File I/O ---- */

/*
 * All file I/O uses sdmc:/ (the SD card), which is always available to
 * homebrew via libnx without any extra mounting.
 *
 * Assets (read-only):  sdmc:/wipeout/<name>
 *   Copy the 517 game data files from the PSX disc into this directory.
 *
 * User data (read/write):  sdmc:/switch/wipegame/userdata/<name>
 *   Saves and config.  The directory is created automatically on first launch.
 *   No title ID or save-data partition required.
 */

#include <sys/stat.h>   /* mkdir */

/* SD card paths */
#define USERDATA_PATH "sdmc:/switch/wipegame/userdata"
/* Asset paths use sdmc:/<name> since asset names already include the
 * wipeout/ prefix (e.g. "wipeout/textures/drfonts.cmp") */

/* Ensure the userdata directory exists; called once from main() */
static void userdata_dir_init(void) {
    /* mkdir silently succeeds if the directory already exists */
    mkdir("sdmc:/switch/wipegame", 0777);
    mkdir(USERDATA_PATH, 0777);
}

/* file_exists() in utils.c uses bare relative paths which don't resolve on
 * Switch. The linker --wrap=file_exists flag redirects all calls here.
 * We prepend sdmc:/ for relative paths so game_init() finds the assets.
 * Absolute paths (starting with '/') and already-prefixed sdmc: paths
 * are passed through unchanged. */
bool __wrap_file_exists(const char *path) {
    char full[512];
    if (path[0] == '/' || strncmp(path, "sdmc:", 5) == 0) {
        snprintf(full, sizeof(full), "%s", path);
    } else {
        snprintf(full, sizeof(full), "sdmc:/%s", path);
    }
    struct stat st;
    bool result = (stat(full, &st) == 0);
    TRACE("file_exists: %s -> %s (%s)", path, full, result ? "yes" : "no");
    return result;
}

FILE *platform_open_asset(const char *name, const char *mode) {
    char path[512];
    snprintf(path, sizeof(path), "sdmc:/%s", name);
    return fopen(path, mode);
}

uint8_t *platform_load_asset(const char *name, uint32_t *out_size) {
    FILE *f = platform_open_asset(name, "rb");
    if (!f) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return NULL; }

    /*
     * Use mem_temp_alloc, not malloc. All callers in the upstream codebase
     * (image.c, track.c, object.c, sfx.c) call mem_temp_free() on the returned
     * pointer — matching the SDL platform's file_load() which also uses
     * mem_temp_alloc. Using malloc() here causes memory corruption on free.
     */
    uint8_t *data = (uint8_t *)mem_temp_alloc((uint32_t)size);
    if (!data) { fclose(f); return NULL; }

    if (out_size) *out_size = (uint32_t)fread(data, 1, size, f);
    fclose(f);
    return data;
}

uint8_t *platform_load_userdata(const char *name, uint32_t *out_size) {
    char path[512];
    snprintf(path, sizeof(path), USERDATA_PATH "/%s", name);

    FILE *f = fopen(path, "rb");
    if (!f) {
        /* Match upstream SDL platform: zero *out_size when file doesn't exist */
        if (out_size) *out_size = 0;
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return NULL; }

    /* game.c calls mem_temp_free() on the returned buffer — must match */
    uint8_t *data = (uint8_t *)mem_temp_alloc((uint32_t)size);
    if (!data) { fclose(f); return NULL; }

    if (out_size) *out_size = (uint32_t)fread(data, 1, size, f);
    fclose(f);
    return data;
}

uint32_t platform_store_userdata(const char *name, void *data, int32_t size) {
    char path[512];
    snprintf(path, sizeof(path), USERDATA_PATH "/%s", name);

    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    bool ok = ((int32_t)fwrite(data, 1, size, f) == size);
    fclose(f);
    /* sdmc:/ writes are committed immediately – no extra flush needed */
    return ok ? (uint32_t)size : 0;
}

/* ---- Input pump (called each frame by the main loop) ---- */

void platform_pump_events(void) {
    /* Check if the user requested exit via appletMainLoop */
    if (!appletMainLoop()) {
        s_wants_exit = true;
        return;
    }

    /* Detect dock/undock by polling actual EGL surface dimensions.
     * appletGetOperationMode() is unreliable for hbmenu-launched NROs —
     * it returns Handheld regardless of dock state. eglQuerySurface reports
     * what the compositor actually gave us, which changes on dock/undock. */
    EGLint new_w = s_surface_w, new_h = s_surface_h;
    eglQuerySurface(s_egl_display, s_egl_surface, EGL_WIDTH,  &new_w);
    eglQuerySurface(s_egl_display, s_egl_surface, EGL_HEIGHT, &new_h);
    if ((int)new_w != s_surface_w || (int)new_h != s_surface_h) {
        s_surface_w = (int)new_w;
        s_surface_h = (int)new_h;
        vec2i_t new_size = { s_surface_w, s_surface_h };
        system_resize(new_size);
        TRACE("dock/undock: surface now %dx%d (%s)",
              s_surface_w, s_surface_h,
              (s_surface_w == SWITCH_W_DOCKED) ? "docked 1080p" : "handheld 720p");
    }

    /* Feed the input system */
    input_update();
}

/* -------------------------------------------------------------------------
 * main()
 *
 * On Nintendo Switch, main() must be provided by the application.
 * It mirrors the pattern used by platform_sdl.c (the SDL_main-style loop).
 * ---------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* appletLockExit is deliberately omitted — it caused 2168-0002
     * in testing regardless of launch method. Clean exit is handled
     * by the main loop checking appletMainLoop() each frame. */

    /* ---- libnx services ---- */

    /* Create sdmc:/switch/wipegame/userdata/ if it doesn't exist yet */
    userdata_dir_init();

    /* Open debug log — directory guaranteed to exist after userdata_dir_init */
    log_open();
    TRACE("wipegame starting");

    /* ---- EGL + GLES2 ---- */
    TRACE("egl_init: starting");
    if (!egl_init()) {
        TRACE("egl_init: FAILED");
        log_close();
        return 1;
    }
    TRACE("egl_init: OK");

    /* ---- Audio ---- */
    TRACE("audio_init: starting");
    if (!audio_init()) {
        TRACE("audio_init: failed – continuing without audio");
    } else {
        TRACE("audio_init: OK");
    }

    /* ---- Controller ---- */
    TRACE("pad init: starting");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&s_pad);
    TRACE("pad init: OK");

    /* ---- Game init ---- */
    TRACE("system_init: starting");
    system_init();
    TRACE("system_init: OK");
    { GLenum _e = glGetError(); TRACE("post-system_init glGetError=0x%x", _e); }

    /* ---- Main loop — mirrors DC port exactly ---- */
    TRACE("entering main loop");
    while (!s_wants_exit) {
        platform_pump_events();
        if (s_wants_exit) break;   /* appletMainLoop() signalled exit; don't run another frame */
        platform_prepare_frame();
        { static int _fc=0; if(_fc<3){ GLenum _e=glGetError();
          TRACE("post-prepare_frame %d: glGetError=0x%x",_fc,_e); _fc++; } }
        system_update();   /* owns timing, render_frame_prepare/end, game_update, input_clear */
        { static int _fc=0; if(_fc<3){ GLenum _e=glGetError();
          TRACE("post-system_update %d: glGetError=0x%x",_fc,_e); _fc++; } }
        platform_end_frame();
    }
    TRACE("main loop exited");

    /* ---- Cleanup ---- */
    /* system_cleanup() calls render_cleanup() and input_cleanup(). */
    TRACE("cleanup: starting");
    system_cleanup();
    platform_video_cleanup();
    audio_cleanup();
    TRACE("cleanup: done");
    log_close();

    /*
     * appletLockExit() is not called (see comment above), so there is no
     * exit lock to release here. main() returning 0 hands control back to
     * the OS; if launched from hbmenu the display returns to hbmenu,
     * if launched via Home-button override it returns to the main menu.
     */
    return 0;
}
