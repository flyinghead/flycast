//
// Build the dreamcast BIOS font table using the specified fonts with the help of FreeType2.
// Fonts should be listed in increasing priority order since glyphs are replaced by later fonts if found.
//
// TODO: extended half-width katakana chars (rows 5 and 6) aren't found
//
// biosfont 12x24rk.pcf jiskan24.pcf neep-iso8859-1-12x24.pcf
//
// 12x24rk.pcf: Copyright 1989 by Sony Corp.
//				Attrib license
// jiskan24.pcf: Licensed under Public Domain
// neep-iso8859-1-12x24.pcf: Copyright Jim Knoble <jmknoble@pobox.com>
//							 GPL v2+
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

// BIOS table
// 288 12x24 characters (unicode encoding)
const unsigned charcodes12[] = {
//  from, to
	// overbar
	0xaf, 0xaf,
	// ASCII 33-126
	'!', '~',
	// Yen
	0xa5, 0xa5,
	// ISO-8859-1 (chars 160-255)
	0xa0, 0xff,
	// JIS X 0201 (chars 160-255)
	' ', ' ',
	0xff61, 0xff9f, // regular JIS X 0201 range
	0x30f0, 0x30f1,	// Wi We
	0x30ee, 0x30ee, // small Wa
	0x30ab, 0x30ab, // Ka
	0x30b1, 0x30b1, // Ke
	0x30f4, 0x30f4, // Vu
	0x30ac, 0x30ac, // Ga
	0x30ae, 0x30ae, // Gi
	0x30b0, 0x30b0, // Gu
	0x30b2, 0x30b2, // Ge
	0x30b4, 0x30b4, // Go
	0x30b6, 0x30b6, // Za
	0x30b8, 0x30b8, // Zi
	0x30ba, 0x30ba, // Zu
	0x30bc, 0x30bc, // Ze
	0x30be, 0x30be, // Zo
	0x30c0, 0x30c0, // Da
	0x30c2, 0x30c2, // Di
	0x30c5, 0x30c5, // Du
	0x30c7, 0x30c7, // De
	0x30c9, 0x30c9, // Do
	0x30d0, 0x30d1, // Ba Pa
	0x30d3, 0x30d4, // Bi Pi
	0x30d6, 0x30d7, // Bu Pu
	0x30d9, 0x30da, // Be Pe
	0x30dc, 0x30dd, // Bo Po
	' ', ' ',
};

// 7078 24x24 characters (JIS X0208 encoding)
const unsigned charcodes24[] = {
// prefix, from, to
	// JIS X0208 row 33, Symbols
	0x21, 0x21, 0x7e,
	// JIS X0208 row 34, Symbols
	0x22, 0x21, 0x7e,
	// JIS X0208 row 35, Roman alphabet
	0x23, 0x21, 0x7e,
	// JIS X0208 row 36, Hiragana
	0x24, 0x21, 0x7e,
	// JIS X0208 row 37, Katakana
	0x25, 0x21, 0x7e,
	// JIS X0208 row 38, Greek
	0x26, 0x21, 0x7e,
	// JIS X0208 row 39, Cyrillic
	0x27, 0x21, 0x7e,
	// JIS X0208 row 48
	0x30, 0x21, 0x7e,
	// JIS X0208 row 49
	0x31, 0x21, 0x7e,
	// JIS X0208 row 50
	0x32, 0x21, 0x7e,
	// JIS X0208 row 51
	0x33, 0x21, 0x7e,
	// JIS X0208 row 52
	0x34, 0x21, 0x7e,
	// JIS X0208 row 53
	0x35, 0x21, 0x7e,
	// JIS X0208 row 54
	0x36, 0x21, 0x7e,
	// JIS X0208 row 55
	0x37, 0x21, 0x7e,
	// JIS X0208 row 56
	0x38, 0x21, 0x7e,
	// JIS X0208 row 57
	0x39, 0x21, 0x7e,
	// JIS X0208 row 58
	0x3a, 0x21, 0x7e,
	// JIS X0208 row 59
	0x3b, 0x21, 0x7e,
	// JIS X0208 row 60
	0x3c, 0x21, 0x7e,
	// JIS X0208 row 61
	0x3d, 0x21, 0x7e,
	// JIS X0208 row 62
	0x3e, 0x21, 0x7e,
	// JIS X0208 row 63
	0x3f, 0x21, 0x7e,
	// JIS X0208 row 64
	0x40, 0x21, 0x7e,
	// JIS X0208 row 65
	0x41, 0x21, 0x7e,
	// JIS X0208 row 66
	0x42, 0x21, 0x7e,
	// JIS X0208 row 67
	0x43, 0x21, 0x7e,
	// JIS X0208 row 68
	0x44, 0x21, 0x7e,
	// JIS X0208 row 69
	0x45, 0x21, 0x7e,
	// JIS X0208 row 70
	0x46, 0x21, 0x7e,
	// JIS X0208 row 71
	0x47, 0x21, 0x7e,
	// JIS X0208 row 72
	0x48, 0x21, 0x7e,
	// JIS X0208 row 73
	0x49, 0x21, 0x7e,
	// JIS X0208 row 74
	0x4a, 0x21, 0x7e,
	// JIS X0208 row 75
	0x4b, 0x21, 0x7e,
	// JIS X0208 row 76
	0x4c, 0x21, 0x7e,
	// JIS X0208 row 77
	0x4d, 0x21, 0x7e,
	// JIS X0208 row 78
	0x4e, 0x21, 0x7e,
	// JIS X0208 row 79
	0x4f, 0x21, 0x7e,

	// JIS X0208 row 80
	0x50, 0x21, 0x7e,
	// JIS X0208 row 81
	0x51, 0x21, 0x7e,
	// JIS X0208 row 82
	0x52, 0x21, 0x7e,
	// JIS X0208 row 83
	0x53, 0x21, 0x7e,
	// JIS X0208 row 84
	0x54, 0x21, 0x7e,
	// JIS X0208 row 85
	0x55, 0x21, 0x7e,
	// JIS X0208 row 86
	0x56, 0x21, 0x7e,
	// JIS X0208 row 87
	0x57, 0x21, 0x7e,
	// JIS X0208 row 88
	0x58, 0x21, 0x7e,
	// JIS X0208 row 89
	0x59, 0x21, 0x7e,
	// JIS X0208 row 90
	0x5a, 0x21, 0x7e,
	// JIS X0208 row 91
	0x5b, 0x21, 0x7e,
	// JIS X0208 row 92
	0x5c, 0x21, 0x7e,
	// JIS X0208 row 93
	0x5d, 0x21, 0x7e,
	// JIS X0208 row 94
	0x5e, 0x21, 0x7e,
	// JIS X0208 row 95
	0x5f, 0x21, 0x7e,
	// JIS X0208 row 96
	0x60, 0x21, 0x7e,
	// JIS X0208 row 97
	0x61, 0x21, 0x7e,
	// JIS X0208 row 98
	0x62, 0x21, 0x7e,
	// JIS X0208 row 99
	0x63, 0x21, 0x7e,
	// JIS X0208 row 100
	0x64, 0x21, 0x7e,
	// JIS X0208 row 101
	0x65, 0x21, 0x7e,
	// JIS X0208 row 102
	0x66, 0x21, 0x7e,
	// JIS X0208 row 103
	0x67, 0x21, 0x7e,
	// JIS X0208 row 104
	0x68, 0x21, 0x7e,
	// JIS X0208 row 105
	0x69, 0x21, 0x7e,
	// JIS X0208 row 106
	0x6a, 0x21, 0x7e,
	// JIS X0208 row 107
	0x6b, 0x21, 0x7e,
	// JIS X0208 row 108
	0x6c, 0x21, 0x7e,
	// JIS X0208 row 109
	0x6d, 0x21, 0x7e,
	// JIS X0208 row 110
	0x6e, 0x21, 0x7e,
	// JIS X0208 row 111
	0x6f, 0x21, 0x7e,
	// JIS X0208 row 112
	0x70, 0x21, 0x7e,
	// JIS X0208 row 113
	0x71, 0x21, 0x7e,
	// JIS X0208 row 114
	0x72, 0x21, 0x7e,
	// JIS X0208 row 115
	0x73, 0x21, 0x7e,
	// JIS X0208 row 116
	0x74, 0x21, 0x26,
	// TODO Dreamcast symbols (22 chars)
	// copyright U+24B8
	// register  U+24C7
	// trademark U+2122
	// up arrow  U+2B06
	// down		 U+2B07
	// left		 U+2B05
	// right	 U+27A1 ???
	// up+right  U+2B08
	// down+right U+2B0A
	// down+left U+2B0B
	// up+left	 U+2B09
	// circled A U+24B6
	// circled B U+24B7
	// circled C U+24B8
	// circled D U+24B8
	// circled X U+24CD
	// circled Y U+24CE
	// circled Z U+24CF
	// squared L U+1F13B (Supplementary Multilingual Plane)
	// squared R U+1F141 (Supplementary Multilingual Plane)
	// start button U+1F142 (Squared S, SMP)
	// VMU	U+1F4DF (pager) U+1F4F1 (mobile phone)
};

static uint8_t biosfont[288 * 36 + 7078 * 72];

static FT_Library library;
static FT_Face face;

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
		FT_Get_BDF_Charset_ID(face, &encoding, &registry);
		printf("%s: %s %s\n%ld glyphs\n%d fixed sizes\n%d charmaps\n", fontname, encoding, registry, face->num_glyphs, face->num_fixed_sizes, face->num_charmaps);
		FT_Bitmap_Size *size = face->available_sizes;
		for (int i = 0; i < face->num_fixed_sizes; i++, size++)
			printf("%d: %d x %d\n", i + 1, size->width, size->height);
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
		/* list chars
		unsigned gindex;
		long charcode = FT_Get_First_Char(face, &gindex);
		while (gindex != 0)
		{
			printf("code %lx index %d\n", charcode, gindex);
		    charcode = FT_Get_Next_Char(face, charcode, &gindex);
		}
		*/

		unsigned offset = 0;
		if (jisx0208Encoding)
		{
			offset = 288 * 36;
		}
		else
		{
			for (size_t i = 0; i < std::size(charcodes12); i += 2)
			{
				int from = charcodes12[i];
				int to = charcodes12[i + 1];
				for (int j = from; j <= to; j++)
				{
					int code = j;
					if (jisx0201Encoding)
					{
						if (code >= 0xff61 && code <= 0xff9f)
							code = code - 0xff61 + 0xa1;
						else if (code >= 0x80) {
							offset += 36;
							continue;
						}
					}
					if (!loadGlyph(face, code, 12, 24)) {
						offset += 36;
						continue;
					}
					uint8_t *p = face->glyph->bitmap.buffer;
					for (int r = 0; r < 24; r++)
					{
						if (r & 1)
						{
							biosfont[offset + 1] |= p[0] >> 4;
							biosfont[offset + 2] = ((p[0] & 0xf) << 4) | (p[1] >> 4);
							offset += 3;
						}
						else
						{
							biosfont[offset] = p[0];
							biosfont[offset + 1] = p[1];
						}
						p += face->glyph->bitmap.pitch;
					}
				}
			}
			printf("Final offset(12) %d 288 * 36 = %d\n", offset, 288 * 36);
		}

		if (jisx0201Encoding)
		{
			offset += 7078 * 72;
		}
		else
		{
			for (size_t range = 0; range < std::size(charcodes24); range += 3)
			{
				int prefix = charcodes24[range];
				int from = charcodes24[range + 1];
				int to = charcodes24[range + 2];
				for (int j = from; j <= to; j++)
				{
					uint16_t jis = (prefix << 8) | j;
					wchar_t u;
					if (jisx0208Encoding) {
						u = jis;
					}
					else
					{
						u = jisx208[jis];
						if (u == 0) {
							printf("JISX208 conversion failed: [%02x %02x]\n", prefix, j);
							offset += 72;
							continue;
						}
					}
					if (!loadGlyph(face, u, 24, 24)) {
						offset += 72;
						continue;
					}
					const FT_GlyphSlot glyph = face->glyph;
					uint8_t *p = glyph->bitmap.buffer;
					// left=0 top=22 is ok
					// other values need to shift the bitmap
					if (glyph->bitmap.pitch == 3 && glyph->bitmap_left == 0 && glyph->bitmap_top == 22) {
						memcpy(&biosfont[offset], p, 72);
						offset += 72;
					}
					else
					{
						const int topFill = std::max(0, 21 - glyph->bitmap_top);
						memset(&biosfont[offset], 0, topFill * 3);
						offset += topFill * 3;
						const int rows = std::min((int)glyph->bitmap.rows, 24 - topFill);
						const int width = glyph->bitmap.width;
						for (int r = 0; r < rows; r++)
						{
							if (width == 24)
							{
								memcpy(&biosfont[offset], p, 3);
							}
							else
							{
								int left = glyph->bitmap_left;
								unsigned o = offset;
								while (left >= 8 && o - offset < 3)
								{
									left -= 8;
									biosfont[o++] = 0;
								}
								if (o - offset < 3)
								{
									biosfont[o++] = p[0] >> left;
									if (left > 0 && o - offset < 3)
										biosfont[o] = p[0] << (8 - left);
									if (width > 8 && o - offset < 3)
									{
										biosfont[o++] |= p[1] >> left;
										if (o - offset < 3)
										{
											if (left > 0)
												biosfont[o] = p[1] << (8 - left);
											if (width > 16)
												biosfont[o++] |= p[2] >> left;
										}
									}
									else
									{
										o++;
										if (o - offset < 3)
											biosfont[o] = 0;
									}
								}
							}
							offset += 3;
							p += glyph->bitmap.pitch;
						}
						const int bottomFill = 24 - (topFill + rows);
						assert(bottomFill >= 0);
						memset(&biosfont[offset], 0, bottomFill * 3);
						offset += bottomFill * 3;
					}
				}
			}
		}
		printf("Final offset(24) %d 288 * 36 + 7078 * 72 = %d\n", offset, 288 * 36 + 7078 * 72);
		FT_Done_Face(face);
	}
	FT_Done_FreeType(library);

	FILE *f = fopen("biosfont.bin", "wb");
	if (f == nullptr) {
		perror("biosfont.bin");
		return 1;
	}
	fwrite(biosfont, 1, sizeof(biosfont), f);
	fclose(f);

	return 0;
}
