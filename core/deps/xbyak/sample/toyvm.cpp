/*
	toy vm
	register A, B : 32bit
	PC : program counter

	mem_  4byte x 65536

	すべての命令は4byte固定
	即値は全て16bit

	R = A or B
	vldiR, imm  ; R = imm
	vldR, idx   ; R = mem_[idx]
	vstR, idx   ; mem_[idx] = R
	vaddiR, imm ; R += imm
	vsubiR, imm ; R -= imm
	vaddR, idx  ; R += mem_[idx]
	vsubR, idx  ; R -= mem_[idx]
	vputR       ; print R
	vjnzR, offset; if (R != 0) then jmp(PC += offset(signed))
*/
#if defined(_MSC_VER) && (_MSC_VER <= 1200)
	#pragma warning(disable:4514)
	#pragma warning(disable:4786)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <vector>
#define XBYAK_NO_OP_NAMES
#include "xbyak/xbyak.h"
#include "xbyak/xbyak_util.h"
#define NUM_OF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

#ifdef XBYAK64
	#error "only 32bit"
#endif

using namespace Xbyak;

class ToyVm : public Xbyak::CodeGenerator {
	typedef std::vector<uint32_t> Buffer;
public:
	enum Reg {
		A, B
	};
	enum Code {
		LD, LDI, ST, ADD, ADDI, SUB, SUBI, PUT, JNZ,
		END_OF_CODE
	};
	ToyVm()
		 : mark_(0)
	{
		::memset(mem_, 0, sizeof(mem_));
	}
	void vldi(Reg r, uint16_t imm) { encode(LDI, r, imm); }
	void vld(Reg r, uint16_t idx) { encode(LD, r, idx); }
	void vst(Reg r, uint16_t idx) { encode(ST, r, idx); }
	void vadd(Reg r, uint16_t idx) { encode(ADD, r, idx); }
	void vaddi(Reg r, uint16_t imm) { encode(ADDI, r, imm); }
	void vsub(Reg r, uint16_t idx) { encode(SUB, r, idx); }
	void vsubi(Reg r, uint16_t imm) { encode(SUBI, r, imm); }
	void vjnz(Reg r, int offset) { encode(JNZ, r, static_cast<uint16_t>(offset)); }
	void vput(Reg r) { encode(PUT, r); }
	void setMark()
	{
		mark_ = (int)code_.size();
	}
	int getMarkOffset()
	{
		return mark_ - (int)code_.size() - 1;
	}
	void run()
	{
		bool debug = false;//true;
		uint32_t reg[2] = { 0, 0 };
		const size_t end = code_.size();
		uint32_t pc = 0;
		for (;;) {
			uint32_t x = code_[pc];
			uint32_t code, r, imm;
			decode(code, r, imm, x);
			if (debug) {
				printf("---\n");
				printf("A %08x B %08x\n", reg[0], reg[1]);
				printf("mem_[] = %08x %08x %08x\n", mem_[0], mem_[1], mem_[2]);
				printf("pc=%4d, code=%02x, r=%d, imm=%04x\n", pc, code, r, imm);
			}
			switch (code) {
			case LDI:
				reg[r] = imm;
				break;
			case LD:
				reg[r] = mem_[imm];
				break;
			case ST:
				mem_[imm] = reg[r];
				break;
			case ADD:
				reg[r] += mem_[imm];
				break;
			case ADDI:
				reg[r] += imm;
				break;
			case SUB:
				reg[r] -= mem_[imm];
				break;
			case SUBI:
				reg[r] -= imm;
				break;
			case PUT:
				printf("%c %8d(0x%08x)\n", 'A' + r, reg[r], reg[r]);
				break;
			case JNZ:
				if (reg[r] != 0) pc += static_cast<signed short>(imm);
				break;
			default:
				assert(0);
				break;
			}
			pc++;
			if (pc >= end) break;
		} // for (;;)
	}
	void recompile()
	{
		using namespace Xbyak;
		/*
			esi : A
			edi : B
			ebx : mem_
			for speed up
			mem_[0] : eax
			mem_[1] : ecx
			mem_[2] : edx
		*/
		push(ebx);
		push(esi);
		push(edi);

		const Reg32 reg[2] = { esi, edi };
		const Reg32 mem(ebx);

		const Reg32 memTbl[] = { eax, ecx, edx };
		const size_t memTblNum = NUM_OF_ARRAY(memTbl);
		for (size_t i = 0; i < memTblNum; i++) xor_(memTbl[i], memTbl[i]);

		xor_(esi, esi);
		xor_(edi, edi);
		mov(mem, (size_t)mem_);
		const size_t end = code_.size();
		uint32_t pc = 0;
		uint32_t labelNum = 0;
		for (;;) {
			uint32_t x = code_[pc];
			uint32_t code, r, imm;
			decode(code, r, imm, x);
		L(Label::toStr(labelNum++));
			switch (code) {
			case LDI:
				mov(reg[r], imm);
				break;
			case LD:
				if (imm < memTblNum) {
					mov(reg[r], memTbl[imm]);
				} else {
					mov(reg[r], ptr[mem + imm * 4]);
				}
				break;
			case ST:
				if (imm < memTblNum) {
					mov(memTbl[imm], reg[r]);
				} else {
					mov(ptr [mem + imm * 4], reg[r]);
				}
				break;
			case ADD:
				if (imm < memTblNum) {
					add(reg[r], memTbl[imm]);
				} else {
					add(reg[r], ptr [mem + imm * 4]);
				}
				break;
			case ADDI:
				add(reg[r], imm);
				break;
			case SUB:
				if (imm < memTblNum) {
					sub(reg[r], memTbl[imm]);
				} else {
					sub(reg[r], ptr [mem + imm * 4]);
				}
				break;
			case SUBI:
				sub(reg[r], imm);
				break;
			case PUT:
				{
					static const char *str = "%c %8d(0x%08x)\n";
					push(eax);
					push(edx);
					push(ecx);
					push(reg[r]);
					push(reg[r]);
					push('A' + r);
					push((int)str);
					call(reinterpret_cast<const void*>(printf));
					add(esp, 4 * 4);
					pop(ecx);
					pop(edx);
					pop(eax);
				}
				break;
			case JNZ:
				test(reg[r], reg[r]);
				jnz(Label::toStr(labelNum + static_cast<signed short>(imm)));
				break;
			default:
				assert(0);
				break;
			}
			pc++;
			if (pc >= end) break;
		} // for (;;)

		pop(edi);
		pop(esi);
		pop(ebx);
		ret();
	}
private:
	uint32_t mem_[65536];
	Buffer code_;
	int mark_;
	void decode(uint32_t& code, uint32_t& r, uint32_t& imm, uint32_t x)
	{
		code = x >> 24;
		r = (x >> 16) & 0xff;
		imm = x & 0xffff;
	}
	void encode(Code code, Reg r, uint16_t imm = 0)
	{
		uint32_t x = (code << 24) | (r << 16) | imm;
		code_.push_back(x);
	}
};

class Fib : public ToyVm {
public:
	Fib(int n)
	{
		if (n >= 65536) {
			fprintf(stderr, "current version support only imm16\n");
			return;
		}
		/*
			A : c
			B : temporary
			mem_[0] : p
			mem_[1] : t
			mem_[2] : n
		*/
		vldi(A, 1); // c
		vst(A, 0); // p(1)
		vldi(B, static_cast<uint16_t>(n));
		vst(B, 2); // n
		// lp
	setMark();
		vst(A, 1); // t = c
		vadd(A, 0); // c += p
		vld(B, 1);
		vst(B, 0); // p = t
//		vput(A);
		vld(B, 2);
		vsubi(B, 1);
		vst(B, 2); // n--
		vjnz(B, getMarkOffset());
		vput(A);
	}
	void runByJIT()
	{
		getCode<void (*)()>();
	}
};

void fibC(uint32_t n)
{
	uint32_t p, c, t;
	p = 1;
	c = 1;
lp:
	t = c;
	c += p;
	p = t;
	n--;
	if (n != 0) goto lp;
	printf("c=%d(0x%08x)\n", c, c);
}

int main()
{
	try {
		const int n = 10000;
		Fib fib(n);

		fib.recompile();

		{
			Xbyak::util::Clock clk;
			clk.begin();
			fib.run();
			clk.end();
			printf("vm       %.2fKclk\n", clk.getClock() * 1e-3);
		}

		{
			Xbyak::util::Clock clk;
			clk.begin();
			fib.runByJIT();
			clk.end();
			printf("jit      %.2fKclk\n", clk.getClock() * 1e-3);
		}

		{
			Xbyak::util::Clock clk;
			clk.begin();
			fibC(n);
			clk.end();
			printf("native C %.2fKclk\n", clk.getClock() * 1e-3);
		}
	} catch (std::exception& e) {
		printf("ERR:%s\n", e.what());
	} catch (...) {
		printf("unknown error\n");
	}
	return 0;
}

/*
	the code generated by Xbyak
   push        ebx
   push        esi
   push        edi
   xor         eax,eax
   xor         ecx,ecx
   xor         edx,edx
   xor         esi,esi
   xor         edi,edi
   mov         ebx,0EFF58h
   mov         esi,1
   mov         eax,esi
   mov         edi,2710h
   mov         edx,edi
.lp:
   mov         ecx,esi
   add         esi,eax
   mov         edi,ecx
   mov         eax,edi
   mov         edi,edx
   sub         edi,1
   mov         edx,edi
   test        edi,edi
   jne         .lp
   push        eax
   push        edx
   push        ecx
   push        esi
   push        esi
   push        41h
   push        42C434h
   call        printf (409342h)
   add         esp,10h
   pop         ecx
   pop         edx
   pop         eax
   pop         edi
   pop         esi
   pop         ebx
   ret
*/
