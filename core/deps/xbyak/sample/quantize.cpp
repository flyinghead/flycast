/*
	@author herumi

	JPEG quantize sample
	This program generates a quantization routine by using fast division algorithm in run-time.

	time(sec)
	quality  1(high) 10     50   100(low)
	VC2005   8.0     8.0   8.0  8.0
	Xbyak    1.6     0.8   0.5  0.5


; generated code at q = 1
    push        esi
    push        edi
    mov         edi,dword ptr [esp+0Ch]
    mov         esi,dword ptr [esp+10h]
    mov         eax,dword ptr [esi]
    shr         eax,4
    mov         dword ptr [edi],eax
    mov         eax,dword ptr [esi+4]
    mov         edx,0BA2E8BA3h
    mul         eax,edx
    shr         edx,3
    ...

; generated code at q = 100
     push        esi
     push        edi
     mov         edi,dword ptr [esp+0Ch]
     mov         esi,dword ptr [esp+10h]
     mov         eax,dword ptr [esi]
     mov         dword ptr [edi],eax
     mov         eax,dword ptr [esi+4]
     mov         dword ptr [edi+4],eax
     mov         eax,dword ptr [esi+8]
     mov         dword ptr [edi+8],eax
     mov         eax,dword ptr [esi+0Ch]
	 ...

*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define XBYAK_NO_OP_NAMES
#include "xbyak/xbyak.h"
#ifdef _MSC_VER
	#pragma warning(disable : 4996) // scanf
#endif

typedef Xbyak::uint64 uint64;
typedef Xbyak::uint32 uint32;

const int N = 64;

class Quantize : public Xbyak::CodeGenerator {
	static int ilog2(int x)
	{
		int shift = 0;
		while ((1 << shift) <= x) shift++;
		return shift - 1;
	}
public:
	/*
		input : esi
		output : eax = [esi+offset] / dividend
		destroy : edx
	*/
	void udiv(uint32 dividend, int offset)
	{
		mov(eax, ptr[esi + offset]);

		/* dividend = odd x 2^exponent */
		int exponent = 0, odd = dividend;
		while ((odd & 1) == 0) {
			odd >>= 1; exponent++;
		}

		if (odd == 1) { // trivial case
			if (exponent) {
				shr(eax, exponent);
			}
			return;
		}

		uint64 mLow, mHigh;
		int len = ilog2(odd) + 1;
		{
			uint64 roundUp = uint64(1) << (32 + len);
			uint64 k = roundUp / (0xFFFFFFFFL - (0xFFFFFFFFL % odd));
			mLow = roundUp / odd;
			mHigh = (roundUp + k) / odd;
		}

		while (((mLow >> 1) < (mHigh >> 1)) && (len > 0)) {
			mLow >>= 1; mHigh >>= 1; len--;
		}

		uint64 m; int a;
		if ((mHigh >> 32) == 0) {
			m = mHigh; a = 0;
		} else {
			len = ilog2(odd);
			uint64 roundDown = uint64(1) << (32 + len);
			mLow = roundDown / odd;
			int r = (int)(roundDown % odd);
			m = (r <= (odd >> 1)) ? mLow : mLow + 1;
			a = 1;
		}
		while ((m & 1) == 0) {
			m >>= 1; len--;
		}
		len += exponent;

		mov(edx, int(m));
		mul(edx);
		if (a) {
			add(eax, int(m));
			adc(edx, 0);
		}
		if (len) {
			shr(edx, len);
		}
		mov(eax, edx);
	}
	/*
		quantize(uint32 dest[64], const uint32 src[64]);
	*/
	Quantize(const uint32 qTbl[64])
	{
		push(esi);
		push(edi);
		const int P_ = 4 * 2;
		mov(edi, ptr [esp+P_+4]); // dest
		mov(esi, ptr [esp+P_+8]); // src
		for (int i = 0; i < N; i++) {
			udiv(qTbl[i], i * 4);
			mov(ptr[edi+i*4], eax);
		}
		pop(edi);
		pop(esi);
		ret();
	}
};

void quantize(uint32 dest[64], const uint32 src[64], const uint32 qTbl[64])
{
	for (int i = 0; i < N; i++) {
		dest[i] = src[i] / qTbl[i];
	}
}

#ifdef XBYAK64
int main()
{
	puts("not implemented for 64bit");
	return 1;
}
#else
int main(int argc, char *argv[])
{
	int q;
	if (argc > 1) {
		q = atoi(argv[1]);
	} else {
		printf("input quantize=");
		if (scanf("%d", &q) != 1) {
			fprintf(stderr, "bad number\n");
			return 1;
		}
	}
	printf("q=%d\n", q);
	uint32 qTbl[] = {
		16, 11, 10, 16, 24, 40, 51, 61,
		12, 12, 14, 19, 26, 58, 60, 55,
		14, 13, 16, 24, 40, 57, 69, 56,
		14, 17, 22, 29, 51, 87, 80, 62,
		18, 22, 37, 56, 68, 109, 103, 77,
		24, 35, 55, 64, 81, 104, 113, 92,
		49, 64, 78, 87, 103, 121, 120, 101,
		72, 92, 95, 98, 112, 100, 103, 99
	};

	for (int i = 0; i < N; i++) {
		qTbl[i] /= q;
		if (qTbl[i] == 0) qTbl[i] = 1;
	}

	try {
		uint32 src[N];
		uint32 dest[N];
		uint32 dest2[N];
		for (int i = 0; i < N; i++) {
			src[i] = rand() % 2048;
		}

		Quantize jit(qTbl);
//printf("jit size=%d, ptr=%p\n", jit.getSize(), jit.getCode());
		void (*quantize2)(uint32*, const uint32*, const uint32 *) = jit.getCode<void (*)(uint32*, const uint32*, const uint32 *)>();

		quantize(dest, src, qTbl);
		quantize2(dest2, src, qTbl);
		for (int i = 0; i < N; i++) {
			if (dest[i] != dest2[i]) {
				printf("err[%d] %d %d\n", i, dest[i], dest2[i]);
			}
		}

		const int count = 10000000;
		int begin;

		begin = clock();
		for (int i = 0; i < count; i++) {
			quantize(dest, src, qTbl);
		}
		printf("time=%.1fsec\n", (clock() - begin) / double(CLOCKS_PER_SEC));

		begin = clock();
		for (int i = 0; i < count; i++) {
			quantize2(dest, src, qTbl);
		}
		printf("time=%.1fsec\n", (clock() - begin) / double(CLOCKS_PER_SEC));
	} catch (std::exception& e) {
		printf("ERR:%s\n", e.what());
	} catch (...) {
		printf("unknown error\n");
	}
}
#endif
