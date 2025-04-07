#include <stdio.h>
#include <xbyak/xbyak.h>

struct Code : Xbyak::CodeGenerator {
	Code(bool nf) {
		xor_(eax, eax); // CF = 0
		mov(eax, -1);
		if (nf) {
			puts("no flags (with T_nf)");
			add(eax|T_nf, eax, 1); // does not change CF
		} else {
			puts("change flags (without T_nf)");
			add(eax, eax, 1); // CF = 1
		}
		adc(eax, 0); // eax = CF ? 1 : 0
		ret();
	}
};

int main() {
	for (int i = 0; i < 2; i++) {
		Code c(i);
		printf("i=%d ret=%d\n", i, c.getCode<int(*)()>()());
	}
}
