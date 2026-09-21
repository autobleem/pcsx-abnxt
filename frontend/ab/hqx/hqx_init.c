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

/* pcsx-abnxt: grom358/hqx's init.c over RGB565 - the two 64K tables hqx_common.h describes */

#include <stdint.h>
#include "hqx_common.h"

uint32_t hqx_565_yuv[65536];
uint32_t hqx_565_to_888[65536];

void hqx_init(void)
{
    uint32_t c, r, g, b, y, u, v;
    static int done;

    if (done)
        return;
    done = 1;
    for (c = 0; c < 65536; c++) {
        r = (c >> 11) & 0x1f; r = (r << 3) | (r >> 2);
        g = (c >> 5) & 0x3f;  g = (g << 2) | (g >> 4);
        b = c & 0x1f;         b = (b << 3) | (b >> 2);
        hqx_565_to_888[c] = (r << 16) | (g << 8) | b;
        y = (uint32_t)(0.299*r + 0.587*g + 0.114*b);
        u = (uint32_t)((int)(-0.169*r - 0.331*g + 0.5*b) + 128);   /* the original casts the negative double */
        v = (uint32_t)((int)(0.5*r - 0.419*g - 0.081*b) + 128);
        hqx_565_yuv[c] = (y << 16) + (u << 8) + v;
    }
}
