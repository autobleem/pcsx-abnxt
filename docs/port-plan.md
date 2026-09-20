# pcsx-abnxt - plan (2026-09-20)

AutoBleem's next PS1 emulator: **upstream PCSX-ReARMed as it is today, with everything Sony and AutoBleem
added to pcsx-ab carried over** - the console's front buttons, the resume-point contract, the in-game menu, the
filters, the disc change flow - for every platform AutoBleem runs on: the PlayStation Classic, the two Raspberry
Pi builds and the Windows dev host. It replaces `pcsx-ab` (`github.com/autobleem/pcsx-ab2`,
`E:\Programming\pcsx-rearmed-develop`), which is a 2017 core with 2018-2026 patches on top. This file is the plan;
the analysis behind it is `docs/reference/features.md` over the two patches next to it. It is deleted when every
step is done (the owner's rule for finished plans); the CLAUDE.md keeps what matters afterwards.

## What the analysis found

Done on 2026-09-20 by diffing pcsx-ab (whitespace and line endings ignored - the console SDK export is CRLF)
against every upstream commit around Sony's era, and reading the delta file by file.

**pcsx-ab's upstream base is `notaz/pcsx_rearmed` master as of 2017-10-17 (`bebe989b`)** - r22 plus 25 commits;
Sony's 2018 firmware took a snapshot, not a release. libpicofe's base is `21604a0` (2015-11-08). Against those,
pcsx-ab differs in **62 files / ~6300 diff lines** in the emulator and **14 files / ~1850 lines** in libpicofe,
plus what is only ours (the CMake build, `toolchains/`, `ci/`, `third_party/libchdr`, `frontend/win32/`,
`in_sdl2gc.c`). Upstream since then: **993 commits to r26**, the latest stable (`r26`/`r26l`, 2026-03-29; master
2026-09-09). The pieces of upstream that change the picture:

| Upstream today has | Which makes this pcsx-ab work moot |
|---|---|
| Ari64 dynarec for **aarch64** (`new_dynarec/assem_arm64.c`) and aarch64 GTE | the 64-bit Pi ran the C interpreter - "no aarch64 dynarec in this fork" |
| **lightrec** (JIT for x86 and other non-ARM) | the Windows build ran the interpreter |
| gpu_neon with a **C SIMD fallback** (`psx_gpu_simd.c`) for aarch64/x86 | Windows and the 64-bit Pi used P.E.Op.S.; the console/32-bit Pi keep real NEON |
| libchdr as a submodule, CHD in `cdriso.c` | our vendored libchdr + `handlechd`/`cdread_chd` |
| `Config.SlowBoot` (0/1/2), fast boot loading the exe, KSEG1 I/O decoding | our three 2026 commits (`377da70`, `aeac3f9`, `6dc9de2`) |
| **CD lid emulation** (`DRIVESTATE_LID_OPEN`, `SetCdOpenCaseTime`/`LidInterrupt`) and multi-disc PBP select in the menu | Sony's `CheckDiscChange` state machines (5 types, only 0/1 ever used) |
| a per-serial hack database, `libpcsxcore/database.c` (63 serials, cdr timing / gpu / analog / framerate) | the place any Sony hack that still proves necessary goes |
| `cdrom.c` rewritten (2021-2024), `psxevents.c`, `psxbios.c` HLE much improved, threaded async CD/GPU/SPU options | the 2017 timing the per-title hacks were papering over |
| `SDL` platform in libpicofe (`plat_sdl.c`, `in_sdl.c`, `sndout_sdl.c`) - still **SDL 1.2** | nothing: the SDL2 platform is ours to write (again) |

### The delta, as features

What Sony and AutoBleem added, with the decision for each. "Port" means re-implement on the new tree, not
re-apply the old diff.

**Console integration (Sony)** - `frontend/main.c` (+649), `main.h` (+55), `misc.c` (+948)

1. **Front buttons.** Open arrives as the `EJECT` key -> `SACTION_CD_CHANGE` (refused for `open_invalid_time`
   = 22 s after boot, `CheckOpenEnabled`; pcsx.cfg has 16). Reset as `AUDIOPLAY` -> `SACTION_RESET_EVENT`:
   write the resume state and its screenshot, write `filename.txt`, exit. Power: an inotify watch on
   `/data/power/prepare_suspend` (the power daemon creates it) sets `power_off_flg`, polled in every loop
   (`plugin_lib.c`, the menu waits) -> the same save-and-exit. **Port** - all three are the product.
2. **CPU temperature.** A thread watches `/dev/shm/power/cpu_temp` against `temp_limit`'s
   `CPU_AUTO_START_TEMP` (default 80000); over it -> `ERROR_CPUOVERHEAT` + the reset action (save and exit).
   **Port, console only.**
3. **The autosave ring** (`OPT_AUTOSAVE`, `emu_sync_state` + `SACTION_SYNC_STATE`): every 2 s a full state is
   serialised **into memory** (6 snapshots, 10 s), and the reset/power/overheat exit writes the *oldest*
   kept one - so the resume point is from ~10 s before the player reached for the button. Skipped for the
   first 10 s (BIOS screen) and while the memory card is being written (`memcardFlag` from `sio.c`, so a
   resume never lands mid-write; a reset pressed during a write is held - `memcardResetFlag` - until it is
   over). Sony implemented the memory serialisation as a `Mode == 2` branch in *every* subsystem's freeze
   (`sio`, `psxcounters`, `mdec`, `spu`, `gpu`, `new_dyna_freeze`, `cdrom`'s disc-change state) - ~400
   invasive lines. **Port the behaviour, not the mechanism**: upstream's `SaveState()` goes through
   `SaveFuncs` callbacks, and `frontend/libretro.c` already serialises to memory that way
   (`retro_serialize`). The ring is one frontend module.
4. **`filename.txt`** (`make_file_name`: the disc's label-id base name the launcher's `ResumePointService`
   reads), `lastcdimg.txt` (which disc the state was on), `.pcsx/screenshots/<name>.png` on exit. **Port
   exactly** - it is the contract (see below).
5. **`.sts` status files** (`save_error` -> `/…/001.sts` with an error id, `unlink` on exit): for Sony's UI.
   Nothing in AutoBleem reads them. **Drop**; log the error instead.
6. **`-lang -region -display -filter -ratio -enter` arguments**, `plat_init(isGame, enter_mode)`, the
   `lang_list` for Sony's localised PNGs. **Port** the ones `launch.sh` passes (`-filter -ratio -lang -region
   -enter -load -cdfile`); `-display` is unused.
7. **Per-title hacks: 131 serials / ~90 titles** (`title.h`, `isTitleName()` at 460 call sites across
   `cdrom.c` 119, `menu.c` 136, `misc.c` 81, `dfsound` 76, `plugin_lib.c` 31, `mdec`/`psxdma`/`psxcounters`/
   `pcsxmem`): CD read timing per sector range (Wild Arms, IQ, Crash, Arc the Lad...), SPU tempo/reverb
   per scene, XA volumes, MGS motion-JPEG frame rate (`HSyncTotal` 297, `frame_interval` 18821), Toshinden
   GPU DMA delays, RR4 MDEC, FF7's second VRAM (`vram2`), Mr Driller, the "bo"/"pmt" frame-skip command
   tables in `gpulib`/`dfxvideo` (~700 lines), the Suikoden SPU hack, the EU licence string rewrite...
   All of it is 2017-core symptom patching for the 20 built-in games. **Do not port.** Test those games
   on r26 (phase 7) and port a hack only for a reproduced regression, as an entry in upstream's
   `database.c` (or a `Config.hacks` flag), never as `isTitleName()` in the core.

**Video and input platform (Sony, then us)** - `plat_sdl.c` (+330), libpicofe `plat_sdl.c` (+454),
`in_sdl.c` (+741), `gl.c` (+95), `gl_platform.c` (+299), `menu.c` (+181)

8. Sony ported libpicofe's SDL 1.2 platform to **SDL2** (`SDL_MAJOR_VERSION == 2` branches everywhere) with
   a hand-written **Wayland + EGL** `gl_platform.c` (`wl_shell`, replacing the X11 one), the **filter**
   (`GL_LINEAR`/`GL_NEAREST` on the frame texture, `-filter`), **16:9** (`-ratio`), an RGB888 frame path,
   the menu background grab. We added `in_sdl2gc.c` (SDL2 GameController, proven on Windows and the
   console), the SDL_Renderer path for the Pi (no Wayland), the Windows layer. **Replace with one SDL2
   platform of ours** (`plat_sdl2.c`): SDL2 window everywhere, a GL/GLES2 context from `SDL_GL_CreateContext`
   (SDL does Wayland on the console, KMSDRM on the Pi, WGL on Windows) feeding libpicofe's `gl.c`, with the
   SDL_Renderer streaming-texture path kept as the fallback. No hand-written Wayland code. The launcher
   already runs SDL2 on the console's Weston through the same SDL2 2.0.12 build, so this is proven ground;
   if GL through SDL falls short on the PowerVR, the fallback is porting the old `gl_platform.c`.
9. **Two pads** (`in_adev[4]`, `in2_a1/in2_a2`, `p2_connected` -> SIO answers "no device" on port 2 when
   nothing is there). **Port** on top of `in_sdl2gc.c`.
10. `bind eject = CD Change button` / `bind reset = RESET button` in pcsx.cfg (the console's front buttons as
    key names), `SDLK_EJECT`/`SDLK_AUDIOPLAY` in the keymap. **Port** (the launcher's `Key::Open`/`Key::Reset`
    are the same scancodes).

**Menu and config (mostly AutoBleem)** - `menu.c` (+525)

11. **Our main menu** `e_menu_main3`: Port 1/2 device, Quick Save / Quick Load (slot 2), Toggle Filter,
    Change CD image, PCSX Menu (the full upstream menu under it), Save AutoBleem CFG, Exit. Entered by the
    PS button (`SDL2GC_BTN_PS` -> `SACTION_ENTER_MENU`) or Select+Start (`in_sdl2gc.c`), `Esc` on a PC.
    **Port.** `-enter 1` chooses keymap set 1 (game) over set 2 - in practice the launcher always passes 1.
12. **`autobleem.cfg`** (`menu_write_config(2)`, `MA_OPT_SAVECFG_AB`) - the launcher's `LaunchService` copies
    it into the game's `!SaveStates`. **Port.**
13. **`Bios = SET_BY_PCSX`** -> `romJP.bin` for a serial starting `SLP*`/`SCP*` (the third character `P`),
    else `romw.bin`. **Port** (every `pcsx.cfg` the launcher writes has it).
14. `config_change()`: region -> `PsxType`, interpolation, `iUseThread=0`, `gpu_peops` dither modes 2/3 +
    `iTrimJaggyFrame` (Sony's P.E.Op.S. tweaks), `allow_interlace`. **Port the config plumbing**, not the
    peops hacks - see 16.
15. Memory cards: `check_memcards` 1..2 (not 1..9), card 2 defaults to `none`, `fsync` after every card
    write (a power cut is normal on the console). **Port** - the launcher swaps `memcards/card1.mcd` in
    and out and expects nothing else touched.
16. Sony's **P.E.Op.S. GPU changes** (`dfxvideo`: dither 2/3, `CHKMAX` 1023/511, `vram2`, skip-frame
    tables) and **SPU changes** (`SPU_fadein` on resume, `RvbConfig`, `detect_pi`, `SPUResetStream`, per-
    title volumes). **Drop** with the per-title hacks; gpu_neon becomes the default GPU on every target
    (NEON where there is NEON, C SIMD elsewhere - upstream's own choice); peops and unai stay as the
    options they are upstream. The resume **fade-in** is the one SPU nicety worth keeping - as a volume
    ramp in the frontend, not inside the plugin.
17. **Ours, superseded upstream**: CHD, SlowBoot, fast boot, KSEG1 (see the table above). **Drop** ours.
18. **Ours, kept**: the CMake build with its `PCSXAB_*` options, `toolchains/`, `make_*.sh`, `ci/build.sh`
    for the Docker image, `tools/check_psc_binary.sh`, the Windows compatibility layer where upstream's
    `MMAP_WIN32`/`libpicofe/win32` does not cover it, the Wayland `wminfo.version` lesson, `-fcommon` and
    the other gcc-6/gcc-14 lessons in pcsx-ab's CLAUDE.md. **Port** as the first phase.

### The contract with AutoBleem (unchanged by this project)

`LaunchService::launchPcsx` and `rc/launch.sh` (console) / `payload_rpi/Autobleem/rc/launch.sh` (Pi):

- **Run directory** `/tmp/runpcsx` with `.pcsx` -> the game's `!SaveStates` folder, `bios` -> `System/Bios`,
  `plugins` -> `emu/plugins`. Command line: `pcsx -filter F -ratio R -lang L -region 4 -enter 1 [-load 1]
  -cdfile "<cue|pbp|chd>"`.
- **`.pcsx/pcsx.cfg`** copied from the game folder per launch; the keys the launcher edits
  (`GameSettingsService`): `Bios`, `Gpu3`, `SlowBoot`, `gpu_peops.iUseDither`, `spu_config.iUseInterpolation`,
  `scanlines`, `scanline_level`, `psx_clock`, `frameskip3`, `gpu_neon.enhancement_enable/_no_main`, `region`
  - `src/resources/pcsx.cfg` is the default it is copied from and the format upstream's `menu.c` writes
  (`config_data[]` + `bind` lines per `binddev`).
- **`.pcsx/memcards/card1.mcd`** (+ `card2.mcd` or `none`), swapped by `MemcardService`.
- **On exit**: `.pcsx/filename.txt` (line 2 = the state's base name), `.pcsx/sstates/<name>.000`,
  `.pcsx/screenshots/<name>.png`, `.pcsx/lastcdimg.txt` - what `ResumePointService` turns into slots.
  `-load 1` resumes from `sstates/<name>.000`. A game that was *killed* leaves no `filename.txt`
  (`exitedCleanly()`).
- **`.pcsx/autobleem.cfg`** - the per-game config the launcher copies in.
- **Logs**: stdout/stderr -> `System/Logs/pcsx.log` (fresh per launch), the exit status appended.

Nothing in the launcher changes for phases 0-7. What can change afterwards, as a separate feature: passing
the `.m3u` `mergeMultiDiscFolders` writes (pcsx-ab counts `.cue` files in the folder instead - keep that as
the fallback).

## Decisions (proposed - the owner confirms at step 0)

1. **Repository**: `github.com/autobleem/pcsx-abnxt`, a **GitHub fork of `notaz/pcsx_rearmed`** (keeps the
   upstream link, `gh repo fork --org autobleem --fork-name pcsx-abnxt`). A fork of a public repo cannot
   be private; the alternative is a private repo with upstream's history pushed (what `pcsx-ab2` is) and an
   `upstream` remote. Recommendation: **public fork** - the emulator is GPL-2 and its binaries ship in
   every release, so the source has to be available anyway, and upstream-able fixes can go back as PRs.
2. **libpicofe**: fork it too (`autobleem/libpicofe`) and point the submodule at our fork's `develop`, as
   upstream does with notaz's. The SDL2 platform, `in_sdl2gc.c` and the disc-change screen live there or
   under `frontend/`, and upstream libpicofe merges stay independent of the emulator's.
3. **Branches**: `master` mirrors upstream master (fast-forward only, never committed to), `develop` is ours
   (gitflow, `feature/<slug>` branches, `--no-ff` merges - the owner's rule), `upstream` remote for both.
   Merging upstream = `git merge upstream/master` into `develop`; the layout rule below is what keeps that
   cheap.
4. **Layout rule**: our behaviour lives in **new files** - `frontend/ab/` (`ab_buttons.c`, `ab_autosave.c`,
   `ab_disc.c`, `ab_console.c` (PSC only), `ab_config.c`, `ab_menu.c`) and `frontend/plat_sdl2.c` - and
   upstream files get **hooks only** (a call at the right place, an enum value, a config entry). No
   `#ifdef PSC` forests in `cdrom.c`. A platform difference is a runtime check or a CMake option, never a
   copy of a file.
5. **Per-title hacks**: not ported; phase 7 tests the titles and ports on evidence, into `database.c`.
6. **Video**: SDL2 + `SDL_GL_CreateContext` everywhere (decision 8 above), SDL_Renderer as the fallback.
7. **Build**: CMake (pcsx-ab's `CMakeLists.txt` is the model), upstream's `configure`/`Makefile` left in
   the tree untouched (they still build the tree on Linux, which is a cheap upstream-parity check).
   **pcsx-ab's `PCSXAB_*` option names stay** so `ci/build.sh` and the launcher's packaging need no
   changes beyond the directory (`AB_PCSX_DIR`).
8. **Targets and their engines** (upstream's `configure` choices, pinned in CMake):

   | Target | CPU | GPU | Notes |
   |---|---|---|---|
   | PSC (armv8-a 32-bit, gcc-6, glibc 2.24) | Ari64 ARM | gpu_neon (NEON asm) | SDL2 2.0.12 Wayland from the Docker image, GLES2; `check_psc_binary.sh` gates |
   | Pi 32-bit (armv7-a+neon) | Ari64 ARM | gpu_neon (NEON asm) | SDL2 KMSDRM |
   | Pi 64-bit (aarch64) | **Ari64 ARM64** | gpu_neon (C SIMD) | the first dynarec on this target |
   | Windows x86_64 (MinGW) | **lightrec** | gpu_neon (C SIMD) | `deps/lightning` builds with MinGW; `MMAP_WIN32` |
   | Linux native (dev, CI) | lightrec | gpu_neon (C SIMD) | what the Docker `native` target builds and smoke-runs |

## Phases

Each step is one feature branch merged `--no-ff` into `develop`, built for every target it touches, and
smoke-run where a game can prove it (Windows always; the Pi 400 and the console for their phases - the
owner at the console). The repo's `CLAUDE.md` is updated in the same commit as any change it describes.

**Phase 0 - the repositories** (the "preparation" half of this request)

1. Fork `notaz/pcsx_rearmed` -> `autobleem/pcsx-abnxt` and `notaz/libpicofe` -> `autobleem/libpicofe`; clone
   to `E:\Programming\pcsx-abnxt`, `upstream` remotes, `master` = upstream `r26` (the tag, not master - a
   release is the base; master is merged later on our terms), `develop` off it, the submodule repointed.
   The other submodules (`deps/libchdr`, `lightrec`, `lightning`, `libretro-common`, `mman`) stay as
   upstream has them.
2. Record the analysis in the new repo: this plan as `docs/port-plan.md`, the Sony/AB delta as
   `docs/reference/pcsx-ab-delta-2017.patch` (pcsx-ab vs `bebe989b`, whitespace-normalised - the diff to
   read when porting a feature, so nobody needs the old tree) and `docs/reference/features.md` (the
   inventory above with file:line pointers into that patch).
3. `CLAUDE.md` for the new repo (the decisions, the layout rule, the contract, the lessons carried from
   pcsx-ab's), `.gitattributes` (LF), `.clang-format` not applied to upstream files (they are notaz's
   style; ours under `frontend/ab/` follow it).

**Phase 1 - the build, no features** (`feature/cmake-build`) - **done 2026-09-20**: Windows plays (lightrec +
C-SIMD gpu_neon over sdl12-compat), the two Pis and the console link headless (`PCSXAB_PLATFORM=headless`,
no SDL 1.2 in any sysroot - the game smoke runs there move to phase 2), gcc-6 needed nothing.

4. `CMakeLists.txt` over r26: the core, the dynarec per arch (ari64 arm/arm64, lightrec + lightning on
   x86), gpu_neon asm/SIMD + peops + unai, dfsound, cdriso with libchdr (`deps/`, static), `MMAP_WIN32`,
   the libpicofe SDL platform as upstream has it (SDL 1.2 - only so the tree links; replaced in phase 2).
   `toolchains/{psc,rpi,rpi64,mingw}` and `make_win.sh`/`make_rpi*.sh`/`make_psc.sh`/`ci/build.sh` copied
   from pcsx-ab and adjusted. Verify: every target compiles and links in the Docker image; the console
   binary passes `check_psc_binary.sh` (GLIBC <= 2.24, GLIBCXX n/a, no RPATH); Windows and the native
   build boot a game with lightrec (SDL 1.2 window is fine for the smoke test); the Pi 400 boots one with
   the ARM dynarec. Expect gcc-6 fallout in upstream's newer C (`-std=gnu11`, statement expressions are
   fine; `_Static_assert` and `__builtin_*` are); fix in place with a comment, upstream if general.

**Phase 2 - the SDL2 platform** (`feature/sdl2-platform`) - **done 2026-09-20** on Windows (window, frame, menu,
keyboard); the Pis and the console build with it, their runs are pending (the Pi 400 over ssh, the console
with the owner).

5. `frontend/plat_sdl2.c` + libpicofe `plat_sdl2.c`/`in_sdl2.c`: window, GL/GLES2 context through SDL,
   `gl.c` for the frame (filter = texture sampling, `-ratio` = viewport), the menu's 16-bit framebuffer
   flip, fullscreen, the SDL_Renderer fallback (`PCSXAB_GLES=OFF`, the current Pi path); keyboard driver
   (`SDLK_*`, `EJECT`/`AUDIOPLAY`); `in_sdl2gc.c` moved over (two pads, `p2_connected`); `sndout_sdl.c`
   checked against SDL2 (`SDL_OpenAudio` still exists; `SDL_OpenAudioDevice` if not). Verify: Windows
   (game + menu + pad), Pi 400 (KMSDRM, both filter states, 16:9), console (Wayland through SDL2 - the
   first time; measure a frame against pcsx-ab's `gl_platform.c` path before deciding the fallback is
   not needed).

**Phase 3 - the AutoBleem contract** (`feature/ab-contract`) - **done 2026-09-20** on Windows but for the
resume load, which needs a real BIOS (see CLAUDE.md, "Known"): checked on the console/Pi with phase 2's run.

6. `frontend/ab/ab_config.c`: the arguments (6), `Bios = SET_BY_PCSX` (13), `autobleem.cfg` (12),
   `open_invalid_time`, `config_change()`'s plumbing (14), `check_memcards` + `fsync` (15), `-load 1`.
7. `frontend/ab/ab_autosave.c`, first half: the exit path - resume state + screenshot + `filename.txt` +
   `lastcdimg.txt` on `SACTION_RESET_EVENT` and on the menu's Exit (4). Verify with **the launcher** on
   Windows (the `usb/` tree): start, play, exit through the menu, the slot appears with its picture,
   resume from it, four slots, the "killed" case.

**Phase 4 - the console's buttons and the ring** (`feature/console-buttons`) - **done 2026-09-20** on Windows
(Reset/Open on the F10/F9 binds, the ring, the raw state loading back); the power daemon and the real
front buttons wait for the console. Phase 5's disc set and lid change came with it, without the picker.

8. `ab_buttons.c`: `SACTION_CD_CHANGE`/`RESET_EVENT`/`POWER_OFF` and the key binds; `ab_console.c` (PSC
   only, a CMake option): the power daemon inotify thread, the temperature thread (2), `power_off_flg`
   polled where Sony polled it. Off the console the file paths do not exist and the threads end at once
   (pcsx-ab's `fclose(NULL)` lesson).
9. `ab_autosave.c`, second half: the ring (3) over `SaveFuncs`-to-memory (from `libretro.c`), 2 s / 6
   snapshots, the BIOS-screen and memory-card guards (`sio.c` gets one hook: "a card write is in
   progress"), the oldest snapshot written on the button exits, the newest on a menu Exit. Measure the
   serialise cost on the console (Sony did it on this hardware, so it fits; the number goes in the
   CLAUDE.md). Verify on the console: reset button -> launcher shows the slot ~10 s back; power button ->
   the state is there after the reboot; a reset during a memcard save is held.

**Phase 5 - disc change** (`feature/disc-change`)

10. `ab_disc.c`: the disc set of the running game (the folder's `.cue`/`.chd`/`.pbp` list as pcsx-ab counts
    it, an `.m3u` when there is one, PBP's own multi-disc through upstream), the change through
    upstream's lid (`SetCdOpenCaseTime`/`LidInterrupt`, `swap_cd_image`), the 22 s boot grace. The screens
    (Sony's `show_text_image` / disc picker): the console draws Sony's localised PNGs from
    `/usr/sony/share/data/images/` as before (they exist on every console, `-lang` picks the language);
    elsewhere the same layout with libpicofe's font (or PNGs we ship next to the binary, later). A
    single-disc game says so; a change while the drive is busy says "not now". Verify: FF7 / MGS disc
    swaps on the console and the Pi; the launcher's `lastcdimg` slot round trip.

**Phase 6 - the menu** (`feature/ab-menu`) - **done 2026-09-20** on Windows, with player 2's sticks; the
memory-card and controller pages are upstream's as they are (under "PCSX menu").

11. `ab_menu.c`: our main menu (11) with the upstream menu beneath it, Quick Save/Load, Toggle Filter
    (live), About text, "Save AutoBleem CFG"; the PS button / Select+Start entry; the memory-card and
    controller pages trimmed to what the console has. Verify on all three.

**Phase 7 - compatibility** (`feature/compat-pass`, the owner at the console)

12. The 20 built-in games and the other titles `title.h` names, on the console (and a subset on the Pi):
    boot, the first minutes, a save, FMV, CDDA, a disc swap where there is one. A regression that
    reproduces gets a `database.c` entry (or an upstream issue with a repro - notaz fixes these). The
    pass list goes in the CLAUDE.md. Also the pcsx-ab test material on `D:\AB\Games` (CHDs with CDDA,
    the 30-track cue, PBPs).

**Phase 8 - release** (`feature/release`)

13. `ci/build.sh` in the Docker image builds pcsx-abnxt for `psc`/`rpi`/`rpi64` (`AB_PCSX_DIR`), the
    launcher's `payload/Autobleem/bin/emu/` + `payload_rpi/.../emu/` + `emu-arm64/` get the new binaries
    and plugins (same file names - `pcsx-ab` stays the binary's name inside the payload so `launch.sh`
    and `LaunchService` need no change; the About screen and `-v` say `pcsx-abnxt`), UPX as before,
    sccache. A pre-release on the site with it; the console and both Pis run it. `pcsx-ab2` is archived
    on GitHub with a README pointing here.

## Out of scope (for now)

- The libretro core build of the fork (RetroArch ships its own pcsx_rearmed; we build none).
- Sony's `UI_INTEGRATION` (dead code), the `.sts` files, the Pandora/Maemo/Caanoo platforms (left in the
  tree, never built).
- Any GPU or SPU feature beyond upstream's (enhancement mode, threaded rendering are upstream options and
  simply become available).
- Changing the launcher. The `.m3u` hand-over and a "which disc" picker in the launcher's resume menu are
  follow-ups once phase 5 works.

## Risks and what settles them

- **Upstream r26 on gcc-6 / glibc 2.24** (the console): libretro's buildbot builds this code with old
  toolchains, but not this one. Phase 1 finds out on day one.
- **GL through SDL2 on the console's Weston**: proven for the launcher's SDL_Renderer (GLES2 underneath),
  not for a raw `SDL_GL_CreateContext` + our shader-less `gl.c`. Phase 2 measures; the fallback is the old
  Wayland `gl_platform.c` (299 lines, in the reference patch).
- **The ring's serialise cost** every 2 s on the console: Sony's code did exactly this with the same
  subsystems, and upstream's state is the same order of size (~4-8 MB). Phase 4 measures.
- **The 20 games without their hacks**: the whole point of phase 7 - and the 2017 core's problems those
  hacks address have had nine years of upstream fixes since.
- **The first aarch64 dynarec on the 64-bit Pi**: upstream's, tested by the libretro crowd on aarch64
  Linux; ours only differs in the platform layer. Phase 1 runs it.
- **Keeping up with upstream**: the layout rule (decision 4) is the only real protection; a merge every
  release tag (r27...) rather than every commit.

## Estimates (working sessions, not calendar)

Phase 0: 1. Phase 1: 2-3 (the gcc-6 unknown). Phase 2: 2-3 (the console's video path is the unknown).
Phase 3: 1-2. Phase 4: 2 (console time). Phase 5: 2. Phase 6: 1. Phase 7: owner-paced, 2+ at the console.
Phase 8: 1. Roughly 15 sessions to a pre-release that replaces pcsx-ab everywhere.
