# CLAUDE.md - Project Context (Authoritative)

> **Note:** This is a **project-specific** CLAUDE.md. It applies only to the QuakeSpasm project.
> For personal preferences across all projects, use `~/.claude/CLAUDE.md`.

---

## Authority & Usage Rules

**This document is the AUTHORITATIVE source for understanding this project.**

Treat this file as the **single source of truth**. Do NOT rediscover information that is already documented here.

**Required behavior:**
* **Trust this file over repository inference** - If something is documented here, accept it as fact
* **Do NOT rescan the repository** to rediscover high-level context already provided
* **Only read additional files when explicitly required for the task** - Don't explore "just in case"
* **If something is not described here, assume it is out of scope** unless the task directly requires it

**Why this matters:**
This prevents token-heavy "verification scans" and repeated exploration. The information here is stable, intentional, and should eliminate the need to re-learn the project structure in every conversation.

---

## Project Summary

**Purpose:** QuakeSpasm is a modern cross-platform Quake engine source port based on FitzQuake. It provides support for 64-bit CPUs, custom music playback, improved sound drivers, graphical enhancements, and numerous bug fixes while maintaining compatibility with the original Quake game and its mods.

**Version:** 0.97.0

**Stack:**
* **Language:** C (C11 standard via GNU extensions)
* **Graphics:** OpenGL (fixed function + GLSL shaders)
* **Platform Layer:** SDL2 (preferred) or SDL1.2
* **Audio Codecs:** Vorbis, FLAC, MP3 (libmad/mpg123), Opus, MikMod, XMP, ModPlug
* **Build Systems:** CMake, GNU Make, Visual Studio (2005+), Xcode
* **License:** GNU General Public License v2

**Supported Platforms:** Windows (MSVC, MinGW), Linux/Unix, macOS, Emscripten (WebAssembly), Haiku

---

## Architecture Overview

QuakeSpasm follows the classic Quake engine client-server architecture within a single executable:

```
/Quake                    -> Main engine source code (all .c/.h files)
  main_sdl.c              -> Application entry point, main loop
  host.c                  -> Host system coordination (client/server lifecycle)

  # Client subsystem (cl_*)
  cl_main.c               -> Client initialization and connection management
  cl_input.c              -> Input processing and command generation
  cl_parse.c              -> Server message parsing
  cl_demo.c               -> Demo recording/playback
  cl_tent.c               -> Temporary entities (particles, beams)

  # Server subsystem (sv_*)
  sv_main.c               -> Server initialization and client management
  sv_phys.c               -> Physics simulation
  sv_move.c               -> Monster/entity movement
  sv_user.c               -> Client command processing

  # Rendering subsystem (gl_*, r_*)
  gl_rmain.c              -> Main rendering loop
  gl_draw.c               -> 2D drawing (HUD, console)
  gl_model.c              -> Model loading (BSP, MDL, SPR)
  gl_texmgr.c             -> Texture management
  gl_vidsdl.c             -> Video/window management via SDL
  gl_screen.c             -> Screen updates and SCR_* functions
  gl_warp.c               -> Water/sky warping effects
  gl_fog.c                -> Fog rendering
  r_alias.c               -> Alias model (MDL) rendering
  r_brush.c               -> Brush model rendering
  r_world.c               -> World/BSP rendering

  # Sound subsystem (snd_*)
  snd_dma.c               -> DMA sound mixing core
  snd_sdl.c               -> SDL audio output
  snd_codec.c             -> Codec dispatcher for music
  snd_vorbis.c            -> Ogg Vorbis decoder
  snd_mp3.c / snd_mpg123.c -> MP3 decoders
  bgmusic.c               -> Background music management

  # Networking (net_*)
  net_main.c              -> Network core and dispatch
  net_dgrm.c              -> Datagram protocol
  net_loop.c              -> Loopback (local) connections
  net_bsd.c / net_udp.c   -> Unix networking
  net_wins.c / net_wipx.c -> Windows networking

  # QuakeC VM (pr_*)
  pr_exec.c               -> QuakeC bytecode interpreter
  pr_edict.c              -> Entity management
  pr_cmds.c               -> Built-in functions for QuakeC

  # Core systems
  cmd.c                   -> Console command system
  cvar.c                  -> Console variable system
  console.c               -> In-game console
  common.c                -> File system, parsing, utilities
  zone.c                  -> Memory allocation (Hunk, Zone, Cache)
  mathlib.c               -> Vector/matrix math

  # Platform specific
  sys_sdl_unix.c          -> Unix system functions
  sys_sdl_win.c           -> Windows system functions
  pl_linux.c / pl_win.c   -> Platform layer

/Windows                  -> Windows-specific build files and dependencies
  /SDL, /SDL2             -> Bundled SDL libraries
  /codecs                 -> Bundled audio codec DLLs
  /VisualStudio           -> Visual Studio 2017+ project files
  /VS2005                 -> Legacy Visual Studio 2005 project files

/MacOSX                   -> macOS-specific build files
  *.framework             -> Bundled frameworks (SDL, SDL2, codecs)
  *.xcodeproj             -> Xcode project files

/Linux                    -> Linux-specific documentation
  /sgml                   -> SGML documentation sources

/Misc                     -> Miscellaneous tools and historical docs
  fitzquake*.txt          -> FitzQuake documentation (historical reference)
```

**Data Flow:**
```
User Input -> IN_* (SDL events) -> Key/Mouse handling
          -> CL_SendCmd() -> Network
          -> SV_RunClients() -> Physics -> Entity updates
          -> SV_SendClientMessages() -> Network
          -> CL_ParseServerMessage() -> Client state
          -> R_* / GL_* rendering -> OpenGL -> Display
```

**Main Loop (in main_sdl.c):**
1. Process SDL events
2. `Host_Frame(time)` - runs both client and server for one frame
3. Sleep to throttle frame rate

---

## Key Data Structures

**Global State:**
* `sv` (server_t) - Server state, models, edicts, time
* `svs` (server_static_t) - Persistent server info, client list
* `cl` (client_state_t) - Client view state, entities, time
* `cls` (client_static_t) - Connection state, demo recording

**Entities:**
* `edict_t` - Game entity (variable-sized, includes QuakeC fields)
* `entity_t` - Client-side entity representation for rendering

**Memory Management:**
* Hunk - Large permanent allocations (models, sounds)
* Zone - Dynamic allocations with tagging
* Cache - Auto-evicting cache for model data
* Temp - Stack-based temporary allocations

---

## Invariants, Conventions & Preferences

### Technical Invariants

These are **always true** and should never be re-inferred from code:

* **Coordinate System:** Right-handed, Z-up (Quake convention)
* **Fixed-point angles:** 0-65535 maps to 0-360 degrees in network protocol
* **Entity numbering:** Entity 0 is always the world, players start at 1
* **Protocol versions:** 15 (NetQuake), 666 (FitzQuake), 999 (RMQ extended)
* **Memory limits:** Many hardcoded limits (MAX_EDICTS, MAX_MODELS, etc.) in quakedef.h
* **Float precision:** Single precision floats throughout (vec3_t = float[3])

### Code Conventions

* **Naming:** `Module_FunctionName` pattern (e.g., `CL_ParseServerMessage`, `SV_Physics`)
* **Prefixes:**
  - `CL_` - Client functions
  - `SV_` - Server functions
  - `GL_` / `R_` - Rendering
  - `S_` - Sound
  - `NET_` - Networking
  - `PR_` - Progs (QuakeC VM)
  - `Con_` - Console
  - `Cmd_` - Commands
  - `Cvar_` - Console variables
  - `Sys_` - System/platform layer
  - `IN_` - Input
  - `VID_` - Video
* **Style:** K&R style braces, tabs for indentation
* **C Standard:** C11 via GNU extensions (`-std=gnu11`)
* **Compiler warnings:** `-Wall -Wno-trigraphs`
* **Comments:** Original id Software comments preserved, additions marked with author names

### Build Conventions

* **CMake presets:** Use `cmake --preset <name>` for standardized builds
* **Makefile variables:**
  - `USE_SDL2=1` - Build with SDL2 (recommended)
  - `DEBUG=1` - Debug build
  - `DO_USERDIRS=1` - Enable user directories support
  - `USE_CODEC_*` - Enable/disable audio codecs

### Project Preferences

* **Backward compatibility:** Must run original Quake content and popular mods
* **Protocol compatibility:** FitzQuake protocol 666 is the default
* **Code style:** Match existing patterns in each file
* **Platform parity:** Features should work on all supported platforms when possible
* **Conservative changes:** Prefer minimal, targeted fixes over large refactors

---

## Boundaries & Constraints

### Off-Limits (Do Not Modify)

Unless explicitly requested:

* `/Windows/SDL/`, `/Windows/SDL2/`, `/Windows/codecs/` - Bundled third-party libraries
* `/MacOSX/*.framework/` - Bundled macOS frameworks
* `miniz.c`, `miniz.h` - Third-party compression library
* `lodepng.c`, `lodepng.h` - Third-party PNG library
* `stb_image_write.h` - Third-party image writing library
* `.git/` - Git metadata
* Build output directories (Build-*, out/, *.o, *.d)

### Behavioral Constraints

Unless explicitly requested, avoid:

* Full repository scans or architecture analysis
* Suggesting major refactors or modernization efforts
* Breaking backward compatibility with original Quake
* Changing network protocol behavior
* Modifying build configurations for untested platforms

### Locked-In Assumptions

These are intentional; do not question or re-validate:

* SDL2 is the preferred platform layer (SDL1.2 is legacy)
* OpenGL 1.x/2.x compatibility is required (no modern OpenGL-only features)
* The QuakeC VM must remain compatible with original progs.dat files
* Memory management follows the original Quake patterns (Hunk/Zone/Cache)
* Entity limits can be raised but the edict_t structure must remain compatible

---

## Build & Development

### Quick Start (CMake - Recommended)

**Windows (MSVC):**
```bash
cmake --preset msvc-x64-release
cmake --build --preset msvc-x64-release
```

**Windows (MinGW):**
```bash
cmake --preset mingw-x64-release
cmake --build --preset mingw-x64-release
```

**Linux:**
```bash
cmake --preset release
cmake --build --preset release
```

### Quick Start (Makefile - Linux/Unix)

```bash
cd Quake
make USE_SDL2=1
```

### Quick Start (Visual Studio)

Open `Windows/VisualStudio/quakespasm.sln` in Visual Studio 2017 or newer.

### Dependencies

**Required:**
* SDL2 (or SDL 1.2)
* OpenGL headers and libraries
* Standard C library

**Optional audio codecs:**
* libvorbis/libogg (Ogg Vorbis music)
* libmad or libmpg123 (MP3 music)
* libFLAC (FLAC music)
* libopus/libopusfile (Opus music)
* libmikmod, libxmp, or libmodplug (tracker music)

### Running

Place the `quakespasm` executable alongside your Quake game data:
```
quake/
  quakespasm.exe (or quakespasm)
  quakespasm.pak (optional custom data)
  id1/
    pak0.pak
    pak1.pak (registered version)
    music/    (optional music files)
```

### Debug Build

```bash
# CMake
cmake --preset debug
cmake --build --preset debug

# Makefile
make DEBUG=1 USE_SDL2=1
```

---

## Documentation Map

* `CLAUDE.md` - **Authoritative architectural overview and constraints** (you are here)
* `Quakespasm.txt` - User documentation, changelog, feature list
* `Quakespasm-Music.txt` - Music playback documentation
* `LICENSE.txt` - GNU GPL v2 license
* `Misc/fitzquake*.txt` - Historical FitzQuake documentation (reference for inherited features)

### External Resources

* Project homepage: http://quakespasm.sourceforge.net
* Source repository: https://sourceforge.net/p/quakespasm/quakespasm/
* GitHub mirror: https://github.com/sezero/quakespasm
* FitzQuake reference: https://celephais.net/fitzquake/

---

## Working Approach

When completing tasks:

1. **Start here** - Use CLAUDE.md as the authoritative context foundation (do NOT rescan the repository for information already documented)
2. **Read targeted files** - Only open files directly relevant to the task
3. **Minimal changes** - Prefer focused edits over broad refactors
4. **Follow patterns** - Match existing code style and architecture
5. **Test compatibility** - Consider impact on all supported platforms
6. **Preserve history** - Keep original id Software comments intact

**If the task truly cannot be completed without more info, ask specific questions.**

---

## Key Entry Points by Task

| Task | Start Here |
|------|-----------|
| Rendering bug | `gl_rmain.c`, `gl_screen.c`, `glquake.h` |
| Sound issue | `snd_dma.c`, `snd_sdl.c`, `snd_codec.c` |
| Network/multiplayer | `net_main.c`, `sv_main.c`, `cl_main.c` |
| Input handling | `in_sdl.c`, `keys.c` |
| Console/commands | `cmd.c`, `cvar.c`, `console.c` |
| QuakeC/game logic | `pr_exec.c`, `pr_cmds.c`, `pr_edict.c` |
| File loading | `common.c` (COM_LoadFile, etc.) |
| Memory issues | `zone.c` |
| Build system | `CMakeLists.txt`, `Quake/Makefile` |
| Platform-specific | `sys_sdl_*.c`, `pl_*.c` |
| Video/window | `gl_vidsdl.c`, `vid.h` |
| Model loading | `gl_model.c` |
| Music playback | `bgmusic.c`, `snd_*.c` (codecs) |

---
