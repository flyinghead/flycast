/*
	How to profile JIT-code with perf or VTune
	sudo perf record ./profiler 1
	amplxe-cl -collect hotspots -result-dir r001hs -quiet ./profiler-vtune 2
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <xbyak/xbyak_util.h>

const int N = 3000000;
struct Code : public Xbyak::CodeGenerator {
	Code()
	{
		mov(eax, N);
	Xbyak::Label lp = L();
		for (int i = 0; i < 10; i++) {
			sub(eax, 1);
		}
		jg(lp);
		mov(eax, 1);
		ret();
	}
};

struct Code2 : public Xbyak::CodeGenerator {
	Code2()
	{
		mov(eax, N);
	Xbyak::Label lp = L();
		for (int i = 0; i < 10; i++) {
			xorps(xm0, xm0);
		}
		sub(eax, 1);
		jg(lp);
		mov(eax, 1);
		ret();
	}
};

double s1(int n)
{
	double r = 0;
	for (int i = 0; i < n; i++) {
		r += 1.0 / (i + 1);
	}
	return r;
}

double s2(int n)
{
	double r = 0;
	for (int i = 0; i < n; i++) {
		r += 1.0 / (i * i + 1) + 2.0 / (i + 3);
	}
	return r;
}

int main(int argc, char *argv[])
{
	int mode = argc == 1 ? 0 : atoi(argv[1]);
	Code c;
	Code2 c2;
	int (*f)() = (int (*)())c.getCode();
	int (*g)() = (int (*)())c2.getCode();

	printf("f:%p, %d\n", (const void*)f, (int)c.getSize());
	printf("g:%p, %d\n", (const void*)g, (int)c2.getSize());
	Xbyak::util::Profiler prof;
	printf("mode=%d\n", mode);
	prof.init(mode);
	prof.set("f", (const void*)f, c.getSize());
	prof.set("g", (const void*)g, c2.getSize());

	double sum = 0;
	for (int i = 0; i < 20000; i++) {
		sum += s1(i);
		sum += s2(i);
	}
	printf("sum=%f\n", sum);
	for (int i = 0; i < 2000; i++) {
		sum += f();
	}
	printf("f=%f\n", sum);
	for (int i = 0; i < 2000; i++) {
		sum += g();
	}
	printf("g=%f\n", sum);
	puts("end");
}
