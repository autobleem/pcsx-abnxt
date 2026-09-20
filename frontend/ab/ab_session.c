/*
 * What a run leaves behind for AutoBleem's launcher - see ab_session.h.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../../libpcsxcore/psxcommon.h"
#include "../../libpcsxcore/misc.h"
#include "../../libpcsxcore/plugins.h"
#include "../libpicofe/readpng.h"
#include "../plugin_lib.h"
#include "../main.h"
#include "ab_session.h"

static int exit_saved;

void ab_session_game_name(char *buf, int size)
{
	char trimlabel[33];
	int j;

	/* as main.c's get_gameid_filename(): the label without its trailing spaces, then the id */
	strncpy(trimlabel, CdromLabel, 32);
	trimlabel[32] = 0;
	for (j = 31; j >= 0; j--)
		if (trimlabel[j] == ' ')
			trimlabel[j] = 0;
		else
			break;
	snprintf(buf, size, "%.32s-%.9s", trimlabel, CdromId);
}

static int write_lines(const char *fname, const char *line1, const char *line2)
{
	FILE *f = fopen(fname, "w");
	if (f == NULL) {
		SysPrintf("autobleem: cannot write %s\n", fname);
		return -1;
	}
	fprintf(f, "%s\n", line1);
	if (line2 != NULL)
		fprintf(f, "%s\n", line2);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	return 0;
}

static int save_picture(const char *name)
{
	char path[MAXPATHLEN + 64], fname[MAXPATHLEN];
	void *scrbuf;
	int w, h, bpp, ret;

	scrbuf = pl_prepare_screenshot(&w, &h, &bpp);
	if (scrbuf == NULL || bpp != 16) {
		SysPrintf("autobleem: no picture for the resume point (bpp %d)\n", bpp);
		return -1;
	}
	snprintf(fname, sizeof(fname), "%s.png", name);
	emu_make_path(path, sizeof(path), SCREENSHOTS_DIR, fname);
	ret = writepng(path, scrbuf, w, h);
	if (ret != 0)
		SysPrintf("autobleem: writepng %s: %d\n", path, ret);
	return ret;
}

int ab_session_save_exit(void)
{
	char name[64], path[MAXPATHLEN + 64];
	const char *iso = GetIsoFile();
	int ret = 0;

	if (CdromId[0] == 0 || iso == NULL || iso[0] == 0) {
		SysPrintf("autobleem: no disc loaded, nothing to leave behind\n");
		return -1;
	}
	ab_session_game_name(name, sizeof(name));

	if (emu_save_state(0) != 0)
		ret = -1;
	if (save_picture(name) != 0)
		ret = -1;

	emu_make_path(path, sizeof(path), PCSX_DOT_DIR, "lastcdimg.txt");
	if (write_lines(path, iso, NULL) != 0)
		ret = -1;
	/* last: the launcher takes its presence as "the run ended cleanly" */
	emu_make_path(path, sizeof(path), PCSX_DOT_DIR, "filename.txt");
	if (write_lines(path, iso, name) != 0)
		ret = -1;

	SysPrintf("autobleem: resume point %s %s\n", name, ret == 0 ? "saved" : "incomplete");
	return ret;
}

void ab_session_exit(void)
{
	if (exit_saved)
		return;
	exit_saved = 1;
	ab_session_save_exit();
}
