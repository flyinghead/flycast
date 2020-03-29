#include <stdio.h>
#include <string.h>

#define NUM_OF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

void genVsibSub(bool isJIT, const char *name, const char *tbl[], size_t tblSize)
{
	for (size_t i = 0; i < tblSize; i++) {
		if (isJIT) {
			printf("%s (ymm7, ptr[", name);
		} else {
			printf("%s ymm7, [", name);
		}
		printf("%s", tbl[i]);
		if (isJIT) {
			printf("], ymm4); dump();\n");
		} else {
			printf("], ymm4\n");
		}
	}
}
void genVsib(bool isJIT)
{
	if (isJIT) puts("void genVsib() {");
	const char *vm32xTbl[] = {
		"xmm0",
		"xmm0 * 1",
		"xmm0 + 4",
		"xmm0 + eax",
		"xmm0 * 4 + ecx",
		"xmm3 * 8 + edi + 123",
		"xmm2 * 2 + 5",
		"eax + xmm0",
		"esp + xmm4",
	};
	const char *vm32yTbl[] = {
		"ymm0",
		"ymm0 * 1",
		"ymm0 + 4",
		"ymm0 + eax",
		"ymm0 * 4 + ecx",
		"ymm3 * 8 + edi + 123",
		"ymm2 * 2 + 5",
		"eax + ymm0",
		"esp + ymm4",
	};
	genVsibSub(isJIT, "vgatherdpd", vm32xTbl, NUM_OF_ARRAY(vm32xTbl));
	genVsibSub(isJIT, "vgatherqpd", vm32yTbl, NUM_OF_ARRAY(vm32yTbl));
#ifdef XBYAK64
	const char *vm32x64Tbl[] = {
		"xmm0 + r11",
		"r13 + xmm15",
		"123 + rsi + xmm2 * 4",
	};
	genVsibSub(isJIT, "vgatherdpd", vm32x64Tbl, NUM_OF_ARRAY(vm32x64Tbl));
#endif
	if (isJIT) puts("}");
}

void genAddress(bool isJIT, const char regTbl[][5], size_t regTblNum)
{
	int count = 0;
	int funcNum = 1;
	if (isJIT) {
		puts("void gen0(){");
	}
	for (size_t i = 0; i < regTblNum + 1; i++) {
		const char *base = regTbl[i];
		for (size_t j = 0; j < regTblNum + 1; j++) {
			if (j == 4) continue; /* esp is not index register */
			const char *index = regTbl[j];
			static const int scaleTbl[] = { 0, 1, 2, 4, 8 };
			for (size_t k = 0; k < NUM_OF_ARRAY(scaleTbl); k++) {
				int scale = scaleTbl[k];
				static const int dispTbl[] = { 0, 1, 1000, -1, -1000 };
				for (size_t m = 0; m < NUM_OF_ARRAY(dispTbl); m++) {
					int disp = dispTbl[m];
					bool isFirst = true;
					if (isJIT) {
						printf("mov (ecx, ptr[");
					} else {
						printf("mov ecx, [");
					}
					if (i < regTblNum) {
						printf("%s", base);
						isFirst = false;
					}
					if (j < regTblNum) {
						if (!isFirst) putchar('+');
						printf("%s", index);
						if (scale) printf("*%d", scale);
						isFirst = false;
					}
					if (isFirst) {
						if (isJIT) printf("(void*)");
						printf("0x%08X", disp);
					} else {
						if (disp >= 0) {
							putchar('+');
						}
						printf("%d", disp);
						isFirst = false;
					}
					if (isJIT) {
						printf("]); dump();\n");
					} else {
						printf("]\n");
					}
					if (isJIT) {
						count++;
						if ((count % 100) == 0) {
							printf("}\n    void gen%d(){\n", funcNum++);
						}
					}
				}
			}
		}
	}
	if (isJIT) puts("}");
	genVsib(isJIT);
	if (isJIT) {
		printf("void gen(){\n");
		for (int i = 0; i < funcNum; i++) {
			printf("   gen%d();\n", i);
		}
		puts("genVsib();");
		printf("}\n");
	}
}

int main(int argc, char *argv[])
{
	argc--, argv++;
	bool phase = argc > 0 && strcmp(*argv, "1") == 0;
	bool isJIT = (argc > 1);
	fprintf(stderr, "phase:%c %s\n", phase ? '1' : '2', isJIT ? "jit" : "asm");
	if (phase) {
		fprintf(stderr, "32bit reg\n");
		static const char reg32Tbl[][5] = {
			"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
#ifdef XBYAK64
			"r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
#endif
		};
		genAddress(isJIT, reg32Tbl, NUM_OF_ARRAY(reg32Tbl));
	} else {
#ifdef XBYAK64
		fprintf(stderr, "64bit reg\n");
		static const char reg64Tbl[][5] = {
			"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
		};
		genAddress(isJIT, reg64Tbl, NUM_OF_ARRAY(reg64Tbl));
#endif
	}
}
