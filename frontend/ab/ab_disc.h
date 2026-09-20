/*
 * The disc set of the running game and the Open button: which images belong to the game (the discs of a
 * multi-disc PBP, an .m3u next to the image, or the game folder's own images of the same kind) and the
 * change from one to the next through the core's CD lid (SetCdOpenCaseTime/LidInterrupt, as upstream's
 * menu swaps discs).
 *
 * The Open button is refused for AB_OPEN_GRACE_S after the run starts (Sony's open_invalid_time: a
 * game does not survive a lid opened while it boots), and says so on the HUD. Today one press moves to
 * the next disc and says which one; the disc picker screen is phase 5's.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef PCSXAB_AB_DISC_H
#define PCSXAB_AB_DISC_H

#define AB_OPEN_GRACE_S 22
#define AB_DISC_MAX 8

/* the Open button: the next disc of the set, or a message */
void ab_disc_change(void);
/* once per frame: learns the set when the disc id becomes known */
void ab_disc_tick(void);
/* how many discs the set has (1 = no change possible), and which is in the drive (0-based) */
int ab_disc_count(void);
int ab_disc_current(void);

#endif
