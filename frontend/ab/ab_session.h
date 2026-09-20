/*
 * What a run leaves behind for AutoBleem's launcher (ResumePointService reads them from the game's
 * !SaveStates folder, which is .pcsx/ here):
 *
 *   .pcsx/sstates/<label>-<id>.000       the resume state (save slot 0)
 *   .pcsx/screenshots/<label>-<id>.png   its picture
 *   .pcsx/filename.txt                   line 1 the disc image, line 2 the "<label>-<id>" base name
 *   .pcsx/lastcdimg.txt                  the disc image in the drive when the run ended
 *
 * A run that was killed leaves no filename.txt, which is how the launcher tells a clean exit from a
 * crash. Written on every way out while a disc is loaded: the menu's Exit and the window's close button
 * leave the game as it is; the console's Reset and Power buttons (ab_buttons) leave the autosave ring's
 * oldest snapshot, ~10 s back (ab_session_exit_from_ring). The "<label>-<id>" name is the disc in the
 * drive at the end: the launcher records it with the disc image and starts the resume on that disc.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef PCSXAB_AB_SESSION_H
#define PCSXAB_AB_SESSION_H

/* the "<label>-<id>" base name of the run's files, as main.c derives it for the save states; "" without
 * a disc */
const char *ab_session_game_name(void);

/* the next ab_session_exit() takes the resume point from the autosave ring instead of the live state */
void ab_session_exit_from_ring(void);

/* the resume state, its picture and the two text files; 0 when all of them are written */
int ab_session_save_exit(void);

/* main()'s way out: ab_session_save_exit() once, whichever path led here */
void ab_session_exit(void);

#endif
