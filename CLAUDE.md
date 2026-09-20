# pcsx-abnxt - CLAUDE.md

AutoBleem's next PS1 emulator: **upstream PCSX-ReARMed (notaz) with what Sony and AutoBleem added to pcsx-ab
re-implemented on top**, for the PlayStation Classic, the two Raspberry Pi builds and the Windows dev host.
Author: screemer (the repo owner). It replaces `pcsx-ab` (`github.com/autobleem/pcsx-ab2`,
`E:\Programming\pcsx-rearmed-develop` - a 2017 core with patches), which AutoBleem
(`E:\Programming\autobleem-develop`) still ships until phase 8 of the plan. This file is the project knowledge
of record; keep it current in the same commit as any change it describes. Git history has the reasoning per
change (commit messages are prose).

## State (2026-09-20)

Phases 0 and 1 of `docs/port-plan.md` are done: the repositories exist and the CMake build produces the
emulator for every target - upstream r26 untouched but for one portability helper.

| | |
|---|---|
| Repository | `github.com/autobleem/pcsx-abnxt`, a **public GitHub fork** of `notaz/pcsx_rearmed` (GPL-2; a fork of a public repo cannot be private - the owner's call, 2026-09-20) |
| Base | upstream tag **`r26`** (2026-03-29, `56fef013`), the latest stable; `develop` starts there |
| Branches | `master` mirrors upstream master (fast-forward only, never committed to); `develop` is ours; `feature/<slug>` off `develop`, merged `--no-ff` (gitflow, as in every AutoBleem repo); `upstream` remote = notaz |
| libpicofe | submodule `frontend/libpicofe` -> **`github.com/autobleem/libpicofe`** (our fork of notaz's), branch `develop` at the commit r26 pins (`dd11f2d`), `upstream` remote there too. The other submodules (`deps/libchdr`, `lightrec`, `lightning`, `libretro-common`, `mman`, `frontend/warm`) are upstream's, untouched |
| Build | `CMakeLists.txt` (phase 1, 2026-09-20): upstream's `configure`/`Makefile` as CMake options (`PCSXAB_*`), the plugins, libchdr/lightrec/lightning/mman compiled from `deps/`; upstream's own build files stay untouched |
| Windows | `./make_win.sh` -> `build_win/pcsx-ab.exe`: **lightrec + C-SIMD gpu_neon, plays games** (Crash Bandicoot's intro renders, HLE BIOS) over upstream's SDL 1.2 frontend on sdl12-compat - use `plat_target.vout_method = 0` in `.pcsx/pcsx.cfg`, the overlay path is black there |
| Pi 32-bit / 64-bit | `./make_rpi.sh`, `./make_rpi64.sh` -> `build_rpi*/dist/`: Ari64 ARM / ARM64 dynarec, NEON asm / C-SIMD GPU - **link, headless** (no SDL 1.2 in any sysroot; the window comes with phase 2), unrun |
| PlayStation Classic | `ci/build.sh psc` in the Docker image (gcc-6, `/opt/psc`): links **headless**, GLIBC <= 2.12, no RPATH, ARM dynarec + NEON - the phase-1 unknown settled; `make_psc.sh` is the Sony-toolchain path over ssh, untested here |
| Local checkout | `E:\Programming\pcsx-abnxt` |

Upstream files edited so far (the whole list - keep it that way): `frontend/main.c` gained `path_is_absolute()`
for `C:\...` paths on Windows (two call sites) - a candidate for an upstream PR. Everything else Windows-
specific is a shim: `frontend/win32/` (the host layer, `<dirent.h>` with `d_type`/`scandir`,
`win32_compat.h` force-included by CMake) and `NO_DYLIB` (upstream's own Windows recipe).

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
