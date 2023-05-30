#include <stdio.h>

void printAsciiTestPage(bool largeFont)
{
	const char *fname = largeFont ? "test12x24.dump" : "test8x16.dump";
	FILE *f = fopen(fname, "wb");
	if (!f) {
		perror(fname);
		return;
	}
	fprintf(f, "\33H\22F%c\n", largeFont); // disable Kanji, select font
	fprintf(f, "    \33-\2TEST PAGE %s\33-%c\n\n", largeFont ? "12x24" : "8x16", 0);
	for (int i = 0x20; i < 0x100; i += 0x10)
	{
		fprintf(f, "%02X", i);
		for (int j = 0; j < 0x10; j++)
		{
			if (i + j == 0xa0 || i + j == 0xff)
				fprintf(f, "  ");
			else
				fprintf(f, " %c", i + j);
		}
		fputc('\n', f);
	}
	fputc('\n', f);
	fputc('\n', f);
	fprintf(f, "\33i"); // full cut
	fclose(f);

	printf("%s created\n", fname);
}

void printKanjiTestPage(bool largeFont)
{
	const char *fname = largeFont ? "test24x24.dump" : "test16x16.dump";
	FILE *f = fopen(fname, "wb");
	if (!f) {
		perror(fname);
		return;
	}
	fprintf(f, "\33H\22F%c\n", largeFont); // disable Kanji, select font
	fprintf(f, "    \33-\2KANJI TEST PAGE %s\33-%c\n\n", largeFont ? "24x24" : "16x16", 0);
	for (int plane = 0x21; plane <= 0x7e; plane++)
	{
		fprintf(f, " Plane %02X\n\n", plane);
		for (int c = 0x21; c <= 0x7e; c++)
		{
			if (c == 0x21 || (c & 0xf) == 0)
				fprintf(f, " %02X ", (c & 0xf0));
			if (c == 0x21)
				fprintf(f, "  ");
			fprintf(f, "\33K%c%c\33H ", plane, c);
			if ((c & 0xf) == 0xf)
				fputc('\n', f);
		}
		fputc('\n', f);
		fputc('\n', f);
	}
	fputc('\n', f);
	fputc('\n', f);
	fprintf(f, "\33i"); // full cut
	fclose(f);

	printf("%s created\n", fname);
}

int main(int argc, char *argv[])
{
	printAsciiTestPage(false);
	printAsciiTestPage(true);
	printKanjiTestPage(false);
	printKanjiTestPage(true);

	return 0;
}
