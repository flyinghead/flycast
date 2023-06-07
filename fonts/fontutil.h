#include <ft2build.h>
#include FT_FREETYPE_H

extern wchar_t jisx208[65536];
void loadjisx208Table();

bool loadGlyph(FT_Face face, unsigned glyph, unsigned w, unsigned h);
