#if defined(_MSC_VER) && (_MSC_VER <= 1200)
	#pragma warning(disable:4514)
	#pragma warning(disable:4786)
#endif
#include <stdio.h>
#include <stdlib.h>
#include "../../include.mie/mie_thread.h"
#include "xbyak/xbyak.h"

class WriteMMX : public Xbyak::CodeGenerator {
public:
	WriteMMX()
	{
#ifdef XBYAK32
		mov(ecx, ptr [esp + 4]);
#endif
		movd(mm0, ecx);
		ret();
	}
	void (*set() const)(int x) { return (void (*)(int x))getCode(); }
};

class ReadMMX : public Xbyak::CodeGenerator {
public:
	ReadMMX()
	{
		movd(eax, mm0);
		ret();
	}
	int (*get() const)() { return (int (*)())getCode(); }
};

class Test : public MIE::ThreadBase<Test> {
	int n_;
public:
	Test(int n)
		: n_(n)
	{
	}
	void threadEntry()
	{
		printf("n=%d\n", n_);
		WriteMMX w;
		w.set()(n_);
		ReadMMX r;
		for (;;) {
			int b = r.get()();
			printf("b=%d\n", b);
			if (b != n_) {
				printf("mm0 has changed!\n");
			}
			MIE::MIE_Sleep(1000);
		}
	}
	void stopThread() { }
};

int main(int argc, char *argv[])
{
#ifdef XBYAK32
	puts("32bit");
#else
	puts("64bit");
#endif
	try {
		int n = atoi(argc == 1 ? "1223" : argv[1]);
		Test test0(n), test1(n + 1);
		test0.beginThread();
		test1.beginThread();

		test0.joinThread();
		test1.joinThread();
	} catch (std::exception& e) {
		printf("ERR:%s\n", e.what());
	} catch (...) {
		printf("unknown error\n");
	}
}
