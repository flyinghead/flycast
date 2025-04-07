#include <stdio.h>
#include "xbyak/xbyak_util.h"

#define NUM_OF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

struct PopCountTest : public Xbyak::CodeGenerator {
	PopCountTest(int n)
		: Xbyak::CodeGenerator(4096, Xbyak::DontSetProtectRWE)
	{
		mov(eax, n);
		popcnt(eax, eax);
		ret();
	}
};

void putCPUinfo(bool onlyCpuidFeature)
{
	using namespace Xbyak::util;
	Cpu cpu;
	printf("vendor %s\n", cpu.has(Cpu::tINTEL) ? "intel" : "amd");
	static const struct {
		Cpu::Type type;
		const char *str;
	} tbl[] = {
		{ Cpu::tMMX, "mmx" },
		{ Cpu::tMMX2, "mmx2" },
		{ Cpu::tCMOV, "cmov" },
		{ Cpu::tSSE, "sse" },
		{ Cpu::tSSE2, "sse2" },
		{ Cpu::tSSE3, "sse3" },
		{ Cpu::tSSSE3, "ssse3" },
		{ Cpu::tSSE41, "sse41" },
		{ Cpu::tSSE42, "sse42" },
		{ Cpu::tSSE4a, "sse4a" },
		{ Cpu::tPOPCNT, "popcnt" },
		{ Cpu::t3DN, "3dn" },
		{ Cpu::tE3DN, "e3dn" },
		{ Cpu::tAESNI, "aesni" },
		{ Cpu::tRDTSCP, "rdtscp" },
		{ Cpu::tXSAVE, "xsave(xgetvb)" },
		{ Cpu::tOSXSAVE, "osxsave" },
		{ Cpu::tPCLMULQDQ, "pclmulqdq" },
		{ Cpu::tAVX, "avx" },
		{ Cpu::tFMA, "fma" },
		{ Cpu::tAVX2, "avx2" },
		{ Cpu::tBMI1, "bmi1" },
		{ Cpu::tBMI2, "bmi2" },
		{ Cpu::tLZCNT, "lzcnt" },
		{ Cpu::tPREFETCHW, "prefetchw" },
		{ Cpu::tENHANCED_REP, "enh_rep" },
		{ Cpu::tRDRAND, "rdrand" },
		{ Cpu::tADX, "adx" },
		{ Cpu::tRDSEED, "rdseed" },
		{ Cpu::tSMAP, "smap" },
		{ Cpu::tHLE, "hle" },
		{ Cpu::tRTM, "rtm" },
		{ Cpu::tMPX, "mpx" },
		{ Cpu::tSHA, "sha" },
		{ Cpu::tPREFETCHWT1, "prefetchwt1" },
		{ Cpu::tF16C, "f16c" },
		{ Cpu::tMOVBE, "movbe" },
		{ Cpu::tAVX512F, "avx512f" },
		{ Cpu::tAVX512DQ, "avx512dq" },
		{ Cpu::tAVX512IFMA, "avx512_ifma" },
		{ Cpu::tAVX512PF, "avx512pf" },
		{ Cpu::tAVX512ER, "avx512er" },
		{ Cpu::tAVX512CD, "avx512cd" },
		{ Cpu::tAVX512BW, "avx512bw" },
		{ Cpu::tAVX512VL, "avx512vl" },
		{ Cpu::tAVX512VBMI, "avx512_vbmi" },
		{ Cpu::tAVX512_4VNNIW, "avx512_4vnniw" },
		{ Cpu::tAVX512_4FMAPS, "avx512_4fmaps" },

		{ Cpu::tAVX512_VBMI2, "avx512_vbmi2" },
		{ Cpu::tGFNI, "gfni" },
		{ Cpu::tVAES, "vaes" },
		{ Cpu::tVPCLMULQDQ, "vpclmulqdq" },
		{ Cpu::tAVX512_VNNI, "avx512_vnni" },
		{ Cpu::tAVX512_BITALG, "avx512_bitalg" },
		{ Cpu::tAVX512_VPOPCNTDQ, "avx512_vpopcntdq" },
		{ Cpu::tAVX512_BF16, "avx512_bf16" },
		{ Cpu::tAVX512_VP2INTERSECT, "avx512_vp2intersect" },
		{ Cpu::tAMX_TILE, "amx(tile)" },
		{ Cpu::tAMX_INT8, "amx(int8)" },
		{ Cpu::tAMX_BF16, "amx(bf16)" },
		{ Cpu::tAVX_VNNI, "avx_vnni" },
		{ Cpu::tAVX512_FP16, "avx512_fp16" },
		{ Cpu::tWAITPKG, "waitpkg" },
		{ Cpu::tCLFLUSHOPT, "clflushopt" },
		{ Cpu::tCLDEMOTE, "cldemote" },
		{ Cpu::tCLWB, "clwb" },
		{ Cpu::tMOVDIRI, "movdiri" },
		{ Cpu::tMOVDIR64B, "movdir64b" },
		{ Cpu::tUINTR, "uintr" },
		{ Cpu::tSERIALIZE, "serialize" },
		{ Cpu::tCLZERO, "clzero" },
		{ Cpu::tAMX_FP16, "amx_fp16" },
		{ Cpu::tAVX_VNNI_INT8, "avx_vnni_int8" },
		{ Cpu::tAVX_NE_CONVERT, "avx_ne_convert" },
		{ Cpu::tAVX_IFMA, "avx_ifma" },
		{ Cpu::tRAO_INT, "rao-int" },
		{ Cpu::tCMPCCXADD, "cmpccxadd" },
		{ Cpu::tPREFETCHITI, "prefetchiti" },
		{ Cpu::tSHA512, "sha512" },
		{ Cpu::tSM3, "sm3" },
		{ Cpu::tSM4, "sm4" },
		{ Cpu::tAVX_VNNI_INT16, "avx_vnni_int16" },
		{ Cpu::tAPX_F, "apx_f" },
		{ Cpu::tAVX10, "avx10" },
		{ Cpu::tAESKLE, "aeskle" },
		{ Cpu::tWIDE_KL, "wide_kl" },
		{ Cpu::tKEYLOCKER, "keylocker" },
		{ Cpu::tKEYLOCKER_WIDE, "keylocker_wide" },
		{ Cpu::tTSXLDTRK, "tsxldtrk" },
	};
	for (size_t i = 0; i < NUM_OF_ARRAY(tbl); i++) {
		if (cpu.has(tbl[i].type)) printf(" %s", tbl[i].str);
	}
	printf("\n");
	if (onlyCpuidFeature) return;
	if (cpu.has(Cpu::tAVX10)) {
		printf("AVX10 version %d\n", cpu.getAVX10version());
	}
	if (cpu.has(Cpu::tPOPCNT)) {
		const int n = 0x12345678; // bitcount = 13
		const int ok = 13;
		PopCountTest code(n);
		code.setProtectModeRE();
		int (*f)() = code.getCode<int (*)()>();
		int r = f();
		if (r == ok) {
			puts("popcnt ok");
		} else {
			printf("popcnt ng %d %d\n", r, ok);
		}
		code.setProtectModeRW();
	}
	/*
		                displayFamily displayModel
		Opteron 2376        10            4
		Core2 Duo T7100      6            F
		Core i3-2120T        6           2A
		Core i7-2600         6           2A
		Xeon X5650           6           2C
		Core i7-3517         6           3A
		Core i7-3930K        6           2D
	*/
	cpu.putFamily();
	for (unsigned int i = 0; i < cpu.getDataCacheLevels(); i++) {
		printf("cache level=%u data cache size=%u cores sharing data cache=%u\n", i, cpu.getDataCacheSize(i), cpu.getCoresSharingDataCache(i));
	}
	printf("SmtLevel =%u\n", cpu.getNumCores(Xbyak::util::SmtLevel));
	printf("CoreLevel=%u\n", cpu.getNumCores(Xbyak::util::CoreLevel));
}

int main(int argc, char *argv[])
{
	bool onlyCpuidFeature = argc == 2 && strcmp(argv[1], "-cpuid") == 0;
	if (!onlyCpuidFeature) {
#ifdef XBYAK32
		puts("32bit");
#else
		puts("64bit");
#endif
	}
	putCPUinfo(onlyCpuidFeature);
}
