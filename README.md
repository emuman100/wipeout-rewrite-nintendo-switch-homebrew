# wipEout Rewrite — Nintendo Switch Homebrew Port

A Nintendo Switch NRO port of [phoboslab/wipeout-rewrite](https://github.com/phoboslab/wipeout-rewrite), a faithful reimplementation of the 1995 PlayStation WipEout using the original game data files.

> **You need a legally obtained copy of the original PlayStation WipEout disc** to supply the game data. The port itself contains no game content.

> **This port is functional but largely untested.** It has been developed and verified on a Nintendo Switch Lite and a Nintendo Switch running Atmosphère. Compatibility across different firmware versions, hardware revisions, and CFW configurations has not been broadly tested. If you try it, feedback is welcome — bug reports and test results from different setups are especially useful at this stage.

> **Developed against upstream commit `bf7c825`** (`Merge pull request #183 from unofficial-fish/trig-removal`). Testing was done using this revision of wipeout-rewrite.

---

## Requirements

### Host (build machine)

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the Switch group installed
- The following devkitPro packages:

```bash
dkp-pacman -S switch-dev switch-mesa switch-libdrm_nouveau
```

- CMake ≥ 3.13, or GNU make

### Target (Nintendo Switch)

- Homebrew-enabled Switch running a current version of [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere)
- [hbmenu](https://github.com/switchbrew/nx-hbmenu) (the Homebrew Menu)
- A microSD card with at least 200 MB free for game data

---

## Building

### 1 — Clone the upstream game source

```bash
git clone https://github.com/phoboslab/wipeout-rewrite
cd wipeout-rewrite
```

### 2 — Merge the port files

Copy the downloaded port files into the cloned repo, merging with the existing
directory structure. `CMakeLists.txt` replaces the upstream one directly:

```bash
cp platform_switch.c       src/
cp render_gles2_compat.h   src/
cp CMakeLists.txt          .                # replaces upstream CMakeLists.txt
cp Makefile.switch         .
mkdir -p switch
cp icon.jpg                switch/icon.jpg  # hbmenu icon
```

The resulting layout should look like:

```
wipeout-rewrite/
├── src/
│   ├── platform_switch.c       ← port: full Switch platform backend
│   ├── render_gles2_compat.h   ← port: GLES2 compatibility shim
│   └── (existing upstream files unchanged)
├── CMakeLists.txt              ← port: replaces upstream CMakeLists.txt
├── Makefile.switch             ← port: GNU make alternative
└── switch/
    └── icon.jpg                ← hbmenu icon (256×256 JPEG)
```

> **Do not modify any other upstream source files.** The port is entirely
> self-contained in `platform_switch.c`, `render_gles2_compat.h`, and
> the build files.

### 3 — Build with CMake (recommended)

```bash
export DEVKITPRO=/opt/devkitpro

cmake -S . -B build-switch \
      -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
      -DPLATFORM=SWITCH \
      -DRENDERER=GLES2

cmake --build build-switch
```

Output: `build-switch/wipegame.nro`

### 4 — Build with GNU make (alternative)

```bash
export DEVKITPRO=/opt/devkitpro
make -f Makefile.switch
```

Output: `build-switch/wipegame.nro`

---

## Installation

### 1 — Copy the NRO

On your Switch's microSD card, create the directory
`/switch/wipegame/` and copy the NRO there:

```
sdmc:/switch/wipegame/wipegame.nro
```

### 2 — Install game data

For instructions on obtaining and extracting the required game data files, follow
the directions in the [upstream wipeout-rewrite README](https://github.com/phoboslab/wipeout-rewrite#usage).

> **The game data must be present and in the correct location before launching.**
> If the required files are missing or in the wrong place the app will
> terminate immediately with no error message. The intro movie
> (`intro.mpeg`) is the only optional file — its absence is handled gracefully.

Once extracted, your SD card should have the following structure:

```
sdmc:/
├── switch/
│   └── wipegame/
│       └── wipegame.nro
└── wipeout/
    ├── common/
    ├── sound/
    ├── textures/
    ├── track01/
    ├── track02/
    │   ...
    ├── track15/
    ├── music/               (optional)
    │   ├── track01.qoa
    │   ├── track02.qoa
    │   │   ...
    │   └── track08.qoa
    └── intro.mpeg           (optional)
```

### 3 — Install music (optional)

Background music is stored separately as QOA-compressed audio. If you have
the music tracks, place them at:

```
sdmc:/wipeout/music/track01.qoa
sdmc:/wipeout/music/track02.qoa
...
sdmc:/wipeout/music/track08.qoa
```

Without these files the game runs normally with sound effects only.

### 4 — Install intro movie (optional)

The opening cinematic is an MPEG-1 video file. If you have it, place it at:

```
sdmc:/wipeout/intro.mpeg
```

Without this file the game skips the intro and goes directly to the title
screen. If present but unreadable, the same skip occurs — it will not crash.

### 5 — Launch

Open the Homebrew Menu on your Switch, find **wipegame** — it will appear
with the original WipEout logo icon — and press **A** to launch it.

---

## Controls

### Gameplay

| Switch Button | Action |
|---|---|
| **A** | Thrust (accelerate) |
| **B** | Brake |
| **X** | Fire weapon |
| **Y** | Change camera view |
| **ZR** | Fire weapon (trigger) |
| **ZL** | Absorb pick-up (trigger) |
| **L** | Left airbrake |
| **R** | Right airbrake |
| **D-Pad** | Steer / pitch nose up-down |
| **+** (Plus) | Pause |

### Menus

| Switch Button | Action |
|---|---|
| **D-Pad** | Navigate |
| **Left stick** | Navigate (analog) |
| **A** | Confirm / Select |
| **B** or **X** | Back |
| **+** (Plus) | Pause / resume race |

### Notes

- **Left stick steering** — the left stick navigates menus but does **not** steer in-game by default. This is an upstream limitation: the default control bindings map steering to the D-pad only. To use the left stick for steering, go to Options → Controls and manually bind Left/Right/Up/Down to the stick axes.
- **Right stick** — reported to the input system but not bound to any action in the upstream game code. There is no camera look action to drive. It can be bound to any action via Options → Controls, but camera look is not available as a rebindable target.
- **ZR and ZL** are mapped to fire weapon and absorb respectively. They can be rebound to any action via Options → Controls.
- **Unpausing**: press **+** to open the pause menu, navigate to **CONTINUE**, press **A**.
- **Home button** exits the game and returns to the Switch main menu. Save data is written before exit.

---

## Save Data

Save files and configuration are stored at:

```
sdmc:/switch/wipegame/userdata/
```

This directory is created automatically on first launch. High scores and race
progress persist here across sessions.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  wipeout-rewrite upstream core                               │
│  game.c  race.c  ship.c  sfx.c  menu.c  track.c  …         │
│                                                              │
│  renderer: render_gl.c  (compiled -DUSE_GLES2=1)            │
│            offscreen FBO → fullscreen quad blit              │
│            post-effects: none / CRT scanline                 │
└──────────────────────────┬──────────────────────────────────┘
                           │  platform.h
┌──────────────────────────▼──────────────────────────────────┐
│  platform_switch.c                                           │
│                                                              │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────────┐  │
│  │  Graphics   │  │    Audio     │  │      Input        │  │
│  │  EGL+GLES2  │  │ audout/libnx │  │  padXxx / libnx   │  │
│  │ mesa-switch │  │ 48 kHz s16le │  │ Joy-Con, handheld │  │
│  │720p or 1080p│  │  2 DMA bufs  │  │  Pro Controller   │  │
│  └─────────────┘  └──────────────┘  └───────────────────┘  │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  File I/O                                            │    │
│  │  Assets:    sdmc:/wipeout/<name>                     │    │
│  │  Saves:     sdmc:/switch/wipegame/userdata/<name>    │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│                    libnx / devkitA64                         │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Reference

### Graphics

**Backend:** OpenGL ES 2.0 via EGL and mesa-switch (Nouveau driver for
Tegra X1). The upstream `render_gl.c` renderer is compiled with
`-DUSE_GLES2=1`, which enables GLSL `precision highp float;` qualifiers
and selects `GL_DEPTH_COMPONENT16` for the renderbuffer — both required
for strict GLES2 compliance. libGLESv2 is linked statically; GL functions
are called directly rather than through `eglGetProcAddress` dispatch stubs,
which crash on Switch mesa due to lazy shader compiler initialisation.

**Framebuffer:** The render resolution is fixed at launch based on dock
state at startup. If launched in **handheld mode**, the NWindow is set to
**1280×720** for the session. If launched **docked**, it is set to
**1920×1080**. The render resolution does not change mid-session — the TV
upscales a handheld-launched session and the handheld screen downscales a
docked-launched session. The Switch hardware compositor handles the
final output in both cases.

**Render pipeline:** The game renders to an offscreen FBO (render-to-texture),
then blits it to the EGL surface via a fullscreen quad. This supports the three built-in render resolutions (240p, 480p, native)
and both post-effect modes (none, CRT scanline), all selectable in-game.
Native resolution is 1280×720 if launched handheld, or 1920×1080 if
launched docked.

**Texture atlas:** A single 2048×2048 RGBA8 GPU texture holds all game
textures. The CPU-side packing buffer is allocated from the hunk
allocator temporarily during load and freed immediately after
upload — it does not persist at runtime.

**GLES2 compatibility shim (`render_gles2_compat.h`):** A force-included
header translates desktop GL assumptions into strict GLES2 requirements:

- `GL_RGB` FBO attachments redirected to `GL_RGBA` (GLES2 requires
  `GL_RGBA` for color-renderable FBO textures)
- `GL_LINEAR_MIPMAP_*` min filter modes downgraded to `GL_LINEAR` —
  `RENDER_USE_MIPMAPS=1` is hardcoded upstream but `glGenerateMipmap`
  crashes the Switch mesa nv50 blitter; without this fix the atlas
  texture is mipmap-incomplete and returns black for all samples
- `glGenerateMipmap` no-opped entirely
- VAO emulation — `GL_OES_vertex_array_object` is not exposed in the
  Switch SDK GLES2 headers; a minimal fake-VAO layer records attribute
  indices at shader init time and replays the known `vertex_t` layout
  on every `glBindVertexArray` call, correctly restoring per-shader
  attribute state when switching between the game shader and post shader
- Anisotropic filtering suppressed — `glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)`
  returns a safe 1.0 without making the real call (which generates
  `GL_INVALID_VALUE` on Switch GLES2); `glTexParameterf` for anisotropy
  is no-opped entirely
- GLEW stubbed out — `glewInit()` and `glewExperimental` are no-ops;
  the Switch SDK has no GLEW equivalent

---

### Audio

**Backend:** libnx `audout` API (native AudioOut service).

**Format:** 48 000 Hz, stereo, signed 16-bit little-endian (s16le). The
game's mix callback produces 44 100 Hz float samples; the platform layer
resamples linearly to 48 kHz and converts to s16le each frame.

**Buffering:** Two DMA buffers of 8 192 bytes each (2048 samples at
48 kHz) are allocated with `memalign(0x1000, …)` and pre-queued at
initialisation. Each frame, `audoutGetReleasedAudioOutBuffer` polls
non-blocking for a completed buffer; if one is available it is filled
by the mix callback and requeued, otherwise audio is skipped for that
frame. This non-blocking approach prevents the audio buffer from
stalling the game loop.

**Music streaming:** Background music is QOA-compressed (`.qoa` files).
`fread` is called from the mix callback approximately every 3–4 frames
to decode a 4 KB QOA frame (~5 120 samples). At worst-case SD card
speeds this takes under 0.5 ms — negligible against the 33 ms frame
budget. If a music file is absent the game runs silently; if it ends,
the game advances to the next track or loops.

---

### Input

**Backend:** libnx HID v2 (`padXxx` API).

`padConfigureInput(1, HidNpadStyleSet_NpadStandard)` and
`padInitializeDefault(&pad)` are called once at startup. This single
`PadState` transparently handles:

- Joy-Con pair (held horizontally or vertically)
- Handheld mode (Joy-Cons attached to the console)
- Nintendo Switch Pro Controller

`padUpdate(&pad)` is called once per frame before input is read.
Button states are reported as float values (0.0 or 1.0 for digital
buttons; 0.0–1.0 for analog axes) via `input_set_button_state()`,
matching the contract expected by the upstream input layer.

**Analog stick:** The left stick's Y axis is corrected for the libnx
convention (positive = up physically) vs the SDL/game convention
(positive = down), so menu navigation with the stick is correct in
both axes. The left stick navigates menus out of the box via a
system-layer binding, but does not steer in-game by default — the
upstream game binds steering actions to the D-pad only. Analog
steering requires manual remapping via Options → Controls.

The right stick is polled and all four axes plus stick-click are
reported to the input layer, but the upstream game defines no camera
look action for it to drive. It can be rebound to any available action
via Options → Controls, but camera look is not a rebindable target.
Both stick-click buttons (L3 / R3) are reported.

---

### File I/O

**Assets** are loaded from `sdmc:/wipeout/` via standard `fopen`/`fread`.
The SD card is automatically mounted by the Switch OS; no explicit
`fsdevMountSdmc()` call is required.

All buffers returned by `platform_load_asset()` and
`platform_load_userdata()` are allocated with `mem_temp_alloc()` from
the game's hunk allocator, matching the contract expected by every
caller in the upstream codebase (`image.c`, `track.c`, `object.c`,
`sfx.c`, `game.c`). These callers all call `mem_temp_free()` on the
returned pointer; using `malloc()` here would corrupt the allocator.

**Save data** is written to `sdmc:/switch/wipegame/userdata/`. This
directory is created with `mkdir()` on first launch. Writes use
standard `fopen`/`fwrite`; no `fsdevCommitDevice()` is needed because
sdmc write-through is handled by the OS.

**Intro video** (`intro.mpeg`) is decoded by `pl_mpeg`, which reads from
a `FILE *` via a 128 KB internal buffer. The buffer is refilled
approximately every 0.5 seconds. If the file is absent, `intro_init()`
detects the null file handle and skips directly to the title screen.

---

### Memory

The game's memory footprint is well within the standard homebrew applet
limit (~308 MB heap from hbmenu). **High memory mode is not required.**

| Region | Size |
|---|---|
| Hunk allocator (BSS, `mem.c`) | 16 MB |
| Triangle buffer (BSS) | 144 KB |
| Audio DMA buffers (heap) | 16 KB |
| GPU texture atlas (VRAM) | 16 MB |
| Code + libnx + mesa stack | ~15 MB (estimated) |
| **Total CPU RAM** | **~32 MB** |

The texture atlas CPU-side packing buffer and all asset file data are
transient — they live in the hunk's temp region during loading and are
freed before gameplay begins.

---

### Debugging

At startup, `psmInitialize()` (Power State Manager service) is called
before the first `appletGetOperationMode()` query — without it,
`appletGetOperationMode()` always returns Handheld regardless of actual
dock state. Each frame, `appletMainLoop()` processes any pending PSM
messages from dock/undock events, after which `appletGetOperationMode()`
is polled to read the updated state. When a transition is detected, it
is logged to `debug.log` with the current mode and render resolution.
The render resolution does not change on transition — this is a
diagnostic feature only. See Known Limitations.

All debug output is written to:

```
sdmc:/switch/wipegame/debug.log
```

---

## Known Limitations and Future Work

| Item | Notes |
|---|---|
| **Resolution fixed at launch** | Render resolution is set from dock state at startup: 720p if launched handheld, 1080p if launched docked. It does not change when docking or undocking mid-session. To get the correct resolution for your display, launch the game in the mode you intend to play in. Dynamic mid-session resolution switching is not supported due to a mesa-switch GPU memory limitation that causes crashes on repeated EGL surface recreation. |
| **Analog stick steering** | Left stick does not steer in-game by default — requires manual remapping via Options → Controls. A future fix would set default bindings for the Switch. |
| **Rumble** | libnx exposes `hidSendVibrationValue()`; mapping it to ship collision events in `ship.c` would improve feel. |

---

## Credits

| Role | Credit |
|---|---|
| **Original game reimplementation** | [Dominic Szablewski (phoboslab)](https://github.com/phoboslab) — [wipeout-rewrite](https://github.com/phoboslab/wipeout-rewrite) |
| **Original WipEout** | Psygnosis / Sony Interactive Entertainment (1995) |
| **Nintendo Switch port** | Claude (Anthropic) — AI-developed under the direction of **emuman100** |
| **Port direction & oversight** | emuman100 |
| **Renderer reference** | [m4xw (ppsspp)](https://github.com/m4xw/ppsspp) Switch port — direct source of the static libGLESv2 linking requirement and GL error drain approach that fixed the renderer. [fgsfdsfgs (gzdoom)](https://github.com/fgsfdsfgs/gzdoom) Switch port — used as a corroborating cross-reference for GLES2 context setup and applet handling |
| **Dock/undock detection reference** | [devkitPro SDL2-switch](https://github.com/devkitPro/SDL) (`switch-sdl-2.28` branch) — definitive reference for `psmInitialize()` requirement, per-frame `appletGetOperationMode()` polling pattern, and EGL surface resize sequence (`SWITCH_PumpEvents`, `SWITCH_VideoInit`, `SWITCH_SetWindowSize`) |
| **mgba / RetroArch Switch ports** | [Epicpkmn11 (mgba)](https://github.com/mgba-emu/mgba) and [libretro (RetroArch)](https://github.com/libretro/RetroArch) — reference for `nwindowSetCrop`-based dock/undock approach; confirmed that repeated raw EGL surface recreation is not used by any working Switch port |
| **Atmosphère** | [Sciresm and the Atmosphère team](https://github.com/Atmosphere-NX/Atmosphere) — custom firmware that makes Switch homebrew possible |
| **devkitPro** | [devkitPro team](https://devkitpro.org) — toolchain, libnx, and documentation without which this port could not exist. [switchbrew/libnx](https://github.com/switchbrew/libnx) `native_window.c` source was analysed to determine correct `nwindowReleaseBuffers` ordering during the dock/undock investigation |

---

## Licence

The upstream wipeout-rewrite source is released without a formal licence;
the author asks that it not be used commercially. This port adds no new
copyrightable content and is subject to the same terms. The original
WipEout game data remains the intellectual property of Sony Interactive
Entertainment; you must own a legitimate copy of the PlayStation release
to use it.
