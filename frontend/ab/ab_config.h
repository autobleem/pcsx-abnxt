/*
 * AutoBleem's command line and configuration on top of pcsx-rearmed's: what rc/launch.sh passes and what
 * the launcher writes into pcsx.cfg (see docs/port-plan.md, "The contract with AutoBleem").
 *
 *   pcsx-ab -filter F -ratio R -lang L -region N -enter E [-display D] [-load 1] [-language Name] -cdfile <image>
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef PCSXAB_AB_CONFIG_H
#define PCSXAB_AB_CONFIG_H

struct ab_options {
	int filter;	/* -filter: 1 = bilinear scaling, 0 = nearest */
	int ratio;	/* -ratio: 1 = fill the 16:9 screen, 0 = keep 4:3 */
	int lang;	/* -lang: the console UI's language, 1..13 (Sony's numbering; 13 = Japanese) */
	int region;	/* -region: accepted for the launch script's sake, the disc decides */
	int enter;	/* -enter: accepted, always 1 from the launcher */
	int display;	/* -display: accepted, unused */
	/* -language: the launcher's language by the name of its lang file (English, Polski, Chinese_Simplified...),
	 * what lang/<Name>.txt next to the emulator translates the emulator's own screens with (ab_ui.h);
	 * only pcsx-abnxt gets it - the launch scripts keep it from the classic pcsx-ab */
	char language[64];
};
extern struct ab_options ab_opts;

/* Takes AutoBleem's options out of argv before main() parses the rest (it treats an unknown argument as
 * an executable to load). Returns the new argc. */
int ab_args_take(int argc, char *argv[]);

/* After menu_load_config(is_game) parsed a config file: "Bios = SET_BY_PCSX" becomes the per-region BIOS
 * files AutoBleem's System/Bios holds (romJP.bin for Japan, romw.bin for the rest - the core then picks by
 * the disc's region, HLE when the file is missing), an unset second memory card is "none" (the launcher
 * swaps card1.mcd in and out and nothing else), and the command line's filter/ratio win over the file. */
void ab_config_loaded(int is_game);

#endif
