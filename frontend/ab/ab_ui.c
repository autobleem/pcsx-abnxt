/*
 * The emulator's own screens in the launcher's language - see ab_ui.h.
 *
 * (C) AutoBleem team, 2026
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "../../libpcsxcore/system.h"
#include "../main.h"
#include "ab_ui.h"

/* the keys as the launcher's English.txt has them - what lang/<Name>.txt translates */
static const char *english[AB_STR_COUNT] = {
	"Change disc",
	"Disc",
	"This game has only one disc",
	"You can't change discs now",
	"OK",
	"Select",
	"Back",
};

static char *translated[AB_STR_COUNT];
static char font_name[128];		/* |@font| from the language file, a file in skin/ or fonts/ */
static unsigned char *font_data;
static stbtt_fontinfo font;
static int font_ok, loaded;

static void take_line(char *line)
{
	char *eq = strchr(line, '=');
	char *e;
	int i;

	if (line[0] == '#' || eq == NULL)
		return;
	*eq++ = 0;
	for (e = eq + strlen(eq); e > eq && (e[-1] == '\n' || e[-1] == '\r'); e--)
		e[-1] = 0;
	if (*eq == 0)
		return;
	if (strcmp(line, "|@font|") == 0) {
		snprintf(font_name, sizeof(font_name), "%s", eq);
		return;
	}
	for (i = 0; i < AB_STR_COUNT; i++) {
		if (strcmp(line, english[i]) == 0) {
			free(translated[i]);
			translated[i] = strdup(eq);
			return;
		}
	}
}

static void read_strings(const char *language)
{
	char path[MAXPATHLEN], line[512];
	char end[160];
	FILE *f;

	if (language == NULL || language[0] == 0 || strchr(language, '/') != NULL)
		return;
	snprintf(end, sizeof(end), "lang/%s.txt", language);
	emu_make_data_path(path, end, sizeof(path));
	f = fopen(path, "r");
	if (f == NULL) {
		SysPrintf("autobleem: no %s, the emulator's screens are in English\n", path);
		return;
	}
	while (fgets(line, sizeof(line), f) != NULL)
		take_line(line);
	fclose(f);
	SysPrintf("autobleem: ui strings from %s\n", path);
}

static int load_font_file(const char *end)
{
	char path[MAXPATHLEN];
	FILE *f;
	long size;

	emu_make_data_path(path, end, sizeof(path));
	f = fopen(path, "rb");
	if (f == NULL)
		return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0 || size > 64 * 1024 * 1024) {
		fclose(f);
		return 0;
	}
	font_data = malloc(size);
	if (font_data == NULL || fread(font_data, 1, size, f) != (size_t)size) {
		fclose(f);
		free(font_data);
		font_data = NULL;
		return 0;
	}
	fclose(f);
	if (!stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0))) {
		SysPrintf("autobleem: %s is not a font stb_truetype reads\n", path);
		free(font_data);
		font_data = NULL;
		return 0;
	}
	SysPrintf("autobleem: ui font %s\n", path);
	return 1;
}

void ab_ui_load(const char *language)
{
	char end[192];

	if (loaded)
		return;
	loaded = 1;
	read_strings(language);
	if (font_name[0] != 0) {
		snprintf(end, sizeof(end), "skin/%s", font_name);
		font_ok = load_font_file(end);
		if (!font_ok) {
			snprintf(end, sizeof(end), "fonts/%s", font_name);
			font_ok = load_font_file(end);
		}
		if (!font_ok)
			SysPrintf("autobleem: the font %s the language file names is not in skin/ or fonts/\n", font_name);
	}
	if (!font_ok)
		font_ok = load_font_file("skin/ui.ttf");
	if (!font_ok)
		SysPrintf("autobleem: no ui font, the emulator's screens use the built-in font, in English\n");
}

const char *ab_ui_str(enum ab_ui_str s)
{
	if (s < 0 || s >= AB_STR_COUNT)
		return "";
	/* a translation can only be drawn with a real font: the 8x8 one is ASCII */
	if (font_ok && translated[s] != NULL)
		return translated[s];
	return english[s];
}

int ab_ui_has_font(void)
{
	return font_ok;
}

/* ---- drawing into RGB565 ---- */

static inline void blend(unsigned short *p, int r, int g, int b, int a)
{
	int dr, dg, db;

	if (a <= 0)
		return;
	if (a >= 255) {
		*p = AB_RGB565(r, g, b);
		return;
	}
	dr = (*p >> 8) & 0xf8;
	dg = (*p >> 3) & 0xfc;
	db = (*p << 3) & 0xf8;
	dr += (r - dr) * a / 255;
	dg += (g - dg) * a / 255;
	db += (b - db) * a / 255;
	*p = AB_RGB565(dr, dg, db);
}

static inline void plot(struct ab_canvas *c, int x, int y, int r, int g, int b, int a)
{
	if (x < 0 || y < 0 || x >= c->w || y >= c->h)
		return;
	blend(c->fb + y * c->pitch + x, r, g, b, a);
}

static void unpack(unsigned short rgb565, int *r, int *g, int *b)
{
	*r = (rgb565 >> 8) & 0xf8;
	*g = (rgb565 >> 3) & 0xfc;
	*b = (rgb565 << 3) & 0xf8;
}

static int utf8_next(const char **s)
{
	const unsigned char *p = (const unsigned char *)*s;
	int cp, extra, i;

	if (p[0] < 0x80) {
		cp = p[0];
		extra = 0;
	} else if ((p[0] & 0xe0) == 0xc0) {
		cp = p[0] & 0x1f;
		extra = 1;
	} else if ((p[0] & 0xf0) == 0xe0) {
		cp = p[0] & 0x0f;
		extra = 2;
	} else if ((p[0] & 0xf8) == 0xf0) {
		cp = p[0] & 0x07;
		extra = 3;
	} else {
		*s += 1;
		return '?';
	}
	for (i = 1; i <= extra; i++) {
		if ((p[i] & 0xc0) != 0x80) {
			*s += i;
			return '?';
		}
		cp = (cp << 6) | (p[i] & 0x3f);
	}
	*s += extra + 1;
	return cp;
}

int ab_ui_text_width(const char *utf8, int px)
{
	float scale, w = 0;
	const char *s = utf8;
	int cp, adv, lsb;

	if (!font_ok)
		return 0;
	scale = stbtt_ScaleForPixelHeight(&font, px);
	while ((cp = utf8_next(&s)) != 0) {
		stbtt_GetCodepointHMetrics(&font, cp, &adv, &lsb);
		w += adv * scale;
	}
	return (int)(w + 0.5f);
}

int ab_ui_text(struct ab_canvas *c, int x, int y, int align, const char *utf8, int px, unsigned short rgb565)
{
	float scale, pen;
	const char *s = utf8;
	int cp, ascent, descent, gap, width, r, g, b;

	if (!font_ok)
		return 0;
	width = ab_ui_text_width(utf8, px);
	if (align == AB_UI_CENTER)
		x -= width / 2;
	else if (align == AB_UI_RIGHT)
		x -= width;
	scale = stbtt_ScaleForPixelHeight(&font, px);
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
	unpack(rgb565, &r, &g, &b);
	pen = x;
	while ((cp = utf8_next(&s)) != 0) {
		int adv, lsb, gw, gh, gx, gy, gi, row, col;
		unsigned char *bitmap;

		gi = stbtt_FindGlyphIndex(&font, cp);
		stbtt_GetGlyphHMetrics(&font, gi, &adv, &lsb);
		bitmap = stbtt_GetGlyphBitmapSubpixel(&font, scale, scale, pen - (int)pen, 0, gi, &gw, &gh, &gx, &gy);
		if (bitmap != NULL) {
			int top = y + (int)(ascent * scale + 0.5f) + gy;
			int left = (int)pen + gx;
			for (row = 0; row < gh; row++)
				for (col = 0; col < gw; col++)
					plot(c, left + col, top + row, r, g, b, bitmap[row * gw + col]);
			stbtt_FreeBitmap(bitmap, NULL);
		}
		pen += adv * scale;
	}
	return width;
}

/* coverage of a point at distance d from an edge at radius r: 1 inside, 0 outside, a pixel wide ramp */
static inline float edge(float r, float d)
{
	float v = r + 0.5f - d;
	return v < 0 ? 0 : v > 1 ? 1 : v;
}

void ab_ui_disc(struct ab_canvas *c, int cx, int cy, int r, int accent, int dim)
{
	float hole = r * 0.22f, ring = r * 0.30f;
	int x, y;

	for (y = -r - 1; y <= r + 1; y++) {
		for (x = -r - 1; x <= r + 1; x++) {
			float d = sqrtf((float)(x * x + y * y));
			float cov = edge((float)r, d) * (1 - edge(hole, d));
			float t, shine;
			int cr, cg, cb;

			if (cov <= 0)
				continue;
			/* the data area: light silver, lighter towards the hub, a soft sheen across it */
			t = (d - hole) / (r - hole);
			if (t < 0)
				t = 0;
			shine = 0.5f + 0.5f * cosf(atan2f((float)y, (float)x) * 2 + 0.8f);
			cr = cg = cb = (int)(232 - 42 * t + 18 * shine);
			if (accent) {
				/* the disc in the drive: AutoBleem's cyan on it */
				cr = (int)(cr * 0.45f);
				cg = (int)(cg * 0.85f);
			}
			/* the clear ring around the hub */
			if (d < ring) {
				cr = (cr * 3 + 60) / 4;
				cg = (cg * 3 + 70) / 4;
				cb = (cb * 3 + 80) / 4;
			}
			/* the rim */
			if (d > r - 2.5f) {
				cr = cr * 2 / 3;
				cg = cg * 2 / 3;
				cb = cb * 2 / 3;
			}
			if (dim) {
				cr = cr * 11 / 20;
				cg = cg * 11 / 20;
				cb = cb * 11 / 20;
			}
			plot(c, cx + x, cy + y, cr, cg, cb, (int)(cov * 255));
		}
	}
}

void ab_ui_ring(struct ab_canvas *c, int cx, int cy, int r, int t, unsigned short rgb565)
{
	int x, y, cr, cg, cb, lim = r + t;
	float half = t / 2.0f;

	unpack(rgb565, &cr, &cg, &cb);
	for (y = -lim; y <= lim; y++) {
		for (x = -lim; x <= lim; x++) {
			float d = fabsf(sqrtf((float)(x * x + y * y)) - r);
			float cov = edge(half, d);
			if (cov > 0)
				plot(c, cx + x, cy + y, cr, cg, cb, (int)(cov * 255));
		}
	}
}

/* an anti-aliased line of thickness t from (x0, y0) to (x1, y1) */
static void line(struct ab_canvas *c, float x0, float y0, float x1, float y1, float t, int cr, int cg, int cb)
{
	float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
	int xmin = (int)floorf((x0 < x1 ? x0 : x1) - t), xmax = (int)ceilf((x0 > x1 ? x0 : x1) + t);
	int ymin = (int)floorf((y0 < y1 ? y0 : y1) - t), ymax = (int)ceilf((y0 > y1 ? y0 : y1) + t);
	int x, y;

	for (y = ymin; y <= ymax; y++) {
		for (x = xmin; x <= xmax; x++) {
			float u = len2 > 0 ? ((x - x0) * dx + (y - y0) * dy) / len2 : 0;
			float px, py, d, cov;
			if (u < 0)
				u = 0;
			if (u > 1)
				u = 1;
			px = x0 + u * dx;
			py = y0 + u * dy;
			d = sqrtf((x - px) * (x - px) + (y - py) * (y - py));
			cov = edge(t / 2, d);
			if (cov > 0)
				plot(c, x, y, cr, cg, cb, (int)(cov * 255));
		}
	}
}

void ab_ui_cross(struct ab_canvas *c, int cx, int cy, int r, unsigned short rgb565)
{
	int cr, cg, cb;
	float a = r * 0.62f, t = r * 0.22f;

	unpack(rgb565, &cr, &cg, &cb);
	ab_ui_ring(c, cx, cy, r, r * 0.18f > 1 ? (int)(r * 0.18f) : 1, rgb565);
	line(c, cx - a, cy - a, cx + a, cy + a, t, cr, cg, cb);
	line(c, cx - a, cy + a, cx + a, cy - a, t, cr, cg, cb);
}

void ab_ui_circle(struct ab_canvas *c, int cx, int cy, int r, unsigned short rgb565)
{
	ab_ui_ring(c, cx, cy, r, r * 0.18f > 1 ? (int)(r * 0.18f) : 1, rgb565);
	ab_ui_ring(c, cx, cy, (int)(r * 0.55f), r * 0.2f > 1 ? (int)(r * 0.2f) : 1, rgb565);
}
