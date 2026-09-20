/*
 * The autosave ring: every AB_RING_INTERVAL_MS a save state is taken into memory (SaveState() through
 * memory SaveFuncs, the way the libretro build serialises) together with the frame on screen, and the
 * last AB_RING_SLOTS of them are kept. When the console's Reset or Power button ends the run, the
 * *oldest* kept snapshot becomes the resume point - the game as it was ~10 s before the player reached
 * for the button, which is what Sony's firmware did (OPT_AUTOSAVE, emu_sync_state in pcsx-ab).
 *
 * No snapshot is taken during the first AB_RING_BOOT_HOLD_MS (the BIOS screen), nor within
 * AB_RING_MEMCARD_QUIET_MS of a memory-card write (a resume from mid-write would corrupt the card).
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef PCSXAB_AB_AUTOSAVE_H
#define PCSXAB_AB_AUTOSAVE_H

#define AB_RING_INTERVAL_MS      2000
#define AB_RING_SLOTS            6		/* 10 s / 2 s + 1 */
#define AB_RING_BOOT_HOLD_MS     10000
#define AB_RING_MEMCARD_QUIET_MS 2000

/* called once per frame (from inside the CPU's slice): 1 when a snapshot is due */
int ab_autosave_due(void);
/* takes it - from the emulator-action context only, between two CPU slices, as SaveState() requires */
void ab_autosave_take(void);
/* sio.c's SaveMcd() hook: a memory-card sector was just written */
void ab_memcard_written(void);
/* 1 within AB_RING_MEMCARD_QUIET_MS of a memory-card write */
int ab_memcard_busy(void);
/* 1 when at least one snapshot is kept */
int ab_autosave_available(void);
/* the oldest kept snapshot as the state file and its picture as the PNG; 0 when both are written */
int ab_autosave_write_oldest(const char *state_path, const char *picture_path);
/* forget every snapshot (a new disc, a state loaded by hand) */
void ab_autosave_reset(void);

#endif
