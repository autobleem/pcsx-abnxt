/*
 * Memory-card writes, as far as leaving a game is concerned: sio.c's SaveMcd() says when a sector was
 * written, and a way out that lands within AB_MEMCARD_QUIET_MS of one waits until the game is done
 * (ab_buttons), so a resume point never has the game half-way through writing its card - the state and
 * the card file would disagree (Sony's memcardResetFlag).
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef PCSXAB_AB_MEMCARD_H
#define PCSXAB_AB_MEMCARD_H

#define AB_MEMCARD_QUIET_MS 2000

/* sio.c's SaveMcd() hook: a memory-card sector was just written */
void ab_memcard_written(void);
/* 1 within AB_MEMCARD_QUIET_MS of a memory-card write */
int ab_memcard_busy(void);

#endif
