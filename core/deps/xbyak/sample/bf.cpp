#include "xbyak/xbyak.h"
#include <stdio.h>
#include <stdlib.h>
#include <stack>
#include <fstream>
#ifdef _MSC_VER
	#pragma warning(disable : 4996) // scanf
	#define snprintf _snprintf_s
#endif

class Brainfuck : public Xbyak::CodeGenerator {
public:
	int getContinuousChar(std::istream& is, char c)
	{
		int count = 1;
		char p;
		while (is >> p) {
			if (p != c) break;
			count++;
		}
		is.unget();
		return count;
	}
	Brainfuck(std::istream& is) : CodeGenerator(100000)
	{
		// void (*)(void* putchar, void* getchar, int *stack)
		using namespace Xbyak;
#ifdef XBYAK32
		const Reg32& pPutchar(esi);
		const Reg32& pGetchar(edi);
		const Reg32& stack(ebp);
		const Address cur = byte [stack];
		push(ebp); // stack
		push(esi);
		push(edi);
		const int P_ = 4 * 3;
		mov(pPutchar, ptr[esp + P_ + 4]); // putchar
		mov(pGetchar, ptr[esp + P_ + 8]); // getchar
		mov(stack, ptr[esp + P_ + 12]); // stack
#elif defined(XBYAK64_WIN)
		const Reg64& pPutchar(rsi);
		const Reg64& pGetchar(rdi);
		const Reg64& stack(rbp); // stack
		const Address cur = byte [stack];
		push(rsi);
		push(rdi);
		push(rbp);
		mov(pPutchar, rcx); // putchar
		mov(pGetchar, rdx); // getchar
		mov(stack, r8); // stack
#else
		const Reg64& pPutchar(rbx);
		const Reg64& pGetchar(rbp);
		const Reg64& stack(r12); // stack
		const Address cur = byte [stack];
		push(rbx);
		push(rbp);
		push(r12);
		mov(pPutchar, rdi); // putchar
		mov(pGetchar, rsi); // getchar
		mov(stack, rdx); // stack
#endif
		std::stack<Label> labelF, labelB;
		char c;
		while (is >> c) {
			switch (c) {
			case '+':
			case '-':
				{
					int count = getContinuousChar(is, c);
					if (count == 1) {
						c == '+' ? inc(cur) : dec(cur);
					} else {
						add(cur, (c == '+' ? count : -count));
					}
				}
				break;
			case '>':
			case '<':
				{
					int count = getContinuousChar(is, c);
					add(stack, (c == '>' ? count : -count));
				}
				break;
			case '.':
#ifdef XBYAK32
				push(cur);
				call(pPutchar);
				pop(eax);
#elif defined(XBYAK64_WIN)
				movzx(ecx, cur);
				sub(rsp, 32);
				call(pPutchar);
				add(rsp, 32);
#else
				movzx(edi, cur);
				call(pPutchar);
#endif
				break;
			case ',':
#if defined(XBYAK32) || defined(XBYAK64_GCC)
				call(pGetchar);
#elif defined(XBYAK64_WIN)
				sub(rsp, 32);
				call(pGetchar);
				add(rsp, 32);
#endif
				mov(cur, al);
				break;
			case '[':
				{
					Label B = L();
					labelB.push(B);
					movzx(eax, cur);
					test(eax, eax);
					Label F;
					jz(F, T_NEAR);
					labelF.push(F);
				}
				break;
			case ']':
				{
					Label B = labelB.top(); labelB.pop();
					jmp(B);
					Label F = labelF.top(); labelF.pop();
					L(F);
				}
				break;
			default:
				break;
			}
		}
#ifdef XBYAK32
		pop(edi);
		pop(esi);
		pop(ebp);
#elif defined(XBYAK64_WIN)
		pop(rbp);
		pop(rdi);
		pop(rsi);
#else
		pop(r12);
		pop(rbp);
		pop(rbx);
#endif
		ret();
	}
};

void dump(const uint8_t *code, size_t size)
{
	puts("#include <stdio.h>\nstatic int stack[128 * 1024];");
#ifdef _MSC_VER
	printf("static __declspec(align(4096)) ");
#else
	printf("static __attribute__((aligned(4096)))");
#endif
	puts("const unsigned char code[] = {");
	for (size_t i = 0; i < size; i++) {
		printf("0x%02x,", code[i]); if ((i % 16) == 15) putchar('\n');
	}
	puts("\n};");
#ifdef _MSC_VER
	puts("#include <windows.h>");
#else
	puts("#include <unistd.h>");
	puts("#include <sys/mman.h>");
#endif
	puts("int main()\n{");
#ifdef _MSC_VER
	puts("\tDWORD oldProtect;");
	puts("\tVirtualProtect((void*)code, sizeof(code), PAGE_EXECUTE_READWRITE, &oldProtect);");
#else
	puts("\tlong pageSize = sysconf(_SC_PAGESIZE) - 1;");
	puts("\tmprotect((void*)code, (sizeof(code) + pageSize) & ~pageSize, PROT_READ | PROT_EXEC);");
#endif
	puts(
		"\t((void (*)(void*, void*, int *))code)((void*)putchar, (void*)getchar, stack);\n"
		"}"
	);
}

int main(int argc, char *argv[])
{
#ifdef XBYAK32
	fprintf(stderr, "32bit mode\n");
#else
	fprintf(stderr, "64bit mode\n");
#endif
	if (argc == 1) {
		fprintf(stderr, "bf filename.bf [0|1|2]\n");
		return 1;
	}
	std::ifstream ifs(argv[1]);
	int mode = argc == 3 ? atoi(argv[2]) : 0;
	try {
		Brainfuck bf(ifs);
		switch (mode) {
		case 0: {
			static int stack[128 * 1024];
			bf.getCode<void (*)(const void*, const void*, int *)>()(reinterpret_cast<const void*>(putchar), reinterpret_cast<const void*>(getchar), stack);
			break;
			}
		case 1: {
			dump(bf.getCode(), bf.getSize());
			break;
			}
		default: {
			const char *dumpName = "bf.dump";
			printf("dump to %s\n", dumpName);
			std::ofstream ofs(dumpName, std::ios::binary);
			ofs.write((const char*)bf.getCode(), bf.getSize());
			break;
			}
		}
	} catch (std::exception& e) {
		printf("ERR:%s\n", e.what());
	} catch (...) {
		printf("unknown error\n");
	}
}

