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
#include "ab_console.h"
#include "ab_ui.h"

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
static int ab_disc_screen(void);

static const char h_ab_filter[] = "Off = plain pixels, Linear = smoothed, Sharp = crisp pixels without shimmer";
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
	mee_handler_id_h("Filter",                 MA_AB_FILTER,        ab_menu_handler, h_ab_filter),
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
		/* the same screens the Open button shows; a disc put in resumes the game */
		if (ab_disc_screen())
			return 1;
		break;
	case MA_AB_FILTER:
		if (plat_target.hwfilters == NULL)
			break;
		plat_target.hwfilter++;
		if (plat_target.hwfilters[plat_target.hwfilter] == NULL)
			plat_target.hwfilter = 0;
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
/* ---- the disc picker (docs/port-plan.md, phase 5) ----
 *
 * Drawn on the menu's canvas over the darkened last frame, the way the menus are, at a 1280x720 design
 * scaled to the canvas: the title, the set's discs in a row (the one in the drive in AutoBleem's cyan,
 * the focused one bright with a ring, the rest dimmed), "Disc n" under each, and Cross/Circle hints at
 * the bottom. The text is the launcher's language through ab_ui (the built-in 8x8 font, in English, when
 * there is no ui font). Sony's picker started on the next disc and so does this one: Open, Cross is the
 * common case. */

static const unsigned short ab_col_text   = AB_RGB565(0xf4, 0xf6, 0xf8);
static const unsigned short ab_col_dim    = AB_RGB565(0x9a, 0xa4, 0xb2);
static const unsigned short ab_col_accent = AB_RGB565(0x4f, 0xc3, 0xf7);

static struct ab_canvas ab_canvas(void)
{
	struct ab_canvas c = { g_menuscreen_ptr, g_menuscreen_w, g_menuscreen_h, g_menuscreen_pp };
	return c;
}

static int ab_text_width(const char *s, int px)
{
	return ab_ui_has_font() ? ab_ui_text_width(s, px) : (int)strlen(s) * me_mfont_w;
}

static void ab_text(struct ab_canvas *c, int x, int y, int align, const char *s, int px, unsigned short col)
{
	int w;

	if (ab_ui_has_font()) {
		ab_ui_text(c, x, y, align, s, px, col);
		return;
	}
	w = ab_text_width(s, px);
	if (align == AB_UI_CENTER)
		x -= w / 2;
	else if (align == AB_UI_RIGHT)
		x -= w;
	text_out16(x, y + (px - me_mfont_h) / 2, "%s", s);
}

/* "(x) Select   (o) Back" centred near the bottom; back == NULL for a plain "(x) OK" */
static void ab_footer(struct ab_canvas *c, const char *ok, const char *back)
{
	float s = c->h / 720.0f;
	int px = (int)(24 * s), r = (int)(13 * s), gap = (int)(12 * s), sep = (int)(48 * s);
	int y = (int)(c->h * 0.86f), x, w1, w2 = 0, total;

	w1 = ab_text_width(ok, px);
	if (back != NULL)
		w2 = ab_text_width(back, px);
	total = 2 * r + gap + w1 + (back != NULL ? sep + 2 * r + gap + w2 : 0);
	x = (c->w - total) / 2;
	ab_ui_cross(c, x + r, y + px / 2, r, ab_col_accent);
	ab_text(c, x + 2 * r + gap, y, AB_UI_LEFT, ok, px, ab_col_text);
	if (back != NULL) {
		x += 2 * r + gap + w1 + sep;
		ab_ui_circle(c, x + r, y + px / 2, r, ab_col_accent);
		ab_text(c, x + 2 * r + gap, y, AB_UI_LEFT, back, px, ab_col_text);
	}
}

static void ab_draw_disc_picker(int n, int cur, int sel)
{
	struct ab_canvas c;
	float s;
	int r, gap, step, x0, cy, i;
	char label[80];

	menu_draw_begin(1, 1);
	c = ab_canvas();
	s = c.h / 720.0f;
	ab_text(&c, c.w / 2, (int)(c.h * 0.15f), AB_UI_CENTER, ab_ui_str(AB_STR_CHANGE_DISC), (int)(40 * s), ab_col_text);

	r = (int)(62 * s);
	gap = (int)(58 * s);
	step = 2 * r + gap;
	x0 = (c.w - (n * 2 * r + (n - 1) * gap)) / 2 + r;
	cy = (int)(c.h * 0.50f);
	for (i = 0; i < n; i++) {
		int cx = x0 + i * step;
		ab_ui_disc(&c, cx, cy, r, i == cur, i != sel);
		if (i == sel)
			ab_ui_ring(&c, cx, cy, r + (int)(9 * s), (int)(4 * s) > 1 ? (int)(4 * s) : 1, ab_col_accent);
		snprintf(label, sizeof(label), "%s %d", ab_ui_str(AB_STR_DISC), i + 1);
		ab_text(&c, cx, cy + r + (int)(20 * s), AB_UI_CENTER, label, (int)(26 * s),
			i == sel ? ab_col_text : ab_col_dim);
	}
	ab_footer(&c, ab_ui_str(AB_STR_SELECT), ab_ui_str(AB_STR_BACK));
	menu_draw_end();
}

static void ab_draw_message(const char *msg)
{
	struct ab_canvas c;
	float s;

	menu_draw_begin(1, 1);
	c = ab_canvas();
	s = c.h / 720.0f;
	ab_text(&c, c.w / 2, (int)(c.h * 0.44f), AB_UI_CENTER, msg, (int)(34 * s), ab_col_text);
	ab_footer(&c, ab_ui_str(AB_STR_OK), NULL);
	menu_draw_end();
}

/* the buttons that got us here are not the screen's */
static void ab_wait_released(void)
{
	while (in_menu_wait_any(NULL, 50) & (PBTN_MOK|PBTN_MBACK|PBTN_MENU)) {
		if (ab_console_power_off_requested)
			break;
	}
}

static void ab_message_screen(const char *msg)
{
	int inp;

	ab_draw_message(msg);
	ab_wait_released();
	for (;;) {
		inp = in_menu_wait(PBTN_MOK|PBTN_MBACK|PBTN_MENU, NULL, 70);
		if (ab_console_power_off_requested || (inp & (PBTN_MOK|PBTN_MBACK|PBTN_MENU)))
			break;
		if (inp & PBTN_RDRAW)
			ab_draw_message(msg);
	}
}

/* in the menu's context; 1 when a disc went in (the game should go on), 0 otherwise */
static int ab_disc_screen(void)
{
	int n, cur, sel, inp;

	ab_ui_load(ab_opts.language);
	n = ab_disc_count();
	cur = ab_disc_current();
	if (!ab_disc_can_change() || n == 0) {
		ab_message_screen(ab_ui_str(AB_STR_NOT_NOW));
		return 0;
	}
	if (n <= 1) {
		ab_message_screen(ab_ui_str(AB_STR_ONE_DISC));
		return 0;
	}
	sel = (cur + 1) % n;
	ab_draw_disc_picker(n, cur, sel);
	ab_wait_released();
	for (;;) {
		inp = in_menu_wait(PBTN_LEFT|PBTN_RIGHT|PBTN_MOK|PBTN_MBACK|PBTN_MENU, NULL, 70);
		if (ab_console_power_off_requested || (inp & (PBTN_MBACK|PBTN_MENU)))
			return 0;
		if (inp & PBTN_MOK) {
			if (sel == cur)
				return 0;	/* the disc that is in already: nothing to do */
			return ab_disc_insert(sel) == 0;
		}
		if ((inp & PBTN_LEFT) && sel > 0)
			sel--;
		if ((inp & PBTN_RIGHT) && sel < n - 1)
			sel++;
		ab_draw_disc_picker(n, cur, sel);
	}
}

/* the Open button's path: from the running game and back to it (ab_disc_change) */
void ab_menu_change_disc(void)
{
	menu_leave_emu();
	in_set_config_int(0, IN_CFG_BLOCKING, 1);
	ab_disc_screen();
	ab_wait_released();
	in_set_config_int(0, IN_CFG_BLOCKING, 0);
	menu_prepare_emu();
}

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
