# pcsx-abnxt - CLAUDE.md

AutoBleem's next PS1 emulator: **upstream PCSX-ReARMed (notaz) with what Sony and AutoBleem added to pcsx-ab
re-implemented on top**, for the PlayStation Classic, the two Raspberry Pi builds and the Windows dev host.
Author: screemer (the repo owner). It replaces `pcsx-ab` (`github.com/autobleem/pcsx-ab2`,
`E:\Programming\pcsx-rearmed-develop` - a 2017 core with patches), which AutoBleem
(`E:\Programming\autobleem-develop`) still ships until phase 8 of the plan. This file is the project knowledge
of record; keep it current in the same commit as any change it describes. Git history has the reasoning per
change (commit messages are prose).

## State (2026-09-20)

Phases 0-4 and 6 of `docs/port-plan.md` are done: the repositories, the CMake build for every target, the
SDL2 platform, the launcher's contract (arguments, config, exit files), the front buttons with the autosave
ring and the power daemon, the in-game menu, and phase 5's disc change without its picker screen. All of it
verified on Windows only; the first runs on the Pi 400 and the console are the next thing, then phase 5's
picker and phase 7 (the compatibility pass).

| | |
|---|---|
| Repository | `github.com/autobleem/pcsx-abnxt`, a **public GitHub fork** of `notaz/pcsx_rearmed` (GPL-2; a fork of a public repo cannot be private - the owner's call, 2026-09-20) |
| Base | upstream tag **`r26`** (2026-03-29, `56fef013`), the latest stable; `develop` starts there |
| Branches | `master` mirrors upstream master (fast-forward only, never committed to); `develop` is ours; `feature/<slug>` off `develop`, merged `--no-ff` (gitflow, as in every AutoBleem repo); `upstream` remote = notaz |
| libpicofe | submodule `frontend/libpicofe` -> **`github.com/autobleem/libpicofe`** (our fork of notaz's), branch `develop`: r26's commit plus our SDL2 files (`plat_sdl2.*`, `in_sdl2.*`, `in_sdl2gc.*`); `upstream` remote there too. The other submodules (`deps/libchdr`, `lightrec`, `lightning`, `libretro-common`, `mman`, `frontend/warm`) are upstream's, untouched |
| Build | `CMakeLists.txt`: upstream's `configure`/`Makefile` as CMake options (`PCSXAB_*`), the plugins, libchdr/lightrec/lightning/mman compiled from `deps/`; upstream's own build files stay untouched. `PCSXAB_PLATFORM=sdl2` (ours, the default), `sdl` (upstream's SDL 1.2 frontend, needs sdl12-compat on a PC) or `headless` |
| Windows | `./make_win.sh` -> `build_win/pcsx-ab.exe`: **lightrec + C-SIMD gpu_neon, plays games** - Crash Bandicoot's intro in a 1280x720 window, Esc opens the menu, `tools/win_drive.ps1` drives it from a script (keys, screenshots, `-EmuArgs`, `close`) |
| Pi 32-bit / 64-bit | `./make_rpi.sh`, `./make_rpi64.sh` -> `build_rpi*/dist/`: Ari64 ARM / ARM64 dynarec, NEON asm / C-SIMD GPU, the SDL2 platform - **build, unrun** |
| PlayStation Classic | `ci/build.sh psc` in the Docker image (gcc-6, `/opt/psc`, SDL 2.0.12): builds and links, GLIBC <= 2.12, no RPATH, ARM dynarec + NEON - **unrun** (`build_psc/dist/` on the PC holds the last fetch); `make_psc.sh` is the Sony-toolchain path over ssh, untested here |
| Local checkout | `E:\Programming\pcsx-abnxt` |

**The SDL2 platform** (`frontend/plat_sdl2.c` over libpicofe's `plat_sdl2`/`in_sdl2`/`in_sdl2gc`): one window
and one accelerated renderer everywhere; the GPU plugin draws into a shadow RGB565 buffer that
`plat_gvideo_flip()` uploads to a streaming texture and presents into the layer plugin_lib works out
(`g_layer_*`; `plat_target.hwfilter` 0 = linear, 1 = nearest); the menu draws into a buffer the size of the
window (the 2x font from 640x480 up). Fullscreen-desktop on the ARM targets, a resizable 1280x720 window on a
PC (`plat_target.vout_fullscreen` / F11 toggles). Keys are SDL scancodes named in lower case ("escape", "f1",
"eject", "reset"); pads are `sdl2gc:pad N` with fixed PlayStation button names, Select+Start = Home on a pad
without a Guide button, player 1's sticks wired to `in_adev[]` at probe time. `PCSXAB_GLES` (libpicofe's own
EGL output) is off on every target.

**`frontend/ab/`** is ours. `ab_config`: the launch script's arguments taken out of argv (`ab_args_take`),
`Bios = SET_BY_PCSX` -> `Config.Bios[US/EU] = romw.bin`, `[JP] = romJP.bin` (upstream picks by the disc's
region, HLE without the file), `card2.mcd` -> `none`, `-filter`/`-ratio` -> hwfilter and `g_scaler`
(`ab_config_loaded`, hooked at the end of `menu_load_config`); stdout unbuffered. `ab_session`: the exit
files (`ab_session_exit`, hooked after main()'s loop) - `sstates/<label>-<id>.000`,
`screenshots/<label>-<id>.png`, `lastcdimg.txt`, and last `filename.txt`, named by the disc in the drive.
`ab_buttons`: our emulator actions (`SACTION_AB_RESET` = "RESET button", `SACTION_AB_CD_CHANGE` = "CD
Change button" - the pcsx.cfg bind names, on the `reset`/`eject` keys the console's front buttons send -
`SACTION_AB_POWER_OFF`, `SACTION_AB_SNAPSHOT`), handled in `do_emu_action`'s default branch, and
`ab_frame_tick()` from `pl_frame_limit`. **Rule**: anything that touches the emulator's state runs as an
action, between CPU slices - `SaveState()` from inside a slice froze the game. `ab_autosave`: the ring
(a memory `SaveState()` + the frame every 2 s, six kept, none in the first 10 s or near a memory-card
write); Reset/Power leave the *oldest* one as the resume point, ~10 s back, written uncompressed (gzread
takes it); the menu's Exit and the window's close leave the live state. `AB_NO_AUTOSAVE=1` turns it off.
`ab_console`: the power daemon's `prepare_suspend` and `cpu_temp`/`temp_limit` watchers (inotify threads,
Linux only, ending at once without the files). `ab_disc`: the disc set (multi-disc PBP, an `.m3u`, or the
folder's images of the same kind) and the Open button through the core's lid, refused for 22 s after the
start; one press = the next disc, with a HUD line. `SaveMcd()` fsyncs and tells the ring. `ab_menu.c`:
the in-game menu (Resume, Quick save/load = slot 2, Change disc, Toggle filter, PCSX menu = upstream's
whole menu beneath, Save AutoBleem config = pcsx.cfg + a copy as `autobleem.cfg`, Exit), `#include`d into
`frontend/menu.c` like libpicofe's menu.c because the menu machinery is static there. Player 2's sticks:
`in_adev[4]` ([2]/[3]), `update_analogs()` over both players.

Upstream files edited so far (the whole list - keep it that way): `frontend/main.c` (`path_is_absolute()`
for `C:\` paths - a candidate for an upstream PR; the `ab_*` hooks: the arguments, the exit, the action
default; `PCSX_MEMCARD_COUNT` instead of a fixed nine cards, 2 here), `frontend/main.h` (the macro's
default, our four `SACTION_AB_*` values), `frontend/menu.c` (the `ab_config_loaded` hook, two action
names), `frontend/plugin_lib.c` (`ab_frame_tick()`; the analog tables at 4 and `update_analogs()` over both
players), `frontend/plugin_lib.h` (the same tables), `frontend/menu.c` also `#include`s `ab/ab_menu.c` and
runs `ab_menu_loop_d()`, `libpcsxcore/sio.c` (`ab_memcard_written()` + fsync in `SaveMcd`), `.gitignore` (`/tools/*` so a file of ours under it can be tracked). Everything
else Windows-specific is a shim: `frontend/win32/` (the host layer, `<dirent.h>` with `d_type`/`scandir`,
`win32_compat.h` force-included by CMake) and `NO_DYLIB` (upstream's own Windows recipe).

**Known**: a save state loaded within the first seconds of a **HLE** boot (`-load 1` at start, or F2 at one
second) leaves the game spinning in the HLE BIOS - with lightrec and with the interpreter alike; at 25 s the
same state loads fine. Upstream's HLE keeps state outside RAM, so a state taken later cannot be put into a
freshly booting HLE. The console and the Pi run real BIOS files, where this does not arise; a PC without one
cannot test the resume path.

## What this is built from - read first

- **`docs/port-plan.md`** - the plan: the analysis, the decisions, the eight phases. Delete it when every step
  is done and move what still matters here.
- **`docs/reference/features.md`** - every feature Sony and AutoBleem added to pcsx-ab, with line pointers
  into **`docs/reference/pcsx-ab-delta-2017.patch`** (pcsx-ab against its real upstream base, `bebe989b` of
  2017-10-17, whitespace-normalised) and **`libpicofe-delta-2015.patch`** (its libpicofe against `21604a0`).
  Port a feature from the patch and the inventory, never from the old tree.
- The AutoBleem side of the contract: `autobleem-develop`'s `CLAUDE.md`, `src/code/core/services/launch.cpp`,
  `resume_point.h`, `game_settings.cpp`, `src/resources/pcsx.cfg`, `payload/Autobleem/rc/launch.sh` and
  `payload_rpi/Autobleem/rc/launch.sh`.

## Decisions (made by the owner - do not re-ask)

- **Re-implement, do not re-apply.** The old delta is 2017-core patching; upstream r26 already has CD lid
  emulation, `SlowBoot`, the fast boot, KSEG1 decoding, CHD, an aarch64 dynarec, lightrec, a C-SIMD NEON GPU
  and a per-serial hack database. Each feature is written again against what upstream has now.
- **Per-title hacks are not ported** (131 serials, `isTitleName()` at 460 sites). Phase 7 tests the titles;
  a reproduced regression gets a `libpcsxcore/database.c` entry (or a `Config.hacks` flag), never an
  `isTitleName()` in the core.
- **Layout rule**: our behaviour lives in new files - `frontend/ab/` and `frontend/plat_sdl2.c` (+ libpicofe's
  SDL2 files in our fork) - and upstream files get hooks only: a call, an enum value, a config entry. No
  `#ifdef PSC` in `cdrom.c`. A platform difference is a runtime check or a CMake option, never a second copy
  of a file. This is what keeps `git merge upstream/master` cheap; merge at upstream release tags.
- **Video**: one SDL2 platform everywhere - window + `SDL_GL_CreateContext` (Wayland on the console, KMSDRM
  on the Pi, WGL on Windows) into libpicofe's `gl.c`, SDL_Renderer as the fallback. No hand-written Wayland
  code (Sony's `gl_platform.c` is in the reference patch if the PowerVR ever needs it back).
- **Engines per target** (CMake pins them): PSC and Pi 32-bit = Ari64 ARM + gpu_neon (NEON asm); Pi 64-bit =
  Ari64 ARM64 + gpu_neon (C SIMD); Windows and Linux = lightrec + gpu_neon (C SIMD). peops/unai stay as options.
- **The contract with AutoBleem does not change**: the run directory (`.pcsx`, `bios`, `plugins` links), the
  command line (`-filter -ratio -lang -region 4 -enter 1 [-load 1] -cdfile`), `pcsx.cfg`'s keys
  (`Bios = SET_BY_PCSX`, `SlowBoot`, the launcher's nine values), `memcards/card1.mcd` (+ `none`),
  and on exit `filename.txt` / `sstates/<name>.000` / `screenshots/<name>.png` / `lastcdimg.txt`,
  `autobleem.cfg`. The binary keeps the name `pcsx-ab` inside AutoBleem's payloads so no launcher script
  changes; About and `-v` say pcsx-abnxt.
- **Style**: upstream files keep notaz's style (tabs); ours under `frontend/ab/` follow it - no reformatting
  of upstream code, ever (merge noise). `.gitattributes` covers only our paths (33 upstream files are CRLF and
  stay so).

## Lessons carried over from pcsx-ab (still true here)

- **GCC 14 (the Pi toolchains) vs the console's GCC 6**: `-fcommon` for tentative definitions in headers and
  `.comm` in `linkage_arm.S`; keep a few `-Wno-error=` for pre-existing implicit prototypes. The console's
  binary must need nothing above **GLIBC 2.24** and no RPATH (`tools/check_psc_binary.sh` from
  autobleem-develop gates it).
- **Wayland via SDL2**: `SDL_SysWMinfo::version` must be set before `SDL_GetWindowWMInfo` - unset it read
  uninitialised stack and pcsx-ab segfaulted at its first frame on the console (pcsx-ab2 `70c5dcb`).
- **A window has a surface or a renderer, never both** (SDL >= 2.28 refuses).
- **`SDL_CONTROLLER_BUTTON_MAX` grew** (15 -> 21): an unmapped default of `0` in the GameController key map
  cleared d-pad UP on every new button; the default is `-1`.
- **`SysLibError()` must return NULL on success** - `plugins.c`'s `CheckErr` treats any non-NULL as failure.
- **The console's power/temperature files do not exist elsewhere** - the watcher threads must end at once
  when `access()` fails (pcsx-ab's `fclose(NULL)` segfault on the Pi).
- **`spu.c`'s `tanh()` without `<math.h>`** read its result from r0 on hard-float ARM.
- **CRLF**: pcsx-ab's sources are CRLF; upstream's are LF. Compare with `-w` / strip `\r` before diffing.
  This clone runs with `git config core.autocrlf false` (set once per clone): upstream's files are LF and
  must stay LF in the working copy, or every patch and every upstream merge fights the line endings.
- **`CMAKE_TRY_COMPILE_PLATFORM_VARIABLES`**: a toolchain file's own `-D` variables are invisible inside
  `try_compile` unless listed there.
- The PSC sysroot's `SDL_config.h` defines `SDL_VIDEO_DRIVER_X11` with no X11 headers; the console FindSDL2
  copies the headers with that define removed.

## Working agreements (as in autobleem-develop)

One feature branch per step, one commit per logical change, each building on every target it touches and
smoke-run where a game can prove it (Windows always; the Pi 400 and the console for their phases - the owner
at the console). Commit messages explain why, including what was deliberately not done. Vendored and upstream
code is not made warning-free. Update this file in the same commit when the state table, a decision or the
layout changes. Test material: `D:\AB\Games` (cue/bin, PBP, CHDs with CDDA), `D:\AB\Games (copy)\MDK (US)`.
