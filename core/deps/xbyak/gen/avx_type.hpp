#include <assert.h>
#include "avx_type_def.h"

const int NONE = 256; // same as Xbyak::CodeGenerator::NONE

std::string type2String(uint64_t type)
{
	if (type == 0) return "T_NONE";
	std::string str;
	int low = type & T_NX_MASK;
	if (0 < low && low < 7) {
		const char *tbl[8] = {
			"T_N1", "T_N2", "T_N4", "T_N8", "T_N16", "T_N32"
		};
		assert(low < int(sizeof(tbl) / sizeof(tbl[0])));
		str = tbl[low - 1];
	}
	if (type & T_N_VL) str += "|T_N_VL";
	if (type & T_APX) str += "|T_APX";
	if ((type & T_NX_MASK) == T_DUP) str += "|T_DUP";
	if (type & T_66) str += "|T_66";
	if (type & T_F3) str += "|T_F3";
	if (type & T_F2) str += "|T_F2";
	if (type & T_0F) {
		if (type & T_FP16) {
			str += "|T_MAP5";
		} else {
			str += "|T_0F";
		}
	}
	if (type & T_0F38) {
		if (type & T_FP16) {
			str += "|T_MAP6";
		} else {
			str += "|T_0F38";
		}
	}
	if (type & T_0F3A) str += "|T_0F3A";
	if (type & T_L0) str += "|T_L0";
	if (type & T_L1) str += "|T_L1";
	if (type & T_W0) str += "|T_W0";
	if (type & T_W1) str += "|T_W1";
	if (type & T_EW0) str += "|T_EW0";
	if (type & T_EW1) str += "|T_EW1";
	if (type & T_YMM) str += "|T_YMM";
	if (type & T_EVEX) str += "|T_EVEX";
	if (type & T_ER_X) str += "|T_ER_X";
	if (type & T_ER_Y) str += "|T_ER_Y";
	if (type & T_ER_Z) str += "|T_ER_Z";
	if (type & T_ER_R) str += "|T_ER_R";
	if (type & T_SAE_X) str += "|T_SAE_X";
	if (type & T_SAE_Y) str += "|T_SAE_Y";
	if (type & T_SAE_Z) str += "|T_SAE_Z";
	if (type & T_MUST_EVEX) str += "|T_MUST_EVEX";

	switch (type & T_B16) { // T_B16 = T_B32 | T_B64
	case T_B16: str += "|T_B16"; break;
	case T_B32: str += "|T_B32"; break;
	case T_B64: str += "|T_B64"; break;
	default: break;
	}
	if (type & T_M_K) str += "|T_M_K";
	if (type & T_VSIB) str += "|T_VSIB";
	if (type & T_MEM_EVEX) str += "|T_MEM_EVEX";
	if (type & T_NF) str += "|T_NF";
	if (type & T_CODE1_IF1) str += "|T_CODE1_IF1";
	if (type & T_ND1) str += "|T_ND1";
	if (type & T_ZU) str += "|T_ZU";

	if (str[0] == '|') str = str.substr(1);
	return str;
}
