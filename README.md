# wipEout Rewrite — Nintendo Switch Homebrew Port

A Nintendo Switch NRO port of [phoboslab/wipeout-rewrite](https://github.com/phoboslab/wipeout-rewrite), a faithful reimplementation of the 1995 PlayStation WipEout using the original game data files.

> **You need a legally obtained copy of the original PlayStation WipEout disc** to supply the game data. The port itself contains no game content.

---

## Requirements

### Host (build machine)

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the Switch group installed
- The following devkitPro packages:

```bash
dkp-pacman -S switch-dev switch-sdl2
```

(`switch-sdl2` pulls in `switch-mesa` and `switch-libdrm_nouveau` as dependencies.)

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
directory structure. Note that `CMakeLists_switch.txt` must be **renamed** to
`CMakeLists.txt`, replacing the upstream one:

```bash
cp platform_switch.c       src/
cp render_gles2_compat.h   src/
cp CMakeLists_switch.txt   CMakeLists.txt   # rename: replaces upstream file
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
> self-contained in `platform_switch.c` and the build files.

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
    ├── music/
    │   ├── track01.qoa
    │   ├── track02.qoa
    │   │   ...
    │   └── track08.qoa
    └── intro.mpeg
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

Open the Homebrew Menu on your Switch (hold **R** while launching any game,
or use Album if configured), find **wipegame** — it will appear with the
original WipEout logo icon — and launch it.

---

## Controls

### Gameplay

| Switch Button | Action |
|---|---|
| **A** | Thrust (accelerate) |
| **X** | Fire weapon |
| **Y** | Change camera view |
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

- **ZR and ZL are not mapped.** The game has no trigger actions (shield and
  absorb are passive power-up effects, not player-triggered). ZR/ZL can be
  remapped to any action via the in-game Options → Controls menu.
- **Analog steering** requires remapping in Options → Controls. By default
  the left stick navigates menus only; D-Pad steers during races. This
  matches the upstream SDL behaviour — the analog response curve code is
  present and works once A_LEFT/A_RIGHT are rebound to the stick.
- **Unpausing**: press **+** to open the pause menu, navigate to **CONTINUE**,
  press **A**.

---

## Save Data

Save files and configuration are stored at:

```
sdmc:/switch/wipegame/userdata/
```

This directory is created automatically on first launch. It requires no title
ID, no save-data partition, and no special permissions. Files can be copied to
or from a PC using any USB file manager. High scores, control remappings,
graphics settings, and race progress all persist here across sessions.

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
│  │  SDL2+GLES2 │  │ audout/libnx │  │  padXxx / libnx   │  │
│  │ mesa-switch │  │ 48 kHz s16le │  │ Joy-Con, handheld │  │
│  │  1280×720   │  │  2 DMA bufs  │  │  Pro Controller   │  │
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

**Backend:** OpenGL ES 2.0 via SDL2 (window and GL context creation) and
mesa-switch (Nouveau driver for Tegra X1). SDL2 is used exclusively for
video initialisation; audio, input, and filesystem continue to use libnx
directly. The upstream `render_gl.c` renderer is compiled with
`-DUSE_GLES2=1`, which enables GLSL `precision highp float;` qualifiers
and selects `GL_DEPTH_COMPONENT16` for the renderbuffer — both required
for strict GLES2 compliance.

**Framebuffer:** The SDL2 window is created at **1280×720** with
`SDL_WINDOW_FULLSCREEN`. In handheld mode this is native. In docked mode
the Switch hardware compositor upscales to the TV's output resolution
automatically; no application-side reconfiguration is needed.

**Render pipeline:** The game renders to an offscreen FBO (render-to-texture),
then blits it to the SDL2 window surface via a fullscreen quad. This supports
the three built-in render resolutions (240p, 480p, native 720p) and both
post-effect modes (none, CRT scanline), all selectable in-game.

**Texture atlas:** A single 2048×2048 RGBA8 GPU texture holds all game
textures. The CPU-side packing buffer is allocated from the hunk
allocator temporarily during load and freed immediately after
upload — it does not persist at runtime.

**Dock/undock transitions:** SDL2's GL context and the underlying mesa-switch
EGL surface survive a dock/undock event transparently. The game loop continues
uninterrupted across mode changes.

---

### Audio

**Backend:** libnx `audout` API (native AudioOut service).

**Format:** 48 000 Hz, stereo, signed 16-bit little-endian (s16le). The
game's mix callback produces 44 100 Hz float samples; the platform layer
resamples linearly to 48 kHz and converts to s16le each frame.

**Buffering:** Two DMA buffers of 8 192 bytes each (~33 ms of audio per
buffer) are allocated with `memalign(0x1000, …)` and pre-queued at
initialisation. Each frame, `audoutWaitPlayFinish` blocks until the
hardware finishes a buffer, then the mix callback fills it and it is
requeued. This blocking approach matches the official switchbrew audio
examples and prevents silent gaps under load.

**Music streaming:** Background music is QOA-compressed (`.qoa` files).
`fread` is called from the mix callback approximately every 3–4 frames
to decode a 4 KB QOA frame (~5 120 samples). At worst-case SD card
speeds this takes under 0.5 ms — negligible against the 33 ms frame
budget. If a music file is absent the game runs silently; if it ends,
the game advances to the next track or loops.

**Dock/undock transitions:** Audio output is independent of the display
subsystem and continues uninterrupted across mode changes.

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
both axes.

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

## Known Limitations and Future Work

| Item | Notes |
|---|---|
| **Docked 1080p rendering** | Currently renders at 720p in both modes; hardware upscales when docked. Native 1080p is achievable by polling `appletGetOperationMode()` and calling `SDL_SetWindowSize()` + `system_resize()` per frame. |
| **Rumble** | libnx exposes `hidSendVibrationValue()`; mapping it to ship collision events in `ship.c` would improve feel. |
| **Touchscreen** | Not implemented. All input is button-based. |
| **Analog steering default** | Left stick steers menus only by default. Remap A_LEFT/A_RIGHT to the stick in Options → Controls for analog ship steering. |
| **ZR / ZL** | Unbound by default; no trigger actions exist in the base game. Remap freely in Options → Controls. |

---

## Credits

| Role | Credit |
|---|---|
| **Original game reimplementation** | [Dominic Szablewski (phoboslab)](https://github.com/phoboslab) — [wipeout-rewrite](https://github.com/phoboslab/wipeout-rewrite) |
| **Original WipEout** | Psygnosis / Sony Interactive Entertainment (1995) |
| **Nintendo Switch port** | Claude (Anthropic) — AI-developed under the direction of **emuman100** |
| **Port direction & oversight** | emuman100 |
| **Dreamcast port reference** | [jnmartin84](https://github.com/jnmartin84) — [wipeout-dc](https://github.com/jnmartin84/wipeout-dc) |

---

## Licence

The upstream wipeout-rewrite source is released without a formal licence;
the author asks that it not be used commercially. This port adds no new
copyrightable content and is subject to the same terms. The original
WipEout game data remains the intellectual property of Sony Interactive
Entertainment; you must own a legitimate copy of the PlayStation release
to use it.
