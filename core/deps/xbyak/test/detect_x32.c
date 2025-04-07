#include <stdio.h>

int main()
{
#if defined(__x86_64__) && defined(__ILP32__)
	puts("x32");
#endif
}
