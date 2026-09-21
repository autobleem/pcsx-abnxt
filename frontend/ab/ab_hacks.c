/*
 * Sony's per-title configuration overrides by serial - see ab_hacks.h.
 *
 * (C) AutoBleem team, 2026 - GPL v2 or later, as the frontend.
 */
#include <stdio.h>
#include <string.h>

#include "../../libpcsxcore/psxcommon.h"
#include "../../libpcsxcore/misc.h"
#include "../../plugins/dfsound/spu_config.h"
#include "../plugin_lib.h"
#include "ab_config.h"
#include "ab_hacks.h"
#include "ab_hacks_table.h"

void ab_hacks_apply(void)
{
	const struct ab_hack_entry *e = NULL;
	size_t i;

	if (!ab_opts.sonyhacks || CdromId[0] == 0)
		return;
	for (i = 0; i < sizeof(ab_hack_table) / sizeof(ab_hack_table[0]); i++) {
		if (strcmp(ab_hack_table[i].serial, CdromId) == 0) {
			e = &ab_hack_table[i];
			break;
		}
	}
	if (e == NULL) {
		fprintf(stderr, "autobleem: sony hacks: nothing for %s\n", CdromId);
		return;
	}
	if (e->flags & AB_HACK_SPU_GAUSSIAN) {
		spu_config.iUseInterpolation = 2;
		fprintf(stderr, "autobleem: sony hack %s (%s): spu interpolation gaussian\n", CdromId, e->title);
	}
	if (e->flags & AB_HACK_SPU_CUBIC) {
		spu_config.iUseInterpolation = 3;
		fprintf(stderr, "autobleem: sony hack %s (%s): spu interpolation cubic\n", CdromId, e->title);
	}
	if (e->flags & AB_HACK_SPU_NO_THREAD) {
		spu_config.iUseThread = 0;
		fprintf(stderr, "autobleem: sony hack %s (%s): spu thread off\n", CdromId, e->title);
	}
	if (e->flags & AB_HACK_NO_INTERLACE) {
		pl_rearmed_cbs.gpu_neon.allow_interlace = 0;
		fprintf(stderr, "autobleem: sony hack %s (%s): interlace off\n", CdromId, e->title);
	}
	if (e->flags & AB_HACK_NTSC) {
		Config.PsxAuto = 0;
		Config.PsxType = PSX_TYPE_NTSC;
		fprintf(stderr, "autobleem: sony hack %s (%s): region NTSC\n", CdromId, e->title);
	}
}
