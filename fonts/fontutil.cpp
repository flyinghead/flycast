#include "fontutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

wchar_t jisx208[65536];

void loadjisx208Table()
{
	FILE *f = fopen("jisx0213-2004-8bit-std.txt" ,"rb");
	if (f == nullptr) {
		perror("jisx0213-2004-8bit-std.txt");
		return;
	}
	char buf[512];
	while (fgets(buf, sizeof(buf), f) != nullptr)
	{
		if (buf[0] == '#')
			continue;
		char *p;
		uint16_t jis = strtoul(buf, &p, 16);
		p += 3; // "\tU+"
		wchar_t utf = strtoul(p, &p, 16);
		if (!isspace((int8_t)*p))
			continue;
		jisx208[jis] = utf;
	}
	fclose(f);
}

bool loadGlyph(FT_Face face, unsigned glyph, unsigned w, unsigned h)
{
	unsigned glyph_index = FT_Get_Char_Index(face, glyph);
	if (glyph_index == 0) {
		//fprintf(stderr, "Missing glyph(%d) code %x char %lc\n", w, glyph, glyph);
		return false;
	}
	int error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
	if (error) {
		fprintf(stderr, "Can't load glyph(%d) code %x char %lc\n", w, glyph, glyph);
		return false;
	}
	if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
	{
		error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
		if (error) {
			fprintf(stderr, "Render glyph failed: glyph(%d) code %x char %lc\n", w, glyph, glyph);
			return false;
		}
	}
	else if (face->glyph->bitmap.width != w || face->glyph->bitmap.rows != h) {
		//fprintf(stderr, "glyph(%d) code %x char %lc wrong size %d x %d\n", w, glyph, glyph, face->glyph->bitmap.width, face->glyph->bitmap.rows);
		return false;
	}
	//	printf("%lc bitmap %d x %d pitch %d px mode %d left %d top %d\n", glyph < 0x80 || glyph >= 0xA0 ? glyph : '?',
	//			face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap.pitch, face->glyph->bitmap.pixel_mode,
	//			face->glyph->bitmap_left, face->glyph->bitmap_top);
	return true;
}
