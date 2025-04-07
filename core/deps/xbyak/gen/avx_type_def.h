	// @@@begin of avx_type_def.h
	static const uint64_t T_NONE = 0ull;
		// low 3 bit
	static const uint64_t T_N1 = 1ull;
	static const uint64_t T_N2 = 2ull;
	static const uint64_t T_N4 = 3ull;
	static const uint64_t T_N8 = 4ull;
	static const uint64_t T_N16 = 5ull;
	static const uint64_t T_N32 = 6ull;
	static const uint64_t T_NX_MASK = 7ull;
	static const uint64_t T_DUP = T_NX_MASK;//1 << 4, // N = (8, 32, 64)
	static const uint64_t T_N_VL = 1ull << 3; // N * (1, 2, 4) for VL
	static const uint64_t T_APX = 1ull << 4;
	static const uint64_t T_66 = 1ull << 5; // pp = 1
	static const uint64_t T_F3 = 1ull << 6; // pp = 2
	static const uint64_t T_ER_R = 1ull << 7; // reg{er}
	static const uint64_t T_0F = 1ull << 8;
	static const uint64_t T_0F38 = 1ull << 9;
	static const uint64_t T_0F3A = 1ull << 10;
	static const uint64_t T_L0 = 1ull << 11;
	static const uint64_t T_L1 = 1ull << 12;
	static const uint64_t T_W0 = 1ull << 13;
	static const uint64_t T_W1 = 1ull << 14;
	static const uint64_t T_EW0 = 1ull << 15;
	static const uint64_t T_EW1 = 1ull << 16;
	static const uint64_t T_YMM = 1ull << 17; // support YMM, ZMM
	static const uint64_t T_EVEX = 1ull << 18;
	static const uint64_t T_ER_X = 1ull << 19; // xmm{er}
	static const uint64_t T_ER_Y = 1ull << 20; // ymm{er}
	static const uint64_t T_ER_Z = 1ull << 21; // zmm{er}
	static const uint64_t T_SAE_X = 1ull << 22; // xmm{sae}
	static const uint64_t T_SAE_Y = 1ull << 23; // ymm{sae}
	static const uint64_t T_SAE_Z = 1ull << 24; // zmm{sae}
	static const uint64_t T_MUST_EVEX = 1ull << 25; // contains T_EVEX
	static const uint64_t T_B32 = 1ull << 26; // m32bcst
	static const uint64_t T_B64 = 1ull << 27; // m64bcst
	static const uint64_t T_B16 = T_B32 | T_B64; // m16bcst (Be careful)
	static const uint64_t T_M_K = 1ull << 28; // mem{k}
	static const uint64_t T_VSIB = 1ull << 29;
	static const uint64_t T_MEM_EVEX = 1ull << 30; // use evex if mem
	static const uint64_t T_FP16 = 1ull << 31; // avx512-fp16
	static const uint64_t T_MAP5 = T_FP16 | T_0F;
	static const uint64_t T_MAP6 = T_FP16 | T_0F38;
	static const uint64_t T_NF = 1ull << 32; // T_nf
	static const uint64_t T_CODE1_IF1 = 1ull << 33; // code|=1 if !r.isBit(8)

	static const uint64_t T_ND1 = 1ull << 35; // ND=1
	static const uint64_t T_ZU = 1ull << 36; // ND=ZU
	static const uint64_t T_F2 = 1ull << 37; // pp = 3
	// T_66 = 1, T_F3 = 2, T_F2 = 3
	static const uint64_t T_ALLOW_DIFF_SIZE = 1ull << 38; // allow difference reg size
	static inline uint32_t getPP(uint64_t type) { return (type & T_66) ? 1 : (type & T_F3) ? 2 : (type & T_F2) ? 3 : 0; }
	// @@@end of avx_type_def.h
