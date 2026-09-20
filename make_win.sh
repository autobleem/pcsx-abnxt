#!/usr/bin/env bash
# Windows development build with MSYS2 UCRT64 (gcc, cmake, ninja, and the mingw-w64-ucrt-x86_64-{libpng,zlib,
# sdl12-compat,SDL2} packages), the same way AutoBleem's make_win.sh works. The PC build runs games for real:
# the lightrec dynarec and the C-SIMD NEON GPU (upstream's own x86 configuration).
#
# Until the SDL2 platform exists (phase 2 of the port) the frontend is upstream's SDL 1.2 one, built
# over sdl12-compat: its YUV-overlay video path stays black there, so run with a .pcsx/pcsx.cfg that says
# "plat_target.vout_method = 0" (the plain surface path draws).
#
# Run from an MSYS2 UCRT64 shell, or:
#   C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/pcsx-abnxt && ./make_win.sh"
set -e
cd "$(dirname "$0")"
mkdir -p build_win
cmake -G Ninja -S . -B build_win -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build_win

# the MSYS2 runtime DLLs next to the exe, so it also starts outside an MSYS2 shell (Explorer, a debugger);
# sdl12-compat's SDL.dll loads SDL2.dll at run time, which ldd cannot see
for dll in $(ldd build_win/pcsx-ab.exe | awk '/ucrt64/ {print $3}') /ucrt64/bin/SDL2.dll; do
    cp -u "$dll" build_win/
done
# the menu's skin, looked up next to the executable
mkdir -p build_win/skin build_win/lang
cp -u frontend/pandora/skin/* frontend/ab/skin/* build_win/skin/
# the emulator's own screens in the launcher's languages (frontend/ab/ab_ui.h)
cp -u frontend/ab/lang/*.txt build_win/lang/
echo "==> build_win/pcsx-ab.exe"
