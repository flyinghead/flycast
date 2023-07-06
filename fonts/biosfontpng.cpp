//
// Make PNGs of the BIOS font for half- and full-width glyphs.
// Compare with http://submarine.org.uk/info/biosfont/
//
#include <stdio.h>
#include <stdint.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <vector>

// BIOS table
// 288 12x24 characters (unicode encoding)
const unsigned charcodes12[] = {
//  from, to
	// overbar
	1,
	// ASCII 33-126
	94,
	// Yen
	1,
	// ISO-8859-1 (chars 160-255)
	96,
	// JIS X0201 (chars 160-255)
	96,
};

// 7078 24x24 characters (JIS X0208 encoding)
const unsigned charcodes24[] = {
	// JIS X0208 row 33, Symbols
	94,
	// JIS X0208 row 34, Symbols
	94,
	// JIS X0208 row 35, Roman alphabet
	94,
	// JIS X0208 row 36, Hiragana
	94,
	// JIS X0208 row 37, Katakana
	94,
	// JIS X0208 row 38, Greek
	94,
	// JIS X0208 row 39, Cyrillic
	94,
	// JIS X0208 row 48
	94,
	// JIS X0208 row 49
	94,
	// JIS X0208 row 50
	94,
	// JIS X0208 row 51
	94,
	// JIS X0208 row 52
	94,
	// JIS X0208 row 53
	94,
	// JIS X0208 row 54
	94,
	// JIS X0208 row 55
	94,
	// JIS X0208 row 56
	94,
	// JIS X0208 row 57
	94,
	// JIS X0208 row 58
	94,
	// JIS X0208 row 59
	94,
	// JIS X0208 row 60
	94,
	// JIS X0208 row 61
	94,
	// JIS X0208 row 62
	94,
	// JIS X0208 row 63
	94,
	// JIS X0208 row 64
	94,
	// JIS X0208 row 65
	94,
	// JIS X0208 row 66
	94,
	// JIS X0208 row 67
	94,
	// JIS X0208 row 68
	94,
	// JIS X0208 row 69
	94,
	// JIS X0208 row 70
	94,
	// JIS X0208 row 71
	94,
	// JIS X0208 row 72
	94,
	// JIS X0208 row 73
	94,
	// JIS X0208 row 74
	94,
	// JIS X0208 row 75
	94,
	// JIS X0208 row 76
	94,
	// JIS X0208 row 77
	94,
	// JIS X0208 row 78
	94,
	// JIS X0208 row 79
	94,

	// JIS X0208 row 80
	94,
	// JIS X0208 row 81
	94,
	// JIS X0208 row 82
	94,
	// JIS X0208 row 83
	94,
	// JIS X0208 row 84
	94,
	// JIS X0208 row 85
	94,
	// JIS X0208 row 86
	94,
	// JIS X0208 row 87
	94,
	// JIS X0208 row 88
	94,
	// JIS X0208 row 89
	94,
	// JIS X0208 row 90
	94,
	// JIS X0208 row 91
	94,
	// JIS X0208 row 92
	94,
	// JIS X0208 row 93
	94,
	// JIS X0208 row 94
	94,
	// JIS X0208 row 95
	94,
	// JIS X0208 row 96
	94,
	// JIS X0208 row 97
	94,
	// JIS X0208 row 98
	94,
	// JIS X0208 row 99
	94,
	// JIS X0208 row 100
	94,
	// JIS X0208 row 101
	94,
	// JIS X0208 row 102
	94,
	// JIS X0208 row 103
	94,
	// JIS X0208 row 104
	94,
	// JIS X0208 row 105
	94,
	// JIS X0208 row 106
	94,
	// JIS X0208 row 107
	94,
	// JIS X0208 row 108
	94,
	// JIS X0208 row 109
	94,
	// JIS X0208 row 110
	94,
	// JIS X0208 row 111
	94,
	// JIS X0208 row 112
	94,
	// JIS X0208 row 113
	94,
	// JIS X0208 row 114
	94,
	// JIS X0208 row 115
	94,
	// JIS X0208 row 116
	6,
	// Dreamcast symbols
	22
};

static uint8_t biosfont[288 * 36 + 7078 * 72];

void make12x24()
{
	std::vector<uint8_t> bitmap;
	constexpr int WIDTH = 16 * 32;
	bitmap.resize(WIDTH * 20 * 32); // 16 cols and 20 rows of 32x32 pix
	int c = 0;
	int y = 0;
	for (int pg : charcodes12)
	{
		while (pg > 0)
		{
			for (int x = 0; x < 16 && pg > 0; x++, pg--, c++)
			{
				uint8_t *src = &biosfont[c * 3 * 12];
				for (int row = 0; row < 24; row += 2)
				{
					uint8_t *dst = &bitmap[y * 32 * WIDTH + x * 32 + WIDTH * row];
					for (int i = 0; i < 8; i++, dst++)
						if (src[0] & (0x80 >> i))
							*dst = 0xff;
					for (int i = 0; i < 4; i++, dst++)
						if (src[1] & (0x80 >> i))
							*dst = 0xff;

					dst += WIDTH - 12;
					for (int i = 0; i < 4; i++, dst++)
						if (src[1] & (0x8 >> i))
							*dst = 0xff;
					for (int i = 0; i < 8; i++, dst++)
						if (src[2] & (0x80 >> i))
							*dst = 0xff;
					src += 3;
				}
			}
			y++;
		}
	}
	stbi_write_png("bios12x24.png", WIDTH, 20 * 32, 1, &bitmap[0], WIDTH);
}

void make24x24()
{
	int lines = 0;
	for (int pg : charcodes24)
		lines += (pg + 15) / 16;

	std::vector<uint8_t> bitmap;
	constexpr int WIDTH = 16 * 32;
	bitmap.resize(WIDTH * lines * 32); // 16 cols and n rows of 32x32 pix
	uint8_t *fontbase = &biosfont[288 * 3 * 12];
	int c = 0;
	int y = 0;
	for (int pg : charcodes24)
	{
		while (pg > 0)
		{
			for (int x = 0; x < 16 && pg > 0; x++, pg--, c++)
			{
				uint8_t *src = &fontbase[c * 3 * 24];
				for (int row = 0; row < 24; row++)
				{
					uint8_t *dst = &bitmap[y * 32 * WIDTH + x * 32 + WIDTH * row];
					for (int i = 0; i < 8; i++, dst++)
						if (src[0] & (0x80 >> i))
							*dst = 0xff;
					for (int i = 0; i < 8; i++, dst++)
						if (src[1] & (0x80 >> i))
							*dst = 0xff;
					for (int i = 0; i < 8; i++, dst++)
						if (src[2] & (0x80 >> i))
							*dst = 0xff;
					src += 3;
				}
			}
			y++;
		}
	}
	stbi_write_png("bios24x24.png", WIDTH, lines * 32, 1, &bitmap[0], WIDTH);
}

int main(int argc, char *argv[])
{
	FILE *f = fopen("biosfont.bin", "rb");
	if (f == nullptr) {
		perror("biosfont.bin");
		return 1;
	}
	if (fread(biosfont, 1, sizeof(biosfont), f) != sizeof(biosfont))
	{
		fprintf(stderr, "Invalid bios font file: truncated read");
		fclose(f);
		return 1;
	}
	fclose(f);

	make12x24();
	make24x24();
}
