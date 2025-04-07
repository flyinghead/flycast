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
		puts("void kmovq(const Reg64& r, const Opmask& k) { opKmov(k, r, true, 64); }");
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
	for (int i = 0; i < 4; i++) {
		const char tbl[] = "bwdq";
		const int bitTbl[] = { 8, 16, 32, 64 };
		int bit = bitTbl[i];
		printf("void kmov%c(const Opmask& k, const Operand& op) { opKmov(k, op, false, %d); }\n", tbl[i], bit);
		printf("void kmov%c(const Address& addr, const Opmask& k) { opKmov(k, addr, true, %d); }\n", tbl[i], bit);
		if (i < 3) printf("void kmov%c(const Reg32& r, const Opmask& k) { opKmov(k, r, true, %d); }\n", tbl[i], bit);
	}
}

// vcmppd(k, x, op)
void putVcmp()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
		bool hasIMM;
	} tbl[] = {
		{ 0xC2, "vcmppd", T_0F | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_YMM | T_66 | T_B64, true },
		{ 0xC2, "vcmpps", T_0F | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_YMM | T_B32, true },
		{ 0xC2, "vcmpsd", T_0F | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_F2 | T_N8, true },
		{ 0xC2, "vcmpss", T_0F | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_F3 | T_N4, true },
		{ 0xC2, "vcmpph", T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_YMM | T_B16, true },
		{ 0xC2, "vcmpsh", T_F3 | T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N2, true },

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
		std::string s = type2String(p->type);
		printf("void %s(const Opmask& k, const Xmm& x, const Operand& op%s) { opAVX_K_X_XM(k, x, op, %s, 0x%02X%s); }\n"
			, p->name, p->hasIMM ? ", uint8_t imm" : "", s.c_str(), p->code, p->hasIMM ? ", imm" : "");
	}
}

void putVcmpAlias()
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
			printf("void vcmp%s%s(const Opmask& k, const Xmm& x, const Operand& op) { vcmp%s(k, x, op, %d); }\n", pred[j], s, s, j);
		}
	}
}

// XM_X
void putX_XM()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
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
		{ 0x42, "vgetexpph", T_66 | T_MAP6 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_SAE_Z },

		{ 0x7D, "vcvtph2uw", T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_ER_Z },
		{ 0x7D, "vcvtph2w", T_66 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_ER_Z },
		{ 0x7C, "vcvttph2uw", T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_SAE_Z },
		{ 0x7C, "vcvttph2w", T_66 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_SAE_Z },
		{ 0x7D, "vcvtuw2ph", T_F2 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_ER_Z },
		{ 0x7D, "vcvtw2ph", T_F3 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_ER_Z },

		{ 0x51, "vsqrtnepbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_YMM | T_B16 },

		{ 0x2F, "vcomish", T_MUST_EVEX | T_MAP5 | T_EW0 | T_SAE_X | T_N2 },
		{ 0x2E, "vucomish", T_MUST_EVEX | T_MAP5 | T_EW0 | T_SAE_X | T_N2 },

		{ 0x2F, "vcomxsd", T_MUST_EVEX | T_F2 | T_0F | T_EW1 | T_SAE_X | T_N8 },
		{ 0x2F, "vcomxsh", T_MUST_EVEX | T_F3 | T_MAP5 | T_EW0 | T_SAE_X | T_N2 },
		{ 0x2F, "vcomxss", T_MUST_EVEX | T_F3 | T_0F | T_EW0  | T_SAE_X | T_N4 },

		{ 0x2E, "vucomxsd", T_MUST_EVEX | T_F2 | T_0F | T_EW1 | T_SAE_X | T_N8 },
		{ 0x2E, "vucomxsh", T_MUST_EVEX | T_F3 | T_MAP5 | T_EW0 | T_SAE_X | T_N2 },
		{ 0x2E, "vucomxss", T_MUST_EVEX | T_F3 | T_0F | T_EW0  | T_SAE_X | T_N4 },

		// 13.1
		{ 0x69, "vcvtnebf162ibs", T_MUST_EVEX | T_YMM | T_F2 | T_MAP5 | T_EW0 | T_B16 },
		{ 0x6B, "vcvtnebf162iubs", T_MUST_EVEX | T_YMM | T_F2 | T_MAP5 | T_EW0 | T_B16 },
		{ 0x68, "vcvttnebf162ibs", T_MUST_EVEX | T_YMM | T_F2 | T_MAP5 | T_EW0 | T_B16 },
		{ 0x6A, "vcvttnebf162iubs", T_MUST_EVEX | T_YMM | T_F2 | T_MAP5 | T_EW0 | T_B16 },
		// 13.3
		{ 0x6D, "vcvttpd2qqs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW1 | T_B64 | T_SAE_Y | T_SAE_Z },
		// 13.5
		{ 0x6C, "vcvttpd2uqqs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW1 | T_B64 | T_SAE_Y | T_SAE_Z },
		// 13.6
		{ 0x69, "vcvtph2ibs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW0 | T_B16 | T_ER_Y | T_ER_Z },
		{ 0x6B, "vcvtph2iubs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW0 | T_B16 | T_ER_Y | T_ER_Z },
		{ 0x68, "vcvttph2ibs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW0 | T_B16 | T_ER_Y | T_ER_Z },
		{ 0x6A, "vcvttph2iubs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW0 | T_B16 | T_ER_Y | T_ER_Z },
		// 13.7
		{ 0x6D, "vcvttps2dqs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW0 | T_B32 | T_SAE_Y | T_SAE_Z },
		// 13.8
		{ 0x69, "vcvtps2ibs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW0 | T_B32 | T_ER_Y | T_ER_Z },
		{ 0x6B, "vcvtps2iubs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW0 | T_B32 | T_ER_Y | T_ER_Z },
		{ 0x68, "vcvttps2ibs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW0 | T_B32 | T_ER_Y | T_ER_Z },
		{ 0x6A, "vcvttps2iubs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW0 | T_B32 | T_ER_Y | T_ER_Z },
		// 13.10
		{ 0x6C, "vcvttps2udqs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW0 | T_B32 | T_SAE_Y | T_SAE_Z },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string s = type2String(p->type);
		printf("void %s(const Xmm& x, const Operand& op) { opAVX_X_XM_IMM(x, op, %s, 0x%02X); }\n", p->name, s.c_str(), p->code);
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
		uint64_t type;
	} tbl[] = {
		{ 0x7F, "vmovdqa32", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqa64", T_66 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu8", T_F2 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu16", T_F2 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu32", T_F3 | T_0F | T_MUST_EVEX | T_YMM | T_EW0 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x7F, "vmovdqu64", T_F3 | T_0F | T_MUST_EVEX | T_YMM | T_EW1 | T_ER_X | T_ER_Y | T_ER_Z | T_M_K },
		{ 0x11, "vmovsh", T_F3 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_N2 | T_M_K },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string s = type2String(p->type);
		printf("void %s(const Address& addr, const Xmm& x) { opAVX_X_XM_IMM(x, addr, %s, 0x%02X); }\n", p->name, s.c_str(), p->code);
	}
}

void putXM_X()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
	} tbl[] = {
		{ 0x8A, "vcompresspd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N8 },
		{ 0x8A, "vcompressps", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 },

		{ 0x8B, "vpcompressd", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 },
		{ 0x8B, "vpcompressq", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N8 },

		{ 0x63, "vpcompressb", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N1 },
		{ 0x63, "vpcompressw", T_66 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW1 | T_N2 },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string s = type2String(p->type);
		printf("void %s(const Operand& op, const Xmm& x) { opAVX_X_XM_IMM(x, op, %s, 0x%02X); }\n", p->name, s.c_str(), p->code);
	}
}

void putX_X_XM_IMM()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
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

		{ 0xB4, "vpmadd52luq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0xB5, "vpmadd52huq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x25, "vpternlogd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, true },
		{ 0x25, "vpternlogq", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, true },

		{ 0x43, "vgetexpsd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_SAE_X | T_N8, false },
		{ 0x43, "vgetexpss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N4, false },
		{ 0x43, "vgetexpsh", T_66 | T_MAP6 | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N2, false },
		{ 0x27, "vgetmantsd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_SAE_X | T_N8, true },
		{ 0x27, "vgetmantss", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N4, true },
		{ 0x27, "vgetmantsh", T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N2, true },

		{ 0x54, "vfixupimmpd", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x54, "vfixupimmps", T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0 | T_B32 | T_SAE_Z, true },
		{ 0x55, "vfixupimmsd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_N8, true },
		{ 0x55, "vfixupimmss", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_N4, true },

		{ 0x4D, "vrcp14sd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_N8, false },
		{ 0x4D, "vrcp14ss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_N4, false },

		{ 0x4D, "vrcpsh", T_66 | T_MAP6 | T_MUST_EVEX | T_EW0 | T_N2, false },

		{ 0x4F, "vrsqrt14sd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_N8, false },
		{ 0x4F, "vrsqrt14ss", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_N4, false },

		{ 0x4F, "vrsqrtsh", T_66 | T_MAP6 | T_MUST_EVEX | T_EW0 | T_N2, false },
		{ 0x51, "vsqrtsh", T_F3 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_ER_X | T_N2, false },

		{ 0x0B, "vrndscalesd", T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_N8 | T_SAE_X, true },
		{ 0x0A, "vrndscaless", T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_N4 | T_SAE_X, true },
		{ 0x0A, "vrndscalesh", T_0F3A | T_MUST_EVEX | T_EW0 | T_N2 | T_SAE_X, true },

		{ 0x2C, "vscalefpd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, false },
		{ 0x2C, "vscalefps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_ER_Z, false },
		{ 0x2D, "vscalefsd", T_66 | T_0F38 | T_MUST_EVEX | T_EW1 | T_ER_X | T_N8, false },
		{ 0x2D, "vscalefss", T_66 | T_0F38 | T_MUST_EVEX | T_EW0 | T_ER_X | T_N4, false },

		{ 0x2C, "vscalefph", T_66 | T_MAP6 | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_ER_Z, false },
		{ 0x2D, "vscalefsh", T_66 | T_MAP6 | T_MUST_EVEX | T_EW0 | T_ER_X | T_N2, false },

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
		{ 0x57, "vreducesh", T_0F3A | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N2, true },

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

		{ 0x5A, "vcvtsd2sh", T_F2 | T_MAP5 | T_MUST_EVEX | T_EW1 | T_ER_X | T_N8, false },
		{ 0x5A, "vcvtsh2sd", T_F3 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N2, false },
		{ 0x13, "vcvtsh2ss", T_MAP6 | T_MUST_EVEX | T_EW0 | T_SAE_X | T_N2, false },
		{ 0x1D, "vcvtss2sh", T_MAP5 | T_MUST_EVEX | T_EW0 | T_ER_X | T_N4, false },

		{ 0x58, "vaddnepbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_YMM | T_B16, false },
		{ 0x5E, "vdivnepbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_YMM | T_B16, false },
		{ 0x5F, "vmaxpbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_YMM | T_B16, false },
		{ 0x5D, "vminpbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_YMM | T_B16, false },
		{ 0x59, "vmulnepbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_YMM | T_B16, false },
		{ 0x2C, "vscalefpbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16,false },
		{ 0x5C, "vsubnepbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_YMM | T_B16, false },

		{ 0x98, "vfmadd132nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xA8, "vfmadd213nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xB8, "vfmadd231nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },

		{ 0x9C, "vfnmadd132nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xAC, "vfnmadd213nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xBC, "vfnmadd231nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },

		{ 0x9A, "vfmsub132nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xAA, "vfmsub213nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xBA, "vfmsub231nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },

		{ 0x9E, "vfnmsub132nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xAE, "vfnmsub213nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0xBE, "vfnmsub231nepbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },

		{ 0x67, "vcvt2ps2phx", T_MUST_EVEX | T_66 | T_0F38 | T_EW0 | T_YMM | T_B32 | T_ER_Y | T_ER_Z, false },
		{ 0x74, "vcvtne2ph2bf8", T_MUST_EVEX | T_F2 | T_0F38 | T_EW0 | T_YMM | T_B16 | T_N1, false },
		{ 0x74, "vcvtne2ph2bf8s", T_MUST_EVEX | T_F2 | T_MAP5 | T_EW0 | T_YMM | T_B16 | T_N1, false },
		{ 0x18, "vcvtne2ph2hf8", T_MUST_EVEX | T_F2 | T_MAP5 | T_EW0 | T_YMM | T_B16 | T_N1, false },
		{ 0x1B, "vcvtne2ph2hf8s", T_MUST_EVEX | T_F2 | T_MAP5 | T_EW0 | T_YMM | T_B16 | T_N1, false },

		{ 0x52, "vdpphps", T_MUST_EVEX | T_0F38 | T_EW0 | T_YMM | T_B32, false },
		{ 0x52, "vminmaxnepbf16", T_MUST_EVEX | T_F2 | T_0F3A | T_EW0 | T_YMM | T_B16, true },
		{ 0x52, "vminmaxpd", T_MUST_EVEX | T_66 | T_0F3A | T_EW1 | T_YMM | T_B64 | T_SAE_Y | T_SAE_Z, true },
		{ 0x52, "vminmaxph", T_MUST_EVEX | T_0F3A | T_EW0 | T_YMM | T_B16 | T_SAE_Y | T_SAE_Z, true },
		{ 0x52, "vminmaxps", T_MUST_EVEX | T_66 | T_0F3A | T_EW0 | T_YMM | T_B32 | T_SAE_Y | T_SAE_Z, true },
		{ 0x53, "vminmaxsd", T_MUST_EVEX | T_66 | T_0F3A | T_EW1 | T_SAE_X | T_N8, true },
		{ 0x53, "vminmaxsh", T_MUST_EVEX | T_0F3A | T_EW0 | T_SAE_X | T_N2, true },
		{ 0x53, "vminmaxss", T_MUST_EVEX | T_66 | T_0F3A | T_EW0 | T_SAE_X | T_N4, true },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string s = type2String(p->type);
		printf("void %s(const Xmm& x1, const Xmm& x2, const Operand& op%s) { opAVX_X_X_XM(x1, x2, op, %s, 0x%02X%s); }\n"
			, p->name, p->hasIMM ? ", uint8_t imm" : "", s.c_str(), p->code, p->hasIMM ? ", imm" : "");
	}
}

void putX_X_XM_IMM_AVX10()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
		uint64_t typeVex;
		uint64_t typeEvex;
		int sel;
		bool hasIMM;
	} tbl[] = {
		// vpdpb[su,uu,ss]d[,s]
		{ 0x50, "vpdpbssd", T_F2|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0x51, "vpdpbssds", T_F2|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0x50, "vpdpbsud", T_F3|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0x51, "vpdpbsuds", T_F3|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0x50, "vpdpbuud", T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0x51, "vpdpbuuds", T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },

		// vpdpw[su,us,uu]d[,s]
		{ 0xD2, "vpdpwsud", T_F3|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0xD3, "vpdpwsuds", T_F3|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0xD2, "vpdpwusd", T_66|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0xD3, "vpdpwusds", T_66|T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0xD2, "vpdpwuud", T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },
		{ 0xD3, "vpdpwuuds", T_0F38|T_YMM, T_W0, T_EW0|T_B32, 1, false },

		{ 0x42, "vmpsadbw", T_0F3A|T_YMM, T_66|T_W0|T_YMM, T_F3|T_0F3A|T_EW0|T_B32, 1, true },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string s = type2String(p->type);
		std::string sVex = type2String(p->typeVex);
		std::string sEvex = type2String(p->typeEvex);
		printf("void %s(const Xmm& x1, const Xmm& x2, const Operand& op%s, PreferredEncoding encoding = DefaultEncoding) { opEncoding(x1, x2, op, %s, 0x%02X, encoding, %s, %s, %s, %d); }\n"
			, p->name, p->hasIMM ? ", uint8_t imm" : "", s.c_str(), p->code, p->hasIMM ? "imm" : "NONE", sVex.c_str(), sEvex.c_str(), p->sel);
	}
}

void putShift()
{
	const struct Tbl {
		const char *name;
		uint8_t code;
		int idx;
		uint64_t type;
	} tbl[] = {
		{ "vpsraq", 0x72, 4, T_0F | T_66 | T_YMM | T_MUST_EVEX |T_EW1 | T_B64 },
		{ "vprold", 0x72, 1, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 },
		{ "vprolq", 0x72, 1, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 },
		{ "vprord", 0x72, 0, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 },
		{ "vprorq", 0x72, 0, T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl& p = tbl[i];
		std::string s = type2String(p.type);
		printf("void %s(const Xmm& x, const Operand& op, uint8_t imm) { opAVX_X_X_XM(Xmm(x.getKind(), %d), x, op, %s, 0x%02X, imm); }\n", p.name, p.idx, s.c_str(), p.code);
	}
}

void putExtractInsert()
{
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
			uint64_t type;
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
			std::string s = type2String(p.type);
			const char *kind = p.isZMM ? "Operand::MEM | Operand::YMM" : "Operand::MEM | Operand::XMM";
			printf("void %s(const Operand& op, const %s& r, uint8_t imm) { if (!op.is(%s)) XBYAK_THROW(ERR_BAD_COMBINATION) opVex(r, 0, op, %s, 0x%2X, imm); }\n", p.name, p.isZMM ? "Zmm" : "Ymm", kind, s.c_str(), p.code);
		}
	}
	{
		const struct Tbl {
			const char *name;
			uint8_t code;
			uint64_t type;
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
			std::string s = type2String(p.type);
			const char *x = p.isZMM ? "Zmm" : "Ymm";
			const char *cond = p.isZMM ? "op.is(Operand::MEM | Operand::YMM)" : "(r1.getKind() == r2.getKind() && op.is(Operand::MEM | Operand::XMM))";
			printf("void %s(const %s& r1, const %s& r2, const Operand& op, uint8_t imm) {"
				"if (!%s) XBYAK_THROW(ERR_BAD_COMBINATION) "
				"opVex(r1, &r2, op, %s, 0x%2X, imm); }\n", p.name, x, x, cond, s.c_str(), p.code);
		}
	}
}

void putBroadcast(bool only64bit)
{
	{
		const struct Tbl {
			uint8_t code;
			const char *name;
			uint64_t type;
			int reg;
		} tbl[] = {
			{ 0x7A, "vpbroadcastb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, 8 },
			{ 0x7B, "vpbroadcastw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, 16 },
			{ 0x7C, "vpbroadcastd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0, 32 },
			{ 0x7C, "vpbroadcastq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1, 64},
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string s = type2String(p.type);
			if ((only64bit && p.reg == 64) || (!only64bit && p.reg != 64)) {
				printf("void %s(const Xmm& x, const Reg%d& r) { opVex(x, 0, r, %s, 0x%02X); }\n", p.name, p.reg, s.c_str(), p.code);
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
	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
		int ptn;
	} tbl[] = {
		{ 0x79, "vcvtsd2usi", T_F2 | T_0F | T_MUST_EVEX | T_N8 | T_ER_X, 0 },
		{ 0x79, "vcvtss2usi", T_F3 | T_0F | T_MUST_EVEX | T_N4 | T_ER_X, 0 },
		{ 0x78, "vcvttsd2usi", T_F2 | T_0F | T_MUST_EVEX | T_N8 | T_SAE_X, 0 },
		{ 0x78, "vcvttss2usi", T_F3 | T_0F | T_MUST_EVEX | T_N4 | T_SAE_X, 0 },
		{ 0x2D, "vcvtsh2si", T_F3 | T_MAP5 | T_MUST_EVEX | T_N2 | T_ER_X, 0 },
		{ 0x79, "vcvtsh2usi", T_F3 | T_MAP5 | T_MUST_EVEX | T_N2 | T_ER_X, 0 },
		{ 0x2C, "vcvttsh2si", T_F3 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_N2 | T_SAE_X, 0 },
		{ 0x78, "vcvttsh2usi", T_F3 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_N2 | T_SAE_X, 0 },

		{ 0x7B, "vcvtps2qq", T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_ER_Y, 1 },
		{ 0x79, "vcvtps2uqq", T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_ER_Y, 1 },
		{ 0x7A, "vcvttps2qq", T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_SAE_Y, 1 },
		{ 0x78, "vcvttps2uqq", T_66 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL | T_SAE_Y, 1 },
		{ 0x7A, "vcvtudq2pd", T_F3 | T_0F | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_N8 | T_N_VL, 1 },
		{ 0x5B, "vcvtph2dq", T_66 | T_MAP5 | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_ER_Y | T_N8 | T_N_VL, 1 },
		{ 0x13, "vcvtph2psx", T_66 | T_MAP6 | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_SAE_Y | T_N8 | T_N_VL, 1 },
		{ 0x79, "vcvtph2udq", T_MAP5 | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_ER_Y | T_N8 | T_N_VL, 1 },
		{ 0x5B, "vcvttph2dq", T_F3 | T_MAP5 | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_SAE_Y | T_N8 | T_N_VL, 1 },
		{ 0x78, "vcvttph2udq", T_MAP5 | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_SAE_Y | T_N8 | T_N_VL, 1 },

		{ 0x79, "vcvtpd2udq", T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, 2 },
		{ 0x5B, "vcvtqq2ps", T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, 2 },
		{ 0x78, "vcvttpd2udq", T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, 2 },
		{ 0x7A, "vcvtuqq2ps", T_F2 | T_0F | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z, 2 },

		{ 0x5A, "vcvtph2pd", T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_N4 | T_N_VL | T_SAE_X, 3 },
		{ 0x7B, "vcvtph2qq", T_66 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_N4 | T_N_VL | T_ER_X, 3 },
		{ 0x79, "vcvtph2uqq", T_66 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_N4 | T_N_VL | T_ER_X, 3 },
		{ 0x78, "vcvttph2uqq", T_66 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_N4 | T_N_VL | T_SAE_X, 3 },
		{ 0x7A, "vcvttph2qq", T_66 | T_MAP5 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16 | T_N4 | T_N_VL | T_SAE_X, 3 },

		{ 0x5B, "vcvtdq2ph", T_MAP5 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_ER_Z | T_N16 | T_N_VL, 4 },
		{ 0x1D, "vcvtps2phx", T_66 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_B32 | T_ER_Z | T_N16 | T_N_VL, 4 },
		{ 0x7A, "vcvtudq2ph", T_F2 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_B32 | T_ER_Z | T_N16 | T_N_VL, 4 },

		{ 0x5A, "vcvtpd2ph", T_66 | T_MAP5 | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z | T_N16 | T_N_VL, 5 },
		{ 0x5B, "vcvtqq2ph", T_MAP5 | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z | T_N16 | T_N_VL, 5 },
		{ 0x7A, "vcvtuqq2ph", T_F2 | T_MAP5 | T_MUST_EVEX | T_EW1 | T_B64 | T_ER_Z | T_N16 | T_N_VL, 5 },

		{ 0x2A, "vcvtsi2sh", T_F3 | T_MAP5 | T_MUST_EVEX | T_ER_R | T_M_K, 6 },
		{ 0x7B, "vcvtusi2sh", T_F3 | T_MAP5 | T_MUST_EVEX | T_ER_R | T_M_K, 6 },

		{ 0x72, "vcvtneps2bf16", T_MUST_EVEX | T_F3 | T_0F38 | T_EW0 | T_YMM | T_SAE_Z | T_B32, 2 },
		// 13.2
		{ 0x6D, "vcvttpd2dqs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW1 | T_B64 | T_SAE_Y | T_SAE_Z, 2 },
		// 13.4
		{ 0x6C, "vcvttpd2udqs", T_MUST_EVEX | T_YMM | T_MAP5 | T_EW1 | T_B64 | T_SAE_Y | T_SAE_Z, 2 },
		// 13.9
		{ 0x6D, "vcvttps2qqs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW0 | T_B32 | T_SAE_X | T_SAE_Y | T_N8 | T_N_VL, 1 },
		// 13.11
		{ 0x6C, "vcvttps2uqqs", T_MUST_EVEX | T_YMM | T_66 | T_MAP5 | T_EW0 | T_B32 | T_SAE_X | T_SAE_Y | T_N8 | T_N_VL, 1 },
		// 13.12
		{ 0x6D, "vcvttsd2sis", T_MUST_EVEX | T_F2 | T_MAP5 | T_EW0 | T_SAE_X | T_N8, 0 },
		// 13.13
		{ 0x6C, "vcvttsd2usis", T_MUST_EVEX | T_F2 | T_MAP5 | T_EW0 | T_SAE_X | T_N8, 0 },
		// 13.14
		{ 0x6D, "vcvttss2sis", T_MUST_EVEX | T_F3 | T_MAP5 | T_EW0 | T_SAE_X | T_N4, 0 },
		// 13.15
		{ 0x6C, "vcvttss2usis", T_MUST_EVEX | T_F3 | T_MAP5 | T_EW0 | T_SAE_X | T_N4, 0 },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl& p = tbl[i];
		std::string s = type2String(p.type);
		switch (p.ptn) {
		case 0:
			printf("void %s(const Reg32e& r, const Operand& op) { uint64_t type = (%s) | (r.isREG(64) ? T_EW1 : T_EW0); opVex(r, &xm0, op, type, 0x%02X); }\n", p.name, s.c_str(), p.code);
			break;
		case 1: // (x, x/m), (y, x/m256), (z, y/m)
			printf("void %s(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, %s, 0x%02X); }\n", p.name, s.c_str(), p.code);
			break;
		case 2: // (x, x/m), (x, y/m256), (y, z/m)
			printf("void %s(const Xmm& x, const Operand& op) { opCvt2(x, op, %s, 0x%02X); }\n", p.name, s.c_str(), p.code);
			break;
		case 3:
			printf("void %s(const Xmm& x, const Operand& op) { if (!op.isXMM() && !op.isMEM()) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(x, 0, op, %s, 0x%02X); }\n", p.name, s.c_str(), p.code);
			break;
		case 4:
			printf("void %s(const Xmm& x, const Operand& op) { checkCvt4(x, op); opCvt(x, op, %s, 0x%02X); }\n", p.name, s.c_str(), p.code);
			break;
		case 5:
			printf("void %s(const Xmm& x, const Operand& op) { opCvt5(x, op, %s, 0x%02X); }\n", p.name, s.c_str(), p.code);
			break;
		case 6:
			printf("void %s(const Xmm& x1, const Xmm& x2, const Operand& op) { if (!(x1.isXMM() && x2.isXMM() && op.isBit(32|64))) XBYAK_THROW(ERR_BAD_COMBINATION) uint64_t type = (%s) | (op.isBit(32) ? (T_EW0 | T_N4) : (T_EW1 | T_N8)); opVex(x1, &x2, op, type, 0x%02X); }\n", p.name, s.c_str(), p.code);
			break;
		}
	}
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
		uint64_t type;
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
		std::string s = type2String(p.type | T_VSIB);
		printf("void %s(const Xmm& x, const Address& addr) { opGather2(x, addr, %s, 0x%02X, %d); }\n", p.name, s.c_str(), p.code, p.mode);
	}
}
void putScatter()
{
	const struct Tbl {
		const char *name;
		uint64_t type;
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
		std::string s = type2String(p.type | T_VSIB);
		printf("void %s(const Address& addr, const Xmm& x) { opGather2(x, addr, %s, 0x%02X, %d); }\n", p.name, s.c_str(), p.code, p.mode);
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
			uint64_t type;
			int mode;
		} tbl[] = {
			{ 0x32, "vpmovqb",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N2 | T_N_VL | T_M_K, false },
			{ 0x22, "vpmovsqb",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N2 | T_N_VL | T_M_K, false },
			{ 0x12, "vpmovusqb", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N2 | T_N_VL | T_M_K, false },

			{ 0x34, "vpmovqw",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL | T_M_K, false },
			{ 0x24, "vpmovsqw",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL | T_M_K, false },
			{ 0x14, "vpmovusqw", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL | T_M_K, false },

			{ 0x35, "vpmovqd",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },
			{ 0x25, "vpmovsqd",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },
			{ 0x15, "vpmovusqd", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },

			{ 0x31, "vpmovdb",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL | T_M_K, false },
			{ 0x21, "vpmovsdb",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL | T_M_K, false },
			{ 0x11, "vpmovusdb", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N4 | T_N_VL | T_M_K, false },

			{ 0x33, "vpmovdw",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },
			{ 0x23, "vpmovsdw",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },
			{ 0x13, "vpmovusdw", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },

			{ 0x30, "vpmovwb",   T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },
			{ 0x20, "vpmovswb",  T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },
			{ 0x10, "vpmovuswb", T_F3 | T_0F38 | T_MUST_EVEX | T_YMM | T_EW0 | T_N8 | T_N_VL | T_M_K, true },
		};
		for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
			const Tbl& p = tbl[i];
			std::string s = type2String(p.type);
			printf("void %s(const Operand& op, const Xmm& x) { opVmov(op, x, %s, 0x%02X, %s); }\n", p.name, s.c_str(), p.code, p.mode ? "true" : "false");
		}
	}
}

void putX_XM_IMM()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
		bool hasIMM;
	} tbl[] = {
		{ 0x26, "vgetmantpd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x26, "vgetmantps", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_SAE_Z, true },
		{ 0x26, "vgetmantph", T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_SAE_Z, true },
		{ 0x4C, "vrcp14pd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0x4C, "vrcp14ps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },

		{ 0x4C, "vrcpph", T_66 | T_MAP6 | T_MUST_EVEX | T_YMM | T_EW0 | T_B16, false },

		{ 0x4E, "vrsqrt14pd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },
		{ 0x4E, "vrsqrt14ps", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },

		{ 0x4E, "vrsqrtph", T_66 | T_MAP6 | T_YMM | T_MUST_EVEX | T_EW0 | T_B16, false },
		{ 0x51, "vsqrtph", T_MAP5| T_YMM | T_MUST_EVEX  | T_EW0 | T_ER_Z | T_B16, false },

		{ 0x09, "vrndscalepd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x08, "vrndscaleps", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_SAE_Z, true },
		{ 0x08, "vrndscaleph", T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_SAE_Z, true },

		{ 0xC4, "vpconflictd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0xC4, "vpconflictq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x44, "vplzcntd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_B32, false },
		{ 0x44, "vplzcntq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_B64, false },

		{ 0x56, "vreducepd", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW1 | T_B64 | T_SAE_Z, true },
		{ 0x56, "vreduceps", T_66 | T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B32 | T_SAE_Z, true },
		{ 0x56, "vreduceph", T_0F3A | T_YMM | T_MUST_EVEX | T_EW0 | T_B16 | T_SAE_Z, true },

		{ 0x54, "vpopcntb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z, false },
		{ 0x54, "vpopcntw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z, false },
		{ 0x55, "vpopcntd", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_B32, false },
		{ 0x55, "vpopcntq", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_B64, false },

		{ 0x62, "vpexpandb", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW0 | T_SAE_Z | T_N1, false },
		{ 0x62, "vpexpandw", T_66 | T_0F38 | T_YMM | T_MUST_EVEX | T_EW1 | T_SAE_Z | T_N2, false },

		{ 0x2F, "vcomsbf16", T_MUST_EVEX | T_66 | T_MAP5 | T_EW0 | T_N2, false },
		{ 0x42, "vgetexppbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0x26, "vgetmantpbf16", T_MUST_EVEX | T_F2 | T_0F3A | T_EW0 | T_YMM | T_B16, true },
		{ 0x4C, "vrcppbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0x56, "vreducenepbf16", T_MUST_EVEX | T_F2 | T_0F3A | T_EW0 | T_YMM | T_B16, true },
		{ 0x08, "vrndscalenepbf16", T_MUST_EVEX | T_F2 | T_0F3A | T_EW0 | T_YMM | T_B16, true },
		{ 0x4E, "vrsqrtpbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
		{ 0x2C, "vscalefpbf16", T_MUST_EVEX | T_MAP6 | T_EW0 | T_YMM | T_B16, false },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		std::string s = type2String(p->type);
		printf("void %s(const Xmm& x, const Operand& op%s) { opAVX_X_XM_IMM(x, op, %s, 0x%02X%s); }\n"
			, p->name, p->hasIMM ? ", uint8_t imm" : "", s.c_str(), p->code, p->hasIMM ? ", imm" : "");
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
			uint64_t type;
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
			std::string s = type2String(p.type | T_66 | T_0F38 | T_MUST_EVEX | T_M_K | T_VSIB);
			printf("void %s(const Address& addr) { opGatherFetch(addr, zm%d, %s, 0x%2X, Operand::%s); }\n"
				, p.name, p.zm, s.c_str(), p.code, p.isZmm ? "ZMM" : "YMM");
		}
	}

	puts("void vfpclasspd(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isBit(128|256|512)) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k.changeBit(op.getBit()), 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW1 | T_B64, 0x66, imm); }");
	puts("void vfpclassps(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isBit(128|256|512)) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k.changeBit(op.getBit()), 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_YMM | T_EW0 | T_B32, 0x66, imm); }");
	puts("void vfpclassph(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isBit(128|256|512)) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k.changeBit(op.getBit()), 0, op, T_0F3A | T_MUST_EVEX | T_YMM | T_EW0 | T_B16, 0x66, imm); }");
	puts("void vfpclasssd(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isXMEM()) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k, 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_EW1 | T_N8, 0x67, imm); }");
	puts("void vfpclassss(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isXMEM()) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k, 0, op, T_66 | T_0F3A | T_MUST_EVEX | T_EW0 | T_N4, 0x67, imm); }");
	puts("void vfpclasssh(const Opmask& k, const Operand& op, uint8_t imm) { if (!op.isXMEM()) XBYAK_THROW(ERR_BAD_MEM_SIZE) opVex(k, 0, op, T_0F3A | T_MUST_EVEX | T_EW0 | T_N2, 0x67, imm); }");

	puts("void vpshufbitqmb(const Opmask& k, const Xmm& x, const Operand& op) { opVex(k, &x, op, T_66 | T_0F38 | T_EW0 | T_YMM | T_MUST_EVEX, 0x8F); }");

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

void putFP16_1()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		int mode;
	} tbl[] = {
		{ 0x58, "add", 0 },
		{ 0x5C, "sub", 0 },
		{ 0x59, "mul", 0 },
		{ 0x5E, "div", 0 },
		{ 0x5F, "max", 1 },
		{ 0x5D, "min", 1 },
	};
	const char *erTbl[] = { "ER", "SAE" };
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		printf("void v%sph(const Xmm& xmm, const Operand& op1, const Operand& op2 = Operand()) { opAVX_X_X_XM(xmm, op1, op2, T_MAP5 | T_EW0 | T_YMM | T_MUST_EVEX | T_%s_Z | T_B16, 0x%02X); }\n", p->name, erTbl[p->mode], p->code);
		printf("void v%ssh(const Xmm& xmm, const Operand& op1, const Operand& op2 = Operand()) { opAVX_X_X_XM(xmm, op1, op2, T_MAP5 | T_F3 | T_EW0 | T_MUST_EVEX | T_%s_X | T_N2, 0x%02X); }\n", p->name, erTbl[p->mode], p->code);
	}
}

void putFP16_FMA()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		bool isPH;
	} tbl[] = {
		{ 0x06, "vfmaddsub", true },
		{ 0x07, "vfmsubadd", true },
		{ 0x08, "vfmadd", true },
		{ 0x0C, "vfnmadd", true },
		{ 0x0A, "vfmsub", true },
		{ 0x0E, "vfnmsub", true },
		{ 0x09, "vfmadd", false },
		{ 0x0D, "vfnmadd", false },
		{ 0x0B, "vfmsub", false },
		{ 0x0F, "vfnmsub", false },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		for (int k = 0; k < 3; k++) {
			const struct Ord {
				const char *str;
				uint8_t code;
			} ord[] = {
				{ "132", 0x90 },
				{ "213", 0xA0 },
				{ "231", 0xB0 },
			};
			uint64_t type = T_66 | T_MAP6 | T_EW0 | T_MUST_EVEX;
			const char *suf = 0;
			if (tbl[i].isPH) {
				type |= T_ER_Z | T_YMM | T_B16;
				suf = "ph";
			} else {
				type |= T_ER_X | T_N2;
				suf = "sh";
			}
			std::string s = type2String(type);
			printf("void %s%s%s(const Xmm& x1, const Xmm& x2, const Operand& op) { opAVX_X_X_XM(x1, x2, op, %s, 0x%02X); }\n"
				, tbl[i].name, ord[k].str, suf, s.c_str(), tbl[i].code | ord[k].code);
		}
	}
}
void putFP16_FMA2()
{
	const struct Tbl {
		uint8_t code;
		const char *name;
		bool isPH;
	} tbl[] = {
		{ 0x56, "maddc", true },
		{ 0xD6, "mulc", true },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		for (int j = 0; j < 2; j++) {
			uint64_t type = T_MAP6 | T_EW0 | T_MUST_EVEX;
			if (j == 0) {
				type |= T_F2;
			} else {
				type |= T_F3;
			}
			const char *suf = 0;
			if (tbl[i].isPH) {
				type |= T_ER_Z | T_YMM | T_B32;
				suf = "ph";
			} else {
				type |= T_ER_X | T_N2;
				suf = "sh";
			}
			std::string s = type2String(type);
			printf("void vf%s%s%s(const Xmm& x1, const Xmm& x2, const Operand& op) { opAVX_X_X_XM(x1, x2, op, %s, 0x%02X); }\n"
				, j == 0 ? "c" : "", tbl[i].name, suf, s.c_str(), tbl[i].code);
		}
	}
}

void putFP16_2()
{
	{
		uint64_t type = T_F3 | T_MAP5 | T_MUST_EVEX | T_EW0 | T_N2;
		std::string s = type2String(type);
		printf("void vmovsh(const Xmm& x, const Address& addr) { opAVX_X_X_XM(x, xm0, addr, %s, 0x10); }\n", s.c_str());
		printf("void vmovsh(const Xmm& x1, const Xmm& x2, const Xmm& x3) { opAVX_X_X_XM(x1, x2, x3, %s, 0x10); }\n", s.c_str());
	}
}

void putFP16()
{
	putFP16_1();
	putFP16_FMA();
	putFP16_FMA2();
	putFP16_2();
}

void putAVX10_2()
{
	puts("void vcmppbf16(const Opmask& k, const Xmm& x, const Operand& op, uint8_t imm) { opVex(k, &x, op, T_MUST_EVEX|T_F2|T_0F3A|T_EW0|T_YMM|T_B16, 0xC2, imm); }");
	puts("void vfpclasspbf16(const Opmask& k, const Operand& op, uint8_t imm) { opVex(k.changeBit(op.getBit()), 0, op, T_MUST_EVEX|T_F2|T_0F3A|T_EW0|T_YMM|T_B16, 0x66, imm); }");

	const struct Tbl {
		uint8_t code;
		const char *name;
		uint64_t type;
	} tbl1[] = {
		{ 0x74, "vcvtbiasph2bf8", T_MUST_EVEX | T_0F38 | T_EW0 |T_YMM | T_B16 },
		{ 0x74, "vcvtbiasph2bf8s", T_MUST_EVEX | T_MAP5 | T_EW0 |T_YMM | T_B16 },
		{ 0x18, "vcvtbiasph2hf8", T_MUST_EVEX | T_MAP5 | T_EW0 | T_YMM | T_B16 },
		{ 0x1B, "vcvtbiasph2hf8s", T_MUST_EVEX | T_MAP5 | T_EW0 | T_YMM | T_B16 },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl1); i++) {
		const Tbl *p = &tbl1[i];
		std::string s = type2String(p->type);
		printf("void %s(const Xmm& x1, const Xmm& x2, const Operand& op) { opCvt6(x1, x2, op, %s, 0x%02X); }\n" , p->name, s.c_str(), p->code);
	}
	puts("void vcvthf82ph(const Xmm& x, const Operand& op) { checkCvt1(x, op); opVex(x, 0, op, T_MUST_EVEX | T_F2 | T_MAP5 | T_EW0 | T_YMM | T_N1, 0x1E); }");

	const Tbl tbl2[] = {
		{ 0x74, "vcvtneph2bf8", T_MUST_EVEX | T_F3 | T_0F38 | T_EW0 | T_YMM | T_B16 },
		{ 0x74, "vcvtneph2bf8s", T_MUST_EVEX | T_F3 |T_MAP5 | T_EW0 | T_YMM | T_B16 },
		{ 0x18, "vcvtneph2hf8", T_MUST_EVEX | T_F3 |T_MAP5 | T_EW0 | T_YMM | T_B16 },
		{ 0x1B, "vcvtneph2hf8s", T_MUST_EVEX | T_F3 |T_MAP5 | T_EW0 | T_YMM | T_B16 },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl2); i++) {
		const Tbl *p = &tbl2[i];
		std::string s = type2String(p->type);
		printf("void %s(const Xmm& x, const Operand& op) { opCvt2(x, op, %s, 0x%02X); }\n" , p->name, s.c_str(), p->code);
	}
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
	putVcmpAlias();
	putX_XM();
	putM_X();
	putXM_X();
	putX_X_XM_IMM();
	putX_X_XM_IMM_AVX10();
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
	putFP16();
	putAVX10_2();
}
