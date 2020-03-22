#pragma once
#include <stdio.h>

struct Reg {
	int r_;
	Reg(int r) : r_(r) {}
};

inline const Reg& getReg0() { static const Reg r(0); return r; }
inline const Reg& getReg1() { static const Reg r(1); return r; }
inline const Reg& getReg2() { static const Reg r(2); return r; }

static const Reg& r0 = getReg0();
static const Reg& r1 = getReg1();
static const Reg& r2 = getReg2();

inline void putReg()
{
	puts("putReg");
	printf("r0=%p, %d\n", &r0, r0.r_);
	printf("r0=%p, %d\n", &r0, r1.r_);
	printf("r0=%p, %d\n", &r0, r2.r_);
}

struct A {
	int a;
	A()
		: a(5)
	{
		puts("A cstr");
	}
	~A()
	{
		puts("A dstr");
	}
	void put() const
	{
		printf("a=%d\n", a);
	}
};

template<int dummy = 0>
struct XT {
	static A a;
};

template<int dummy>
A XT<dummy>::a;

typedef XT<0> X;

void init();

struct Init {
	Init()
	{
		puts("Init");
		init();
		putReg();
	}
};
static Init s_init;

