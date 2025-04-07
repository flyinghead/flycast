/*
	An example of T_zu (zero upper) flag
	> g++ zero_upper.cpp -I ../xbyak
	> sde -future -- ./a.out
*/
#include <stdio.h>
#include <xbyak/xbyak.h>

using namespace Xbyak;

struct Code : Xbyak::CodeGenerator {
	Code(int mode)
	{
		mov(eax, 0x12345678);
		cmp(eax, eax); // ZF=1
		switch (mode) {
		case 0: // imul
			puts("imul");
			imul(ax,ax, 0x1234);
			break;
		case 1: // imul+zu
			puts("imul+zu");
			imul(ax|T_zu, ax, 0x1234);
			break;
		case 2: // setz
			puts("setz");
			setz(al);
			break;
		case 3: // setz+zu
			puts("setz+zu");
			setz(al|T_zu);
			break;
		}
		ret();
	}
};

int main()
	try
{
	for (int mode = 0; mode < 4; mode++) {
		Code c(mode);
		auto f = c.getCode<int (*)()>();
		printf("ret=%08x\n", f());
	}
} catch (std::exception& e) {
	printf("ERR %s\n", e.what());
}
