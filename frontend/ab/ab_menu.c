/*
 * AutoBleem's in-game menu: what the PlayStation Classic player sees on the Home button (pcsx-ab's
 * e_menu_main3, "Enhanced Edition by AutoBleem Team"). Quick save/load, the disc, the filter, the whole
 * upstream menu one level down, the AutoBleem config, Exit.
 *
 * Not a translation unit of its own: frontend/menu.c #includes it after its own menus, the way it includes
 * libpicofe/menu.c, because everything a menu is built from (me_loop_d, mee_*, main_menu_handler,
 * menu_loop_savestate, menu_write_config...) is static in that unit.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "ab_disc.h"
#include "ab_buttons.h"

/* our ids, past the menu.c enum's */
enum {
	MA_AB_QUICKSAVE = 1000,
	MA_AB_QUICKLOAD,
	MA_AB_DISC,
	MA_AB_FILTER,
	MA_AB_PCSX_MENU,
	MA_AB_SAVECFG,
};

#define AB_QUICK_SLOT 2		/* slot 0 is the resume point, 1 the launcher's copy of it */

static int ab_menu_handler(int id, int keys);
static int ab_menu_pcsx_handler(int id, int keys);

static const char h_ab_filter[] = "Bilinear or nearest scaling of the picture";
static const char h_ab_pcsx[]   = "PCSX-ReARMed's own menu: options, controls, cheats...";
static const char h_ab_savecfg[] = "Keeps today's settings for this game in AutoBleem";
static const char h_ab_scanlines[] = "Dark lines between the picture's rows, 1-3 rows thick;"
                                     " brightness is how dark";
/* the picture's shape as the launcher's "Widescreen" option sets it (-ratio): 4:3 in the middle of the
 * screen, or the whole 16:9 screen; the PCSX menu's "Scaler" is the full set, this is the switch */
static int ab_aspect_sel;
static const char *men_ab_aspect[] = { "4:3", "16:9 (fullscreen)", NULL };
static const char h_ab_aspect[]  = "4:3 as the PlayStation drew it, or stretched over the whole screen";
static const char h_ab_pad[]     = "Standard (digital), analog (DualShock), a gun or nothing;"
                                   " takes effect when the game goes on";

static menu_entry e_menu_ab[] =
{
	mee_label     (""),
	mee_label     (""),
	mee_handler_id("Resume game",              MA_MAIN_RESUME_GAME, main_menu_handler),
	mee_handler_id("Quick save",               MA_AB_QUICKSAVE,     ab_menu_handler),
	mee_handler_id("Quick load",               MA_AB_QUICKLOAD,     ab_menu_handler),
	mee_handler_id("Change disc",              MA_AB_DISC,          ab_menu_handler),
	mee_handler_id_h("Toggle filter",          MA_AB_FILTER,        ab_menu_handler, h_ab_filter),
	mee_enum_h    ("Screen",                   0,                   ab_aspect_sel, men_ab_aspect, h_ab_aspect),
	mee_enum_h    ("Scanlines",                MA_OPT_SCANLINES,    scanlines, men_scanlines, h_ab_scanlines),
	mee_range_h   ("Scanline brightness",      MA_OPT_SCANLINE_LEVEL, scanline_level, 0, 100, h_scanline_l),
	mee_enum_h    ("Controller 1",             0,                   in_type_sel1, men_in_type_sel, h_ab_pad),
	mee_enum_h    ("Controller 2",             0,                   in_type_sel2, men_in_type_sel, h_ab_pad),
	mee_handler_id_h("PCSX menu",              MA_AB_PCSX_MENU,     ab_menu_pcsx_handler, h_ab_pcsx),
	mee_handler_id_h("Save AutoBleem config",  MA_AB_SAVECFG,       ab_menu_handler, h_ab_savecfg),
	mee_handler_id("Exit",                     MA_MAIN_EXIT,        main_menu_handler),
	mee_end,
};

/* the config as pcsx.cfg (upstream's writer) and a copy as autobleem.cfg, which the launcher makes the
 * game's pcsx.cfg after the run */
static int ab_save_config(void)
{
	char src[MAXPATHLEN], dst[MAXPATHLEN], buf[4096];
	FILE *fi, *fo;
	size_t n;

	if (menu_write_config(0) != 0)
		return -1;
	emu_make_path(src, sizeof(src), PCSX_DOT_DIR, cfgfile_basename);
	emu_make_path(dst, sizeof(dst), PCSX_DOT_DIR, "autobleem.cfg");
	fi = fopen(src, "rb");
	if (fi == NULL)
		return -1;
	fo = fopen(dst, "wb");
	if (fo == NULL) {
		fclose(fi);
		return -1;
	}
	while ((n = fread(buf, 1, sizeof(buf), fi)) > 0)
		fwrite(buf, 1, n, fo);
	fflush(fo);
	fsync(fileno(fo));
	fclose(fo);
	fclose(fi);
	return 0;
}

static int ab_menu_handler(int id, int keys)
{
	char msg[64];
	int ret;

	switch (id)
	{
	case MA_AB_QUICKSAVE:
		if (!ready_to_go || !CdromId[0])
			break;
		ret = emu_save_state(AB_QUICK_SLOT);
		snprintf(msg, sizeof(msg), ret == 0 ? "Quick save done" : "Quick save failed");
		menu_update_msg(msg);
		if (ret == 0)
			return 1;
		break;
	case MA_AB_QUICKLOAD:
		if (!ready_to_go || !CdromId[0])
			break;
		if (emu_check_state(AB_QUICK_SLOT) != 0) {
			menu_update_msg("No quick save yet");
			break;
		}
		ret = emu_load_state(AB_QUICK_SLOT);
		snprintf(msg, sizeof(msg), ret == 0 ? "Quick save loaded" : "Quick load failed");
		menu_update_msg(msg);
		if (ret == 0)
			return 1;
		break;
	case MA_AB_DISC:
		if (!ready_to_go || !CdromId[0])
			break;
		if (ab_disc_count() <= 1) {
			menu_update_msg("This game has one disc");
			break;
		}
		/* the change goes through the same path as the Open button, as an emulator action */
		ab_request_action(SACTION_AB_CD_CHANGE);
		return 1;
	case MA_AB_FILTER:
		if (plat_target.hwfilters == NULL)
			break;
		plat_target.hwfilter = !plat_target.hwfilter;
		snprintf(msg, sizeof(msg), "Filter: %s", plat_target.hwfilters[plat_target.hwfilter]);
		menu_update_msg(msg);
		break;
	case MA_AB_SAVECFG:
		menu_update_msg(ab_save_config() == 0 ? "AutoBleem config saved" : "Failed to save the config");
		break;
	default:
		break;
	}
	return 0;
}

/* upstream's main menu, one level down; 1 when it asked to go back to the game or to exit */
static int ab_menu_pcsx_handler(int id, int keys)
{
	static int sel = 0;

	me_enable(e_menu_main, MA_MAIN_RESUME_GAME, ready_to_go);
	me_enable(e_menu_main, MA_MAIN_SAVE_STATE,  ready_to_go && CdromId[0]);
	me_enable(e_menu_main, MA_MAIN_LOAD_STATE,  ready_to_go && CdromId[0]);
	me_enable(e_menu_main, MA_MAIN_RESET_GAME,  ready_to_go);
	me_enable(e_menu_main, MA_MAIN_CHEATS,      ready_to_go && NumCheats);

	return me_loop_d(e_menu_main, &sel, NULL, draw_frame_main);
}

/* menu_loop()'s loop: our menu at the top, until the game is to go on or the run is to end */
static void ab_menu_loop_d(void)
{
	static int sel = 0;

	me_enable(e_menu_ab, MA_MAIN_RESUME_GAME, ready_to_go);
	me_enable(e_menu_ab, MA_AB_QUICKSAVE, ready_to_go && CdromId[0]);
	me_enable(e_menu_ab, MA_AB_QUICKLOAD, ready_to_go && CdromId[0]);
	me_enable(e_menu_ab, MA_AB_DISC,      ready_to_go && CdromId[0]);
	me_enable(e_menu_ab, MA_AB_FILTER,    plat_target.hwfilters != NULL);

	do {
		ab_aspect_sel = g_scaler == SCALE_FULLSCREEN;
		me_loop_d(e_menu_ab, &sel, NULL, draw_frame_main);
		/* the row is a two-way switch over g_scaler; a scaler the row cannot name (custom, 1x1) is left
		 * alone unless the row was moved */
		if (ab_aspect_sel != (g_scaler == SCALE_FULLSCREEN))
			g_scaler = ab_aspect_sel ? SCALE_FULLSCREEN : SCALE_4_3;
	} while (!ready_to_go && !g_emu_want_quit);
}
