#include <assert.h>
// copy CodeGenerator::AVXtype
	enum AVXtype {
		// low 3 bit
		T_N1 = 1,
		T_N2 = 2,
		T_N4 = 3,
		T_N8 = 4,
		T_N16 = 5,
		T_N32 = 6,
		T_NX_MASK = 7,
		//
		T_N_VL = 1 << 3, // N * (1, 2, 4) for VL
		T_DUP = 1 << 4, // N = (8, 32, 64)
		T_66 = 1 << 5,
		T_F3 = 1 << 6,
		T_F2 = 1 << 7,
		T_0F = 1 << 8,
		T_0F38 = 1 << 9,
		T_0F3A = 1 << 10,
		T_L0 = 1 << 11,
		T_L1 = 1 << 12,
		T_W0 = 1 << 13,
		T_W1 = 1 << 14,
		T_EW0 = 1 << 15,
		T_EW1 = 1 << 16,
		T_YMM = 1 << 17, // support YMM, ZMM
		T_EVEX = 1 << 18,
		T_ER_X = 1 << 19, // xmm{er}
		T_ER_Y = 1 << 20, // ymm{er}
		T_ER_Z = 1 << 21, // zmm{er}
		T_SAE_X = 1 << 22, // xmm{sae}
		T_SAE_Y = 1 << 23, // ymm{sae}
		T_SAE_Z = 1 << 24, // zmm{sae}
		T_MUST_EVEX = 1 << 25, // contains T_EVEX
		T_B32 = 1 << 26, // m32bcst
		T_B64 = 1 << 27, // m64bcst
		T_M_K = 1 << 28, // mem{k}
		T_VSIB = 1 << 29,
		T_MEM_EVEX = 1 << 30, // use evex if mem
		T_XXX
	};

const int NONE = 256; // same as Xbyak::CodeGenerator::NONE

std::string type2String(int type)
{
	std::string str;
	int low = type & T_NX_MASK;
	if (0 < low) {
		const char *tbl[8] = {
			"T_N1", "T_N2", "T_N4", "T_N8", "T_N16", "T_N32"
		};
		assert(low < int(sizeof(tbl) / sizeof(tbl[0])));
		str = tbl[low - 1];
	}
	if (type & T_N_VL) {
		if (!str.empty()) str += " | ";
		str += "T_N_VL";
	}
	if (type & T_DUP) {
		if (!str.empty()) str += " | ";
		str += "T_DUP";
	}
	if (type & T_66) {
		if (!str.empty()) str += " | ";
		str += "T_66";
	}
	if (type & T_F3) {
		if (!str.empty()) str += " | ";
		str += "T_F3";
	}
	if (type & T_F2) {
		if (!str.empty()) str += " | ";
		str += "T_F2";
	}
	if (type & T_0F) {
		if (!str.empty()) str += " | ";
		str += "T_0F";
	}
	if (type & T_0F38) {
		if (!str.empty()) str += " | ";
		str += "T_0F38";
	}
	if (type & T_0F3A) {
		if (!str.empty()) str += " | ";
		str += "T_0F3A";
	}
	if (type & T_L0) {
		if (!str.empty()) str += " | ";
		str += "VEZ_L0";
	}
	if (type & T_L1) {
		if (!str.empty()) str += " | ";
		str += "VEZ_L1";
	}
	if (type & T_W0) {
		if (!str.empty()) str += " | ";
		str += "T_W0";
	}
	if (type & T_W1) {
		if (!str.empty()) str += " | ";
		str += "T_W1";
	}
	if (type & T_EW0) {
		if (!str.empty()) str += " | ";
		str += "T_EW0";
	}
	if (type & T_EW1) {
		if (!str.empty()) str += " | ";
		str += "T_EW1";
	}
	if (type & T_YMM) {
		if (!str.empty()) str += " | ";
		str += "T_YMM";
	}
	if (type & T_EVEX) {
		if (!str.empty()) str += " | ";
		str += "T_EVEX";
	}
	if (type & T_ER_X) {
		if (!str.empty()) str += " | ";
		str += "T_ER_X";
	}
	if (type & T_ER_Y) {
		if (!str.empty()) str += " | ";
		str += "T_ER_Y";
	}
	if (type & T_ER_Z) {
		if (!str.empty()) str += " | ";
		str += "T_ER_Z";
	}
	if (type & T_SAE_X) {
		if (!str.empty()) str += " | ";
		str += "T_SAE_X";
	}
	if (type & T_SAE_Y) {
		if (!str.empty()) str += " | ";
		str += "T_SAE_Y";
	}
	if (type & T_SAE_Z) {
		if (!str.empty()) str += " | ";
		str += "T_SAE_Z";
	}
	if (type & T_MUST_EVEX) {
		if (!str.empty()) str += " | ";
		str += "T_MUST_EVEX";
	}
	if (type & T_B32) {
		if (!str.empty()) str += " | ";
		str += "T_B32";
	}
	if (type & T_B64) {
		if (!str.empty()) str += " | ";
		str += "T_B64";
	}
	if (type & T_M_K) {
		if (!str.empty()) str += " | ";
		str += "T_M_K";
	}
	if (type & T_VSIB) {
		if (!str.empty()) str += " | ";
		str += "T_VSIB";
	}
	if (type & T_MEM_EVEX) {
		if (!str.empty()) str += " | ";
		str += "T_MEM_EVEX";
	}
	return str;
}
