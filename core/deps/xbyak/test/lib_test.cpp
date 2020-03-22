#include "lib.h"

void init()
{
	static bool init = true;
	printf("in lib_test %d\n", init);
	if (!init) return;
	init = false;
	X::a.put();
	putReg();
}


