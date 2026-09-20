/*
 * What only the PlayStation Classic has: its power daemon. Two watcher threads (inotify), started on the
 * first frame, that end at once on any machine without the files - a Raspberry Pi, a PC.
 *
 *   /data/power/prepare_suspend    created by the daemon when the Power button is pressed (or held): the
 *                                  game has a moment to save itself before the console sleeps
 *   /dev/shm/power/cpu_temp        the SoC temperature, rewritten by the daemon; over the limit in
 *                                  /dev/shm/power/temp_limit ("CPU_AUTO_START_TEMP <millidegrees>",
 *                                  80000 without it) the game is stopped as by Reset
 *
 * (C) AutoBleem team, 2026 - after Sony's power_manage/watch_cpu_temperature in pcsx-ab
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef PCSXAB_AB_CONSOLE_H
#define PCSXAB_AB_CONSOLE_H

#define AB_POWER_DIR            "/data/power/"
#define AB_POWER_OFF_FILE       "prepare_suspend"
#define AB_CPU_TEMP_FILE        "/dev/shm/power/cpu_temp"
#define AB_CPU_TEMP_LIMIT_FILE  "/dev/shm/power/temp_limit"
#define AB_CPU_TEMP_LIMIT_KEY   "CPU_AUTO_START_TEMP"
#define AB_CPU_TEMP_LIMIT_DEFAULT 80000

/* 1 when the power daemon's files are there - this is a PlayStation Classic, with its front buttons */
int ab_console_present(void);
/* starts the watchers; 1 when the power daemon's files are there (a console), 0 elsewhere */
int ab_console_start(void);
/* set by the watcher threads, read from the main thread's frame tick */
extern volatile int ab_console_power_off_requested;
extern volatile int ab_console_overheated;

#endif
