#include <stdio.h>

int main()
{
	puts("enum {");
	for (int i = 0; i < 256; i++) {
		printf("	B");
		for (int j = 0; j < 8; j++) {
			putchar(i & (1 << (7 - j)) ? '1' : '0');
		}
		printf("= %d", i);
		if (i < 255) putchar(',');
		putchar('\n');
	}
	puts("};");
	return 0;
}