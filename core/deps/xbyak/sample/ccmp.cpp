/*
	An example of ccmp
	> g++ ccmp.cpp -I ../xbyak
	> sde -future -- ./a.out
*/
#include <stdio.h>
#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>

using namespace Xbyak;

struct Code1 : Xbyak::CodeGenerator {
	Code1()
	{
		Xbyak::util::StackFrame sf(this, 2);
		const auto& p1 = sf.p[0];
		const auto& p2 = sf.p[1];
		int dfv = 0;
		cmp(p1, 3);
		ctesta(p2, 1, dfv); // eflags = (p1 > 3) ? ((p2 & 1) == 0) : dfv;
		setz(al|T_zu);
	}
};

struct Code2 : Xbyak::CodeGenerator {
	Code2()
	{
		Xbyak::util::StackFrame sf(this, 3);
		const auto& p1 = sf.p[0];
		const auto& p2 = sf.p[1];
		const auto& p3 = sf.p[2];
		int dfv = 0;
		cmp(p1, 1);
		ccmpe(p2, 2, dfv); // eflags = p1==1 ? p2==2 : dfv;
		ccmpe(p3, 3, dfv); // eflags = (p1==1 && p2==2) ? p3==3 : dfv;
		setz(al|T_zu); // p1==1 && p2==2 && p3==3
	}
};


int main()
	try
{
	{
		puts("(p1 > 3) && ((p2 & 1) == 0)");
		Code1 c;
		auto f = c.getCode<int (*)(int, int)>();
		for (int p1 = 2; p1 < 5; p1++) {
			for (int p2 = 0; p2 < 3; p2++) {
				printf("p1=%d p2=%d ret=%d (%d)\n", p1, p2, f(p1, p2), p1 > 3 && ((p2&1) == 0));
			}
		}
	}
	{
		puts("p1 == 1 && p2 == 2 && p3 == 3");
		Code2 c;
		auto f = c.getCode<int (*)(int, int, int)>();
		for (int p1 = 0; p1 < 3; p1++) {
			for (int p2 = 1; p2 < 4; p2++) {
				for (int p3 = 2; p3 < 5; p3++) {
					printf("p1=%d p2=%d p3=%d ret=%d (%d)\n", p1, p2, p3, f(p1, p2, p3), p1==1 && p2==2 && p3==3);
				}
			}
		}
	}
} catch (std::exception& e) {
	printf("ERR %s\n", e.what());
}
