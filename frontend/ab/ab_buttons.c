/*
 * The front buttons and the per-frame tick - see ab_buttons.h.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <stdio.h>
#include <string.h>

#include "../../libpcsxcore/psxcommon.h"
#include "../../libpcsxcore/r3000a.h"
#include "../libpicofe/plat.h"
#include "../main.h"
#include "ab_buttons.h"
#include "ab_autosave.h"
#include "ab_console.h"
#include "ab_debug.h"
#include "ab_session.h"
#include "ab_disc.h"

extern enum sched_action emu_action, emu_action_old;

static int reset_held;		/* a Reset that waits for a memory-card write to finish */
static int power_off_seen, overheat_seen;

#define AB_MENU_HOLD_MS   2000	/* the menu button held this long is Reset, off the console */
#define AB_MENU_HINT_MS   500	/* ...and says so on the HUD from here */
static const char hold_hint[] = "HOLD TO EXIT";

void ab_request_action(int action)
{
	/* plugin_lib's emu_set_action(), which is static there */
	if (action == SACTION_NONE)
		emu_action_old = 0;
	else if (action != emu_action_old)
		psxRegs.stop++;
	emu_action = action;
}

/* the run ends with the ring's oldest snapshot as the resume point */
static void reset_now(const char *why)
{
	SysPrintf("autobleem: %s - leaving with the resume point\n", why);
	ab_session_exit_from_ring();
	emu_core_ask_exit();
}

int ab_filter_action(int action)
{
	static int on_console = -1;
	static int held, fired;
	static unsigned int held_since;
	unsigned int now;

	if (on_console < 0)
		on_console = ab_console_present();
	if (on_console)
		return action;

	now = plat_get_ticks_ms();
	if (action == SACTION_ENTER_MENU) {
		if (!held) {
			held = 1;
			fired = 0;
			held_since = now;
		} else if (!fired && now - held_since >= AB_MENU_HOLD_MS) {
			fired = 1;
			hud_msg[0] = 0;
			return SACTION_AB_RESET;
		} else if (!fired && now - held_since >= AB_MENU_HINT_MS && hud_msg[0] == 0) {
			snprintf(hud_msg, sizeof(hud_msg), "%s", hold_hint);
			hud_new_msg = 2;
		}
		return SACTION_NONE;
	}
	if (held) {
		/* released: a press, unless the hold already went out as Reset */
		held = 0;
		if (strcmp(hud_msg, hold_hint) == 0)
			hud_msg[0] = 0;
		if (!fired)
			return SACTION_ENTER_MENU;
	}
	return action;
}

int ab_emu_action(int action)
{
	switch (action) {
	case SACTION_AB_RESET:
		if (ab_memcard_busy()) {
			SysPrintf("autobleem: Reset held, the memory card is being written\n");
			reset_held = 1;
			snprintf(hud_msg, sizeof(hud_msg), "SAVING...");
			hud_new_msg = 3;
			return 1;
		}
		reset_now("Reset");
		return 1;
	case SACTION_AB_POWER_OFF:
		reset_now("Power");
		return 1;
	case SACTION_AB_CD_CHANGE:
		ab_disc_change();
		return 1;
	case SACTION_AB_SNAPSHOT:
		ab_autosave_take();
		return 1;
	default:
		return 0;
	}
}

void ab_frame_tick(void)
{
	static int started;

	ab_debug_screen("game");	/* a frame of the game: what the debug driver reports until a menu draws */
	if (!started) {
		started = 1;
		ab_console_start();
	}

	if (ab_console_power_off_requested && !power_off_seen) {
		power_off_seen = 1;
		ab_request_action(SACTION_AB_POWER_OFF);
		return;
	}
	if (ab_console_overheated && !overheat_seen) {
		overheat_seen = 1;
		ab_request_action(SACTION_AB_RESET);
		return;
	}
	if (reset_held && !ab_memcard_busy()) {
		reset_held = 0;
		ab_request_action(SACTION_AB_RESET);
		return;
	}

	ab_disc_tick();
	/* SaveState() must run between two CPU slices, not from here inside one: as an action */
	if (ab_autosave_due())
		ab_request_action(SACTION_AB_SNAPSHOT);
}
