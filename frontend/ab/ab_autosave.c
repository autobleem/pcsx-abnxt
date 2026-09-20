/*
 * The autosave ring - see ab_autosave.h.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../libpcsxcore/psxcommon.h"
#include "../../libpcsxcore/misc.h"
#include "../libpicofe/plat.h"
#include "../libpicofe/readpng.h"
#include "../plugin_lib.h"
#include "../main.h"
#include "ab_autosave.h"

/* what a SaveState() produces uncompressed - 4.4 MB today (libretro's retro_serialize_size), with room */
#define SNAPSHOT_BYTES (0x480000)

struct snapshot {
	unsigned char *state;
	size_t state_len;
	unsigned short *picture;
	int pic_w, pic_h;
	int valid;
};

static struct snapshot ring[AB_RING_SLOTS];
static int ring_next;		/* the slot the next snapshot goes into */
static int ring_count;		/* how many slots hold one, up to AB_RING_SLOTS */
static unsigned int last_snapshot_ms, first_tick_ms, last_memcard_ms;
static int memcard_ever;

/* --- SaveState() into memory: the same trick as frontend/libretro.c's save_open & co --------------- */

struct mem_fp {
	unsigned char *buf;
	size_t pos, size;
	int overflow;
};

static void *mem_open(const char *name, const char *mode)
{
	struct mem_fp *fp = (struct mem_fp *)name;	/* the "name" is the mem_fp itself */
	(void)mode;
	fp->pos = 0;
	fp->overflow = 0;
	return fp;
}

static int mem_read(void *file, void *buf, u32 len)
{
	struct mem_fp *fp = file;
	if (fp->pos + len > fp->size)
		return -1;
	memcpy(buf, fp->buf + fp->pos, len);
	fp->pos += len;
	return len;
}

static int mem_write(void *file, const void *buf, u32 len)
{
	struct mem_fp *fp = file;
	if (fp->pos + len > fp->size) {
		fp->overflow = 1;
		return -1;
	}
	memcpy(fp->buf + fp->pos, buf, len);
	fp->pos += len;
	return len;
}

static long mem_seek(void *file, long offs, int whence)
{
	struct mem_fp *fp = file;
	switch (whence) {
	case SEEK_CUR: fp->pos += offs; return fp->pos;
	case SEEK_SET: fp->pos = offs; return fp->pos;
	default: return -1;
	}
}

static void mem_close(void *file)
{
	(void)file;
}

static const struct PcsxSaveFuncs mem_funcs = {
	mem_open, mem_read, mem_write, mem_seek, mem_close
};

/* --- the ring ------------------------------------------------------------------------------------ */

static int snapshot_alloc(struct snapshot *s)
{
	if (s->state == NULL)
		s->state = malloc(SNAPSHOT_BYTES);
	if (s->picture == NULL)
		s->picture = malloc(1024 * 512 * 2);
	return s->state != NULL && s->picture != NULL ? 0 : -1;
}

static int take_snapshot(struct snapshot *s)
{
	struct PcsxSaveFuncs file_funcs = SaveFuncs;
	struct mem_fp fp;
	void *scr;
	int w = 0, h = 0, bpp = 0, ret;

	if (snapshot_alloc(s) != 0)
		return -1;
	fp.buf = s->state;
	fp.size = SNAPSHOT_BYTES;
	SaveFuncs = mem_funcs;
	ret = SaveState((const char *)&fp);
	SaveFuncs = file_funcs;
	if (ret != 0 || fp.overflow) {
		SysPrintf("autobleem: snapshot failed (%d, overflow %d)\n", ret, fp.overflow);
		s->valid = 0;
		return -1;
	}
	s->state_len = fp.pos;

	scr = pl_prepare_screenshot(&w, &h, &bpp);
	if (scr != NULL && bpp == 16 && w > 0 && h > 0 && w * h <= 1024 * 512) {
		memcpy(s->picture, scr, w * h * 2);
		s->pic_w = w;
		s->pic_h = h;
	} else {
		s->pic_w = s->pic_h = 0;
	}
	s->valid = 1;
	return 0;
}

int ab_autosave_due(void)
{
	static int disabled = -1;
	unsigned int now = plat_get_ticks_ms();

	if (disabled < 0)
		disabled = getenv("AB_NO_AUTOSAVE") != NULL;	/* for telling the ring's effects apart */
	if (disabled || CdromId[0] == 0)
		return 0;
	if (first_tick_ms == 0) {
		first_tick_ms = now ? now : 1;
		return 0;
	}
	if (now - first_tick_ms < AB_RING_BOOT_HOLD_MS)
		return 0;
	if (now - last_snapshot_ms < AB_RING_INTERVAL_MS)
		return 0;
	last_snapshot_ms = now;
	return !ab_memcard_busy();
}

void ab_autosave_take(void)
{
	unsigned int now = plat_get_ticks_ms();

	if (take_snapshot(&ring[ring_next]) == 0) {
		ring_next = (ring_next + 1) % AB_RING_SLOTS;
		if (ring_count < AB_RING_SLOTS)
			ring_count++;
		SysPrintf("autobleem: snapshot at %u.%us (%d kept)\n", (now - first_tick_ms) / 1000,
			(now - first_tick_ms) % 1000 / 100, ring_count);
	}
}

void ab_memcard_written(void)
{
	if (!ab_memcard_busy())
		SysPrintf("autobleem: memory card write\n");
	last_memcard_ms = plat_get_ticks_ms();
	memcard_ever = 1;
}

int ab_memcard_busy(void)
{
	return memcard_ever && plat_get_ticks_ms() - last_memcard_ms < AB_RING_MEMCARD_QUIET_MS;
}

int ab_autosave_available(void)
{
	return ring_count > 0;
}

static struct snapshot *oldest(void)
{
	int i = ring_count < AB_RING_SLOTS ? 0 : ring_next;
	return &ring[i];
}

int ab_autosave_write_oldest(const char *state_path, const char *picture_path)
{
	struct snapshot *s;
	FILE *f;
	int ret = 0;

	if (ring_count == 0)
		return -1;
	s = oldest();
	if (!s->valid)
		return -1;

	/* the raw image: LoadState reads through gzread, which takes an uncompressed file as it is */
	f = fopen(state_path, "wb");
	if (f == NULL) {
		SysPrintf("autobleem: cannot write %s\n", state_path);
		ret = -1;
	} else {
		if (fwrite(s->state, 1, s->state_len, f) != s->state_len)
			ret = -1;
		fflush(f);
		fsync(fileno(f));
		fclose(f);
	}

	if (s->pic_w > 0 && s->pic_h > 0) {
		if (writepng(picture_path, s->picture, s->pic_w, s->pic_h) != 0)
			ret = -1;
	} else
		ret = -1;

	SysPrintf("autobleem: resume point from the ring (%d kept, %u KB) %s\n", ring_count,
		(unsigned)(s->state_len / 1024), ret == 0 ? "written" : "incomplete");
	return ret;
}

void ab_autosave_reset(void)
{
	int i;
	for (i = 0; i < AB_RING_SLOTS; i++)
		ring[i].valid = 0;
	ring_next = ring_count = 0;
	last_snapshot_ms = 0;
	first_tick_ms = 0;
}
