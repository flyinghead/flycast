/*
	sample of move(reg, LABEL);, L(LABEL), putL(LABEL);
*/
#include <stdio.h>
#include <xbyak/xbyak.h>

const int expectTbl[] = {
	5, 9, 12
};

struct Code : Xbyak::CodeGenerator {
	explicit Code(int mode, size_t size, void *p)
		: Xbyak::CodeGenerator(size, p)
	{
		inLocalLabel();
#ifdef XBYAK64
		const Xbyak::Reg64& a = rax;
		const Xbyak::Reg64& c = rcx;
#ifdef XBYAK64_WIN
		mov(rax, rcx);
#else
		mov(rax, rdi);
#endif
#else
		const Xbyak::Reg32& a = eax;
		const Xbyak::Reg32& c = ecx;
		mov(a, ptr [esp + 4]);
#endif

		switch (mode) {
		case 0:
			mov(c, ".jmp_table");
			lea(c, ptr [c + a * 8]);
			jmp(c);
		align(8);
			L(".jmp_table");
			mov(a, expectTbl[0]);
			ret();
		align(8);
			mov(a, expectTbl[1]);
			ret();
		align(8);
			mov(a, expectTbl[2]);
			ret();
			break;

		case 1:
			/*
				the label for putL is defined when called
			*/
			mov(c, ".jmp_table");
			jmp(ptr [c + a * (int)sizeof(size_t)]);
		L(".label1");
			mov(a, expectTbl[0]);
			jmp(".end");
		L(".label2");
			mov(a, expectTbl[1]);
			jmp(".end");
		L(".label3");
			mov(a, expectTbl[2]);
			jmp(".end");
		L(".end");
			ret();
			ud2();

			align(8);
		L(".jmp_table");
		putL(".label1");
		putL(".label2");
		putL(".label3");
			break;

		case 2:
			/*
				the label for putL is not defined when called
			*/
			jmp(".in");
			ud2();
			align(8);
		L(".jmp_table");
		putL(".label1");
		putL(".label2");
		putL(".label3");
		L(".in");
			mov(c, ".jmp_table");
			jmp(ptr [c + a * (int)sizeof(size_t)]);
		L(".label1");
			mov(a, expectTbl[0]);
			jmp(".end");
		L(".label2");
			mov(a, expectTbl[1]);
			jmp(".end");
		L(".label3");
			mov(a, expectTbl[2]);
			jmp(".end");
		L(".end");
			ret();
			break;
		}
		outLocalLabel();
	}
};

int main()
	try
{
	for (int mode = 0; mode < 3; mode++) {
		printf("mode=%d\n", mode);
		for (int grow = 0; grow < 2; grow++) {
			printf("auto grow=%s\n", grow ? "on" : "off");
			Code c(mode, grow ? 30 : 4096, grow ? Xbyak::AutoGrow : 0);
			int (*f)(int) = c.getCode<int (*)(int)>();
			c.ready();
			for (int i = 0; i < 3; i++) {
				const int a = expectTbl[i];
				const int b = f(i);
				if (a != b) {
					printf("ERR i=%d, a=%d, b=%d\n", i, a, b);
					exit(1);
				}
			}
		}
	}
	puts("ok");
} catch (std::exception& e) {
	printf("ERR %s\n", e.what());
}
