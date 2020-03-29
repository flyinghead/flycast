#define XBYAK_NO_OP_NAMES
#include <xbyak/xbyak_util.h>

#ifdef XBYAK32
	#error "this sample is for only 64-bit mode"
#endif

struct Code : public Xbyak::CodeGenerator {
	Code()
	{
		// see xbyak/sample/sf_test.cpp for how to use other parameter
		Xbyak::util::StackFrame sf(this, 3);
		mov(rax, sf.p[0]);
		add(rax, sf.p[1]);
		add(rax, sf.p[2]);
	}
};

int main()
{
	Code c;
	int (*f)(int, int, int) = c.getCode<int(*) (int, int, int)>();
	int ret = f(3, 5, 2);
	if (ret == 3 + 5 + 2) {
		puts("ok");
	} else {
		puts("ng");
	}
}
