#define XBYAK_DONT_READ_LIST
#include <stdio.h>
#include <string.h>
#include "xbyak/xbyak.h"
#define NUM_OF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

using namespace Xbyak;
#ifdef _MSC_VER
	#pragma warning(disable : 4996) // scanf
	#define snprintf _snprintf_s
#endif

#include "avx_type.hpp"
/*
	reg = cx/ecx/rcx
	insert 0x67 if prefix is true
*/
void put_jREGz(const char *reg, bool prefix)
{
	printf("void j%sz(std::string label) { %sopJmp(label, T_SHORT, 0xe3, 0, 0); }\n", reg, prefix ? "db(0x67); " : "");
	printf("void j%sz(const Label& label) { %sopJmp(label, T_SHORT, 0xe3, 0, 0); }\n", reg, prefix ? "db(0x67); " : "");
}

struct GenericTbl {
	const char *name;
	uint8_t code1;
	uint8_t code2;
	uint8_t code3;
	uint8_t code4;
};

void putGeneric(const GenericTbl *p, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		printf("void %s() { db(0x%02X); ", p->name, p->code1);
		if (p->code2) printf("db(0x%02X); ", p->code2);
		if (p->code3) printf("db(0x%02X); ", p->code3);
		if (p->code4) printf("db(0x%02X); ", p->code4);
		printf("}\n");
		p++;
	}
}

void putX_X_XM(bool omitOnly)
{
	// (x, x, x/m[, imm]) or (y, y, y/m[, imm])
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
			bool hasIMM;
			bool enableOmit;
			int mode; // 1 : sse, 2 : avx, 3 : sse + avx
		} tbl[] = {
			{ 0x0D, "blendpd", T_0F3A | T_66 | T_W0 | T_YMM, true, true, 3 },
			{ 0x0C, "blendps", T_0F3A | T_66 | T_W0 | T_YMM, true, true, 3 },
			{ 0x41, "dppd", T_0F3A | T_66 | T_W0, true, true, 3 },
			{ 0x40, "dpps", T_0F3A | T_66 | T_W0 | T_YMM, true, true, 3 },
			{ 0x42, "mpsadbw", T_0F3A | T_66 | T_W0 | T_YMM, true, true, 1 },
			{ 0x0E, "pblendw", T_0F3A | T_66 | T_W0 | T_YMM, true, true, 3 },
			{ 0x02, "pblendd", T_0F3A | T_66 | T_W0 | T_YMM, true, true, 2 },
			{ 0x0B, "roundsd", T_0F3A | T_66 | T_W0, true, true, 3 },
			{ 0x0A, "roundss", T_0F3A | T_66 | T_W0, true, true, 3 },
			{ 0x44, "pclmulqdq", T_0F3A | T_66 | T_W0 | T_YMM | T_EVEX, true, true, 3 },
			{ 0x0C, "permilps", T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_EW0 | T_B32, false, false, 2 },
			{ 0x0D, "permilpd", T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_EW1 | T_B64, false, false, 2 },

			{ 0x47, "psllvd", T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_EW0 | T_B32, false, false, 2 },
			{ 0x47, "psllvq", T_0F38 | T_66 | T_W1 | T_YMM | T_EVEX | T_EW1 | T_B64, false, false, 2 },
			{ 0x46, "psravd", T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_EW0 | T_B32, false, false, 2 },
			{ 0x45, "psrlvd", T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_EW0 | T_B32, false, false, 2 },
			{ 0x45, "psrlvq", T_0F38 | T_66 | T_W1 | T_YMM | T_EVEX | T_EW1 | T_B64, false, false, 2 },

			{ 0xC2, "cmppd", T_0F | T_66 | T_YMM, true, true, 2 },
			{ 0xC2, "cmpps", T_0F | T_YMM, true, true, 2 },
			{ 0xC2, "cmpsd", T_0F | T_F2, true, true, 2 },
			{ 0xC2, "cmpss", T_0F | T_F3, true, true, 2 },
			{ 0x5A, "cvtsd2ss", T_0F | T_F2 | T_EVEX | T_EW1 | T_N8 | T_ER_X, false, true, 2 },
			{ 0x5A, "cvtss2sd", T_0F | T_F3 | T_EVEX | T_EW0 | T_N4 | T_SAE_X, false, true, 2 },
			{ 0x21, "insertps", T_0F3A | T_66 | T_W0 | T_EVEX | T_EW0 | T_N4, true, true, 2 },
			{ 0x63, "packsswb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x6B, "packssdw", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 2 },
			{ 0x67, "packuswb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x2B, "packusdw", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 3 },

			{ 0xFC, "paddb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xFD, "paddw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xFE, "paddd", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 2 },
			{ 0xD4, "paddq", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 2 },

			{ 0xEC, "paddsb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xED, "paddsw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },

			{ 0xDC, "paddusb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xDD, "paddusw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },

			{ 0x0F, "palignr", T_0F3A | T_66 | T_YMM | T_EVEX, true, true, 2 },

			{ 0xDB, "pand", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0xDF, "pandn", T_0F | T_66 | T_YMM, false, true, 2 },

			{ 0xE0, "pavgb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xE3, "pavgw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },

			{ 0x74, "pcmpeqb", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0x75, "pcmpeqw", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0x76, "pcmpeqd", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0x29, "pcmpeqq", T_0F38 | T_66 | T_YMM, false, true, 3 },

			{ 0x64, "pcmpgtb", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0x65, "pcmpgtw", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0x66, "pcmpgtd", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0x37, "pcmpgtq", T_0F38 | T_66 | T_YMM, false, true, 3 },

			{ 0x01, "phaddw", T_0F38 | T_66 | T_YMM, false, true, 2 },
			{ 0x02, "phaddd", T_0F38 | T_66 | T_YMM, false, true, 2 },
			{ 0x03, "phaddsw", T_0F38 | T_66 | T_YMM, false, true, 2 },

			{ 0x05, "phsubw", T_0F38 | T_66 | T_YMM, false, true, 2 },
			{ 0x06, "phsubd", T_0F38 | T_66 | T_YMM, false, true, 2 },
			{ 0x07, "phsubsw", T_0F38 | T_66 | T_YMM, false, true, 2 },
			{ 0xF5, "pmaddwd", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x04, "pmaddubsw", T_0F38 | T_66 | T_YMM | T_EVEX, false, true, 2 },

			{ 0x3C, "pmaxsb", T_0F38 | T_66 | T_YMM | T_EVEX, false, true, 3 },
			{ 0xEE, "pmaxsw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x3D, "pmaxsd", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 3 },

			{ 0xDE, "pmaxub", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x3E, "pmaxuw", T_0F38 | T_66 | T_YMM | T_EVEX, false, true, 3 },
			{ 0x3F, "pmaxud", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 3 },

			{ 0x38, "pminsb", T_0F38 | T_66 | T_YMM | T_EVEX, false, true, 3 },
			{ 0xEA, "pminsw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x39, "pminsd", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 3 },

			{ 0xDA, "pminub", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x3A, "pminuw", T_0F38 | T_66 | T_YMM | T_EVEX, false, true, 3 },
			{ 0x3B, "pminud", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 3 },

			{ 0xE4, "pmulhuw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x0B, "pmulhrsw", T_0F38 | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xE5, "pmulhw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xD5, "pmullw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x40, "pmulld", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 3 },

			{ 0xF4, "pmuludq", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 2 },
			{ 0x28, "pmuldq", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 3 },

			{ 0xEB, "por", T_0F | T_66 | T_YMM, false, true, 2 },
			{ 0xF6, "psadbw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },

			{ 0x00, "pshufb", T_0F38 | T_66 | T_YMM | T_EVEX, false, false, 2 },

			{ 0x08, "psignb", T_0F38 | T_66 | T_YMM, false, true, 2 },
			{ 0x09, "psignw", T_0F38 | T_66 | T_YMM, false, true, 2 },
			{ 0x0A, "psignd", T_0F38 | T_66 | T_YMM, false, true, 2 },

			{ 0xF1, "psllw", T_0F | T_66 | T_YMM | T_EVEX | T_N16, false, true, 2 },
			{ 0xF2, "pslld", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_N16, false, true, 2 },
			{ 0xF3, "psllq", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_N16, false, true, 2 },

			{ 0xE1, "psraw", T_0F | T_66 | T_YMM | T_EVEX | T_N16, false, true, 2 },
			{ 0xE2, "psrad", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_N16, false, true, 2 },
			{ 0xD1, "psrlw", T_0F | T_66 | T_YMM | T_EVEX | T_N16, false, true, 2 },
			{ 0xD2, "psrld", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_N16, false, true, 2 },
			{ 0xD3, "psrlq", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_N16, false, true, 2 },

			{ 0xF8, "psubb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xF9, "psubw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xFA, "psubd", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 2 },
			{ 0xFB, "psubq", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 2 },

			{ 0xE8, "psubsb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xE9, "psubsw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },

			{ 0xD8, "psubusb", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0xD9, "psubusw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },

			{ 0x68, "punpckhbw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x69, "punpckhwd", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x6A, "punpckhdq", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 2 },
			{ 0x6D, "punpckhqdq", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 2 },

			{ 0x60, "punpcklbw", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x61, "punpcklwd", T_0F | T_66 | T_YMM | T_EVEX, false, true, 2 },
			{ 0x62, "punpckldq", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 2 },
			{ 0x6C, "punpcklqdq", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 2 },

			{ 0xEF, "pxor", T_0F | T_66 | T_YMM, false, true, 2 },

			{ 0x53, "rcpss", T_0F | T_F3, false, true, 2 },
			{ 0x52, "rsqrtss", T_0F | T_F3, false, true, 2 },

			{ 0xC6, "shufpd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, true, true, 2 },
			{ 0xC6, "shufps", T_0F | T_YMM | T_EVEX | T_EW0 | T_B32, true, true, 2 },

			{ 0x51, "sqrtsd", T_0F | T_F2 | T_EVEX | T_EW1 | T_ER_X | T_N8, false, true, 2 },
			{ 0x51, "sqrtss", T_0F | T_F3 | T_EVEX | T_EW0 | T_ER_X | T_N4, false, true, 2 },

			{ 0x15, "unpckhpd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 2 },
			{ 0x15, "unpckhps", T_0F | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 2 },

			{ 0x14, "unpcklpd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, false, true, 2 },
			{ 0x14, "unpcklps", T_0F | T_YMM | T_EVEX | T_EW0 | T_B32, false, true, 2 },

			{ 0xCF, "gf2p8affineinvqb", T_66 | T_0F3A | T_W1 | T_EVEX | T_YMM | T_EW1 | T_SAE_Z | T_B64, true, false, 3 },
			{ 0xCE, "gf2p8affineqb", T_66 | T_0F3A | T_W1 | T_EVEX | T_YMM | T_EW1 | T_SAE_Z | T_B64, true, false, 3 },
			{ 0xCF, "gf2p8mulb", T_66 | T_0F38 | T_W0 | T_EVEX | T_YMM | T_EW0 | T_SAE_Z, false, false, 3 },
			{ 0xDA, "sm3msg1", T_0F38 | T_W0 | T_EVEX | T_EW0, false, false, 2 },
			{ 0xDA, "sm3msg2", T_66 | T_0F38 | T_W0 | T_EVEX | T_EW0, false, false, 2 },
			{ 0xDE, "sm3rnds2", T_66 | T_0F3A | T_W0 | T_EVEX | T_EW0, true, false, 2 },
			{ 0xDA, "sm4key4", T_F3 | T_0F38 | T_W0 | T_EVEX | T_EW0, false, false, 2 },
			{ 0xDA, "sm4rnds4", T_F2 | T_0F38 | T_W0 | T_EVEX | T_EW0, false, false, 2 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type);
			if (omitOnly) {
				if (p->enableOmit) {
					printf("void v%s(const Xmm& x, const Operand& op%s) { v%s(x, x, op%s); }\n", p->name, p->hasIMM ? ", uint8_t imm" : "", p->name, p->hasIMM ? ", imm" : "");
				}
			} else {
				if (p->mode & 1) {
					if (p->hasIMM) {
						printf("void %s(const Xmm& xmm, const Operand& op, int imm) { opSSE(xmm, op, T_66 | T_0F3A, 0x%02X, isXMM_XMMorMEM, static_cast<uint8_t>(imm)); }\n", p->name, p->code);
					} else {
						printf("void %s(const Xmm& xmm, const Operand& op) { opSSE(xmm, op, T_66 | T_0F38, 0x%02X, isXMM_XMMorMEM); }\n", p->name, p->code);
					}
				}
				if (p->mode & 2) {
					printf("void v%s(const Xmm& x1, const Xmm& x2, const Operand& op%s) { opAVX_X_X_XM(x1, x2, op, %s, 0x%02X%s); }\n"
					, p->name, p->hasIMM ? ", uint8_t imm" : "", s.c_str(), p->code, p->hasIMM ? ", imm" : "");
				}
			}
		}
	}
}

void putMemOp(const char *name, const char *type, uint8_t ext, uint8_t code, int bit, int fwait = false)
{
	printf("void %s(const Address& addr) { %sopMR(addr, Reg%d(%d), %s, 0x%02X); }\n", name, fwait ? "db(0x9B); " : "", bit, ext, type, code);
}

void putLoadSeg(const char *name, uint64_t type, uint8_t code)
{
	printf("void %s(const Reg& reg, const Address& addr) { opLoadSeg(addr, reg, %s, 0x%02X); }\n", name, type ? "T_0F" : "T_NONE", code);
}

void put()
{
	{
		char buf[16];
		unsigned int v = VERSION;
		if (v & 0xF) {
			snprintf(buf, sizeof(buf), "%d.%02X.%x", v >> 12, (v >> 4) & 0xFF, v & 0xF);
		} else {
			snprintf(buf, sizeof(buf), "%d.%02X", v >> 12, (v >> 4) & 0xFF);
		}
		printf("const char *getVersionString() const { return \"%s\"; }\n", buf);
	}
	const int B = 1 << 0;
	const int W = 1 << 1;
	const int D = 1 << 2;
	const int Q = 1 << 3;
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
		} tbl[] = {
			// MMX
			{ 0x6B, "packssdw" },
			{ 0x63, "packsswb" },
			{ 0x67, "packuswb" },

			{ 0xDB, "pand" },
			{ 0xDF, "pandn" },

			{ 0xF5, "pmaddwd" },
			{ 0xE4, "pmulhuw" },
			{ 0xE5, "pmulhw" },
			{ 0xD5, "pmullw" },

			{ 0xEB, "por" },

			{ 0x68, "punpckhbw" },
			{ 0x69, "punpckhwd" },
			{ 0x6A, "punpckhdq" },

			{ 0x60, "punpcklbw" },
			{ 0x61, "punpcklwd" },
			{ 0x62, "punpckldq" },

			{ 0xEF, "pxor" },

			// MMX2
			{ 0xE0, "pavgb" },
			{ 0xE3, "pavgw" },
			{ 0xEE, "pmaxsw" },
			{ 0xDE, "pmaxub" },
			{ 0xEA, "pminsw" },
			{ 0xDA, "pminub" },
			{ 0xF6, "psadbw" },
			//
			{ 0xD4, "paddq" },
			{ 0xF4, "pmuludq" },
			{ 0xFB, "psubq" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Mmx& mmx, const Operand& op) { opMMX(mmx, op, 0x%02X); }\n"
				, p->name, p->code);
		}
	}

	{
		const struct Tbl {
			uint8_t code;
			int mode;
			const char *name;
		} tbl[] = {
			{ 0xFC, B|W|D, "padd" },
			{ 0xEC, B|W , "padds" },
			{ 0xDC, B|W , "paddus" },
			{ 0x74, B|W|D, "pcmpeq" },
			{ 0x64, B|W|D, "pcmpgt" },
			{ 0xF0, W|D|Q, "psll" },
			{ 0xE0, W|D , "psra" },
			{ 0xD0, W|D|Q, "psrl" },
			{ 0xF8, B|W|D, "psub" },
			{ 0xE8, B|W , "psubs" },
			{ 0xD8, B|W , "psubus" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			static const char modTbl[][4] = {
				"b", "w", "d", "q"
			};
			for (int j = 0; j < 4; j++) {
				// B(0), W(1), D(2), Q(3)
				if (!(p->mode & (1 << j))) continue;
				printf("void %s%s(const Mmx& mmx, const Operand& op) { opMMX(mmx, op, 0x%02X); }\n"
					, p->name, modTbl[j]
					, p->code | j
				);
			}
		}
	}

	{
		const struct Tbl {
			uint8_t code;
			int ext;
			int mode;
			const char *name;
		} tbl[] = {
			{ 0x70, 6, W|D|Q, "psll" },
			{ 0x70, 4, W|D , "psra" },
			{ 0x70, 2, W|D|Q, "psrl" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			static const char modTbl[][4] = {
				"b", "w", "d", "q"
			};
			for (int j = 0; j < 4; j++) {
				// B(0), W(1), D(2), Q(3)
				if (!(p->mode & (1 << j))) continue;
				printf("void %s%s(const Mmx& mmx, int imm8) { opMMX_IMM(mmx, imm8, 0x%02X, %d); }\n"
					, p->name, modTbl[j]
					, p->code | j
					, p->ext
				);
			}
		}
		printf("void pslldq(const Xmm& xmm, int imm8) { opMMX_IMM(xmm, imm8, 0x%02X, %d); }\n", 0x73, 7);
		printf("void psrldq(const Xmm& xmm, int imm8) { opMMX_IMM(xmm, imm8, 0x%02X, %d); }\n", 0x73, 3);
	}

	{
		const struct Tbl {
			const char *pref;
			const char *name;
		} tbl[] = {
			{ "T_NONE", "pshufw" },
			{ "T_F2", "pshuflw" },
			{ "T_F3", "pshufhw" },
			{ "T_66", "pshufd" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Mmx& mmx, const Operand& op, uint8_t imm8) { opMMX(mmx, op, 0x70, T_0F, %s, imm8); }\n", p->name, p->pref);
		}
	}
	{
		const struct MmxTbl6 {
			uint8_t code; // for (reg, reg/[mem])
			uint8_t code2; // for ([mem], reg)
			const char *pref;
			const char *name;
		} mmxTbl6[] = {
			{ 0x6F, 0x7F, "T_66", "movdqa" },
			{ 0x6F, 0x7F, "T_F3", "movdqu" },
			// SSE2
			{ 0x28, 0x29, "T_NONE", "movaps" },
			{ 0x10, 0x11, "T_F3", "movss" },
			{ 0x10, 0x11, "T_NONE", "movups" },
			{ 0x28, 0x29, "T_66", "movapd" },
			{ 0x10, 0x11, "T_F2", "movsd" },
			{ 0x10, 0x11, "T_66", "movupd" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(mmxTbl6); i++) {
			const MmxTbl6 *p = &mmxTbl6[i];
			printf("void %s(const Xmm& xmm, const Operand& op) { opMMX(xmm, op, 0x%02X, T_0F, %s); }\n", p->name, p->code, p->pref);
			printf("void %s(const Address& addr, const Xmm& xmm) { opSSE(xmm, addr, T_0F|%s, 0x%02X); }\n", p->name, p->pref, p->code2);
		}
	}
	{
		enum {
			PS = 1 << 0,
			SS = 1 << 1,
			PD = 1 << 2,
			SD = 1 << 3
		};
		const struct {
			const char *type;
			const char *name;
		} sufTbl[] = {
			{ "T_0F", "ps" },
			{ "T_0F | T_F3", "ss" },
			{ "T_0F | T_66", "pd" },
			{ "T_0F | T_F2", "sd" },
		};
		const struct Tbl {
			uint8_t code;
			int mode;
			const char *name;
			bool hasImm;
		} tbl[] = {
			{ 0x58, PS|SS|PD|SD, "add" },
			{ 0x55, PS|PD     , "andn" },
			{ 0x54, PS|PD     , "and" },
			{ 0xC2, PS|SS|PD|SD, "cmp", true },
			{ 0x5E, PS|SS|PD|SD, "div" },
			{ 0x5F, PS|SS|PD|SD, "max" },
			{ 0x5D, PS|SS|PD|SD, "min" },
			{ 0x59, PS|SS|PD|SD, "mul" },
			{ 0x56, PS|PD     , "or" },
			{ 0x53, PS|SS     , "rcp" },
			{ 0x52, PS|SS     , "rsqrt" },
			{ 0xC6, PS|PD     , "shuf", true },
			{ 0x51, PS|SS|PD|SD, "sqrt" },
			{ 0x5C, PS|SS|PD|SD, "sub" },
			{ 0x15, PS|PD     , "unpckh" },
			{ 0x14, PS|PD     , "unpckl" },
			{ 0x57, PS|PD     , "xor" },
			//
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			for (size_t j = 0; j < NUM_OF_ARRAY(sufTbl); j++) {
				if (!(p->mode & (1 << j))) continue;
				if (p->hasImm) {
					// don't change uint8_t to int because NO is not in byte
					printf("void %s%s(const Xmm& xmm, const Operand& op, uint8_t imm8) { opSSE(xmm, op, %s, 0x%02X, isXMM_XMMorMEM, imm8); }\n", p->name, sufTbl[j].name, sufTbl[j].type, p->code);
				} else {
					printf("void %s%s(const Xmm& xmm, const Operand& op) { opSSE(xmm, op, %s, 0x%02X, isXMM_XMMorMEM); }\n", p->name, sufTbl[j].name, sufTbl[j].type, p->code);
				}
			}
		}
	}
	{
		// (XMM, XMM)
		const struct Tbl {
			uint64_t type;
			uint8_t code;
			const char *name;
		} tbl[] = {
			{ T_66 | T_0F, 0xF7, "maskmovdqu" },
			{ T_0F, 0x12, "movhlps" },
			{ T_0F, 0x16, "movlhps" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type);
			printf("void %s(const Xmm& reg1, const Xmm& reg2) { opSSE(reg1, reg2, %s, 0x%02X); }\n", p->name, s.c_str(), p->code);
		}
	}
	{
		// (XMM, XMM|MEM)
		const struct Tbl {
			uint8_t code;
			uint64_t type;
			const char *name;
		} tbl[] = {
			{ 0x6D, T_66, "punpckhqdq" },
			{ 0x6C, T_66, "punpcklqdq" },

			{ 0x2F, T_NONE, "comiss" },
			{ 0x2E, T_NONE, "ucomiss" },
			{ 0x2F, T_66, "comisd" },
			{ 0x2E, T_66, "ucomisd" },

			{ 0x5A, T_66, "cvtpd2ps" },
			{ 0x5A, T_NONE, "cvtps2pd" },
			{ 0x5A, T_F2, "cvtsd2ss" },
			{ 0x5A, T_F3, "cvtss2sd" },
			{ 0xE6, T_F2, "cvtpd2dq" },
			{ 0xE6, T_66, "cvttpd2dq" },
			{ 0xE6, T_F3, "cvtdq2pd" },
			{ 0x5B, T_66, "cvtps2dq" },
			{ 0x5B, T_F3, "cvttps2dq" },
			{ 0x5B, T_NONE, "cvtdq2ps" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type | T_0F);
			printf("void %s(const Xmm& xmm, const Operand& op) { opSSE(xmm, op, %s, 0x%02X, isXMM_XMMorMEM); }\n", p->name, s.c_str(), p->code);
		}
	}
	{
		// special type
		const struct Tbl {
			uint8_t code;
			uint64_t type;
			const char *name;
			const char *cond;
		} tbl[] = {
			{ 0x2A, T_NONE , "cvtpi2ps",  "isXMM_MMXorMEM" },
			{ 0x2D, T_NONE , "cvtps2pi",  "isMMX_XMMorMEM" },
			{ 0x2A, T_F3, "cvtsi2ss",  "isXMM_REG32orMEM" },
			{ 0x2D, T_F3, "cvtss2si",  "isREG32_XMMorMEM" },
			{ 0x2C, T_NONE , "cvttps2pi", "isMMX_XMMorMEM" },
			{ 0x2C, T_F3, "cvttss2si", "isREG32_XMMorMEM" },
			{ 0x2A, T_66, "cvtpi2pd",  "isXMM_MMXorMEM" },
			{ 0x2D, T_66, "cvtpd2pi",  "isMMX_XMMorMEM" },
			{ 0x2A, T_F2, "cvtsi2sd",  "isXMM_REG32orMEM" },
			{ 0x2D, T_F2, "cvtsd2si",  "isREG32_XMMorMEM" },
			{ 0x2C, T_66, "cvttpd2pi", "isMMX_XMMorMEM" },
			{ 0x2C, T_F2, "cvttsd2si", "isREG32_XMMorMEM" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type | T_0F);
			printf("void %s(const Reg& reg, const Operand& op) { opSSE(reg, op, %s, 0x%02X, %s); }\n", p->name, s.c_str(), p->code, p->cond);
		}
	}
	{
		// prefetch
		const struct Tbl {
			int ext;
			const char *name;
			int code;
		} tbl[] = {
			{ 1, "t0", 0x18},
			{ 2, "t1", 0x18},
			{ 3, "t2", 0x18},
			{ 0, "nta", 0x18},
			{ 2, "wt1", 0x0D},
			{ 1, "w", 0x0D},
			{ 7, "it0", 0x18},
			{ 6, "it1", 0x18},
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void prefetch%s(const Address& addr) { opMR(addr, Reg32(%d), T_0F, 0x%02X); }\n", p->name, p->ext, p->code);
		}
	}
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
		} tbl[] = {
			{ 0x16, "movhps", T_0F },
			{ 0x12, "movlps", T_0F },
			{ 0x16, "movhpd", T_0F | T_66 },
			{ 0x12, "movlpd", T_0F | T_66 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type);
			printf("void %s(const Operand& op1, const Operand& op2) { opMovXMM(op1, op2, %s, 0x%02X); }\n", p->name, s.c_str(), p->code);
		}
	}
	{
		// cmov
		const struct Tbl {
			uint8_t ext;
			const char *name;
		} tbl[] = {
			{ 0, "o" },
			{ 1, "no" },
			{ 2, "b" },
			{ 2, "c" },
			{ 2, "nae" },
			{ 3, "nb" },
			{ 3, "ae" },
			{ 3, "nc" },
			{ 4, "e" },
			{ 4, "z" },
			{ 5, "ne" },
			{ 5, "nz" },
			{ 6, "be" },
			{ 6, "na" },
			{ 7, "nbe" },
			{ 7, "a" },
			{ 8, "s" },
			{ 9, "ns" },
			{ 10, "p" },
			{ 10, "pe" },
			{ 11, "np" },
			{ 11, "po" },
			{ 12, "l" },
			{ 12, "nge" },
			{ 13, "nl" },
			{ 13, "ge" },
			{ 14, "le" },
			{ 14, "ng" },
			{ 15, "nle" },
			{ 15, "g" },
		};
		const char *msg = "//-V524"; // disable warning of PVS-Studio
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void cmov%s(const Reg& reg, const Operand& op) { opRO(reg, op, T_0F, 0x40 | %d, op.isREG(16|i32e)); }%s\n", p->name, p->ext, msg);
			printf("void cmov%s(const Reg& d, const Reg& reg, const Operand& op) { opROO(d, op, reg, T_APX|T_ND1, 0x40 | %d); }%s\n", p->name, p->ext, msg);
			printf("void j%s(std::string label, LabelType type = T_AUTO) { opJmp(label, type, 0x%02X, 0x%02X, 0x%02X); }%s\n", p->name, p->ext | 0x70, p->ext | 0x80, 0x0F, msg);
			printf("void j%s(const Label& label, LabelType type = T_AUTO) { opJmp(label, type, 0x%02X, 0x%02X, 0x%02X); }%s\n", p->name, p->ext | 0x70, p->ext | 0x80, 0x0F, msg);
			printf("void j%s(const char *label, LabelType type = T_AUTO) { j%s(std::string(label), type); }%s\n", p->name, p->name, msg);
			printf("void j%s(const void *addr) { opJmpAbs(addr, T_NEAR, 0x%02X, 0x%02X, 0x%02X); }%s\n", p->name, p->ext | 0x70, p->ext | 0x80, 0x0F, msg);
			printf("void set%s(const Operand& op) { opSetCC(op, %d); }%s\n", p->name, p->ext, msg);

			// ccmpscc
			// true if SCC = 0b1010, false if SCC = 0b1011 (see APX Architecture Specification p.266)
			const char *s = p->ext == 10 ? "t" : p->ext == 11 ? "f" : p->name;
			printf("void ccmp%s(const Operand& op1, const Operand& op2, int dfv = 0) { opCcmp(op1, op2, dfv, 0x38, %d); }\n", s, p->ext);
			printf("void ccmp%s(const Operand& op, int imm, int dfv = 0) { opCcmpi(op, imm, dfv, %d); }\n", s, p->ext);
			printf("void ctest%s(const Operand& op, const Reg& r, int dfv = 0) { opCcmp(op, r, dfv, 0x84, %d); }\n", s, p->ext);
			printf("void ctest%s(const Operand& op, int imm, int dfv = 0) { opTesti(op, imm, dfv, %d); }\n", s, p->ext);
		}
	}
	{
		// cfcmov
		const struct Tbl {
			uint8_t code;
			const char *suf;
		} tbl[] = {
			{0x40, "o"},
			{0x41, "no"},
			{0x42, "b"},
			{0x43, "nb"},
			{0x44, "z"},
			{0x45, "nz"},
			{0x46, "be"},
			{0x47, "nbe"},
			{0x48, "s"},
			{0x49, "ns"},
			{0x4A, "p"},
			{0x4B, "np"},
			{0x4C, "l"},
			{0x4D, "nl"},
			{0x4E, "le"},
			{0x4F, "nle"},
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void cfcmov%s(const Operand& op1, const Operand& op2) { opCfcmov(Reg(), op1, op2, 0x%02X); }\n", p->suf, p->code);
			printf("void cfcmov%s(const Reg& d, const Reg& r, const Operand& op) { opCfcmov(d|T_nf, op, r, 0x%02X); }\n", p->suf, p->code);
		}
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "loop", 0xE2 },
			{ "loope", 0xE1 },
			{ "loopne", 0xE0 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(std::string label) { opJmp(label, T_SHORT, 0x%02X, 0, 0); }\n", p->name, p->code);
			printf("void %s(const Label& label) { opJmp(label, T_SHORT, 0x%02X, 0, 0); }\n", p->name, p->code);
			printf("void %s(const char *label) { %s(std::string(label)); }\n", p->name, p->name);
		}
	}
	////////////////////////////////////////////////////////////////
	{
		const GenericTbl tbl[] = {
			{ "bnd", 0xf2 }, /* 0xf2 prefix for MPX */
			{ "cbw", 0x66, 0x98 },
			{ "cdq", 0x99 },
			{ "clc", 0xF8 },
			{ "cld", 0xFC },
			{ "cli", 0xFA },
			{ "cmc", 0xF5 },

			{ "cpuid", 0x0F, 0xA2 },
			{ "cwd", 0x66, 0x99 },
			{ "cwde", 0x98 },
			{ "cmpsb", 0xA6 },
			{ "cmpsw", 0x66, 0xA7 },
			{ "cmpsd", 0xA7 },
			{ "endbr32", 0xF3, 0x0F, 0x1E, 0xFB },
			{ "endbr64", 0xF3, 0x0F, 0x1E, 0xFA },
			{ "hlt", 0xF4 },
			{ "int3", 0xCC },
			{ "scasb", 0xAE },
			{ "scasw", 0x66, 0xAF },
			{ "scasd", 0xAF },
			{ "movsb", 0xA4 },
			{ "leave", 0xC9 },
			{ "lodsb", 0xAC },
			{ "lodsw", 0x66, 0xAD },
			{ "lodsd", 0xAD },
			{ "movsw", 0x66, 0xA5 },
			{ "movsd", 0xA5 },
			{ "outsb", 0x6E },
			{ "outsw", 0x66, 0x6F },
			{ "outsd", 0x6F },
			{ "stosb", 0xAA },
			{ "stosw", 0x66, 0xAB },
			{ "stosd", 0xAB },
			{ "rep", 0xF3 },
			{ "repe", 0xF3 },
			{ "repz", 0xF3 },
			{ "repne", 0xF2 },
			{ "repnz", 0xF2 },

			{ "lahf", 0x9F },
			{ "lock", 0xF0 },

			{ "sahf", 0x9E },
			{ "serialize", 0x0F, 0x01, 0xE8 },
			{ "stc", 0xF9 },
			{ "std", 0xFD },
			{ "sti", 0xFB },
			{ "sysenter", 0x0F, 0x34 },
			{ "sysexit", 0x0F, 0x35 },

			{ "emms", 0x0F, 0x77 },
			{ "pause", 0xF3, 0x90 },
			{ "sfence", 0x0F, 0xAE, 0xF8 },
			{ "lfence", 0x0F, 0xAE, 0xE8 },
			{ "mfence", 0x0F, 0xAE, 0xF0 },
			{ "monitor", 0x0F, 0x01, 0xC8 },
			{ "mwait", 0x0F, 0x01, 0xC9 },

			{ "rdmsr", 0x0F, 0x32 },
			{ "rdpmc", 0x0F, 0x33 },
			{ "rdtsc", 0x0F, 0x31 },
			{ "rdtscp", 0x0F, 0x01, 0xF9 },
			{ "ud2", 0x0F, 0x0B },
			{ "wait", 0x9B },
			{ "fwait", 0x9B },
			{ "wbinvd", 0x0F, 0x09 },
			{ "wrmsr", 0x0F, 0x30 },
			{ "xlatb", 0xD7 },
			{ "xresldtrk", 0xF2, 0x0F, 0x01, 0xE9 },
			{ "xsusldtrk", 0xF2, 0x0F, 0x01, 0xE8 },
			{ "xend", 0x0f, 0x01, 0xd5 },

			{ "popf", 0x9D },
			{ "pushf", 0x9C },
			{ "stac", 0x0F, 0x01, 0xCB },

			{ "vzeroall", 0xC5, 0xFC, 0x77 },
			{ "vzeroupper", 0xC5, 0xF8, 0x77 },
			{ "xgetbv", 0x0F, 0x01, 0xD0 },

			// FPU
			{ "f2xm1", 0xD9, 0xF0 },
			{ "fabs", 0xD9, 0xE1 },
			{ "faddp", 0xDE, 0xC1 },
			{ "fchs", 0xD9, 0xE0 },
			{ "fclex", 0x9B, 0xDB, 0xE2 },
			{ "fnclex", 0xDB, 0xE2 },
			{ "fcom", 0xD8, 0xD1 },
			{ "fcomp", 0xD8, 0xD9 },
			{ "fcompp", 0xDE, 0xD9 },
			{ "fcos", 0xD9, 0xFF },
			{ "fdecstp", 0xD9, 0xF6 },
			{ "fdivp", 0xDE, 0xF9 },
			{ "fdivrp", 0xDE, 0xF1 },
			{ "fincstp", 0xD9, 0xF7 },
			{ "finit", 0x9B, 0xDB, 0xE3 },
			{ "fninit", 0xDB, 0xE3 },
			{ "fld1", 0xD9, 0xE8 },
			{ "fldl2t", 0xD9, 0xE9 },
			{ "fldl2e", 0xD9, 0xEA },
			{ "fldpi", 0xD9, 0xEB },
			{ "fldlg2", 0xD9, 0xEC },
			{ "fldln2", 0xD9, 0xED },
			{ "fldz", 0xD9, 0xEE },
			{ "fmulp", 0xDE, 0xC9 },
			{ "fnop", 0xD9, 0xD0 },
			{ "fpatan", 0xD9, 0xF3 },
			{ "fprem", 0xD9, 0xF8 },
			{ "fprem1", 0xD9, 0xF5 },
			{ "fptan", 0xD9, 0xF2 },
			{ "frndint", 0xD9, 0xFC },
			{ "fscale", 0xD9, 0xFD },
			{ "fsin", 0xD9, 0xFE },
			{ "fsincos", 0xD9, 0xFB },
			{ "fsqrt", 0xD9, 0xFA },
			{ "fsubp", 0xDE, 0xE9 },
			{ "fsubrp", 0xDE, 0xE1 },
			{ "ftst", 0xD9, 0xE4 },
			{ "fucom", 0xDD, 0xE1 },
			{ "fucomp", 0xDD, 0xE9 },
			{ "fucompp", 0xDA, 0xE9 },
			{ "fxam", 0xD9, 0xE5 },
			{ "fxch", 0xD9, 0xC9 },
			{ "fxtract", 0xD9, 0xF4 },
			{ "fyl2x", 0xD9, 0xF1 },
			{ "fyl2xp1", 0xD9, 0xF9 },

			// AMD Zen
			{ "monitorx", 0x0F, 0x01, 0xFA },
			{ "mwaitx", 0x0F, 0x01, 0xFB },
			{ "clzero", 0x0F, 0x01, 0xFC },
		};
		putGeneric(tbl, NUM_OF_ARRAY(tbl));
		puts("void enter(uint16_t x, uint8_t y) { db(0xC8); dw(x); db(y); }");
		puts("void int_(uint8_t x) { db(0xCD); db(x); }");
		putLoadSeg("lss", T_0F, 0xB2);
		putLoadSeg("lfs", T_0F, 0xB4);
		putLoadSeg("lgs", T_0F, 0xB5);
	}
	{
		const struct Tbl {
			uint8_t code; // (reg, reg)
			uint8_t ext; // (reg, imm)
			const char *name;
			bool support3op;
			uint64_t type;
		} tbl[] = {
			{ 0x10, 2, "adc", true, T_NONE },
			{ 0x00, 0, "add", true, T_NF | T_CODE1_IF1 },
			{ 0x20, 4, "and_", true, T_NF | T_CODE1_IF1 },
			{ 0x38, 7, "cmp", false, T_NONE },
			{ 0x08, 1, "or_", true, T_NF | T_CODE1_IF1 },
			{ 0x18, 3, "sbb", true, T_NONE },
			{ 0x28, 5, "sub", true, T_NF | T_CODE1_IF1 },
			{ 0x30, 6, "xor_", true, T_NF | T_CODE1_IF1 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Operand& op1, const Operand& op2) { opRO_MR(op1, op2, 0x%02X); }\n", p->name, p->code);
			printf("void %s(const Operand& op, uint32_t imm) { opOI(op, imm, 0x%02X, %d); }\n", p->name, p->code, p->ext);
			if (!p->support3op) continue;
			std::string s = type2String(p->type);
			printf("void %s(const Reg& d, const Operand& op1, const Operand& op2) { opROO(d, op1, op2, %s, 0x%02X); }\n", p->name, s.c_str(), p->code);
			printf("void %s(const Reg& d, const Operand& op, uint32_t imm) { opROI(d, op, imm, %s, %d); }\n", p->name, s.c_str(), p->ext);
		}
	}
	{
		const struct Tbl {
			const char *name;
			const char *prefix;
		} tbl[] = {
			{ "aadd", "" },
			{ "aand", "|T_66" },
			{ "aor", "|T_F2" },
			{ "axor", "|T_F3" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Address& addr, const Reg32e &reg) { opMR(addr, reg, T_0F38%s, 0x0FC, T_APX%s); }\n", p->name, p->prefix, p->prefix);
		}
	}

	{
		const struct Tbl {
			uint8_t ext;
			const char *name;
		} tbl[] = {
			{ 1, "dec" },
			{ 0, "inc" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Operand& op) { opIncDec(Reg(), op, %d); }\n", p->name, p->ext);
			printf("void %s(const Reg& d, const Operand& op) { opIncDec(d, op, %d); }\n", p->name, p->ext);
		}
	}
	{
		const struct Tbl {
			uint8_t code;
			uint8_t ext;
			const char *name;
		} tbl[] = {
			{ 0xa3, 4, "bt" },
			{ 0xab, 5, "bts" },
			{ 0xb3, 6, "btr" },
			{ 0xbb, 7, "btc" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Operand& op, const Reg& reg) { opRO(reg, op, T_0F, 0x%02X, op.isREG(16|i32e) && op.getBit() == reg.getBit()); }\n", p->name, p->code);
			printf("void %s(const Operand& op, uint8_t imm) { opRext(op, 16|i32e, %d, T_0F, 0xba, false, 1); db(imm); }\n", p->name, p->ext);
		}
	}
	{
		const struct Tbl {
			uint8_t code;
			uint8_t ext;
			const char *name;
			bool NF;
			int n; // # of op
		} tbl[] = {
			{ 0xF6, 6, "div", true, 1 },
			{ 0xF6, 7, "idiv", true, 1 },
			{ 0xF6, 5, "imul", true ,3 },
			{ 0xF6, 4, "mul", true, 1 },
			{ 0xF6, 3, "neg", true, 2 },
			{ 0xF6, 2, "not_", false, 2 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			const std::string name = p->name;
			uint64_t type = T_APX|T_CODE1_IF1;
			if (p->NF) type |= T_NF;
			std::string s = type2String(type);
			printf("void %s(const Operand& op) { opRext(op, 0, %d, %s, 0x%02X); }\n", p->name, p->ext, s.c_str(), p->code);
			if (p->n == 2) {
				type = T_APX|T_ND1|T_CODE1_IF1;
				if (p->NF) type |= T_NF;
				s = type2String(type);
				printf("void %s(const Reg& d, const Operand& op) { opROO(d, op, Reg(%d, Operand::REG, d.getBit()), %s, 0xF6); }\n", p->name, p->ext, s.c_str());
			}
		}
		printf("void imul(const Reg& reg, const Operand& op) { if (opROO(Reg(), op, reg, T_APX|T_NF, 0xAF)) return; opRO(reg, op, T_0F, 0xAF, reg.getKind() == op.getKind()); }\n");
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t ext; // |8 means supporting NF=1
		} tbl[] = {
			{ "rcl", 2|0 },
			{ "rcr", 3|0 },
			{ "rol", 0|8 },
			{ "ror", 1|8 },
			{ "sar", 7|8 },
			{ "shl", 4|8 },
			{ "shr", 5|8 },

			{ "sal", 4|8 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Operand& op, int imm) { opShift(op, imm, %d); }\n", p->name, p->ext);
			printf("void %s(const Operand& op, const Reg8& _cl) { opShift(op, _cl, %d); }\n", p->name, p->ext);
			printf("void %s(const Reg& d, const Operand& op, int imm) { opShift(op, imm, %d, &d); }\n", p->name, p->ext);
			printf("void %s(const Reg& d, const Operand& op, const Reg8& _cl) { opShift(op, _cl, %d, &d); }\n", p->name, p->ext);
		}
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
			uint8_t code2;
		} tbl[] = {
			{ "shld", 0xA4, 0x24 },
			{ "shrd", 0xAC, 0x2C },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Operand& op, const Reg& reg, uint8_t imm) { opShxd(Reg(), op, reg, imm, 0x%02X, 0x%02X); }\n", p->name, p->code, p->code2);
			printf("void %s(const Operand& op, const Reg& reg, const Reg8& _cl) { opShxd(Reg(), op, reg, 0, 0x%02X, 0x%02X, &_cl); }\n", p->name, p->code, p->code2);
			printf("void %s(const Reg& d, const Operand& op, const Reg& reg, uint8_t imm) { opShxd(d, op, reg, imm, 0x%02X, 0x%02X); }\n", p->name, p->code, p->code2);
			printf("void %s(const Reg& d, const Operand& op, const Reg& reg, const Reg8& _cl) { opShxd(d, op, reg, 0, 0x%02X, 0x%02X, &_cl); }\n", p->name, p->code, p->code2);
		}
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "bsf", 0xBC },
			{ "bsr", 0xBD },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Reg&reg, const Operand& op) { opRO(reg, op, T_0F, 0x%02X, op.isREG(16|i32e)); }\n", p->name, p->code);
		}
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
			uint8_t code2;
		} tbl[] = {
			{ "popcnt", 0xB8, 0 },
			{ "tzcnt", 0xBC, 0xF4 },
			{ "lzcnt", 0xBD, 0xF5 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			if (p->code2) {
				printf("void %s(const Reg&reg, const Operand& op) { if (opROO(Reg(), op, reg, T_APX|T_NF, 0x%02X)) return; opCnt(reg, op, 0x%02X); }\n", p->name, p->code2, p->code);
			} else {
				printf("void %s(const Reg&reg, const Operand& op) { opCnt(reg, op, 0x%02X); }\n", p->name, p->code);
			}
		}
	}
	// SSSE3
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
		} tbl[] = {
			{ 0x00, "pshufb" },
			{ 0x01, "phaddw" },
			{ 0x02, "phaddd" },
			{ 0x03, "phaddsw" },
			{ 0x04, "pmaddubsw" },
			{ 0x05, "phsubw" },
			{ 0x06, "phsubd" },
			{ 0x07, "phsubsw" },
			{ 0x08, "psignb" },
			{ 0x09, "psignw" },
			{ 0x0a, "psignd" },
			{ 0x0b, "pmulhrsw" },
			{ 0x1c, "pabsb" },
			{ 0x1d, "pabsw" },
			{ 0x1e, "pabsd" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Mmx& mmx, const Operand& op) { opMMX(mmx, op, 0x%02X, T_0F38, T_66); }\n", p->name, p->code);
		}
		printf("void palignr(const Mmx& mmx, const Operand& op, int imm) { opMMX(mmx, op, 0x0F, T_0F3A, T_66, static_cast<uint8_t>(imm)); }\n");
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "pclmullqlqdq", 0 },
			{ "pclmulhqlqdq", 1 },
			{ "pclmullqhqdq", 0x10 },
			{ "pclmulhqhqdq", 0x11 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Xmm& xmm, const Operand& op) { pclmulqdq(xmm, op, 0x%02X); }\n", p->name, p->code);
			printf("void v%s(const Xmm& x1, const Xmm& x2, const Operand& op) { vpclmulqdq(x1, x2, op, 0x%02X); }\n", p->name, p->code);
		}
	}
	{
		const struct Tbl {
			const char *type;
			uint8_t code;
			uint8_t ext;
			const char *name;
			bool fwait;
		} tbl[] = {
			{ "T_0F", 0xAE, 2, "ldmxcsr", false },
			{ "T_0F", 0xAE, 3, "stmxcsr", false },
			{ "T_0F", 0xAE, 7, "clflush", false },
			{ "T_66 | T_0F", 0xAE, 7, "clflushopt", false},
			{ "0", 0xDF, 4, "fbld", false },
			{ "0", 0xDF, 6, "fbstp", false },
			{ "0", 0xD9, 5, "fldcw", false },
			{ "0", 0xD9, 4, "fldenv", false },
			{ "0", 0xDD, 4, "frstor", false },
			{ "0", 0xDD, 6, "fsave", true  },
			{ "0", 0xDD, 6, "fnsave", false },
			{ "0", 0xD9, 7, "fstcw", true },
			{ "0", 0xD9, 7, "fnstcw", false },
			{ "0", 0xD9, 6, "fstenv", true },
			{ "0", 0xD9, 6, "fnstenv", false },
			{ "0", 0xDD, 7, "fstsw", true },
			{ "0", 0xDD, 7, "fnstsw", false },
			{ "T_0F", 0xAE, 1, "fxrstor", false },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			putMemOp(p->name, p->type, p->ext, p->code, 32, p->fwait);
		}
		puts("void fstsw(const Reg16& r) { if (r.getIdx() != Operand::AX) XBYAK_THROW(ERR_BAD_PARAMETER) db(0x9B); db(0xDF); db(0xE0); }");
		puts("void fnstsw(const Reg16& r) { if (r.getIdx() != Operand::AX) XBYAK_THROW(ERR_BAD_PARAMETER) db(0xDF); db(0xE0); }");
	}
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
		} tbl[] = {
			{ 0x2B, "movntpd" },
			{ 0xE7, "movntdq" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			// cast xmm register to 16bit register to put 0x66
			printf("void %s(const Address& addr, const Xmm& reg) { if (reg.getIdx() >= 16) XBYAK_THROW(ERR_BAD_PARAMETER) opSSE(Reg16(reg.getIdx()), addr, T_0F, 0x%02X); }\n", p->name, p->code);
		}
	}
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
		} tbl[] = {
			{ 0xBE, "movsx" },
			{ 0xB6, "movzx" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Reg& reg, const Operand& op) { opMovxx(reg, op, 0x%02X); }\n", p->name, p->code);
		}
	}
	{
		const struct Tbl {
			uint8_t prefix;
			const char *name;
		} tbl[] = {
			{ 0x66, "adcx" },
			{ 0xF3, "adox" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Reg32e& reg, const Operand& op) { if (!reg.isREG(16|i32e) && reg.getBit() == op.getBit()) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER) if (opROO(Reg(), op, reg, T_%02X, 0x66)) return; opRO(reg, op, T_%02X | T_0F38, 0xF6); }\n", p->name, p->prefix, p->prefix);
			printf("void %s(const Reg32e& d, const Reg32e& reg, const Operand& op) { opROO(d, op, reg, T_%02X, 0x66); }\n", p->name, p->prefix);
		}
	}
	{ // in/out
		puts("void in_(const Reg& a, uint8_t v) { opInOut(a, 0xE4, v); }");
		puts("void in_(const Reg& a, const Reg& d) { opInOut(a, d, 0xEC); }");
		puts("void out_(uint8_t v, const Reg& a) { opInOut(a, 0xE6, v); }");
		puts("void out_(const Reg& d, const Reg& a) { opInOut(a, d, 0xEE); }");
	}
	// mpx
	{
		puts("void bndcl(const BoundsReg& bnd, const Operand& op) { opRext(op, i32e, bnd.getIdx(), T_F3 | T_0F, 0x1A, !op.isMEM()); }");
		puts("void bndcu(const BoundsReg& bnd, const Operand& op) { opRext(op, i32e, bnd.getIdx(), T_F2 | T_0F, 0x1A, !op.isMEM()); }");
		puts("void bndcn(const BoundsReg& bnd, const Operand& op) { opRext(op, i32e, bnd.getIdx(), T_F2 | T_0F, 0x1B, !op.isMEM()); }");
		puts("void bndldx(const BoundsReg& bnd, const Address& addr) { opMIB(addr, bnd, T_0F, 0x1A); }");
		puts("void bndmk(const BoundsReg& bnd, const Address& addr) { opMR(addr, bnd, T_F3 | T_0F, 0x1B); }");
		puts("void bndmov(const BoundsReg& bnd, const Operand& op) { opRO(bnd, op, T_66 | T_0F, 0x1A, op.isBNDREG()); }");
		puts("void bndmov(const Address& addr, const BoundsReg& bnd) { opMR(addr, bnd, T_66 | T_0F, 0x1B); }");
		puts("void bndstx(const Address& addr, const BoundsReg& bnd) { opMIB(addr, bnd, T_0F, 0x1B); }");
	}
	// misc
	{
		puts("void lea(const Reg& reg, const Address& addr) { if (!reg.isBit(16 | i32e)) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER) opMR(addr, reg, 0, 0x8D); }");
		puts("void ret(int imm = 0) { if (imm) { db(0xC2); dw(imm); } else { db(0xC3); } }");
		puts("void retf(int imm = 0) { if (imm) { db(0xCA); dw(imm); } else { db(0xCB); } }");

		puts("void xadd(const Operand& op, const Reg& reg) { opRO(reg, op, T_0F, 0xC0 | (reg.isBit(8) ? 0 : 1), op.getBit() == reg.getBit()); }");
		puts("void cmpxchg(const Operand& op, const Reg& reg) { opRO(reg, op, T_0F, 0xB0 | (reg.isBit(8) ? 0 : 1), op.getBit() == reg.getBit()); }");
		puts("void movbe(const Reg& reg, const Address& addr) { opMR(addr, reg, T_0F38, 0xF0, T_APX, 0x60); }");
		puts("void movbe(const Address& addr, const Reg& reg) { opMR(addr, reg, T_0F38, 0xF1, T_APX, 0x61); }");
		puts("void movdiri(const Address& addr, const Reg32e& reg) { opMR(addr, reg, T_0F38, 0xF9, T_APX); }");
		puts("void movdir64b(const Reg& reg, const Address& addr) { opMR(addr, reg.cvt32(), T_66|T_0F38, 0xF8, T_APX|T_66); }");
		puts("void cmpxchg8b(const Address& addr) { opMR(addr, Reg32(1), T_0F, 0xC7); }");

		puts("void pextrw(const Operand& op, const Mmx& xmm, uint8_t imm) { opExt(op, xmm, 0x15, imm, true); }");
		puts("void pextrb(const Operand& op, const Xmm& xmm, uint8_t imm) { opExt(op, xmm, 0x14, imm); }");
		puts("void pextrd(const Operand& op, const Xmm& xmm, uint8_t imm) { opExt(op, xmm, 0x16, imm); }");
		puts("void extractps(const Operand& op, const Xmm& xmm, uint8_t imm) { opExt(op, xmm, 0x17, imm); }");
		puts("void pinsrw(const Mmx& mmx, const Operand& op, int imm) { if (!op.isREG(32) && !op.isMEM()) XBYAK_THROW(ERR_BAD_COMBINATION) opSSE(mmx, op, T_0F | (mmx.isXMM() ? T_66 : T_NONE), 0xC4, 0, imm); }");
		puts("void insertps(const Xmm& xmm, const Operand& op, uint8_t imm) { opSSE(xmm, op, T_66 | T_0F3A, 0x21, isXMM_XMMorMEM, imm); }");
		puts("void pinsrb(const Xmm& xmm, const Operand& op, uint8_t imm) { opSSE(xmm, op, T_66 | T_0F3A, 0x20, isXMM_REG32orMEM, imm); }");
		puts("void pinsrd(const Xmm& xmm, const Operand& op, uint8_t imm) { opSSE(xmm, op, T_66 | T_0F3A, 0x22, isXMM_REG32orMEM, imm); }");

		puts("void pmovmskb(const Reg32e& reg, const Mmx& mmx) { if (mmx.isXMM()) db(0x66); opSSE(reg, mmx, T_0F, 0xD7); }");
		puts("void maskmovq(const Mmx& reg1, const Mmx& reg2) { opSSE(reg1, reg2, T_0F, 0xF7); }");
		puts("void movmskps(const Reg32e& reg, const Xmm& xmm) { opSSE(reg, xmm, T_0F, 0x50); }");
		puts("void movmskpd(const Reg32e& reg, const Xmm& xmm) { db(0x66); movmskps(reg, xmm); }");
		puts("void movntps(const Address& addr, const Xmm& xmm) { opSSE(Xmm(xmm.getIdx()), addr, T_0F, 0x2B); }");
		puts("void movntdqa(const Xmm& xmm, const Address& addr) { opSSE(xmm, addr, T_66 | T_0F38, 0x2A); }");
		puts("void lddqu(const Xmm& xmm, const Address& addr) { opSSE(xmm, addr, T_F2 | T_0F, 0xF0); }");
		puts("void movnti(const Address& addr, const Reg32e& reg) { opMR(addr, reg, T_0F, 0xC3); }");
		puts("void movntq(const Address& addr, const Mmx& mmx) { if (!mmx.isMMX()) XBYAK_THROW(ERR_BAD_COMBINATION) opSSE(mmx, addr, T_0F, 0xE7); }");

		puts("void movd(const Operand& op, const Mmx& mmx) { if (!(op.isMEM() || op.isREG(32))) XBYAK_THROW(ERR_BAD_COMBINATION) if (mmx.isXMM()) db(0x66); opSSE(mmx, op, T_0F, 0x7E); }");
		puts("void movd(const Mmx& mmx, const Operand& op) { if (!(op.isMEM() || op.isREG(32))) XBYAK_THROW(ERR_BAD_COMBINATION) if (mmx.isXMM()) db(0x66); opSSE(mmx, op, T_0F, 0x6E); }");
		puts("void movq2dq(const Xmm& xmm, const Mmx& mmx) { opSSE(xmm, mmx, T_F3 | T_0F, 0xD6); }");
		puts("void movdq2q(const Mmx& mmx, const Xmm& xmm) { opSSE(mmx, xmm, T_F2 | T_0F, 0xD6); }");
		puts("void movq(const Mmx& mmx, const Operand& op) { if (!op.isMEM() && mmx.getKind() != op.getKind()) XBYAK_THROW(ERR_BAD_COMBINATION) if (mmx.isXMM()) db(0xF3); opSSE(mmx, op, T_0F, mmx.isXMM() ? 0x7E : 0x6F); }");
		puts("void movq(const Address& addr, const Mmx& mmx) { if (mmx.isXMM()) db(0x66); opSSE(mmx, addr, T_0F, mmx.isXMM() ? 0xD6 : 0x7F); }");
		puts("void rdrand(const Reg& r) { if (r.isBit(8)) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER) opRR(Reg(6, Operand::REG, r.getBit()), r, T_0F, 0xC7); }");
		puts("void rdseed(const Reg& r) { if (r.isBit(8)) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER) opRR(Reg(7, Operand::REG, r.getBit()), r, T_0F, 0xC7); }");
		puts("void crc32(const Reg32e& r, const Operand& op) { if (!((r.isBit(32) && op.isBit(8|16|32)) || (r.isBit(64) && op.isBit(8|64)))) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER) int code = 0xF0 | (op.isBit(8) ? 0 : 1); uint64_t type = op.isBit(16) ? T_66:0; type |= T_ALLOW_DIFF_SIZE; if (opROO(Reg(), op, static_cast<const Reg&>(r), T_APX|type, code)) return; opRO(r, op, T_F2|T_0F38|type, code); }");
		puts("void tpause(const Reg32& r) { int idx = r.getIdx(); if (idx > 7) XBYAK_THROW(ERR_BAD_PARAMETER) db(0x66); db(0x0F); db(0xAE); setModRM(3, 6, idx); }");
		puts("void umonitor(const Reg& r) { int idx = r.getIdx(); if (idx > 7) XBYAK_THROW(ERR_BAD_PARAMETER) int bit = r.getBit(); if (BIT != bit) { if ((BIT == 32 && bit == 16) || (BIT == 64 && bit == 32)) { db(0x67); } else { XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER) } } db(0xF3); db(0x0F); db(0xAE); setModRM(3, 6, idx); }");
		puts("void umwait(const Reg32& r) { int idx = r.getIdx(); if (idx > 7) XBYAK_THROW(ERR_BAD_PARAMETER) db(0xF2); db(0x0F); db(0xAE); setModRM(3, 6, idx); }");
		puts("void clwb(const Address& addr) { opMR(addr, esi, T_66 | T_0F, 0xAE); }");
		puts("void cldemote(const Address& addr) { opMR(addr, eax, T_0F, 0x1C); }");
		puts("void xabort(uint8_t imm) { db(0xC6); db(0xF8); db(imm); }");
		puts("void xbegin(uint32_t rel) { db(0xC7); db(0xF8); dd(rel); }");

		puts("void vsha512msg1(const Ymm& y, const Xmm& x) { if (!(y.isYMM() && x.isXMM())) XBYAK_THROW(ERR_BAD_PARAMETER) opVex(y, 0, x, T_F2 | T_0F38 | T_W0 | T_YMM, 0xCC); }");
		puts("void vsha512msg2(const Ymm& y1, const Ymm& y2) { if (!(y1.isYMM() && y2.isYMM())) XBYAK_THROW(ERR_BAD_PARAMETER) opVex(y1, 0, y2, T_F2 | T_0F38 | T_W0 | T_YMM, 0xCD); }");
		puts("void vsha512rnds2(const Ymm& y1, const Ymm& y2, const Xmm& x) { if (!(y1.isYMM() && y2.isYMM() && x.isXMM())) XBYAK_THROW(ERR_BAD_PARAMETER) opVex(y1, &y2, x, T_F2 | T_0F38 | T_W0 | T_YMM, 0xCB); }");
	}
	{
		const struct Tbl {
			uint8_t m16;
			uint8_t m32;
			uint8_t m64;
			uint8_t ext;
			const char *name;
			uint8_t m64ext;
		} tbl[] = {
			{ 0x00, 0xD8, 0xDC, 0, "fadd" },
			{ 0xDE, 0xDA, 0x00, 0, "fiadd" },
			{ 0x00, 0xD8, 0xDC, 2, "fcom" },
			{ 0x00, 0xD8, 0xDC, 3, "fcomp" },
			{ 0x00, 0xD8, 0xDC, 6, "fdiv" },
			{ 0xDE, 0xDA, 0x00, 6, "fidiv" },
			{ 0x00, 0xD8, 0xDC, 7, "fdivr" },
			{ 0xDE, 0xDA, 0x00, 7, "fidivr" },
			{ 0xDE, 0xDA, 0x00, 2, "ficom" },
			{ 0xDE, 0xDA, 0x00, 3, "ficomp" },
			{ 0xDF, 0xDB, 0xDF, 0, "fild", 5 },
			{ 0xDF, 0xDB, 0x00, 2, "fist" },
			{ 0xDF, 0xDB, 0xDF, 3, "fistp", 7 },
			{ 0xDF, 0xDB, 0xDD, 1, "fisttp" },
			{ 0x00, 0xD9, 0xDD, 0, "fld" },
			{ 0x00, 0xD8, 0xDC, 1, "fmul" },
			{ 0xDE, 0xDA, 0x00, 1, "fimul" },
			{ 0x00, 0xD9, 0xDD, 2, "fst" },
			{ 0x00, 0xD9, 0xDD, 3, "fstp" },
			{ 0x00, 0xD8, 0xDC, 4, "fsub" },
			{ 0xDE, 0xDA, 0x00, 4, "fisub" },
			{ 0x00, 0xD8, 0xDC, 5, "fsubr" },
			{ 0xDE, 0xDA, 0x00, 5, "fisubr" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Address& addr) { opFpuMem(addr, 0x%02X, 0x%02X, 0x%02X, %d, %d); }\n", p->name, p->m16, p->m32, p->m64, p->ext, p->m64ext);
		}
	}
	{
		const struct Tbl {
			uint32_t code1;
			uint32_t code2;
			const char *name;
		} tbl[] = {
			{ 0xD8C0, 0xDCC0, "fadd" },
			{ 0x0000, 0xDEC0, "faddp" },

			{ 0xDAC0, 0x00C0, "fcmovb" },
			{ 0xDAC8, 0x00C8, "fcmove" },
			{ 0xDAD0, 0x00D0, "fcmovbe" },
			{ 0xDAD8, 0x00D8, "fcmovu" },
			{ 0xDBC0, 0x00C0, "fcmovnb" },
			{ 0xDBC8, 0x00C8, "fcmovne" },
			{ 0xDBD0, 0x00D0, "fcmovnbe" },
			{ 0xDBD8, 0x00D8, "fcmovnu" },

			{ 0xDBF0, 0x00F0, "fcomi" },
			{ 0xDFF0, 0x00F0, "fcomip" },
			{ 0xDBE8, 0x00E8, "fucomi" },
			{ 0xDFE8, 0x00E8, "fucomip" },

			{ 0xD8F0, 0xDCF8, "fdiv" },
			{ 0x0000, 0xDEF8, "fdivp" },
			{ 0xD8F8, 0xDCF0, "fdivr" },
			{ 0x0000, 0xDEF0, "fdivrp" },
			{ 0xD8C8, 0xDCC8, "fmul" },
			{ 0x0000, 0xDEC8, "fmulp" },
			{ 0xD8E0, 0xDCE8, "fsub" },
			{ 0x0000, 0xDEE8, "fsubp" },
			{ 0xD8E8, 0xDCE0, "fsubr" },
			{ 0x0000, 0xDEE0, "fsubrp" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Fpu& reg1, const Fpu& reg2) { opFpuFpu(reg1, reg2, 0x%04X, 0x%04X); }\n", p->name, p->code1, p->code2);
			// omit st0 version(like nasm)
			if (p->code1) {
				printf("void %s(const Fpu& reg1) { opFpuFpu(st0, reg1, 0x%04X, 0x%04X); }\n", p->name, p->code1, p->code2);
			} else {
				printf("void %s(const Fpu& reg1) { opFpuFpu(reg1, st0, 0x%04X, 0x%04X); }\n", p->name, p->code1, p->code2);
			}
		}
	}
	{
		const struct Tbl {
			uint8_t code1;
			uint8_t code2;
			const char *name;
		} tbl[] = {
			{ 0xD8, 0xD0, "fcom" },
			{ 0xD8, 0xD8, "fcomp" },
			{ 0xDD, 0xC0, "ffree" },
			{ 0xD9, 0xC0, "fld" },
			{ 0xDD, 0xD0, "fst" },
			{ 0xDD, 0xD8, "fstp" },
			{ 0xDD, 0xE0, "fucom" },
			{ 0xDD, 0xE8, "fucomp" },
			{ 0xD9, 0xC8, "fxch" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Fpu& reg) { opFpu(reg, 0x%02X, 0x%02X); }\n", p->name, p->code1, p->code2);
		}
	}
	// AVX
	{ // pd, ps, sd, ss
		const struct Tbl {
			uint8_t code;
			const char *name;
			bool only_pd_ps;
			int mode; // 0 : none, 1 : er, 2 : sae
		} tbl[] = {
			{ 0x58, "add", false, 1 },
			{ 0x5C, "sub", false, 1 },
			{ 0x59, "mul", false, 1 },
			{ 0x5E, "div", false, 1 },
			{ 0x5F, "max", false, 2 },
			{ 0x5D, "min", false, 2 },
			{ 0x54, "and", true, 0 },
			{ 0x55, "andn", true, 0 },
			{ 0x56, "or", true, 0 },
			{ 0x57, "xor", true, 0 },
		};
		const char *xTbl[] = {
			"", " | T_ER_X", " | T_SAE_X"
		};
		const char *zTbl[] = {
			"", " | T_ER_Z", " | T_SAE_Z"
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void v%spd(const Xmm& xmm, const Operand& op1, const Operand& op2 = Operand()) { opAVX_X_X_XM(xmm, op1, op2, T_0F | T_66 | T_EW1 | T_YMM | T_EVEX%s | T_B64, 0x%02X); }\n", p->name, zTbl[p->mode], p->code);
			printf("void v%sps(const Xmm& xmm, const Operand& op1, const Operand& op2 = Operand()) { opAVX_X_X_XM(xmm, op1, op2, T_0F | T_EW0 | T_YMM | T_EVEX%s | T_B32, 0x%02X); }\n", p->name, zTbl[p->mode], p->code);
			if (p->only_pd_ps) continue;
			printf("void v%ssd(const Xmm& xmm, const Operand& op1, const Operand& op2 = Operand()) { opAVX_X_X_XM(xmm, op1, op2, T_0F | T_F2 | T_EW1 | T_EVEX%s | T_N8, 0x%02X); }\n", p->name, xTbl[p->mode], p->code);
			printf("void v%sss(const Xmm& xmm, const Operand& op1, const Operand& op2 = Operand()) { opAVX_X_X_XM(xmm, op1, op2, T_0F | T_F3 | T_EW0 | T_EVEX%s | T_N4, 0x%02X); }\n", p->name, xTbl[p->mode], p->code);
		}
	}
	putX_X_XM(false);

	// (x, x/m[, imm]) or (y, y/m[, imm])
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
			bool hasIMM;
			int mode; // 1 : SSE, 2 : AVX, 3 : SSE + AVX
		} tbl[] = {
			{ 0x15, "blendvpd", T_0F38 | T_66, false, 1 },
			{ 0x14, "blendvps", T_0F38 | T_66, false, 1 },
			{ 0x10, "pblendvb", T_0F38 | T_66, false, 1 },
			{ 0xDF, "aeskeygenassist", T_0F3A | T_66, true, 3 },
			{ 0xDB, "aesimc", T_0F38 | T_66 | T_W0, false, 3 },
			{ 0x09, "roundpd", T_0F3A | T_66 | T_YMM, true, 3 },
			{ 0x08, "roundps", T_0F3A | T_66 | T_YMM, true, 3 },
			{ 0x05, "permilpd", T_0F3A | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64, true, 2 },
			{ 0x04, "permilps", T_0F3A | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, true, 2 },
			{ 0x61, "pcmpestri", T_0F3A | T_66, true, 3 },
			{ 0x60, "pcmpestrm", T_0F3A | T_66, true, 3 },
			{ 0x63, "pcmpistri", T_0F3A | T_66, true, 3 },
			{ 0x62, "pcmpistrm", T_0F3A | T_66, true, 3 },
			{ 0x0E, "testps", T_0F38 | T_66 | T_YMM, false, 2 },
			{ 0x0F, "testpd", T_0F38 | T_66 | T_YMM, false, 2 },
			{ 0x2F, "comisd", T_0F | T_66 | T_EVEX | T_EW1 | T_SAE_X | T_N8, false, 2 },
			{ 0x2F, "comiss", T_0F | T_EVEX | T_EW0 | T_SAE_X | T_N4, false, 2 },
			{ 0x5B, "cvtdq2ps", T_0F | T_YMM | T_EVEX | T_EW0 | T_B32 | T_ER_Z, false, 2 },
			{ 0x5B, "cvtps2dq", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32 | T_ER_Z, false, 2 },
			{ 0x5B, "cvttps2dq", T_0F | T_F3 | T_YMM | T_EVEX | T_EW0 | T_B32 | T_SAE_Z, false, 2 },
			{ 0x28, "movapd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1, false, 2 },
			{ 0x28, "movaps", T_0F | T_YMM | T_EVEX | T_EW0, false, 2 },
			{ 0x12, "movddup", T_0F | T_F2 | T_YMM | T_EVEX | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z | T_DUP, false, 3 },
			{ 0x6F, "movdqa", T_0F | T_66 | T_YMM, false, 2 },
			{ 0x6F, "movdqu", T_0F | T_F3 | T_YMM, false, 2 },
			{ 0x16, "movshdup", T_0F | T_F3 | T_YMM | T_EVEX | T_EW0, false, 3 },
			{ 0x12, "movsldup", T_0F | T_F3 | T_YMM | T_EVEX | T_EW0, false, 3 },
			{ 0x10, "movupd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1, false, 2 },
			{ 0x10, "movups", T_0F | T_YMM | T_EVEX | T_EW0, false, 2 },

			{ 0x1C, "pabsb", T_0F38 | T_66 | T_YMM | T_EVEX, false, 2 },
			{ 0x1D, "pabsw", T_0F38 | T_66 | T_YMM | T_EVEX, false, 2 },
			{ 0x1E, "pabsd", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, false, 2 },
			{ 0x41, "phminposuw", T_0F38 | T_66, false, 3 },

			{ 0x20, "pmovsxbw", T_0F38 | T_66 | T_YMM | T_EVEX | T_N8 | T_N_VL, false, 3 },
			{ 0x21, "pmovsxbd", T_0F38 | T_66 | T_YMM | T_EVEX | T_N4 | T_N_VL, false, 3 },
			{ 0x22, "pmovsxbq", T_0F38 | T_66 | T_YMM | T_EVEX | T_N2 | T_N_VL, false, 3 },
			{ 0x23, "pmovsxwd", T_0F38 | T_66 | T_YMM | T_EVEX | T_N8 | T_N_VL, false, 3 },
			{ 0x24, "pmovsxwq", T_0F38 | T_66 | T_YMM | T_EVEX | T_N4 | T_N_VL, false, 3 },
			{ 0x25, "pmovsxdq", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_N8 | T_N_VL, false, 3 },

			{ 0x30, "pmovzxbw", T_0F38 | T_66 | T_YMM | T_EVEX | T_N8 | T_N_VL, false, 3 },
			{ 0x31, "pmovzxbd", T_0F38 | T_66 | T_YMM | T_EVEX | T_N4 | T_N_VL, false, 3 },
			{ 0x32, "pmovzxbq", T_0F38 | T_66 | T_YMM | T_EVEX | T_N2 | T_N_VL, false, 3 },
			{ 0x33, "pmovzxwd", T_0F38 | T_66 | T_YMM | T_EVEX | T_N8 | T_N_VL, false, 3 },
			{ 0x34, "pmovzxwq", T_0F38 | T_66 | T_YMM | T_EVEX | T_N4 | T_N_VL, false, 3 },
			{ 0x35, "pmovzxdq", T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0 | T_N8 | T_N_VL, false, 3 },

			{ 0x70, "pshufd", T_0F | T_66 | T_YMM | T_EVEX | T_EW0 | T_B32, true, 2 },
			{ 0x70, "pshufhw", T_0F | T_F3 | T_YMM | T_EVEX, true, 2 },
			{ 0x70, "pshuflw", T_0F | T_F2 | T_YMM | T_EVEX, true, 2 },

			{ 0x17, "ptest", T_0F38 | T_66 | T_YMM, false, 3 },
			{ 0x53, "rcpps", T_0F | T_YMM, false, 2 },
			{ 0x52, "rsqrtps", T_0F | T_YMM, false, 2 },

			{ 0x51, "sqrtpd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_ER_Z | T_B64, false, 2 },
			{ 0x51, "sqrtps", T_0F | T_YMM | T_EVEX | T_EW0 | T_ER_Z | T_B32, false, 2 },

			{ 0x2E, "ucomisd", T_0F | T_66 | T_EVEX | T_EW1 | T_SAE_X | T_N8, false, 2 },
			{ 0x2E, "ucomiss", T_0F | T_EVEX | T_EW0 | T_SAE_X | T_N4, false, 2 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type);
			if (p->mode & 1) {
				const char *immS1 = p->hasIMM ? ", uint8_t imm" : "";
				const char *immS2 = p->hasIMM ? ", imm" : ", NONE";
				printf("void %s(const Xmm& xmm, const Operand& op%s) { opSSE(xmm, op, %s, 0x%02X, isXMM_XMMorMEM%s); }\n", p->name, immS1, s.c_str(), p->code, immS2);
			}
			if (p->mode & 2) {
				printf("void v%s(const Xmm& xm, const Operand& op%s) { opAVX_X_XM_IMM(xm, op, %s, 0x%02X%s); }\n"
					, p->name, p->hasIMM ? ", uint8_t imm" : "", s.c_str(), p->code, p->hasIMM ? ", imm" : "");
			}
		}
	}
	// sha
	{
		const struct Tbl {
			uint8_t code;
			uint8_t code2;
			const char *name;
		} tbl[] = {
			{ 0xC8, 0xD8, "sha1nexte" },
			{ 0xC9, 0xD9, "sha1msg1" },
			{ 0xCA, 0xDA, "sha1msg2" },
			{ 0xCB, 0xDB, "sha256rnds2" },
			{ 0xCC, 0xDC, "sha256msg1" },
			{ 0xCD, 0xDD, "sha256msg2" },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void %s(const Xmm& x, const Operand& op) { opSSE_APX(x, op, T_0F38, 0x%02X, T_MUST_EVEX, 0x%02X); }\n", p->name, p->code, p->code2);
		}
		puts("void sha1rnds4(const Xmm& x, const Operand& op, uint8_t imm) { opSSE_APX(x, op, T_0F3A, 0xCC, T_MUST_EVEX, 0xD4, imm); }");
	}
	// (m, x), (m, y)
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
		} tbl[] = {
			{ 0x29, "movapd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_M_K },
			{ 0x29, "movaps", T_0F | T_YMM | T_EVEX | T_EW0 | T_M_K },
			{ 0x7F, "movdqa", T_0F | T_66 | T_YMM  },
			{ 0x7F, "movdqu", T_0F | T_F3 | T_YMM  },
			{ 0x11, "movupd", T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_M_K },
			{ 0x11, "movups", T_0F | T_YMM | T_EVEX | T_EW0 | T_M_K },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type);
			printf("void v%s(const Address& addr, const Xmm& xmm) { opAVX_X_XM_IMM(xmm, addr, %s, 0x%02X); }\n"
				, p->name, s.c_str(), p->code);
		}
	}
	// (x, x/m), (y, y/m), (x, x, x/m), (y, y, y/m)
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
			int mode; // 1 : sse, 2 : avx, 3 : sse + avx
		} tbl[] = {
			{ 0xD0, "addsubpd", T_0F | T_66 | T_YMM, 3 },
			{ 0xD0, "addsubps", T_0F | T_F2 | T_YMM, 3 },
			{ 0x7C, "haddpd", T_0F | T_66 | T_YMM, 3 },
			{ 0x7C, "haddps", T_0F | T_F2 | T_YMM, 3 },
			{ 0x7D, "hsubpd", T_0F | T_66 | T_YMM, 3 },
			{ 0x7D, "hsubps", T_0F | T_F2 | T_YMM, 3 },

			{ 0xDC, "aesenc", T_0F38 | T_66 | T_YMM | T_EVEX, 3 },
			{ 0xDD, "aesenclast", T_0F38 | T_66 | T_YMM | T_EVEX, 3 },
			{ 0xDE, "aesdec", T_0F38 | T_66 | T_YMM | T_EVEX, 3 },
			{ 0xDF, "aesdeclast", T_0F38 | T_66 | T_YMM | T_EVEX, 3 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type);
			if (p->mode & 1) {
				printf("void %s(const Xmm& xmm, const Operand& op) { opSSE(xmm, op, %s, 0x%02X, isXMM_XMMorMEM); }\n", p->name, s.c_str(), p->code);
			}
			if (p->mode & 2) {
				printf("void v%s(const Xmm& xmm, const Operand& op1, const Operand& op2 = Operand()) { opAVX_X_X_XM(xmm, op1, op2, %s, 0x%02X); }\n"
					, p->name, s.c_str(), p->code);
			}
		}
	}
	// vmaskmov
	{
		const char suf[][8] = { "ps", "pd" };
		for (int i = 0; i < 2; i++) {
			printf("void vmaskmov%s(const Xmm& x1, const Xmm& x2, const Address& addr) { opAVX_X_X_XM(x1, x2, addr, T_0F38 | T_66 | T_W0 | T_YMM, 0x%02X); }\n", suf[i], 0x2C + i);
			printf("void vmaskmov%s(const Address& addr, const Xmm& x1, const Xmm& x2) { opAVX_X_X_XM(x2, x1, addr, T_0F38 | T_66 | T_W0 | T_YMM, 0x%02X); }\n", suf[i], 0x2E + i);
		}
	}
	// vpmaskmov
	{
		const char suf[][8] = { "d", "q" };
		for (int i = 0; i < 2; i++) {
			printf("void vpmaskmov%s(const Xmm& x1, const Xmm& x2, const Address& addr) { opAVX_X_X_XM(x1, x2, addr, T_0F38 | T_66 | T_W%d | T_YMM, 0x%02X); }\n", suf[i], i, 0x8C);
			printf("void vpmaskmov%s(const Address& addr, const Xmm& x1, const Xmm& x2) { opAVX_X_X_XM(x2, x1, addr, T_0F38 | T_66 | T_W%d | T_YMM, 0x%02X); }\n", suf[i], i, 0x8E);
		}
	}
	// vpermd, vpermps
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
		} tbl[] = {
			{ 0x36, "vpermd", T_66 | T_0F38 | T_W0 | T_YMM | T_EVEX | T_EW0 | T_B32 },
			{ 0x36, "vpermq", T_66 | T_0F38 | T_W0 | T_YMM | T_EVEX | T_EW1 | T_B64 },
			{ 0x16, "vpermps", T_66 | T_0F38 | T_W0 | T_YMM | T_EVEX | T_EW0 | T_B32 },
			{ 0x16, "vpermpd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_YMM | T_B64 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string s = type2String(p.type);
			printf("void %s(const Ymm& y1, const Ymm& y2, const Operand& op) { opAVX_X_X_XM(y1, y2, op, %s, 0x%02X); }\n", p.name, s.c_str(), p.code);
		}
	}
	// vpermq, vpermpd
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
		} tbl[] = {
			{ 0x00, "vpermq", T_0F3A | T_66 | T_W1 | T_YMM | T_EVEX | T_EW1 | T_B64 },
			{ 0x01, "vpermpd", T_0F3A | T_66 | T_W1 | T_YMM | T_EVEX | T_EW1 | T_B64 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string s = type2String(p.type);
			printf("void %s(const Ymm& y, const Operand& op, uint8_t imm) { opAVX_X_XM_IMM(y, op, %s, 0x%02X, imm); }\n", p.name, s.c_str(), p.code);
		}
	}
	// vcmpeqps
	{
		const char pred[32][16] = {
			"eq", "lt", "le", "unord", "neq", "nlt", "nle", "ord",
			"eq_uq", "nge", "ngt", "false", "neq_oq", "ge", "gt",
			"true", "eq_os", "lt_oq", "le_oq", "unord_s", "neq_us", "nlt_uq", "nle_uq", "ord_s",
			"eq_us", "nge_uq", "ngt_uq", "false_os", "neq_os", "ge_oq", "gt_oq", "true_us"
		};
		const char suf[][4] = { "pd", "ps", "sd", "ss" };
		for (int i = 0; i < 4; i++) {
			const char *s = suf[i];
			for (int j = 0; j < 32; j++) {
				if (j < 8) {
					printf("void cmp%s%s(const Xmm& x, const Operand& op) { cmp%s(x, op, %d); }\n", pred[j], s, s, j);
				}
				printf("void vcmp%s%s(const Xmm& x1, const Xmm& x2, const Operand& op) { vcmp%s(x1, x2, op, %d); }\n", pred[j], s, s, j);
			}
		}
	}
	// vmov(h|l)(pd|ps)
	{
		const struct Tbl {
			bool isH;
			bool isPd;
			uint8_t code;
		} tbl[] = {
			{ true, true, 0x16 },
			{ true, false, 0x16 },
			{ false, true, 0x12 },
			{ false, false, 0x12 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			char c = p.isH ? 'h' : 'l';
			const char *suf = p.isPd ? "pd" : "ps";
			std::string s = type2String(p.isPd ? (T_0F | T_66 | T_EVEX | T_EW1 | T_N8) : (T_0F | T_EVEX | T_EW0 | T_N8));
			printf("void vmov%c%s(const Xmm& x, const Operand& op1, const Operand& op2 = Operand()) { if (!op2.isNone() && !op2.isMEM()) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_X_XM(x, op1, op2, %s, 0x%02X); }\n"
				, c, suf, s.c_str(), p.code);
			printf("void vmov%c%s(const Address& addr, const Xmm& x) { opAVX_X_X_XM(x, xm0, addr, %s, 0x%02X); }\n"
				, c, suf, s.c_str(), p.code + 1);
		}
	}
	// FMA
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			bool supportYMM;
		} tbl[] = {
			{ 0x08, "vfmadd", true },
			{ 0x09, "vfmadd", false },
			{ 0x06, "vfmaddsub", true },
			{ 0x07, "vfmsubadd", true },
			{ 0x0A, "vfmsub", true },
			{ 0x0B, "vfmsub", false },
			{ 0x0C, "vfnmadd", true },
			{ 0x0D, "vfnmadd", false },
			{ 0x0E, "vfnmsub", true },
			{ 0x0F, "vfnmsub", false },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			for (int j = 0; j < 2; j++) {
				const char sufTbl[][2][8] = {
					{ "pd", "ps" },
					{ "sd", "ss" },
				};
				for (int k = 0; k < 3; k++) {
					const struct Ord {
						const char *str;
						uint8_t code;
					} ord[] = {
						{ "132", 0x90 },
						{ "213", 0xA0 },
						{ "231", 0xB0 },
					};
					int t = T_0F38 | T_66 | T_EVEX;
					t |= (j == 0) ? (T_W1 | T_EW1) : (T_W0 | T_EW0);
					if (tbl[i].supportYMM) t |= T_YMM;
					const std::string suf = sufTbl[tbl[i].supportYMM ? 0 : 1][j];
					if (suf == "pd") {
						t |= T_ER_Z | T_B64;
					} else if (suf == "ps") {
						t |= T_ER_Z | T_B32;
					} else if (suf == "sd") {
						t |= T_ER_X | T_N8;
					} else { // ss
						t |= T_ER_X | T_N4;
					}
					std::string s = type2String(t);
					printf("void %s%s%s(const Xmm& x1, const Xmm& x2, const Operand& op) { opAVX_X_X_XM(x1, x2, op, %s, 0x%02X); }\n"
						, tbl[i].name, ord[k].str, suf.c_str(), s.c_str(), tbl[i].code + ord[k].code);
				}
			}
		}
	}
	// FMA others
	{
		printf("void vbroadcastf128(const Ymm& y, const Address& addr) { opAVX_X_XM_IMM(y, addr, T_0F38 | T_66 | T_W0 | T_YMM, 0x1A); }\n");
		printf("void vbroadcasti128(const Ymm& y, const Address& addr) { opAVX_X_XM_IMM(y, addr, T_0F38 | T_66 | T_W0 | T_YMM, 0x5A); }\n");
		printf("void vbroadcastsd(const Ymm& y, const Operand& op) { if (!op.isMEM() && !(y.isYMM() && op.isXMM()) && !(y.isZMM() && op.isXMM())) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_XM_IMM(y, op, T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_EW1 | T_N8, 0x19); }\n");
		const struct Tbl {
			const char *name;
			uint8_t code;
			uint64_t type;
			bool ew1;
		} tbl[] = {
			{ "vbroadcastss", 0x18, T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_N4 },
			{ "vpbroadcastb", 0x78, T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_N1 },
			{ "vpbroadcastw", 0x79, T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_N2 },
			{ "vpbroadcastd", 0x58, T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_N4 },
			{ "vpbroadcastq", 0x59, T_0F38 | T_66 | T_W0 | T_YMM | T_EVEX | T_EW1 | T_N8 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string s = type2String(p.type);
			printf("void %s(const Xmm& x, const Operand& op) { if (!(op.isXMM() || op.isMEM())) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_XM_IMM(x, op, %s, 0x%02X); }\n", p.name, s.c_str(), p.code);
		}

		puts("void vextractf128(const Operand& op, const Ymm& y, uint8_t imm) { if (!(op.isXMEM() && y.isYMM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(y, 0, op, T_0F3A | T_66 | T_W0 | T_YMM, 0x19, imm); }");
		puts("void vextracti128(const Operand& op, const Ymm& y, uint8_t imm) { if (!(op.isXMEM() && y.isYMM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(y, 0, op, T_0F3A | T_66 | T_W0 | T_YMM, 0x39, imm); }");
		puts("void vextractps(const Operand& op, const Xmm& x, uint8_t imm) { if (!((op.isREG(32) || op.isMEM()) && x.isXMM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x, 0, op, T_0F3A | T_66 | T_W0 | T_EVEX | T_N4, 0x17, imm); }");
		puts("void vinsertf128(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { if (!(y1.isYMM() && y2.isYMM() && op.isXMEM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(y1, &y2, op, T_0F3A | T_66 | T_W0 | T_YMM, 0x18, imm); }");
		puts("void vinserti128(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { if (!(y1.isYMM() && y2.isYMM() && op.isXMEM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(y1, &y2, op, T_0F3A | T_66 | T_W0 | T_YMM, 0x38, imm); }");
		puts("void vperm2f128(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { if (!(y1.isYMM() && y2.isYMM() && op.isYMEM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(y1, &y2, op, T_0F3A | T_66 | T_W0 | T_YMM, 0x06, imm); }");
		puts("void vperm2i128(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { if (!(y1.isYMM() && y2.isYMM() && op.isYMEM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(y1, &y2, op, T_0F3A | T_66 | T_W0 | T_YMM, 0x46, imm); }");

		puts("void vlddqu(const Xmm& x, const Address& addr) { opAVX_X_X_XM(x, cvtIdx0(x), addr, T_0F | T_F2 | T_W0 | T_YMM, 0xF0); }");
		puts("void vldmxcsr(const Address& addr) { opAVX_X_X_XM(xm2, xm0, addr, T_0F, 0xAE); }");
		puts("void vstmxcsr(const Address& addr) { opAVX_X_X_XM(xm3, xm0, addr, T_0F, 0xAE); }");
		puts("void vmaskmovdqu(const Xmm& x1, const Xmm& x2) { opAVX_X_X_XM(x1, xm0, x2, T_0F | T_66, 0xF7); }");

		puts("void vpextrb(const Operand& op, const Xmm& x, uint8_t imm) { if (!((op.isREG(8|16|i32e) || op.isMEM()) && x.isXMM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x, 0, op, T_0F3A | T_66 | T_EVEX | T_N1, 0x14, imm); }");
		puts("void vpextrw(const Operand& op, const Xmm& x, uint8_t imm) { if (!((op.isREG(16|i32e) || op.isMEM()) && x.isXMM())) XBYAK_THROW(ERR_BAD_COMBINATION) if (op.isREG() && x.getIdx() < 16) { opAVX_X_X_XM(Xmm(op.getIdx()), xm0, x, T_0F | T_66, 0xC5, imm); } else { opVex(x, 0, op, T_0F3A | T_66 | T_EVEX | T_N2, 0x15, imm); } }");
		puts("void vpextrd(const Operand& op, const Xmm& x, uint8_t imm) { if (!((op.isREG(32) || op.isMEM()) && x.isXMM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x, 0, op, T_0F3A | T_66 | T_W0 | T_EVEX | T_EW0 | T_N4, 0x16, imm); }");
		puts("void vpextrq(const Operand& op, const Xmm& x, uint8_t imm) { if (!((op.isREG(64) || op.isMEM()) && x.isXMM())) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x, 0, op, T_0F3A | T_66 | T_W1 | T_EVEX | T_EW1 | T_N8, 0x16, imm); }");

		puts("void vpinsrb(const Xmm& x1, const Xmm& x2, const Operand& op, uint8_t imm) { if (!(x1.isXMM() && x2.isXMM() && (op.isREG(32) || op.isMEM()))) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x1, &x2, op, T_0F3A | T_66 | T_EVEX | T_N1, 0x20, imm); }");
		puts("void vpinsrw(const Xmm& x1, const Xmm& x2, const Operand& op, uint8_t imm) { if (!(x1.isXMM() && x2.isXMM() && (op.isREG(32) || op.isMEM()))) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x1, &x2, op, T_0F | T_66 | T_EVEX | T_N2, 0xC4, imm); }");
		puts("void vpinsrd(const Xmm& x1, const Xmm& x2, const Operand& op, uint8_t imm) { if (!(x1.isXMM() && x2.isXMM() && (op.isREG(32) || op.isMEM()))) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x1, &x2, op, T_0F3A | T_66 | T_W0 | T_EVEX | T_EW0 | T_N4, 0x22, imm); }");
		puts("void vpinsrq(const Xmm& x1, const Xmm& x2, const Operand& op, uint8_t imm) { if (!(x1.isXMM() && x2.isXMM() && (op.isREG(64) || op.isMEM()))) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x1, &x2, op, T_0F3A | T_66 | T_W1 | T_EVEX | T_EW1 | T_N8, 0x22, imm); }");

		puts("void vpmovmskb(const Reg32e& r, const Xmm& x) { if (!x.is(Operand::XMM | Operand::YMM)) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(x.isYMM() ? Ymm(r.getIdx()) : Xmm(r.getIdx()), 0, x, T_0F | T_66 | T_YMM, 0xD7); }");

	}
	// (x, x, imm), (x, imm)
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
			int idx;
			uint64_t type;
		} tbl[] = {
			{ "pslldq", 0x73, 7, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX },
			{ "psrldq", 0x73, 3, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX },
			{ "psllw", 0x71, 6, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX },
			{ "pslld", 0x72, 6, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX | T_EW0 | T_B32 },
			{ "psllq", 0x73, 6, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX | T_EW1 | T_B64 },
			{ "psraw", 0x71, 4, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX },
			{ "psrad", 0x72, 4, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX | T_EW0 | T_B32 },
			{ "psrlw", 0x71, 2, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX },
			{ "psrld", 0x72, 2, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX | T_EW0 | T_B32 },
			{ "psrlq", 0x73, 2, T_0F | T_66 | T_YMM | T_EVEX | T_MEM_EVEX | T_EW1 | T_B64 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string s = type2String(p.type);
			printf("void v%s(const Xmm& x, const Operand& op, uint8_t imm) { opAVX_X_X_XM(Xmm(x.getKind(), %d), x, op, %s, 0x%02X, imm); }\n", p.name, p.idx, s.c_str(), p.code);
		}
	}
	// 4-op
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "vblendvpd", 0x4B },
			{ "vblendvps", 0x4A },
			{ "vpblendvb", 0x4C },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %s(const Xmm& x1, const Xmm& x2, const Operand& op, const Xmm& x4) { opAVX_X_X_XM(x1, x2, op, T_0F3A | T_66 | T_YMM, 0x%02X, x4.getIdx() << 4); }\n", p.name, p.code);
		}
	}
	// mov
	{
		printf("void vmovq(const Xmm& x, const Address& addr) { uint64_t type; uint8_t code; if (x.getIdx() < 16) { type = T_0F | T_F3; code = 0x7E; } else { type = T_0F | T_66 | T_EVEX | T_EW1 | T_N8; code = 0x6E; } opAVX_X_X_XM(x, xm0, addr, type, code); }\n");
		printf("void vmovq(const Address& addr, const Xmm& x) { opAVX_X_X_XM(x, xm0, addr, T_0F | T_66 | T_EVEX | T_EW1 | T_N8, x.getIdx() < 16 ? 0xD6 : 0x7E); }\n");
		printf("void vmovq(const Xmm& x1, const Xmm& x2) { opAVX_X_X_XM(x1, xm0, x2, T_0F | T_F3 | T_EVEX | T_EW1 | T_N8, 0x7E); }\n");

		printf("void vmovhlps(const Xmm& x1, const Xmm& x2, const Operand& op = Operand()) { if (!op.isNone() && !op.isXMM()) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_X_XM(x1, x2, op, T_0F | T_EVEX | T_EW0, 0x12); }\n");
		printf("void vmovlhps(const Xmm& x1, const Xmm& x2, const Operand& op = Operand()) { if (!op.isNone() && !op.isXMM()) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_X_XM(x1, x2, op, T_0F | T_EVEX | T_EW0, 0x16); }\n");

		printf("void vmovmskpd(const Reg& r, const Xmm& x) { if (!r.isBit(i32e)) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_X_XM(x.isXMM() ? Xmm(r.getIdx()) : Ymm(r.getIdx()), cvtIdx0(x), x, T_0F | T_66 | T_W0 | T_YMM, 0x50); }\n");
		printf("void vmovmskps(const Reg& r, const Xmm& x) { if (!r.isBit(i32e)) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_X_XM(x.isXMM() ? Xmm(r.getIdx()) : Ymm(r.getIdx()), cvtIdx0(x), x, T_0F | T_W0 | T_YMM, 0x50); }\n");

		puts("void vmovntdq(const Address& addr, const Xmm& x) { opVex(x, 0, addr, T_0F | T_66 | T_YMM | T_EVEX | T_EW0, 0xE7); }");
		puts("void vmovntpd(const Address& addr, const Xmm& x) { opVex(x, 0, addr, T_0F | T_66 | T_YMM | T_EVEX | T_EW1, 0x2B); }");
		puts("void vmovntps(const Address& addr, const Xmm& x) { opVex(x, 0, addr, T_0F | T_YMM | T_EVEX | T_EW0, 0x2B); }");
		puts("void vmovntdqa(const Xmm& x, const Address& addr) { opVex(x, 0, addr, T_0F38 | T_66 | T_YMM | T_EVEX | T_EW0, 0x2A); }");

		// vmovsd, vmovss
		for (int i = 0; i < 2; i++) {
			char c1 = i == 0 ? 'd' : 's';
			uint64_t type = T_0F | T_EVEX;
			type |= i == 0 ? (T_F2 | T_EW1 | T_N8) : (T_F3 | T_EW0 | T_N4);
			std::string s = type2String(type);
			printf("void vmovs%c(const Xmm& x1, const Xmm& x2, const Operand& op = Operand()) { if (!op.isNone() && !op.isXMM()) XBYAK_THROW(ERR_BAD_COMBINATION) opAVX_X_X_XM(x1, x2, op, %s, 0x10); }\n", c1, s.c_str());
			printf("void vmovs%c(const Xmm& x, const Address& addr) { opAVX_X_X_XM(x, xm0, addr, %s, 0x10); }\n", c1, s.c_str());
			printf("void vmovs%c(const Address& addr, const Xmm& x) { opAVX_X_X_XM(x, xm0, addr, %s | T_M_K, 0x11); }\n", c1, s.c_str());
		}
	}
	// cvt
	{
		puts("void vcvtss2si(const Reg32& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F3 | T_W0 | T_EVEX | T_EW0 | T_ER_X | T_N8, 0x2D); }");
		puts("void vcvttss2si(const Reg32& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F3 | T_W0 | T_EVEX | T_EW0 | T_SAE_X | T_N8, 0x2C); }");
		puts("void vcvtsd2si(const Reg32& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F2 | T_W0 | T_EVEX | T_EW0 | T_N4 | T_ER_X, 0x2D); }");
		puts("void vcvttsd2si(const Reg32& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F2 | T_W0 | T_EVEX | T_EW0 | T_N4 | T_SAE_X, 0x2C); }");

		puts("void vcvtsi2ss(const Xmm& x1, const Xmm& x2, const Operand& op) { opCvt3(x1, x2, op, T_0F | T_F3 | T_EVEX | T_ER_X, T_W1 | T_EW1 | T_N8, T_W0 | T_EW0 | T_N4, 0x2A); }");
		puts("void vcvtsi2sd(const Xmm& x1, const Xmm& x2, const Operand& op) { opCvt3(x1, x2, op, T_0F | T_F2 | T_EVEX, T_W1 | T_EW1 | T_ER_X | T_N8, T_W0 | T_EW0 | T_N4, 0x2A); }");


		puts("void vcvtps2pd(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_0F | T_YMM | T_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_SAE_Y, 0x5A); }");
		puts("void vcvtdq2pd(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_0F | T_F3 | T_YMM | T_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL, 0xE6); }");

		puts("void vcvtpd2ps(const Xmm& x, const Operand& op) { opCvt2(x, op, T_0F | T_66 | T_YMM | T_EVEX | T_EW1 | T_B64 | T_ER_Z, 0x5A); }");
		puts("void vcvtpd2dq(const Xmm& x, const Operand& op) { opCvt2(x, op, T_0F | T_F2 | T_YMM | T_EVEX | T_EW1 | T_B64 | T_ER_Z, 0xE6); }");

		puts("void vcvttpd2dq(const Xmm& x, const Operand& op) { opCvt2(x, op, T_66 | T_0F | T_YMM | T_EVEX |T_EW1 | T_B64 | T_SAE_Z, 0xE6); }");

		puts("void vcvtph2ps(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_0F38 | T_66 | T_W0 | T_EVEX | T_EW0 | T_N8 | T_N_VL | T_SAE_Y, 0x13); }");
		puts("void vcvtps2ph(const Operand& op, const Xmm& x, uint8_t imm) { checkCvt1(x, op); opVex(x, 0, op, T_0F3A | T_66 | T_W0 | T_EVEX | T_EW0 | T_N8 | T_N_VL | T_SAE_Y | T_M_K, 0x1D, imm); }");

	}
	{
		const struct Tbl {
			const char *name;
			uint64_t type;
			uint8_t code;
		} tbl[] = {
			{ "vbcstnebf162ps", T_F3 | T_0F38 | T_W0 | T_B16 | T_YMM, 0xB1 },
			{ "vbcstnesh2ps", T_66 | T_0F38 | T_W0 | T_B16 | T_YMM, 0xB1 },
			{ "vcvtneebf162ps", T_F3 | T_0F38 | T_W0 | T_YMM, 0xB0 },
			{ "vcvtneeph2ps", T_66 | T_0F38 | T_W0 | T_YMM, 0xB0 },
			{ "vcvtneobf162ps", T_F2 | T_0F38 | T_W0 | T_YMM, 0xB0 },
			{ "vcvtneoph2ps", T_0F38 | T_W0 | T_YMM, 0xB0 }
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %s(const Xmm& x, const Address& addr) { opVex(x, 0, addr, %s, 0x%02X); }\n", p.name, type2String(p.type).c_str(), p.code);
		}
	}
	// haswell gpr(reg, reg, r/m)
	{
		const struct Tbl {
			const char *name;
			uint64_t type;
			uint8_t code;
		} tbl[] = {
			{ "andn", T_0F38 | T_APX | T_NF, 0xF2 },
			{ "mulx", T_F2 | T_0F38 | T_APX , 0xF6 },
			{ "pdep", T_F2 | T_0F38 | T_APX, 0xF5 },
			{ "pext", T_F3 | T_0F38 | T_APX, 0xF5 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %s(const Reg32e& r1, const Reg32e& r2, const Operand& op) { opRRO(r1, r2, op, %s, 0x%x); }\n", p.name, type2String(p.type).c_str(), p.code);
		}
	}
	// gpr(reg, r/m, reg)
	{
		const struct Tbl {
			const char *name;
			uint64_t type;
			uint8_t code;
		} tbl[] = {
			{ "bextr", T_0F38 | T_APX | T_NF, 0xF7 },
			{ "bzhi", T_0F38 | T_APX | T_NF, 0xF5 },
			{ "sarx", T_0F38 | T_F3 | T_APX, 0xF7 },
			{ "shlx", T_0F38 | T_66 | T_APX, 0xF7 },
			{ "shrx", T_0F38 | T_F2 | T_APX, 0xF7 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %s(const Reg32e& r1, const Operand& op, const Reg32e& r2) { opRRO(r1, r2, op, %s, 0x%x); }\n", p.name, type2String(p.type).c_str(), p.code);
		}
		puts("void rorx(const Reg32e& r, const Operand& op, uint8_t imm) { opRRO(r, Reg32e(0, r.getBit()), op, T_0F3A|T_F2|T_APX, 0xF0, imm); }");
	}
	// gpr(reg, r/m)
	{
		const struct Tbl {
			const char *name;
			uint64_t type;
			uint8_t code;
			uint8_t idx;
		} tbl[] = {
			{ "blsi", T_0F38 | T_APX|T_NF, 0xF3, 3 },
			{ "blsmsk", T_0F38 | T_APX|T_NF, 0xF3, 2 },
			{ "blsr", T_0F38 | T_APX|T_NF, 0xF3, 1 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %s(const Reg32e& r, const Operand& op) { opRRO(Reg32e(%d, r.getBit()), r, op, %s, 0x%x); }\n", p.name, p.idx, type2String(p.type).c_str(), p.code);
		}
	}
	// gather
	{
		const int y_vx_y = 0;
		const int y_vy_y = 1;
		const int x_vy_x = 2;
		const struct Tbl {
			const char *name;
			uint8_t code;
			int w;
			int mode;
		} tbl[] = {
			{ "vgatherdpd", 0x92, 1, y_vx_y },
			{ "vgatherqpd", 0x93, 1, y_vy_y },
			{ "vgatherdps", 0x92, 0, y_vy_y },
			{ "vgatherqps", 0x93, 0, x_vy_x },
			{ "vpgatherdd",  0x90, 0, y_vy_y },
			{ "vpgatherqd",  0x91, 0, x_vy_x },
			{ "vpgatherdq",  0x90, 1, y_vx_y },
			{ "vpgatherqq",  0x91, 1, y_vy_y },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %s(const Xmm& x1, const Address& addr, const Xmm& x2) { opGather(x1, addr, x2, T_0F38 | T_66 | T_YMM | T_VSIB | T_W%d, 0x%x, %d); }\n", p.name, p.w, p.code, p.mode);
		}
	}
	// vnni
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
		} tbl[] = {
			{ 0x50, "vpdpbusd", T_66 | T_0F38 | T_YMM | T_EW0 | T_SAE_Z | T_B32},
			{ 0x51, "vpdpbusds", T_66 | T_0F38 | T_YMM | T_EW0 | T_SAE_Z | T_B32},
			{ 0x52, "vpdpwssd", T_66 | T_0F38 | T_YMM | T_EW0 | T_SAE_Z | T_B32},
			{ 0x53, "vpdpwssds", T_66 | T_0F38 | T_YMM | T_EW0 | T_SAE_Z | T_B32},
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s = type2String(p->type);
			printf("void %s(const Xmm& x1, const Xmm& x2, const Operand& op, PreferredEncoding encoding = DefaultEncoding) { opEncoding(x1, x2, op, %s, 0x%02X, encoding); }\n", p->name, s.c_str(), p->code);
		}
	}
}

void put32()
{
	put_jREGz("cx", true);
	put_jREGz("ecx", false);

	const GenericTbl tbl[] = {
		{ "aaa", 0x37 },
		{ "aad", 0xD5, 0x0A },
		{ "aam", 0xD4, 0x0A },
		{ "aas", 0x3F },
		{ "daa", 0x27 },
		{ "das", 0x2F },
		{ "into", 0xCE },
		{ "popad", 0x61 },
		{ "popfd", 0x9D },
		{ "pusha", 0x60 },
		{ "pushad", 0x60 },
		{ "pushfd", 0x9C },
		{ "popa", 0x61 },
	};
	putGeneric(tbl, NUM_OF_ARRAY(tbl));
	putLoadSeg("lds", 0, 0xC5);
	putLoadSeg("les", 0, 0xC4);
}

void put64()
{
	put_jREGz("ecx", true);
	put_jREGz("rcx", false);

	{
		const GenericTbl tbl[] = {
			{ "cdqe", 0x48, 0x98 },
			{ "cqo", 0x48, 0x99 },
			{ "cmpsq", 0x48, 0xA7 },
			{ "popfq", 0x9D },
			{ "pushfq", 0x9C },
			{ "lodsq", 0x48, 0xAD },
			{ "movsq", 0x48, 0xA5 },
			{ "scasq", 0x48, 0xAF },
			{ "stosq", 0x48, 0xAB },
			{ "syscall", 0x0F, 0x05 },
			{ "sysret", 0x0F, 0x07 },
			{ "clui", 0xF3, 0x0F, 0x01, 0xEE },
			{ "stui", 0xF3, 0x0F, 0x01, 0xEF },
			{ "testui", 0xF3, 0x0F, 0x01, 0xED },
			{ "uiret", 0xF3, 0x0F, 0x01, 0xEC },
		};
		putGeneric(tbl, NUM_OF_ARRAY(tbl));
	}

	putMemOp("cmpxchg16b", "T_0F", 1, 0xC7, 64);
	putMemOp("fxrstor64", "T_0F", 1, 0xAE, 64);
	puts("void movq(const Reg64& reg, const Mmx& mmx) { if (mmx.isXMM()) db(0x66); opSSE(mmx, reg, T_0F, 0x7E); }");
	puts("void movq(const Mmx& mmx, const Reg64& reg) { if (mmx.isXMM()) db(0x66); opSSE(mmx, reg, T_0F, 0x6E); }");
	puts("void movsxd(const Reg64& reg, const Operand& op) { if (!op.isBit(32)) XBYAK_THROW(ERR_BAD_COMBINATION) opRO(reg, op, T_ALLOW_DIFF_SIZE, 0x63); }");
	puts("void pextrq(const Operand& op, const Xmm& xmm, uint8_t imm) { if (!op.isREG(64) && !op.isMEM()) XBYAK_THROW(ERR_BAD_COMBINATION) opSSE(Reg64(xmm.getIdx()), op, T_66 | T_0F3A, 0x16, 0, imm); }");
	puts("void pinsrq(const Xmm& xmm, const Operand& op, uint8_t imm) { if (!op.isREG(64) && !op.isMEM()) XBYAK_THROW(ERR_BAD_COMBINATION) opSSE(Reg64(xmm.getIdx()), op, T_66 | T_0F3A, 0x22, 0, imm); }");
	puts("void senduipi(const Reg64& r) { opRR(Reg32(6), r.cvt32(), T_F3 | T_0F, 0xC7); }");

	puts("void vcvtss2si(const Reg64& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F3 | T_W1 | T_EVEX | T_EW1 | T_ER_X | T_N8, 0x2D); }");
	puts("void vcvttss2si(const Reg64& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F3 | T_W1 | T_EVEX | T_EW1 | T_SAE_X | T_N8, 0x2C); }");
	puts("void vcvtsd2si(const Reg64& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F2 | T_W1 | T_EVEX | T_EW1 | T_N4 | T_ER_X, 0x2D); }");
	puts("void vcvttsd2si(const Reg64& r, const Operand& op) { opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, T_0F | T_F2 | T_W1 | T_EVEX | T_EW1 | T_N4 | T_SAE_X, 0x2C); }");

	puts("void vmovq(const Xmm& x, const Reg64& r) { opAVX_X_X_XM(x, xm0, Xmm(r.getIdx()), T_66 | T_0F | T_W1 | T_EVEX | T_EW1, 0x6E); }");
	puts("void vmovq(const Reg64& r, const Xmm& x) { opAVX_X_X_XM(x, xm0, Xmm(r.getIdx()), T_66 | T_0F | T_W1 | T_EVEX | T_EW1, 0x7E); }");
	puts("void jmpabs(uint64_t addr) { db(0xD5); db(0x00); db(0xA1); dq(addr); }");
	puts("void push2(const Reg64& r1, const Reg64& r2) { opROO(r1, r2, Reg64(6), T_APX|T_ND1|T_W0, 0xFF); }");
	puts("void push2p(const Reg64& r1, const Reg64& r2) { opROO(r1, r2, Reg64(6), T_APX|T_ND1|T_W1, 0xFF); }");
	puts("void pop2(const Reg64& r1, const Reg64& r2) { opROO(r1, r2, Reg64(0), T_APX|T_ND1|T_W0, 0x8F); }");
	puts("void pop2p(const Reg64& r1, const Reg64& r2) { opROO(r1, r2, Reg64(0), T_APX|T_ND1|T_W1, 0x8F); }");
	// CMPccXADD
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "be", 0xE6 },
			{ "b", 0xE2 },
			{ "le", 0xEE },
			{ "l", 0xEC },
			{ "nbe", 0xE7 },
			{ "nb", 0xE3 },
			{ "nle", 0xEF },
			{ "nl", 0xED },
			{ "no", 0xE1 },
			{ "np", 0xEB },
			{ "ns", 0xE9 },
			{ "nz", 0xE5 },
			{ "o", 0xE0 },
			{ "p", 0xEA },
			{ "s", 0xE8 },
			{ "z", 0xE4 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			printf("void cmp%sxadd(const Address& addr, const Reg32e& r1, const Reg32e& r2) { opRRO(r1, r2, addr, T_APX|T_66|T_0F38, 0x%02X); }\n", p->name, p->code);
		}
	}
	// aes
	{
		const struct Tbl {
			const char *name;
			uint64_t type1;
			uint64_t type2;
			uint8_t code;
			int idx;
		} tbl[] = {
			{ "aesdec128kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xDD, 8 },
			{ "aesdec256kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xDF, 8 },
			{ "aesdecwide128kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xD8, 1 },
			{ "aesdecwide256kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xD8, 3 },
			{ "aesenc128kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xDC, 8 },
			{ "aesenc256kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xDE, 8 },
			{ "aesencwide128kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xD8, 0 },
			{ "aesencwide256kl", T_F3|T_0F38, T_MUST_EVEX|T_F3, 0xD8, 2 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl *p = &tbl[i];
			std::string s1 = type2String(p->type1);
			std::string s2 = type2String(p->type2);
			if (p->idx == 8) {
				printf("void %s(const Xmm& x, const Address& addr) { opSSE_APX(x, addr, %s, 0x%02X, %s, 0x%02X); }\n", p->name, s1.c_str(), p->code, s2.c_str(), p->code);
			} else {
				printf("void %s(const Address& addr) { opSSE_APX(xmm%d, addr, %s, 0x%02X, %s, 0x%02X); }\n", p->name, p->idx, s1.c_str(), p->code, s2.c_str(), p->code);
			}
		}
	}
	// encodekey
	puts("void encodekey128(const Reg32& r1, const Reg32& r2) { opEncodeKey(r1, r2, 0xFA, 0xDA); }");
	puts("void encodekey256(const Reg32& r1, const Reg32& r2) { opEncodeKey(r1, r2, 0xFB, 0xDB); }");
	// read/write fs/gs
	{
		const char *tbl[] = {
			"rdfs", "rdgs", "wrfs", "wrgs"
		};
		for (int i = 0; i < 4; i++) {
			printf("void %sbase(const Reg32e& r) { opRR(%s, r, T_F3|T_0F|T_ALLOW_DIFF_SIZE, 0xAE); }\n", tbl[i], Reg(i, Operand::REG, 32).toString());
		}
	}
}

void putAMX_TILE()
{
	puts("void ldtilecfg(const Address& addr) { if (opROO(Reg(), addr, tmm0, T_APX|T_0F38|T_W0, 0x49)) return; opVex(tmm0, &tmm0, addr, T_0F38|T_W0, 0x49); }");
	puts("void sttilecfg(const Address& addr) { if (opROO(Reg(), addr, tmm0, T_APX|T_66|T_0F38|T_W0, 0x49)) return; opVex(tmm0, &tmm0, addr, T_66|T_0F38 | T_W0, 0x49); }");
	puts("void tileloadd(const Tmm& tm, const Address& addr) { opAMX(tm, addr, T_F2|T_0F38|T_W0, 0x4B); }");
	puts("void tileloaddt1(const Tmm& tm, const Address& addr) { opAMX(tm, addr, T_66|T_0F38|T_W0, 0x4B); }");
	puts("void tilerelease() { db(0xc4); db(0xe2); db(0x78); db(0x49); db(0xc0); }");
	puts("void tilestored(const Address& addr, const Tmm& tm) { if (opROO(Reg(), addr, tm, T_APX|T_F3|T_0F38|T_W0, 0x4B)) return; opVex(tm, &tmm0, addr, T_F3|T_0F38|T_W0, 0x4B); }");
	puts("void tilezero(const Tmm& Tmm) { opVex(Tmm, &tmm0, tmm0, T_F2 | T_0F38 | T_W0, 0x49); }");
}
void putAMX_INT8()
{
	puts("void tdpbssd(const Tmm& x1, const Tmm& x2, const Tmm& x3) { opVex(x1, &x3, x2, T_F2 | T_0F38 | T_W0, 0x5e); }");
	puts("void tdpbsud(const Tmm& x1, const Tmm& x2, const Tmm& x3) { opVex(x1, &x3, x2, T_F3 | T_0F38 | T_W0, 0x5e); }");
	puts("void tdpbusd(const Tmm& x1, const Tmm& x2, const Tmm& x3) { opVex(x1, &x3, x2, T_66 | T_0F38 | T_W0, 0x5e); }");
	puts("void tdpbuud(const Tmm& x1, const Tmm& x2, const Tmm& x3) { opVex(x1, &x3, x2, T_0F38 | T_W0, 0x5e); }");
	puts("void tdpfp16ps(const Tmm &x1, const Tmm &x2, const Tmm &x3) { opVex(x1, &x3, x2, T_F2 | T_0F38 | T_W0, 0x5c); }");
}
void putAMX_BF16()
{
	puts("void tdpbf16ps(const Tmm& x1, const Tmm& x2, const Tmm& x3) { opVex(x1, &x3, x2, T_F3 | T_0F38 | T_W0, 0x5c); }");
}

void putFixed()
{
	puts("#ifdef XBYAK64");
	put64();
	putAMX_TILE();
	putAMX_INT8();
	putAMX_BF16();
	puts("#else");
	put32();
	puts("#endif");
	puts("#ifndef XBYAK_NO_OP_NAMES");
	const char *tbl[] = {
		"and", "or", "xor",
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const char *name = tbl[i];
		printf("void %s(const Operand& op1, const Operand& op2) { %s_(op1, op2); }\n", name, name);
		printf("void %s(const Operand& op, uint32_t imm) { %s_(op, imm); }\n", name, name);
	}
	puts("void not(const Operand& op) { not_(op); }");
	puts("#endif");
}

void putOmit()
{
	puts("void vpinsrb(const Xmm& x, const Operand& op, uint8_t imm) { vpinsrb(x, x, op, imm); }");
	puts("void vpinsrd(const Xmm& x, const Operand& op, uint8_t imm) { vpinsrd(x, x, op, imm); }");
	puts("void vpinsrq(const Xmm& x, const Operand& op, uint8_t imm) { vpinsrq(x, x, op, imm); }");
	puts("void vpinsrw(const Xmm& x, const Operand& op, uint8_t imm) { vpinsrw(x, x, op, imm); }");

	puts("void vcvtsi2sd(const Xmm& x, const Operand& op) { vcvtsi2sd(x, x, op); }");
	puts("void vcvtsi2ss(const Xmm& x, const Operand& op) { vcvtsi2ss(x, x, op); }");
	{
		const char pred[32][16] = {
			"eq", "lt", "le", "unord", "neq", "nlt", "nle", "ord",
			"eq_uq", "nge", "ngt", "false", "neq_oq", "ge", "gt",
			"true", "eq_os", "lt_oq", "le_oq", "unord_s", "neq_us", "nlt_uq", "nle_uq", "ord_s",
			"eq_us", "nge_uq", "ngt_uq", "false_os", "neq_os", "ge_oq", "gt_oq", "true_us"
		};
		const char suf[][4] = { "pd", "ps", "sd", "ss" };
		for (int i = 0; i < 4; i++) {
			const char *s = suf[i];
			for (int j = 0; j < 32; j++) {
				printf("void vcmp%s%s(const Xmm& x, const Operand& op) { vcmp%s%s(x, x, op); }\n", pred[j], s, pred[j], s);
			}
		}
	}
	{
		const char *tbl[] = {
			"pslldq",
			"psrldq",
			"psllw",
			"pslld",
			"psllq",
			"psraw",
			"psrad",
			"psrlw",
			"psrld",
			"psrlq",
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const char *name = tbl[i];
			printf("void v%s(const Xmm& x, uint8_t imm) { v%s(x, x, imm); }\n", name, name);
		}
	}
	{
		const char *tbl[] = {
			"vblendvpd",
			"vblendvps",
			"vpblendvb",
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const char *name = tbl[i];
			printf("void %s(const Xmm& x1, const Operand& op, const Xmm& x4) { %s(x1, x1, op, x4); }\n", name, name);
		}
	}
	putX_X_XM(true);
}

int main(int argc, char *argv[])
{
	std::string mode = argc == 2 ? argv[1] : "";
	if (mode == "") {
		put();
	} else if (mode == "fixed") {
		putFixed();
	} else {
		putOmit();
	}
}
