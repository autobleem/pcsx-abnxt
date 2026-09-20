/*
 * The PlayStation Classic's front buttons as emulator actions, and the per-frame tick that carries the
 * console's own events (the power daemon, the CPU temperature) and the autosave ring.
 *
 *   Reset  (the AUDIOPLAY scancode, "RESET button" in pcsx.cfg)   -> the resume point from ~10 s back
 *                                                                    (ab_autosave), then exit
 *   Open   (the EJECT scancode, "CD Change button")                -> the disc change (ab_disc)
 *   Power  (/data/power/prepare_suspend appears - ab_console)      -> as Reset
 *   too hot (the power daemon's cpu_temp over its limit)           -> as Reset
 *
 * A Reset that lands while the game is writing its memory card is held until the write is over, so the
 * resume point never contains half a card (Sony's memcardResetFlag).
 *
 * Off the console there are no front buttons, and leaving a game meant the menu, Exit, Cross. So there
 * the menu button (the pad's Home, Select+Start, Escape) does both: a press opens the menu when it is
 * released, a hold of 2 s is Reset (ab_filter_action). On the console the button opens the menu at once,
 * as before - its Reset is on the front.
 *
 * Everything that touches the emulator's state (a snapshot, a disc change, the exit) runs as an emulator
 * action, between two CPU slices; the frame tick only decides and asks (ab_request_action).
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef PCSXAB_AB_BUTTONS_H
#define PCSXAB_AB_BUTTONS_H

/* do_emu_action()'s default branch: 1 when the action was one of ours */
int ab_emu_action(int action);

/* once per frame from pl_frame_limit(): the console's flags, the held Reset, the autosave ring */
void ab_frame_tick(void);

/* asks the emulator to run <action> (SACTION_*) as soon as the current slice ends, from the main thread */
void ab_request_action(int action);

/* update_input()'s emulator action, with the menu button's press/hold told apart (see the top); called
 * every frame with SACTION_NONE when nothing is pressed */
int ab_filter_action(int action);

#endif
