//
// printerfont 12x24rk.pcf jiskan24.pcf jiskan16.pcf ter-u24b.bdf unifont-15.0.01.bdf
//
// TODO:
// 8x16 ascii: missing F1-FD
// 12x24 ascii: missing 80-9F, E0-FF
// 16x16 kanji:
//     plane 22 additional chars line 30
//     plane 23-28 some additional chars
//     plane 2D missing some line 30, line 70 missing all
//     kanji planes ok, some additional chars
// 24x24 kanji: ok
//
// 12x24rk.pcf uses JISX0201.1976
// jiskan24 and jiskan16 kanji only
//
// Copyright
// 12x24rk.pcf: Copyright 1989 by Sony Corp.
//				Attrib license
// jiskan16.pcf, jiskan24.pcf: Licensed under Public Domain
// GNU unifont: GPL
// Terminus: SIL Open Font License, Version 1.1
//
#include "fontutil.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftbdf.h>

#include <locale.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>
#include <stdint.h>
#include <vector>
#include <cassert>

static FT_Library library;
static FT_Face face;

// L"▁▂▃▄▅▆▇█▏▎▍▌▋▊▉┼┴┬┤├▔─│▕┌┐└┘╭╮╰╯"
const wchar_t katakana80[33] = L"\u2581\u2582\u2583\u2584\u2585\u2586\u2587\u2588\u258F\u258E\u258D\u258C\u258B\u258A\u2589\u253C"
								"\u2534\u252C\u2524\u251C\u2594\u2500\u2502\u2595\u250C\u2510\u2514\u2518\u256D\u256E\u2570\u256F";
// L" ｡｢｣､･ｦｧｨｩｪｫｬｭｮｯｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿ"
const wchar_t katakanaA0[33] = L" \uFF61\uFF62\uFF63\uFF64\uFF65\uFF66\uFF67\uFF68\uFF69\uFF6A\uFF6B\uFF6C\uFF6D\uFF6E\uFF6F"
								"\uFF70\uFF71\uFF72\uFF73\uFF74\uFF75\uFF76\uFF77\uFF78\uFF79\uFF7A\uFF7B\uFF7C\uFF7D\uFF7E\uFF7F";
// L"ﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝﾞﾟ"
const wchar_t katakanaC0[33] = L"\uFF80\uFF81\uFF82\uFF83\uFF84\uFF85\uFF86\uFF87\uFF88\uFF89\uFF8A\uFF8B\uFF8C\uFF8D\uFF8E\uFF8F"
								"\uFF90\uFF91\uFF92\uFF93\uFF94\uFF95\uFF96\uFF97\uFF98\uFF99\uFF9A\uFF9B\uFF9C\uFF9D\uFF9E\uFF9F";
// L"゠ￄ‡ￊ◢◣◤◥♠♥♦♣●○╱╲╳円年月日時分秒〒市区町村人▒ "
const wchar_t katakanaE0[33] = L"\u30A0\uFFC4\u2021\uFFCA\u25E2\u25E3\u25E4\u25E5\u2660\u2665\u2666\u2663\u25CF\u25CB\u2571\u2572"
								"\u2573\u5186\u5E74\u6708\u65E5\u6642\u5206\u79D2\u3012\u5E02\u533A\u753A\u6751\u4EBA\u2592 ";

constexpr int ASCII_CHARS = 256 - 32;
constexpr int KANJI_CHARS = 1 + 94 * 94;

static uint8_t ascii8x16[ASCII_CHARS * 16];
static uint8_t kanji16x16[KANJI_CHARS * 2 * 16];
static uint8_t ascii12x24[ASCII_CHARS * 2 * 24];
static uint8_t kanji24x24[KANJI_CHARS * 3 * 24];

bool save(const char *name, void *data, size_t size)
{
	FILE *f = fopen(name, "wb");
	if (f == nullptr) {
		perror(name);
		return false;
	}
	bool status = fwrite(data, size, 1, f) == 1;
	fclose(f);

	return status;
}

wchar_t convertKatakana(wchar_t c)
{
	if (c >= 0xe0)
		return katakanaE0[c - 0xe0];
	if (c >= 0xc0)
		return katakanaC0[c - 0xc0];
	if (c >= 0xa0)
		return katakanaA0[c - 0xa0];
	if (c >= 0x80)
		return katakana80[c - 0x80];
	return c;
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	loadjisx208Table();

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <font>\n", argv[0]);
		return 1;
	}
	int error = FT_Init_FreeType(&library);
	if (error)
	{
		fprintf(stderr, "FreeType init failed\n");
		return 1;
	}
	for (int font = 1; font < argc; font++)
	{
		const char *fontname = argv[font];
		long index = 0;
		if (strlen(fontname) >= 4 && !strcmp(fontname + strlen(fontname) - 4, ".ttc"))
			 // 5: NotoSans Mono CJK.ttc
			index = strtoul(argv[++font], nullptr, 10);
		error = FT_New_Face(library, fontname, index, &face);
		if (error)
		{
			fprintf(stderr, "Can't load %s\n", fontname);
			return 1;
		}

		const char *registry;
		const char *encoding;
		int fontHeight = 0;
		FT_Get_BDF_Charset_ID(face, &encoding, &registry);
		printf("%s: %s %s\n%ld glyphs\n%d fixed sizes\n%d charmaps\n", fontname, encoding, registry, face->num_glyphs, face->num_fixed_sizes, face->num_charmaps);
		FT_Bitmap_Size *size = face->available_sizes;
		for (int i = 0; i < face->num_fixed_sizes; i++, size++)
		{
			printf("%d: %d x %d\n", i + 1, size->width, size->height);
			fontHeight = size->height;
		}
		if (face->num_fixed_sizes == 0)
			FT_Set_Pixel_Sizes(face, 0, 24);

		bool jisx0201Encoding = false;
		bool jisx0208Encoding = false;
		if (registry != nullptr)
		{
			if (!strcmp(registry, "JISX0208.1983"))
			{
				jisx0208Encoding = true;
				FT_Set_Charmap(face, face->charmaps[0]);
			}
			else if (!strcmp(registry, "JISX0201.1976"))
			{
				jisx0201Encoding = true;
				FT_Set_Charmap(face, face->charmaps[0]);
			}
		}

		if (fontHeight == 16)
		{
			for (wchar_t c = 32; c < 256; c++)
			{
				wchar_t uni = convertKatakana(c);
				if (!loadGlyph(face, uni, 8, 16))
					continue;
				uint8_t *src = face->glyph->bitmap.buffer;
				uint8_t *dst = &ascii8x16[(c - 32) * 16];
				memcpy(dst, src, 16);
			}
			for (int plane = 0x21; plane <= 0x7e; plane++)
			{
				for (int c = 0x21; c <= 0x7e; c++)
				{
					wchar_t code = (plane << 8) | c;
					if (!jisx0208Encoding)
						code = jisx208[code];
					if (!loadGlyph(face, code, 16, 16))
						continue;
					uint8_t *src = face->glyph->bitmap.buffer;
					uint8_t *dst = &kanji16x16[(1 + (plane - 0x21) * 94 + (c - 0x21)) * 2 * 16];
					if (face->glyph->bitmap.pitch == 2)
					{
						memcpy(dst, src, 2 * 16);
					}
					else
					{
						for (int y = 0; y < 16; y++)
						{
							*dst++ = src[0];
							*dst++ = src[1];
							src += face->glyph->bitmap.pitch;
						}
					}
				}
			}
		}
		else if (fontHeight == 24)
		{
			for (wchar_t c = 32; c < 256; c++)
			{
				wchar_t uni;
				if (jisx0201Encoding)
				{
					if ((c >= 0x80 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFF))
							continue;
					uni = c;
				}
				else
				{
					if (c == 0x7f)
						continue;
					uni = convertKatakana(c);
				}
				if (!loadGlyph(face, uni, 12, 24))
					continue;
				uint8_t *src = face->glyph->bitmap.buffer;
				uint8_t *dst = &ascii12x24[(c - 32) * 2 * 24];
				if (face->glyph->bitmap.pitch == 2)
					memcpy(dst, src, 2 * 24);
				else
				{
					for (int y= 0; y < 24; y++)
					{
						*dst++ = src[0];
						*dst++ = src[1];
						src += face->glyph->bitmap.pitch;
					}
				}
			}
			for (int plane = 0x21; plane <= 0x7e; plane++)
			{
				for (int c = 0x21; c <= 0x7e; c++)
				{
					wchar_t code = (plane << 8) | c;
					if (!jisx0208Encoding)
						code = jisx208[code];
					if (!loadGlyph(face, code, 24, 24))
						continue;
					uint8_t *src = face->glyph->bitmap.buffer;
					uint8_t *dst = &kanji24x24[(1 + (plane - 0x21) * 94 + (c - 0x21)) * 3 * 24];
					if (face->glyph->bitmap.pitch == 3)
					{
						memcpy(dst, src, 3 * 24);
					}
					else
					{
						for (int y = 0; y < 24; y++)
						{
							*dst++ = *src++;
							*dst++ = *src++;
							*dst++ = *src++;
							src += face->glyph->bitmap.pitch - 3;
						}
					}
				}
			}
		}
		FT_Done_Face(face);
	}
	FT_Done_FreeType(library);
	save("printer_ascii8x16.bin", ascii8x16, sizeof(ascii8x16));
	save("printer_ascii12x24.bin", ascii12x24, sizeof(ascii12x24));
	save("printer_kanji16x16.bin", kanji16x16, sizeof(kanji16x16));
	save("printer_kanji24x24.bin", kanji24x24, sizeof(kanji24x24));

	return 0;
}
