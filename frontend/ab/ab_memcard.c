/*
 * Memory-card writes - see ab_memcard.h.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "../../libpcsxcore/psxcommon.h"
#include "../libpicofe/plat.h"
#include "ab_memcard.h"

static unsigned int last_write_ms;
static int written_ever;

void ab_memcard_written(void)
{
	if (!ab_memcard_busy())
		SysPrintf("autobleem: memory card write\n");
	last_write_ms = plat_get_ticks_ms();
	written_ever = 1;
}

int ab_memcard_busy(void)
{
	return written_ever && plat_get_ticks_ms() - last_write_ms < AB_MEMCARD_QUIET_MS;
}
