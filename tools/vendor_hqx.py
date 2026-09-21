# Derive frontend/ab/hqx/hq2x.c and hq3x.c (RGB565 in and out) from grom358/hqx's hq2x.c / hq3x.c
# (https://github.com/grom358/hqx, LGPL 2.1): the include, the raw-copy macros and the driver loop
# change, the pattern switch bodies stay verbatim. hqx_common.h and hqx_init.c are written by hand.
#
#   tools/vendor_hqx.py <path to a clone of hqx>/src
import os, re, sys

if len(sys.argv) != 2:
    sys.exit(__doc__ or 'usage: tools/vendor_hqx.py <hqx>/src')
SRC = sys.argv[1]
DST = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'frontend', 'ab', 'hqx')

NOTE = '''/*
 * pcsx-abnxt: hq%(n)dx over RGB565, from grom358/hqx's hq%(n)dx.c (LGPL 2.1+, see above). The pattern
 * switch is the original's; the driver reads 16-bit pixels, takes their YUV and 888 expansions from
 * 64K tables instead of the 16M-entry RGB table, and the interpolations write RGB565 (hqx_common.h).
 * Made by tools/vendor_hqx.py, not by hand - regenerate rather than patch.
 */
'''

PREAMBLE = '''void hq%(n)dx_16_16(const uint16_t *sp, uint16_t *dp, unsigned int Xres, unsigned int srb, unsigned int drb, unsigned int Yres)
{
    unsigned int i, j;
    int  k;
    int  prevline, nextline;
    uint32_t  w[10];
    uint16_t  s[10];
    int dpL = (drb >> 1);
    int spL = (srb >> 1);
    const uint8_t *sRowP = (const uint8_t *) sp;
    uint8_t *dRowP = (uint8_t *) dp;
    uint32_t yuv1, yuv2;

    //   +----+----+----+
    //   |    |    |    |
    //   | w1 | w2 | w3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w4 | w5 | w6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w7 | w8 | w9 |
    //   +----+----+----+

    for (j=0; j<Yres; j++)
    {
        if (j>0)      prevline = -spL; else prevline = 0;
        if (j<Yres-1) nextline =  spL; else nextline = 0;

        for (i=0; i<Xres; i++)
        {
            s[2] = *(sp + prevline);
            s[5] = *sp;
            s[8] = *(sp + nextline);

            if (i>0)
            {
                s[1] = *(sp + prevline - 1);
                s[4] = *(sp - 1);
                s[7] = *(sp + nextline - 1);
            }
            else
            {
                s[1] = s[2];
                s[4] = s[5];
                s[7] = s[8];
            }

            if (i<Xres-1)
            {
                s[3] = *(sp + prevline + 1);
                s[6] = *(sp + 1);
                s[9] = *(sp + nextline + 1);
            }
            else
            {
                s[3] = s[2];
                s[6] = s[5];
                s[9] = s[8];
            }

            int pattern = 0;
            int flag = 1;

            yuv1 = hqx_565_yuv[s[5]];

            for (k=1; k<=9; k++)
            {
                w[k] = hqx_565_to_888[s[k]];
                if (k==5) continue;

                if ( s[k] != s[5] )
                {
                    yuv2 = hqx_565_yuv[s[k]];
                    if (yuv_diff(yuv1, yuv2))
                        pattern |= flag;
                }
                flag <<= 1;
            }

'''

for n in (2, 3):
    src = open(os.path.join(SRC, f'hq{n}x.c'), encoding='utf-8', newline='').read().replace('\r\n', '\n')
    assert '\r' not in src
    # 1. the include
    src = src.replace('#include <stdint.h>\n#include "common.h"\n#include "hqx.h"\n',
                      '#include <stdint.h>\n#include "hqx_common.h"\n' + NOTE % {'n': n}, 1)
    # 2. the driver: from the function up to the switch
    m = re.search(rf'HQX_API void HQX_CALLCONV hq{n}x_32_rb\(.*?\n(.*?)            switch \(pattern\)\n', src, re.S)
    assert m
    src = src[:m.start()] + (PREAMBLE % {'n': n}) + '            switch (pattern)\n' + src[m.end():]
    # 3. the raw copies (the macros) write packed pixels
    src, c = re.subn(r'= w\[5\];', '= HQX_PACK(w[5]);', src)
    assert c == (4 if n == 2 else 9), c
    # 4. the tail: 16-bit row pointers, no _32 wrapper
    src = src.replace('        sp = (uint32_t *) sRowP;', '        sp = (const uint16_t *) sRowP;')
    src = src.replace('        dp = (uint32_t *) dRowP;', '        dp = (uint16_t *) dRowP;')
    m = re.search(rf'\nHQX_API void HQX_CALLCONV hq{n}x_32\(.*', src, re.S)
    assert m
    src = src[:m.start()] + '\n'
    assert 'uint32_t *) ' not in src and 'HQX_API' not in src, 'leftover'
    open(os.path.join(DST, f'hq{n}x.c'), 'w', encoding='utf-8', newline='\n').write(src)
    print(f'hq{n}x.c: {src.count(chr(10))} lines')
