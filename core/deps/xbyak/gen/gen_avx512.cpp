#define XBYAK_DONT_READ_LIST
#include <stdio.h>
#include <string.h>
#include "../xbyak/xbyak.h"
#define NUM_OF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

using namespace Xbyak;
#ifdef _MSC_VER
	#pragma warning(disable : 4996) // scanf
	#define snprintf _snprintf_s
#endif

#include "avx_type.hpp"

void putOpmask(bool only64bit)
{
	if (only64bit) {
		puts("void kmovq(const Opmask& k, const Reg64& r) { opVex(k, 0, r, T_L0 | T_0F | T_F2 | T_W1, 0x92); }");
		puts("void kmovq(const Reg64& r, const Opmask& k) { opVex(r, 0, k, T_L0 | T_0F | T_F2 | T_W1, 0x93); }");
		return;
	}

	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "kadd", 0x4A },
			{ "kand", 0x41 },
			{ "kandn", 0x42 },
			{ "kor", 0x45 },
			{ "kxnor", 0x46 },
			{ "kxor", 0x47 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %sw(const Opmask& r1, const Opmask& r2, const Opmask& r3) { opVex(r1, &r2, r3, T_L1 | T_0F | T_W0, 0x%02X); }\n", p.name, p.code);
			printf("void %sq(const Opmask& r1, const Opmask& r2, const Opmask& r3) { opVex(r1, &r2, r3, T_L1 | T_0F | T_W1, 0x%02X); }\n", p.name, p.code);
			printf("void %sb(const Opmask& r1, const Opmask& r2, const Opmask& r3) { opVex(r1, &r2, r3, T_L1 | T_0F | T_66 | T_W0, 0x%02X); }\n", p.name, p.code);
			printf("void %sd(const Opmask& r1, const Opmask& r2, const Opmask& r3) { opVex(r1, &r2, r3, T_L1 | T_0F | T_66 | T_W1, 0x%02X); }\n", p.name, p.code);
		}
		printf("void kunpckbw(const Opmask& r1, const Opmask& r2, const Opmask& r3) { opVex(r1, &r2, r3, T_L1 | T_0F | T_66 | T_W0, 0x4B); }\n");
		printf("void kunpckwd(const Opmask& r1, const Opmask& r2, const Opmask& r3) { opVex(r1, &r2, r3, T_L1 | T_0F | T_W0, 0x4B); }\n");
		printf("void kunpckdq(const Opmask& r1, const Opmask& r2, const Opmask& r3) { opVex(r1, &r2, r3, T_L1 | T_0F | T_W1, 0x4B); }\n");
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "knot", 0x44 },
			{ "kortest", 0x98 },
			{ "ktest", 0x99 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %sw(const Opmask& r1, const Opmask& r2) { opVex(r1, 0, r2, T_0F | T_W0, 0x%02X); }\n", p.name, p.code);
			printf("void %sq(const Opmask& r1, const Opmask& r2) { opVex(r1, 0, r2, T_0F | T_W1, 0x%02X); }\n", p.name, p.code);
			printf("void %sb(const Opmask& r1, const Opmask& r2) { opVex(r1, 0, r2, T_0F | T_66 | T_W0, 0x%02X); }\n", p.name, p.code);
			printf("void %sd(const Opmask& r1, const Opmask& r2) { opVex(r1, 0, r2, T_0F | T_66 | T_W1, 0x%02X); }\n", p.name, p.code);
		}
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
		} tbl[] = {
			{ "kshiftl", 0x32 },
			{ "kshiftr", 0x30 },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			printf("void %sw(const Opmask& r1, const Opmask& r2, uint8_t imm) { opVex(r1, 0, r2, T_66 | T_0F3A | T_W1, 0x%02X, imm); }\n", p.name, p.code);
			printf("void %sq(const Opmask& r1, const Opmask& r2, uint8_t imm) { opVex(r1, 0, r2, T_66 | T_0F3A | T_W1, 0x%02X, imm); }\n", p.name, p.code + 1);
			printf("void %sb(const Opmask& r1, const Opmask& r2, uint8_t imm) { opVex(r1, 0, r2, T_66 | T_0F3A | T_W0, 0x%02X, imm); }\n", p.name, p.code);
			printf("void %sd(const Opmask& r1, const Opmask& r2, uint8_t imm) { opVex(r1, 0, r2, T_66 | T_0F3A | T_W0, 0x%02X, imm); }\n", p.name, p.code + 1);
		}
	}
	puts("void kmovw(const Opmask& k, const Operand& op) { if (!op.isMEM() && !op.isOPMASK()) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(k, 0, op, T_L0 | T_0F | T_W0, 0x90); }");
	puts("void kmovq(const Opmask& k, const Operand& op) { if (!op.isMEM() && !op.isOPMASK()) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(k, 0, op, T_L0 | T_0F | T_W1, 0x90); }");
	puts("void kmovb(const Opmask& k, const Operand& op) { if (!op.isMEM() && !op.isOPMASK()) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(k, 0, op, T_L0 | T_0F | T_66 | T_W0, 0x90); }");
	puts("void kmovd(const Opmask& k, const Operand& op) { if (!op.isMEM() && !op.isOPMASK()) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(k, 0, op, T_L0 | T_0F | T_66 | T_W1, 0x90); }");

	puts("void kmovw(const Address& addr, const Opmask& k) { opVex(k, 0, addr, T_L0 | T_0F | T_W0, 0x91); }");
	puts("void kmovq(const Address& addr, const Opmask& k) { opVex(k, 0, addr, T_L0 | T_0F | T_W1, 0x91); }");
	puts("void kmovb(const Address& addr, const Opmask& k) { opVex(k, 0, addr, T_L0 | T_0F | T_66 | T_W0, 0x91); }");
	puts("void kmovd(const Address& addr, const Opmask& k) { opVex(k, 0, addr, T_L0 | T_0F | T_66 | T_W1, 0x91); }");

	puts("void kmovw(const Opmask& k, const Reg32& r) { opVex(k, 0, r, T_L0 | T_0F | T_W0, 0x92); }");
	puts("void kmovw(const Reg32& r, const Opmask& k) { opVex(r, 0, k, T_L0 | T_0F | T_W0, 0x93); }");
	puts("void kmovb(const Opmask& k, const Reg32& r) { opVex(k, 0, r, T_L0 | T_0F | T_66 | T_W0, 0x92); }");
	puts("void kmovb(const Reg32& r, const Opmask& k) { opVex(r, 0, k, T_L0 | T_0F | T_66 | T_W0, 0x93); }");
	puts("void kmovd(const Opmask& k, const Reg32& r) { opVex(k, 0, r, T_L0 | T_0F | T_F2 | T_W0, 0x92); }");
	puts("void kmovd(const Reg32& r, const Opmask& k) { opVex(r, 0, k, T_L0 | T_0F | T_F2 | T_W0, 0x93); }");
}

// vcmppd(k, x, op)
void putVcmp()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		int type;
		bool hasIMM;
	} tbl[] = {
		{ 0xC2, "vcmppd", T_0F | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_YMM | T_66 | T_B64, true },
		{ 0xC2, "vcmpps", T_0F | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_YMM | T_B32, true },
		{ 0xC2, "vcmpsd", T_0F | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_F2 | T_N8, true },
		{ 0xC2, "vcmpss", T_0F | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_F3 | T_N4, true },

		{ 0x74, "vpcmpeqb", T_66 | T_0F | T_MUST_EVEX | T_YMM, false },
		{ 0x75, "vpcmpeqw", T_66 | T_0F | T_MUST_EVEX | T_YMM, false },
		{ 0x76, "vpcmpeqd", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_B32, false },
		{ 0x29, "vpcmpeqq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },

		{ 0x64, "vpcmpgtb", T_66 | T_0F | T_MUST_EVEX | T_YMM, false },
		{ 0x65, "vpcmpgtw", T_66 | T_0F | T_MUST_EVEX | T_YMM, false },
		{ 0x66, "vpcmpgtd", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, false },
		{ 0x37, "vpcmpgtq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },

		{ 0x3F, "vpcmpb", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0, true },
		{ 0x3E, "vpcmpub", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0, true },

		{ 0x3F, "vpcmpw", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1, true },
		{ 0x3E, "vpcmpuw", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1, true },
		{ 0x1F, "vpcmpd", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, true },
		{ 0x1E, "vpcmpud", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, true },
		{ 0x1F, "vpcmpq", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, true },
		{ 0x1E, "vpcmpuq", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, true },

		{ 0x26, "vptestmb", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0, false },
		{ 0x26, "vptestmw", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, false },
		{ 0x27, "vptestmd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, false },
		{ 0x27, "vptestmq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },

		{ 0x26, "vptestnmb", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0, false },
		{ 0x26, "vptestnmw", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, false },
		{ 0x27, "vptestnmd", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, false },
		{ 0x27, "vptestnmq", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string type = type2String(p->type);
		printf("void %s(const Opmask& k, const Xmm& x, const Operand& op%s) { opAVX_K_X_XM(k, x, op, %s, 0x%02X%s); }\n"
			, p->name, p->hasIMM ? ", uint8_t imm" : "", type.c_str(), p->code, p->hasIMM ? ", imm" : "");
	}
}

// XM_X
void putX_XM()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		int type;
	} tbl[] = {
		{ 0x6F, "vmovdqa32", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z  },
		{ 0x6F, "vmovdqa64", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z  },
		{ 0x6F, "vmovdqu8", T_F2 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z  },
		{ 0x6F, "vmovdqu16", T_F2 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z  },
		{ 0x6F, "vmovdqu32", T_F3 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z  },
		{ 0x6F, "vmovdqu64", T_F3 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z  },
		{ 0x7B, "vcvtpd2qq", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_ER_Z },
		// putCvt
		{ 0x79, "vcvtpd2uqq", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_ER_Z },
		{ 0x79, "vcvtps2udq", T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_ER_Z },
		{ 0xE6, "vcvtqq2pd", T_F3 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_ER_Z },
		{ 0x7A, "vcvttpd2qq", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z },
		{ 0x78, "vcvttpd2uqq", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z },
		{ 0x78, "vcvttps2udq", T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_SAE_Z },
		{ 0x7A, "vcvtudq2ps", T_F2 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_ER_Z },
		{ 0x7A, "vcvtuqq2pd", T_F3 | T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z },

		{ 0x88, "vexpandpd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N8 },
		{ 0x88, "vexpandps", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 },

		{ 0x89, "vpexpandd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 },
		{ 0x89, "vpexpandq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N8 },
		{ 0x42, "vgetexppd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z },
		{ 0x42, "vgetexpps", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_SAE_Z },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string type = type2String(p->type);
		printf("void %s(const Xmm& x, const Operand& op) { opAVX_X_XM_IMM(x, op, %s, 0x%02X); }\n", p->name, type.c_str(), p->code);
	}
	puts("void vpabsq(const Xmm& x, const Operand& op) { opAVX_X_XM_IMM(x, op, T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_B64 | T_YMM, 0x1F); }");

	puts("void vexp2pd(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z, 0xC8); }");
	puts("void vexp2ps(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_SAE_Z, 0xC8); }");
	puts("void vrcp28pd(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z, 0xCA); }");
	puts("void vrcp28ps(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_SAE_Z, 0xCA); }");

	puts("void vrsqrt28pd(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z, 0xCC); }");
	puts("void vrsqrt28ps(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_SAE_Z, 0xCC); }");
}

void putM_X()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		int type;
	} tbl[] = {
		{ 0x7F, "vmovdqa32", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqa64", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu8", T_F2 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu16", T_F2 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu32", T_F3 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu64", T_F3 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string type = type2String(p->type);
		printf("void %s(const Address& addr, const Xmm& x) { opAVX_X_XM_IMM(x, addr, %s, 0x%02X); }\n", p->name, type.c_str(), p->code);
	}
}

void putXM_X()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		int type;
	} tbl[] = {
		{ 0x8A, "vcompresspd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N8 },
		{ 0x8A, "vcompressps", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 },

		{ 0x8B, "vpcompressd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 },
		{ 0x8B, "vpcompressq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N8 },

		{ 0x63, "vcompressb", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N1 },
		{ 0x63, "vcompressw", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N2 },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string type = type2String(p->type);
		printf("void %s(const Operand& op, const Xmm& x) { opAVX_X_XM_IMM(x, op, %s, 0x%02X); }\n", p->name, type.c_str(), p->code);
	}
}

void putX_X_XM_IMM()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		int type;
		bool hasIMM;
	} tbl[] = {
		{ 0x03, "valignd", T_MUST_EVEX | T_66 | T_0F3A | T_EW0 | T_YMM, true },
		{ 0x03, "valignq", T_MUST_EVEX | T_66 | T_0F3A | T_EW1 | T_YMM, true },
		{ 0xDB, "vpandd", T_MUST_EVEX | T_YMM | T_66 | T_0F | T_EW0 | T_B32, false },
		{ 0xDB, "vpandq", T_MUST_EVEX | T_YMM | T_66 | T_0F | T_EW1 | T_B64, false },
		{ 0xDF, "vpandnd", T_MUST_EVEX | T_YMM | T_66 | T_0F | T_EW0 | T_B32, false },
		{ 0xDF, "vpandnq", T_MUST_EVEX | T_YMM | T_66 | T_0F | T_EW1 | T_B64, false },
		{ 0x3D, "vpmaxsq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },
		{ 0x3F, "vpmaxuq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },
		{ 0x39, "vpminsq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },
		{ 0x3B, "vpminuq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },
		{ 0xE2, "vpsraq", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_N16, false },
		{ 0x46, "vpsravq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },
		{ 0x11, "vpsravw", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, false },
		{ 0x12, "vpsllvw", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, false },
		{ 0x10, "vpsrlvw", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, false },
		{ 0xEB, "vpord", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, false },
		{ 0xEB, "vporq", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },

		{ 0xEF, "vpxord", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, false },
		{ 0xEF, "vpxorq", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },

		{ 0x40, "vpmullq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, false },

		{ 0x8D, "vpermb", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0, false },
		{ 0x8D, "vpermw", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, false },

		{ 0x65, "vblendmpd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0x65, "vblendmps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x66, "vpblendmb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, false },
		{ 0x66, "vpblendmw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1, false },
		{ 0x64, "vpblendmd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x64, "vpblendmq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x7D, "vpermt2b", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, false },
		{ 0x7D, "vpermt2w", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1, false },
		{ 0x7E, "vpermt2d", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x7E, "vpermt2q", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0x7F, "vpermt2ps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x7F, "vpermt2pd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x75, "vpermi2b", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, false },
		{ 0x75, "vpermi2w", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1, false },
		{ 0x76, "vpermi2d", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x76, "vpermi2q", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0x77, "vpermi2ps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x77, "vpermi2pd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x25, "vpternlogd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, true },
		{ 0x25, "vpternlogq", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, true },

		{ 0x43, "vgetexpsd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_SAE_X | T_N8, false },
		{ 0x43, "vgetexpss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N4, false },
		{ 0x27, "vgetmantsd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_SAE_X | T_N8, true },
		{ 0x27, "vgetmantss", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N4, true },

		{ 0x54, "vfixupimmpd", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x54, "vfixupimmps", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_SAE_Z, true },
		{ 0x55, "vfixupimmsd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_N8, true },
		{ 0x55, "vfixupimmss", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_N4, true },

		{ 0x4D, "vrcp14sd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_N8, false },
		{ 0x4D, "vrcp14ss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_N4, false },

		{ 0x4F, "vrsqrt14sd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8, false },
		{ 0x4F, "vrsqrt14ss", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4, false },

		{ 0x0B, "vrndscalesd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_N8, true },
		{ 0x0A, "vrndscaless", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_N4, true },

		{ 0x2C, "vscalefpd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, false },
		{ 0x2C, "vscalefps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_ER_Z, false },
		{ 0x2D, "vscalefsd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_ER_X | T_N8, false },
		{ 0x2D, "vscalefss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_ER_X | T_N4, false },

		{ 0x42, "vdbpsadbw", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0, true },
		{ 0x83, "vpmultishiftqb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x15, "vprolvd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x15, "vprolvq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x14, "vprorvd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x14, "vprorvq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0xCB, "vrcp28sd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_N8 | T_SAE_X, false },
		{ 0xCB, "vrcp28ss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_N4 | T_SAE_X, false },

		{ 0xCD, "vrsqrt28sd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_N8 | T_SAE_X, false },
		{ 0xCD, "vrsqrt28ss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_N4 | T_SAE_X, false },

		{ 0x50, "vrangepd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x50, "vrangeps", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_SAE_Z, true },
		{ 0x51, "vrangesd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_SAE_X | T_N8, true },
		{ 0x51, "vrangess", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N4, true },

		{ 0x57, "vreducesd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_SAE_X | T_N8, true },
		{ 0x57, "vreducess", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N4, true },

		{ 0xB4, "vpmadd52luq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0xB5, "vpmadd52huq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x70, "vpshldw", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z, true },
		{ 0x71, "vpshldd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, true },
		{ 0x71, "vpshldq", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_B64, true },

		{ 0x70, "vpshldvw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z, false },
		{ 0x71, "vpshldvd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, false },
		{ 0x71, "vpshldvq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_B64, false },

		{ 0x72, "vpshrdw", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z, true },
		{ 0x73, "vpshrdd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, true },
		{ 0x73, "vpshrdq", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_B64, true },

		{ 0x72, "vpshrdvw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z, false },
		{ 0x73, "vpshrdvd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, false },
		{ 0x73, "vpshrdvq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_B64, false },

		{ 0x72, "vcvtne2ps2bf16", T_F2 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, false },
		{ 0x52, "vdpbf16ps", T_F3 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, false },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string type = type2String(p->type);
		printf("void %s(const Xmm& x1, const Xmm& x2, const Operand& op%s) { opAVX_X_X_XM(x1, x2, op, %s, 0x%02X%s); }\n"
			, p->name, p->hasIMM ? ", uint8_t imm" : "", type.c_str(), p->code, p->hasIMM ? ", imm" : "");
	}
}

void putShift()
{
	const struct Tbl {
		const char *name;
		uint8_t code;
		int idx;
		int type;
	} tbl[] = {
		{ "vpsraq", 0x72, 4, T_0F | T_66 | T_YMM | T_MUST_EVEX |T_EW1 | T_B64 },
		{ "vprold", 0x72, 1, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 },
		{ "vprolq", 0x72, 1, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 },
		{ "vprord", 0x72, 0, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 },
		{ "vprorq", 0x72, 0, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl& p = tbl[i];
		std::string type = type2String(p.type);
		printf("void %s(const Xmm& x, const Operand& op, uint8_t imm) { opAVX_X_X_XM(Xmm(x.getKind(), %d), x, op, %s, 0x%02X, imm); }\n", p.name, p.idx, type.c_str(), p.code);
	}
}

void putExtractInsert()
{
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
			int type;
			bool isZMM;
		} tbl[] = {
			{ "vextractf32x4", 0x19, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N16, false },
			{ "vextractf64x2", 0x19, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N16, false },
			{ "vextractf32x8", 0x1B, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N32, true },
			{ "vextractf64x4", 0x1B, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N32, true },

			{ "vextracti32x4", 0x39, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N16, false },
			{ "vextracti64x2", 0x39, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N16, false },
			{ "vextracti32x8", 0x3B, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N32, true },
			{ "vextracti64x4", 0x3B, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N32, true },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string type = type2String(p.type);
			const char *kind = p.isZMM ? "Operand::MEM | Operand::YMM" : "Operand::MEM | Operand::XMM";
			printf("void %s(const Operand& op, const %s& r, uint8_t imm) { if (!op.is(%s)) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(r, 0, op, %s, 0x%2X, imm); }\n", p.name, p.isZMM ? "Zmm" : "Ymm", kind, type.c_str(), p.code);
		}
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
			int type;
			bool isZMM;
		} tbl[] = {
			{ "vinsertf32x4", 0x18, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N16, false },
			{ "vinsertf64x2", 0x18, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N16, false },
			{ "vinsertf32x8", 0x1A, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N32, true },
			{ "vinsertf64x4", 0x1A, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N32, true },

			{ "vinserti32x4", 0x38, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N16, false },
			{ "vinserti64x2", 0x38, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N16, false },
			{ "vinserti32x8", 0x3A, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_YMM | T_N32, true },
			{ "vinserti64x4", 0x3A, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_YMM | T_N32, true },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string type = type2String(p.type);
			const char *x = p.isZMM ? "Zmm" : "Ymm";
			const char *cond = p.isZMM ? "op.is(Operand::MEM | Operand::YMM)" : "(r1.getKind() == r2.getKind() && op.is(Operand::MEM | Operand::XMM))";
			printf("void %s(const %s& r1, const %s& r2, const Operand& op, uint8_t imm) {"
				"if (!%s) XBYAK_THROW(ERR_BAD_COMBINATION) "
				"opVex(r1, &r2, op, %s, 0x%2X, imm); }\n", p.name, x, x, cond, type.c_str(), p.code);
		}
	}
}

void putBroadcast(bool only64bit)
{
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			int type;
			int reg;
		} tbl[] = {
			{ 0x7A, "vpbroadcastb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, 8 },
			{ 0x7B, "vpbroadcastw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, 16 },
			{ 0x7C, "vpbroadcastd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, 32 },
			{ 0x7C, "vpbroadcastq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1, 64},
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string type = type2String(p.type);
			if ((only64bit && p.reg == 64) || (!only64bit && p.reg != 64)) {
				printf("void %s(const Xmm& x, const Reg%d& r) { opVex(x, 0, r, %s, 0x%02X); }\n", p.name, p.reg, type.c_str(), p.code);
			}
		}
	}
	if (only64bit) return;
	puts("void vbroadcastf32x2(const Ymm& y, const Operand& op) { opAVX_X_XM_IMM(y, op, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N8, 0x19); }");
	puts("void vbroadcastf32x4(const Ymm& y, const Address& addr) { opAVX_X_XM_IMM(y, addr, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N16, 0x1A); }");
	puts("void vbroadcastf64x2(const Ymm& y, const Address& addr) { opAVX_X_XM_IMM(y, addr, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N16, 0x1A); }");
	puts("void vbroadcastf64x4(const Zmm& y, const Address& addr) { opAVX_X_XM_IMM(y, addr, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N32, 0x1B); }");
	puts("void vbroadcastf32x8(const Zmm& y, const Address& addr) { opAVX_X_XM_IMM(y, addr, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N32, 0x1B); }");

	puts("void vbroadcasti32x2(const Xmm& x, const Operand& op) { opAVX_X_XM_IMM(x, op, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N8, 0x59); }");
	puts("void vbroadcasti32x4(const Ymm& y, const Operand& op) { opAVX_X_XM_IMM(y, op, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N16, 0x5A); }");
	puts("void vbroadcasti64x2(const Ymm& y, const Operand& op) { opAVX_X_XM_IMM(y, op, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N16, 0x5A); }");
	puts("void vbroadcasti32x8(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N32, 0x5B); }");
	puts("void vbroadcasti64x4(const Zmm& z, const Operand& op) { opAVX_X_XM_IMM(z, op, T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N32, 0x5B); }");
}

void putCvt()
{
	puts("void vcvtpd2udq(const Xmm& x, const Operand& op) { opCvt2(x, op, T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, 0x79); }");
	puts("void vcvtps2qq(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_ER_Y, 0x7B); }");
	puts("void vcvtps2uqq(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_ER_Y, 0x79); }");
	puts("void vcvtqq2ps(const Xmm& x, const Operand& op) { opCvt2(x, op, T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, 0x5B); }");
	puts("void vcvttpd2udq(const Xmm& x, const Operand& op) { opCvt2(x, op, T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, 0x78); }");
	puts("void vcvttps2qq(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_SAE_Y, 0x7A); }");
	puts("void vcvttps2uqq(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_SAE_Y, 0x78); }");
	puts("void vcvtudq2pd(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_F3 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL, 0x7A); }");

	puts("void vcvtsd2usi(const Reg32e& r, const Operand& op) { int type = (T_F2 | T_0F | T_MUST_EVEX | T_N8 | T_ER_X) | (r.isREG(64) ? T_EW1 : T_EW0); opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, type, 0x79); }");
	puts("void vcvtss2usi(const Reg32e& r, const Operand& op) { int type = (T_F3 | T_0F | T_MUST_EVEX | T_N4 | T_ER_X) | (r.isREG(64) ? T_EW1 : T_EW0); opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, type, 0x79); }");
	puts("void vcvttsd2usi(const Reg32e& r, const Operand& op) { int type = (T_F2 | T_0F | T_MUST_EVEX | T_N8 | T_SAE_X) | (r.isREG(64) ? T_EW1 : T_EW0); opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, type, 0x78); }");
	puts("void vcvttss2usi(const Reg32e& r, const Operand& op) { int type = (T_F3 | T_0F | T_MUST_EVEX | T_N4 | T_SAE_X) | (r.isREG(64) ? T_EW1 : T_EW0); opAVX_X_X_XM(Xmm(r.getIdx()), xm0, op, type, 0x78); }");
	puts("void vcvtuqq2ps(const Xmm& x, const Operand& op) { opCvt2(x, op, T_F2 | T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, 0x7A); }");
	puts("void vcvtusi2sd(const Xmm& x1, const Xmm& x2, const Operand& op) { opCvt3(x1, x2, op, T_F2 | T_0F | T_MUST_EVEX, T_W1 | T_EW1 | T_ER_X | T_N8, T_W0 | T_EW0 | T_N4, 0x7B); }");
	puts("void vcvtusi2ss(const Xmm& x1, const Xmm& x2, const Operand& op) { opCvt3(x1, x2, op, T_F3 | T_0F | T_MUST_EVEX | T_ER_X, T_W1 | T_EW1 | T_N8, T_W0 | T_EW0 | T_N4, 0x7B); }");
}

enum { // same as xbyak.h
	xx_yy_zz = 0,
	xx_yx_zy = 1,
	xx_xy_yz = 2,
};
void putGather()
{
	const struct Tbl {
		const char *name;
		int type;
		uint8_t code;
		int mode;
	} tbl[] = {
		{ "vpgatherdd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4, 0x90, xx_yy_zz },
		{ "vpgatherdq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8, 0x90, xx_yx_zy },
		{ "vpgatherqd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4, 0x91, xx_xy_yz },
		{ "vpgatherqq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8, 0x91, xx_yy_zz },
		{ "vgatherdps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4, 0x92, xx_yy_zz },
		{ "vgatherdpd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8, 0x92, xx_yx_zy },
		{ "vgatherqps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4, 0x93, xx_xy_yz },
		{ "vgatherqpd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8, 0x93, xx_yy_zz },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl& p = tbl[i];
		std::string type = type2String(p.type | T_VSIB);
		printf("void %s(const Xmm& x, const Address& addr) { opGather2(x, addr, %s, 0x%02X, %d); }\n", p.name, type.c_str(), p.code, p.mode);
	}
}
void putScatter()
{
	const struct Tbl {
		const char *name;
		int type;
		uint8_t code;
		int mode; // reverse of gather
	} tbl[] = {
		{ "vpscatterdd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4 | T_M_K, 0xA0, xx_yy_zz },
		{ "vpscatterdq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8 | T_M_K, 0xA0, xx_yx_zy },
		{ "vpscatterqd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4 | T_M_K, 0xA1, xx_xy_yz },
		{ "vpscatterqq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8 | T_M_K, 0xA1, xx_yy_zz },

		{ "vscatterdps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4 | T_M_K, 0xA2, xx_yy_zz },
		{ "vscatterdpd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8 | T_M_K, 0xA2, xx_yx_zy },
		{ "vscatterqps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4 | T_M_K, 0xA3, xx_xy_yz },
		{ "vscatterqpd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8 | T_M_K, 0xA3, xx_yy_zz },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl& p = tbl[i];
		std::string type = type2String(p.type | T_VSIB);
		printf("void %s(const Address& addr, const Xmm& x) { opGather2(x, addr, %s, 0x%02X, %d); }\n", p.name, type.c_str(), p.code, p.mode);
	}
}

void putShuff()
{
	puts("void vshuff32x4(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { opAVX_X_X_XM(y1, y2, op, T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, 0x23, imm); }");
	puts("void vshuff64x2(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { opAVX_X_X_XM(y1, y2, op, T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, 0x23, imm); }");
	puts("void vshufi32x4(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { opAVX_X_X_XM(y1, y2, op, T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, 0x43, imm); }");
	puts("void vshufi64x2(const Ymm& y1, const Ymm& y2, const Operand& op, uint8_t imm) { opAVX_X_X_XM(y1, y2, op, T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, 0x43, imm); }");
}

void putMov()
{
	puts("void vpmovm2b(const Xmm& x, const Opmask& k) { opVex(x, 0, k, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0, 0x28); }");
	puts("void vpmovm2w(const Xmm& x, const Opmask& k) { opVex(x, 0, k, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, 0x28); }");
	puts("void vpmovm2d(const Xmm& x, const Opmask& k) { opVex(x, 0, k, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0, 0x38); }");
	puts("void vpmovm2q(const Xmm& x, const Opmask& k) { opVex(x, 0, k, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, 0x38); }");

	puts("void vpmovb2m(const Opmask& k, const Xmm& x) { opVex(k, 0, x, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0, 0x29); }");
	puts("void vpmovw2m(const Opmask& k, const Xmm& x) { opVex(k, 0, x, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, 0x29); }");
	puts("void vpmovd2m(const Opmask& k, const Xmm& x) { opVex(k, 0, x, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0, 0x39); }");
	puts("void vpmovq2m(const Opmask& k, const Xmm& x) { opVex(k, 0, x, T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1, 0x39); }");

	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			int type;
			int mode;
		} tbl[] = {
			{ 0x32, "vpmovqb",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N2 | T_N_VL, false },
			{ 0x22, "vpmovsqb",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N2 | T_N_VL, false },
			{ 0x12, "vpmovusqb", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N2 | T_N_VL, false },

			{ 0x34, "vpmovqw",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL, false },
			{ 0x24, "vpmovsqw",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL, false },
			{ 0x14, "vpmovusqw", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL, false },

			{ 0x35, "vpmovqd",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },
			{ 0x25, "vpmovsqd",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },
			{ 0x15, "vpmovusqd", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },

			{ 0x31, "vpmovdb",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL, false },
			{ 0x21, "vpmovsdb",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL, false },
			{ 0x11, "vpmovusdb", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL, false },

			{ 0x33, "vpmovdw",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },
			{ 0x23, "vpmovsdw",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },
			{ 0x13, "vpmovusdw", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },

			{ 0x30, "vpmovwb",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },
			{ 0x20, "vpmovswb",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },
			{ 0x10, "vpmovuswb", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL, true },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string type = type2String(p.type);
			printf("void %s(const Operand& op, const Xmm& x) { opVmov(op, x, %s, 0x%02X, %s); }\n", p.name, type.c_str(), p.code, p.mode ? "true" : "false");
		}
	}
}

void putX_XM_IMM()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		int type;
		bool hasIMM;
	} tbl[] = {
		{ 0x26, "vgetmantpd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x26, "vgetmantps", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_SAE_Z, true },
		{ 0x4C, "vrcp14pd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0x4C, "vrcp14ps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },

		{ 0x4E, "vrsqrt14pd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0x4E, "vrsqrt14ps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },

		{ 0x09, "vrndscalepd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, true },
		{ 0x08, "vrndscaleps", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, true },

		{ 0xC4, "vpconflictd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0xC4, "vpconflictq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x44, "vplzcntd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x44, "vplzcntq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x56, "vreducepd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x56, "vreduceps", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_SAE_Z, true },

		{ 0x54, "vpopcntb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z, false },
		{ 0x54, "vpopcntw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z, false },
		{ 0x55, "vpopcntd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, false },
		{ 0x55, "vpopcntq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_B64, false },

		{ 0x62, "vpexpandb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_N1, false },
		{ 0x62, "vpexpandw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_N2, false },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string type = type2String(p->type);
		printf("void %s(const Xmm& x, const Operand& op%s) { opAVX_X_XM_IMM(x, op, %s, 0x%02X%s); }\n"
			, p->name, p->hasIMM ? ", uint8_t imm" : "", type.c_str(), p->code, p->hasIMM ? ", imm" : "");
	}
}

void putMisc()
{
	puts("void vpbroadcastmb2q(const Xmm& x, const Opmask& k) { opVex(x, 0, k, T_F3 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1, 0x2A); }");
	puts("void vpbroadcastmw2d(const Xmm& x, const Opmask& k) { opVex(x, 0, k, T_F3 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, 0x3A); }");
	{
		const struct Tbl {
			const char *name;
			int zm;
			int type;
			uint8_t code;
			bool isZmm;
		} tbl[] = {
			{ "vgatherpf0dps", 1, T_EW0 | T_N4, 0xC6, true },
			{ "vgatherpf0qps", 1, T_EW0 | T_N4, 0xC7, true },
			{ "vgatherpf0dpd", 1, T_EW1 | T_N8, 0xC6, false },
			{ "vgatherpf0qpd", 1, T_EW1 | T_N8, 0xC7, true },

			{ "vgatherpf1dps", 2, T_EW0 | T_N4, 0xC6, true },
			{ "vgatherpf1qps", 2, T_EW0 | T_N4, 0xC7, true },
			{ "vgatherpf1dpd", 2, T_EW1 | T_N8, 0xC6, false },
			{ "vgatherpf1qpd", 2, T_EW1 | T_N8, 0xC7, true },

			{ "vscatterpf0dps", 5, T_EW0 | T_N4, 0xC6, true },
			{ "vscatterpf0qps", 5, T_EW0 | T_N4, 0xC7, true },
			{ "vscatterpf0dpd", 5, T_EW1 | T_N8, 0xC6, false },
			{ "vscatterpf0qpd", 5, T_EW1 | T_N8, 0xC7, true },

			{ "vscatterpf1dps", 6, T_EW0 | T_N4, 0xC6, true },
			{ "vscatterpf1qps", 6, T_EW0 | T_N4, 0xC7, true },
			{ "vscatterpf1dpd", 6, T_EW1 | T_N8, 0xC6, false },
			{ "vscatterpf1qpd", 6, T_EW1 | T_N8, 0xC7, true },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string type = type2String(p.type | T_66 | T_0F38 | T_MUST_EVEX | T_M_K | T_VSIB);
			printf("void %s(const Address& addr) { opGatherFetch(addr, zm%d, %s, 0x%2X, Operand::%s); }\n"
				, p.name, p.zm, type.c_str(), p.code, p.isZmm ? "ZMM" : "YMM");
		}
	}

	puts("void vfpclasspd(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isBit(128|256|512)) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k.changeBit(op.getBit()), 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, 0x66, imm); }");
	puts("void vfpclassps(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isBit(128|256|512)) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k.changeBit(op.getBit()), 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, 0x66, imm); }");
	puts("void vfpclasssd(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isXMEM()) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k, 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_N8, 0x67, imm); }");
	puts("void vfpclassss(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isXMEM()) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k, 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_N4, 0x67, imm); }");

	puts("void vpshufbitqmb(const Opmask& k, const Xmm& x, const Operand& op) { opVex(k, &x, op, T_66 | T_0F38 | T_EW0 | T_YMM | T_MUST_EVEX, 0x8F); }");
	puts("void vcvtneps2bf16(const Xmm& x, const Operand& op) { opCvt2(x, op, T_F3 | T_0F38 | T_EW0 | T_YMM | T_SAE_Z | T_MUST_EVEX | T_B32, 0x72); }");

	puts("void vp2intersectd(const Opmask& k, const Xmm& x, const Operand& op) { if (k.getOpmaskIdx() != 0) XBYAK_THROW(ERR_OPMASK_IS_ALREADY_SET) opAVX_K_X_XM(k, x, op, T_F2 | T_0F38 | T_YMM | T_EVEX | T_EW0 | T_B32, 0x68); }");
	puts("void vp2intersectq(const Opmask& k, const Xmm& x, const Operand& op) { if (k.getOpmaskIdx() != 0) XBYAK_THROW(ERR_OPMASK_IS_ALREADY_SET) opAVX_K_X_XM(k, x, op, T_F2 | T_0F38 | T_YMM | T_EVEX | T_EW1 | T_B64, 0x68); }");
}

void putV4FMA()
{
	puts("void v4fmaddps(const Zmm& z1, const Zmm& z2, const Address& addr) { opAVX_X_X_XM(z1, z2, addr, T_0F38 | T_F2 | T_EW0 | T_YMM | T_MUST_EVEX | T_N16, 0x9A); }");
	puts("void v4fnmaddps(const Zmm& z1, const Zmm& z2, const Address& addr) { opAVX_X_X_XM(z1, z2, addr, T_0F38 | T_F2 | T_EW0 | T_YMM | T_MUST_EVEX | T_N16, 0xAA); }");
	puts("void v4fmaddss(const Xmm& x1, const Xmm& x2, const Address& addr) { opAVX_X_X_XM(x1, x2, addr, T_0F38 | T_F2 | T_EW0 | T_MUST_EVEX | T_N16, 0x9B); }");
	puts("void v4fnmaddss(const Xmm& x1, const Xmm& x2, const Address& addr) { opAVX_X_X_XM(x1, x2, addr, T_0F38 | T_F2 | T_EW0 | T_MUST_EVEX | T_N16, 0xAB); }");
	puts("void vp4dpwssd(const Zmm& z1, const Zmm& z2, const Address& addr) { opAVX_X_X_XM(z1, z2, addr, T_0F38 | T_F2 | T_EW0 | T_YMM | T_MUST_EVEX | T_N16, 0x52); }");
	puts("void vp4dpwssds(const Zmm& z1, const Zmm& z2, const Address& addr) { opAVX_X_X_XM(z1, z2, addr, T_0F38 | T_F2 | T_EW0 | T_YMM | T_MUST_EVEX | T_N16, 0x53); }");
}

int main(int argc, char *[])
{
	bool only64bit = argc == 2;
	putOpmask(only64bit);
	putBroadcast(only64bit);
	if (only64bit) {
		return 0;
	}
	putVcmp();
	putX_XM();
	putM_X();
	putXM_X();
	putX_X_XM_IMM();
	putShift();
	putExtractInsert();
	putCvt();
	putGather();
	putShuff();
	putMov();
	putX_XM_IMM();
	putMisc();
	putScatter();
	putV4FMA();
}
