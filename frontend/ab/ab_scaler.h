/*
 * pcsx-abnxt: the software smoothing scalers (the menu's "Smoothing"), run on the PSX frame before the
 * platform scales it to the screen - see ab_scaler.c.
 *
 * (C) AutoBleem team, 2026 - GPL v2 or later, as the frontend.
 */
#ifndef AB_SCALER_H
#define AB_SCALER_H

/* the whole factor a soft_filter value (menu.h's SOFT_FILTER_*) scales by: 2, 3, or 0 for none */
int ab_soft_scale_factor(int filter);

/* Scale a w x h BGR555 frame at vram (sstride bytes between rows) by the filter's factor into dst
 * (RGB565, dstride bytes between rows). 1 when done, 0 when the filter is not one of ours (the caller
 * then blits as usual). */
int ab_soft_blit(int filter, const void *vram, int sstride, void *dst, int dstride, int w, int h);

#endif
