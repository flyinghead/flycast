#define XBYAK_NO_EXCEPTION
#include <xbyak/xbyak.h>

using namespace Xbyak;

int g_err = 0;
int g_test = 0;

void assertEq(int x, int y)
{
	if (x != y) {
		printf("ERR x=%d y=%d\n", x, y);
		g_err++;
	}
	g_test++;
}

void assertBool(bool b)
{
	if (!b) {
		printf("ERR assertBool\n");
		g_err++;
	}
	g_test++;
}

void test1()
{
	const int v = 123;
	struct Code : CodeGenerator {
		Code()
		{
			mov(eax, v);
			ret();
		}
	} c;
	int (*f)() = c.getCode<int (*)()>();
	assertEq(f(), v);
	assertEq(Xbyak::GetError(), ERR_NONE);
}

void test2()
{
	struct Code : CodeGenerator {
		Code()
		{
			Label lp;
			L(lp);
			L(lp);
		}
	} c;
	assertEq(Xbyak::GetError(), ERR_LABEL_IS_REDEFINED);
	Xbyak::ClearError();
}

void test3()
{
	static struct EmptyAllocator : Xbyak::Allocator {
		uint8_t *alloc() { return 0; }
	} emptyAllocator;
	struct Code : CodeGenerator {
		Code() : CodeGenerator(8, 0, &emptyAllocator)
		{
			mov(eax, 3);
			assertBool(Xbyak::GetError() == 0);
			mov(eax, 3);
			mov(eax, 3);
			assertBool(Xbyak::GetError() != 0);
			Xbyak::ClearError();
			assertBool(Xbyak::GetError() == 0);
		}
	} c;
}

void test4()
{
	struct Code : CodeGenerator {
		Code()
		{
			mov(ptr[eax], 1);
			assertBool(Xbyak::GetError() != 0);
			Xbyak::ClearError();

			test(ptr[eax], 1);
			assertBool(Xbyak::GetError() != 0);
			Xbyak::ClearError();

			adc(ptr[eax], 1);
			assertBool(Xbyak::GetError() != 0);
			Xbyak::ClearError();

			setz(eax);
			assertBool(Xbyak::GetError() != 0);
			Xbyak::ClearError();
		}
	};
}

int main()
{
	test1();
	test2();
	test3();
	test4();
	if (g_err) {
		printf("err %d/%d\n", g_err, g_test);
	} else {
		printf("all ok %d\n", g_test);
	}
	return g_err != 0;
}
