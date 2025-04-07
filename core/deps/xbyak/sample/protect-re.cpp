#include <xbyak/xbyak.h>

struct Code1 : Xbyak::CodeGenerator {
	Code1()
		: Xbyak::CodeGenerator(4096, Xbyak::DontSetProtectRWE)
	{
		mov(eax, 123);
		ret();
	}
	void update()
	{
		db(0);
	}
};

void test1(bool updateCode)
{
	Code1 c;
	c.setProtectModeRE();
	if (updateCode) c.update(); // segmentation fault
	int (*f)() = c.getCode<int (*)()>();
	printf("f=%d\n", f());
	
	c.setProtectModeRW();
	c.update();
	puts("ok");
}

struct Code2 : Xbyak::CodeGenerator {
	Code2()
		: Xbyak::CodeGenerator(4096, Xbyak::AutoGrow)
	{
		mov(eax, 123);
		ret();
	}
	void update()
	{
		db(0);
	}
};

void test2(bool updateCode)
{
	Code2 c;
	c.readyRE();
	if (updateCode) c.update(); // segmentation fault
	int (*f)() = c.getCode<int (*)()>();
	printf("f=%d\n", f());
	
	c.setProtectModeRW();
	c.update();
	puts("ok");
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "%s <testNum> [update]\n", argv[0]);
		return 0;
	}
	bool update = argc == 3;
	int n = atoi(argv[1]);
	printf("n=%d update=%d\n", n, update);
	switch (n) {
	case 1: test1(update); break;
	case 2: test2(update); break;
	default: fprintf(stderr, "no test %d\n", n); break;
	}
}
