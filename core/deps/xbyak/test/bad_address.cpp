#include <xbyak/xbyak.h>
#include <cybozu/test.hpp>

struct Code : Xbyak::CodeGenerator {
	Code()
	{
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [esp + esp]), std::exception);
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [ax]), std::exception); // not support
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [esp * 4]), std::exception);
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [eax * 16]), std::exception);
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [eax + eax + eax]), std::exception);
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [eax * 2 + ecx * 4]), std::exception);
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [eax * 2 + ecx * 4]), std::exception);
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [xmm0]), std::exception);
		CYBOZU_TEST_EXCEPTION(fld(dword [xmm0]), std::exception);
		CYBOZU_TEST_EXCEPTION(vgatherdpd(xmm0, ptr [eax * 2], ymm3), std::exception);
		CYBOZU_TEST_EXCEPTION(vgatherdpd(xmm0, ptr [xmm0 + xmm1], ymm3), std::exception);
#ifdef XBYAK64
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [rax + eax]), std::exception);
		CYBOZU_TEST_EXCEPTION(mov(eax, ptr [xmm0 + ymm0]), std::exception);
#endif
	}
};

CYBOZU_TEST_AUTO(exception)
{
	Code c;
}
