/* POSIX bits the MinGW development build lacks, force-included (-include) into every C source by the
 * CMake build on Windows, so no upstream file needs a #ifdef for it. Kept small on purpose: no
 * <windows.h> here (its macros collide with the emulator's names).
 *   mkdir(path, mode)  the CRT's _mkdir takes no mode; the same macro libpicofe/posix.h uses, so both
 *                      may be seen by one file
 *   SIGPIPE            signal(SIGPIPE, ...) on the CRT just returns SIG_ERR for an unknown signal
 *   <sys/stat.h>       fstat() in cdriso.c, which upstream includes on POSIX only
 *   fsync, strcasestr  frontend/win32/plat_win32.c
 * (dlopen is not needed: the Windows build is NO_DYLIB, upstream's own recipe - the plugins are built in.) */
#ifndef PCSXAB_WIN32_COMPAT_H
#define PCSXAB_WIN32_COMPAT_H
#ifdef _WIN32

#include <direct.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef mkdir
#define mkdir(pathname, mode) mkdir(pathname)
#endif
#ifndef SIGPIPE
#define SIGPIPE 13
#endif

int fsync(int fd);
char *strcasestr(const char *haystack, const char *needle);

#endif
#endif
