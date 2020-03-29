#include <stdio.h>

static const struct XXX {
	XXX() { puts("XXX"); }
} s_sss;

struct A {
	int aaa;
	A()
		: aaa(123)
	{
		puts("A cstr");
	}
	~A()
	{
		puts("A dstr");
	}
	void put() const
	{
		printf("aaa=%d\n", aaa);
	}
};

template<int dummy = 0>
struct XT {
	static A sss;
};

template<int dummy>
A XT<dummy>::sss;

typedef XT<0> X;

static struct Init {
	Init()
	{
		puts("Init");
		X::sss.put();
	}
} s_init;

int f() { puts("f"); return 4; }

static const int r = f();

int main()
{
	puts("main");
	printf("r=%d\n", r);
	X::sss.put();
}
