#define XBYAK_NO_OP_NAMES
#include "xbyak/xbyak.h"
#include <string.h>
#include <vector>

struct Code : Xbyak::CodeGenerator {
	Code(int x)
	{
		mov(eax, x);
		ret();
	}
};

int main()
	try
{
#ifdef XBYAK_USE_MMAP_ALLOCATOR
	puts("use Allocator with mmap");
#else
	puts("use Allocator with posix_memalign");
#endif
	const int N = 70000;
	std::vector<Code*> v(N);
	for (int i = 0; i < N; i++) {
		v[i] = new Code(i);
	}
	long long sum = 0;
	for (int i = 0; i < N; i++) {
		sum += v[i]->getCode<int (*)()>()();
	}
	for (int i = 0; i < N; i++) {
		delete v[i];
	}
	printf("sum=%lld\n", sum);
} catch (std::exception& e) {
	printf("ERR %s\n", e.what());
}
