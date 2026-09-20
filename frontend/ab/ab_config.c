/*
 * AutoBleem's command line and configuration - see ab_config.h.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../libpcsxcore/psxcommon.h"
#include "../../libpcsxcore/sio.h"
#include "../libpicofe/plat.h"
#include "../menu.h"
#include "../main.h"
#include "ab_config.h"

/* the BIOS files AutoBleem keeps in System/Bios (the launcher's launch.sh links it as bios/) */
#define AB_BIOS_WORLD "romw.bin"
#define AB_BIOS_JAPAN "romJP.bin"
/* what the launcher writes into pcsx.cfg's Bios key */
#define AB_BIOS_SET_BY_PCSX "SET_BY_PCSX"

struct ab_options ab_opts = {
	.filter = 0,
	.ratio = 0,
	.lang = 2,	/* English (US) */
	.region = 4,
	.enter = 1,
	.display = 1,
};

static int take_int(const char *name, int *value, int lo, int hi, int argc, char *argv[], int *i)
{
	int v;

	if (strcmp(argv[*i], name) != 0)
		return 0;
	if (*i + 1 >= argc) {
		fprintf(stderr, "%s needs a value\n", name);
		return 1;
	}
	v = atoi(argv[++*i]);
	if (v < lo || v > hi) {
		fprintf(stderr, "%s %d is out of range (%d..%d), ignored\n", name, v, lo, hi);
		return 1;
	}
	*value = v;
	return 1;
}

int ab_args_take(int argc, char *argv[])
{
	int i, out = 1;

	/* the log is read after a crash or a kill more often than not; line buffering is full buffering on
	 * the Windows CRT, so unbuffered it is (the console's AB_*.txt logs are the same) */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	for (i = 1; i < argc; i++) {
		if (take_int("-filter", &ab_opts.filter, 0, 1, argc, argv, &i)
		    || take_int("-ratio", &ab_opts.ratio, 0, 1, argc, argv, &i)
		    || take_int("-lang", &ab_opts.lang, 1, 13, argc, argv, &i)
		    || take_int("-region", &ab_opts.region, 1, 4, argc, argv, &i)
		    || take_int("-enter", &ab_opts.enter, 0, 2, argc, argv, &i)
		    || take_int("-display", &ab_opts.display, 0, 1, argc, argv, &i))
			continue;
		argv[out++] = argv[i];
	}
	argv[out] = NULL;
	printf("autobleem: filter=%d ratio=%d lang=%d region=%d\n",
		ab_opts.filter, ab_opts.ratio, ab_opts.lang, ab_opts.region);
	return out;
}

void ab_config_loaded(int is_game)
{
	if (strcmp(Config.Bios[0], AB_BIOS_SET_BY_PCSX) == 0) {
		snprintf(Config.Bios[PSX_REGION_US], sizeof(Config.Bios[0]), "%s", AB_BIOS_WORLD);
		snprintf(Config.Bios[PSX_REGION_EU], sizeof(Config.Bios[0]), "%s", AB_BIOS_WORLD);
		snprintf(Config.Bios[PSX_REGION_JP], sizeof(Config.Bios[0]), "%s", AB_BIOS_JAPAN);
	}

	/* the default card2.mcd: AutoBleem's memory-card sets are one card, swapped in as card1.mcd */
	if (strstr(Config.Mcd2, "card2.mcd") != NULL) {
		strcpy(Config.Mcd2, "none");
		LoadMcds(Config.Mcd1, Config.Mcd2);
	}

	/* the launcher's per-launch choices beat the file's */
	if (plat_target.hwfilters != NULL)
		plat_target.hwfilter = ab_opts.filter ? 0 : 1;	/* hwfilters[] = { "linear", "nearest" } */
	g_scaler = ab_opts.ratio ? SCALE_FULLSCREEN : SCALE_4_3;
}
