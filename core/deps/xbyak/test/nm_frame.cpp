#include <stdio.h>
#define XBYAK_ENABLE_OMITTED_OPERAND
#include "xbyak/xbyak.h"
#define CYBOZU_TEST_DISABLE_AUTO_RUN
#include "cybozu/test.hpp"

using namespace Xbyak;

#ifdef _MSC_VER
	#pragma warning(disable : 4245)
	#pragma warning(disable : 4312)
#endif
class Sample : public CodeGenerator {
	void operator=(const Sample&);
public:
#include "nm.cpp"
};


class ErrorSample : public CodeGenerator {
	void operator=(const ErrorSample&);
public:
	void gen()
	{
		CYBOZU_TEST_EXCEPTION(mov(ptr[eax],1), std::exception);
		CYBOZU_TEST_EXCEPTION(test(ptr[eax],1), std::exception);
		CYBOZU_TEST_EXCEPTION(adc(ptr[eax],1), std::exception);
		CYBOZU_TEST_EXCEPTION(setz(eax), std::exception);
	}
};

int main()
{
	CYBOZU_TEST_EQUAL(sizeof(Xbyak::Operand), 4u);
	Sample s;
	s.gen();
	ErrorSample es;
	es.gen();
}
