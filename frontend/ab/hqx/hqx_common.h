/*
 * Copyright (C) 2003 Maxim Stepin ( maxst@hiend3d.com )
 *
 * Copyright (C) 2010 Cameron Zemek ( grom@zeminvaders.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 * pcsx-abnxt: grom358/hqx's common.h for RGB565 frames. The source pixels are 16-bit, so the RGB->YUV
 * lookup is a 64K table indexed by the packed pixel (the original's 16M-entry table would be 64 MB on
 * the console), the neighbours are expanded to 888 through another 64K table for the interpolations,
 * and every interpolation packs its result back to RGB565. Alpha is not carried (the frames have none).
 */

#ifndef HQX_COMMON_H
#define HQX_COMMON_H

#include <stdlib.h>
#include <stdint.h>

#define MASK_2     0x0000FF00
#define MASK_13    0x00FF00FF
#define MASK_RGB   0x00FFFFFF

#define Ymask 0x00FF0000
#define Umask 0x0000FF00
#define Vmask 0x000000FF
#define trY   0x00300000
#define trU   0x00000700
#define trV   0x00000006

/* per RGB565 value: its YUV (Y << 16 | U << 8 | V) and its 888 expansion; hqx_init() fills them */
extern uint32_t hqx_565_yuv[65536];
extern uint32_t hqx_565_to_888[65536];

void hqx_init(void);
void hq2x_16_16(const uint16_t *src, uint16_t *dst, unsigned int width, unsigned int srcstride, unsigned int dststride, unsigned int height);
void hq3x_16_16(const uint16_t *src, uint16_t *dst, unsigned int width, unsigned int srcstride, unsigned int dststride, unsigned int height);

/* 888 (as expanded by hqx_565_to_888, or interpolated) back to RGB565 */
static inline uint16_t hqx_pack565(uint32_t c)
{
    return (uint16_t)(((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f));
}
#define HQX_PACK(c) hqx_pack565(c)

/* the YUV of an expanded neighbour: its top bits are the RGB565 it came from */
static inline uint32_t rgb_to_yuv(uint32_t c)
{
    return hqx_565_yuv[hqx_pack565(c)];
}

/* Test if there is difference in color */
static inline int yuv_diff(uint32_t yuv1, uint32_t yuv2) {
    return (( abs((int)(yuv1 & Ymask) - (int)(yuv2 & Ymask)) > trY ) ||
            ( abs((int)(yuv1 & Umask) - (int)(yuv2 & Umask)) > trU ) ||
            ( abs((int)(yuv1 & Vmask) - (int)(yuv2 & Vmask)) > trV ) );
}

static inline int Diff(uint32_t c1, uint32_t c2)
{
    return yuv_diff(rgb_to_yuv(c1), rgb_to_yuv(c2));
}

/* Interpolate functions */
static inline uint32_t Interpolate_2(uint32_t c1, int w1, uint32_t c2, int w2, int s)
{
    if (c1 == c2) {
        return c1;
    }
    return
        ((((c1 & MASK_2) * w1 + (c2 & MASK_2) * w2) >> s) & MASK_2)	+
        ((((c1 & MASK_13) * w1 + (c2 & MASK_13) * w2) >> s) & MASK_13);
}

static inline uint32_t Interpolate_3(uint32_t c1, int w1, uint32_t c2, int w2, uint32_t c3, int w3, int s)
{
    return
        ((((c1 & MASK_2) * w1 + (c2 & MASK_2) * w2 + (c3 & MASK_2) * w3) >> s) & MASK_2) +
        ((((c1 & MASK_13) * w1 + (c2 & MASK_13) * w2 + (c3 & MASK_13) * w3) >> s) & MASK_13);
}

static inline void Interp1(uint16_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1*3+c2) >> 2;
    *pc = HQX_PACK(Interpolate_2(c1, 3, c2, 1, 2));
}

static inline void Interp2(uint16_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*2+c2+c3) >> 2;
    *pc = HQX_PACK(Interpolate_3(c1, 2, c2, 1, c3, 1, 2));
}

static inline void Interp3(uint16_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1*7+c2)/8;
    *pc = HQX_PACK(Interpolate_2(c1, 7, c2, 1, 3));
}

static inline void Interp4(uint16_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*2+(c2+c3)*7)/16;
    *pc = HQX_PACK(Interpolate_3(c1, 2, c2, 7, c3, 7, 4));
}

static inline void Interp5(uint16_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1+c2) >> 1;
    *pc = HQX_PACK(Interpolate_2(c1, 1, c2, 1, 1));
}

static inline void Interp6(uint16_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*5+c2*2+c3)/8;
    *pc = HQX_PACK(Interpolate_3(c1, 5, c2, 2, c3, 1, 3));
}

static inline void Interp7(uint16_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*6+c2+c3)/8;
    *pc = HQX_PACK(Interpolate_3(c1, 6, c2, 1, c3, 1, 3));
}

static inline void Interp8(uint16_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1*5+c2*3)/8;
    *pc = HQX_PACK(Interpolate_2(c1, 5, c2, 3, 3));
}

static inline void Interp9(uint16_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*2+(c2+c3)*3)/8;
    *pc = HQX_PACK(Interpolate_3(c1, 2, c2, 3, c3, 3, 3));
}

static inline void Interp10(uint16_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*14+c2+c3)/16;
    *pc = HQX_PACK(Interpolate_3(c1, 14, c2, 1, c3, 1, 4));
}

#endif
