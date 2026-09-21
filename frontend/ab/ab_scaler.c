/*
 * pcsx-abnxt: the software smoothing scalers - scale2x, eagle2x (libpicofe's NEON code on 32-bit ARM,
 * plain C elsewhere), hq2x and hq3x (frontend/ab/hqx, LGPL) - applied to the PSX frame in plugin_lib's
 * blit, before the platform scales the result to the screen with the hardware filter. The frame comes
 * from VRAM as BGR555; the scalers want their input in the output's format (the NEON ones copy pixels
 * as they are, hqx's tables are indexed by RGB565), so it is converted first into a scratch buffer -
 * bgr555_to_rgb565 is NEON on the console - and scaled from there. A 2D game drawn at 320x240 or
 * 320x480 is what this is for; the menu's row is off by default, none of it runs then.
 *
 * (C) AutoBleem team, 2026 - GPL v2 or later, as the frontend.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "ab_scaler.h"
#include "hqx/hqx_common.h"
#include "../menu.h"
#include "../cspace.h"
#ifdef HAVE_NEON32
#include "../libpicofe/arm/neon_scale2x.h"
#include "../libpicofe/arm/neon_eagle2x.h"
#endif

/* AdvanceMAME's Scale2x: each pixel E with B above, D left, F right, H below becomes
 *   E0 E1     E0 = D==B && B!=F && D!=H ? D : E    E1 = B==F && B!=D && F!=H ? F : E
 *   E2 E3     E2 = D==H && D!=B && H!=F ? D : E    E3 = H==F && D!=H && B!=F ? F : E */
static void scale2x_16_16_c(const uint16_t *src, uint16_t *dst, unsigned int width,
	unsigned int sstride, unsigned int dstride, unsigned int height)
{
	unsigned int x, y;

	for (y = 0; y < height; y++) {
		const uint16_t *s = (const uint16_t *)((const uint8_t *)src + y * sstride);
		const uint16_t *up = y > 0 ? (const uint16_t *)((const uint8_t *)s - sstride) : s;
		const uint16_t *dn = y + 1 < height ? (const uint16_t *)((const uint8_t *)s + sstride) : s;
		uint16_t *d0 = (uint16_t *)((uint8_t *)dst + y * 2 * dstride);
		uint16_t *d1 = (uint16_t *)((uint8_t *)d0 + dstride);
		for (x = 0; x < width; x++) {
			uint16_t B = up[x], H = dn[x], E = s[x];
			uint16_t D = x > 0 ? s[x - 1] : E, F = x + 1 < width ? s[x + 1] : E;
			d0[2 * x]     = (D == B && B != F && D != H) ? D : E;
			d0[2 * x + 1] = (B == F && B != D && F != H) ? F : E;
			d1[2 * x]     = (D == H && D != B && H != F) ? D : E;
			d1[2 * x + 1] = (H == F && D != H && B != F) ? F : E;
		}
	}
}

/* Eagle: with the 3x3 neighbourhood S T U / V C W / X Y Z of C,
 *   C0 = V==S==T ? S : C    C1 = T==U==W ? U : C
 *   C2 = V==X==Y ? X : C    C3 = W==Z==Y ? Z : C */
static void eagle2x_16_16_c(const uint16_t *src, uint16_t *dst, unsigned int width,
	unsigned int sstride, unsigned int dstride, unsigned int height)
{
	unsigned int x, y;

	for (y = 0; y < height; y++) {
		const uint16_t *s = (const uint16_t *)((const uint8_t *)src + y * sstride);
		const uint16_t *up = y > 0 ? (const uint16_t *)((const uint8_t *)s - sstride) : s;
		const uint16_t *dn = y + 1 < height ? (const uint16_t *)((const uint8_t *)s + sstride) : s;
		uint16_t *d0 = (uint16_t *)((uint8_t *)dst + y * 2 * dstride);
		uint16_t *d1 = (uint16_t *)((uint8_t *)d0 + dstride);
		for (x = 0; x < width; x++) {
			unsigned int l = x > 0 ? x - 1 : x, r = x + 1 < width ? x + 1 : x;
			uint16_t S = up[l], T = up[x], U = up[r];
			uint16_t V = s[l],  C = s[x],  W = s[r];
			uint16_t X = dn[l], Y = dn[x], Z = dn[r];
			d0[2 * x]     = (V == S && S == T) ? S : C;
			d0[2 * x + 1] = (T == U && U == W) ? U : C;
			d1[2 * x]     = (V == X && X == Y) ? X : C;
			d1[2 * x + 1] = (W == Z && Z == Y) ? Z : C;
		}
	}
}

int ab_soft_scale_factor(int filter)
{
	switch (filter) {
	case SOFT_FILTER_SCALE2X:
	case SOFT_FILTER_EAGLE2X:
	case SOFT_FILTER_HQ2X:
		return 2;
	case SOFT_FILTER_HQ3X:
		return 3;
	default:
		return 0;
	}
}

/* the frame converted to RGB565, the scalers' input; a PSX frame is at most 1024x512 */
static uint16_t *scratch;
static size_t scratch_size;
static int last_w, last_h, last_filter;

static const char *filter_name(int filter)
{
	switch (filter) {
	case SOFT_FILTER_SCALE2X: return "scale2x";
	case SOFT_FILTER_EAGLE2X: return "eagle2x";
	case SOFT_FILTER_HQ2X: return "hq2x";
	case SOFT_FILTER_HQ3X: return "hq3x";
	default: return "?";
	}
}

int ab_soft_blit(int filter, const void *vram, int sstride, void *dst, int dstride, int w, int h)
{
	size_t need = (size_t)w * h * 2;
	int y;

	if (ab_soft_scale_factor(filter) == 0 || w <= 0 || h <= 0)
		return 0;
	if (scratch_size < need) {
		free(scratch);
		scratch = malloc(need);
		scratch_size = scratch != NULL ? need : 0;
		if (scratch == NULL) {
			fprintf(stderr, "ab_scaler: no %dx%d scratch buffer\n", w, h);
			return 0;
		}
	}
	if (w != last_w || h != last_h || filter != last_filter) {
		int k = ab_soft_scale_factor(filter);
		fprintf(stderr, "ab_scaler: %s %dx%d -> %dx%d\n", filter_name(filter), w, h, w * k, h * k);
		last_w = w;
		last_h = h;
		last_filter = filter;
	}
	for (y = 0; y < h; y++)
		bgr555_to_rgb565(scratch + (size_t)y * w, (const uint8_t *)vram + (size_t)y * sstride, w);

	switch (filter) {
	case SOFT_FILTER_SCALE2X:
#ifdef HAVE_NEON32
		neon_scale2x_16_16(scratch, dst, w, w * 2, dstride, h);
#else
		scale2x_16_16_c(scratch, dst, w, w * 2, dstride, h);
#endif
		break;
	case SOFT_FILTER_EAGLE2X:
#ifdef HAVE_NEON32
		neon_eagle2x_16_16(scratch, dst, w, w * 2, dstride, h);
#else
		eagle2x_16_16_c(scratch, dst, w, w * 2, dstride, h);
#endif
		break;
	case SOFT_FILTER_HQ2X:
		hqx_init();
		hq2x_16_16(scratch, dst, w, w * 2, dstride, h);
		break;
	case SOFT_FILTER_HQ3X:
		hqx_init();
		hq3x_16_16(scratch, dst, w, w * 2, dstride, h);
		break;
	}
	return 1;
}
