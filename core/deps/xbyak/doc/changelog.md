# History

* 2025/Feb/07 ver 7.23.1 revert the behavior of StackFrame::close().
* 2025/Feb/03 ver 7.23 stricter checking of register size, improve handling of 16-bit immediates, change spec of StackFrame::close(), fix push/pop to support APX.
* 2024/Nov/11 ver 7.22 add Reg::cvt{128,256,512}(). tested by xed 2024.11.04
* 2024/Oct/31 ver 7.21 Enhance XMM register validation in SSE instructions
* 2024/Oct/17 ver 7.20.1 Updated to comply with AVX10.2 specification rev 2.0
* 2024/Oct/15 ver 7.20 Fixed the specification of setDefaultEncoding, setDefaultEncodingAVX10.
* 2024/Oct/15 ver 7.11 Added full support for AVX10.2
* 2024/Oct/13 ver 7.10 support AVX10 integer and fp16 vnni, media new instructions. setDefaultEncoding is extended.
* 2024/Oct/10 ver 7.09.1 fix the names of vpcompressb and vpcompressw
* 2024/Oct/08 ver 7.09 support YMM embedded rounding of AVX10.2 and fix some mnemonics with {sae}/{er}.
* 2024/Oct/07 ver 7.08 support rdfsbase etc.
* 2024/Aug/29 ver 7.07.1 adapt to NASM 2.16.03 output of xchg (The functionality stays the same.)
* 2024/Jun/11 ver 7.07 support xresldtrk/xsusldtrk
* 2024/Mar/07 ver 7.06 Xbyak::util::Cpu supports AMD processor
* 2024/Feb/11 ver 7.05.1 fix extractBit() in util::Cpu and align() for autoGrow mode
* 2024/Jan/03 ver 7.05 support RAO-INT for APX
* 2023/Dec/28 ver 7.04 rex2 supports two-byte opecode
* 2023/Dec/26 ver 7.03 set the default value of dfv to 0
* 2023/Dec/20 ver 7.02 SHA* support APX
* 2023/Dec/19 ver 7.01 support AESKLE, WIDE_KL, KEYLOCKER, KEYLOCKER_WIDE, detection of APX10/APX
* 2023/Dec/01 ver 7.00 support APX
* 2023/Aug/07 ver 6.73 add sha512/sm3/sm4/avx-vnni-int16
* 2023/Aug/02 ver 6.72 add xbegin/xabort/xend
* 2023/Jul/27 ver 6.71 Allocator supports huge page
* 2023/Jul/05 ver 6.70 add alias of vclmulqdq, correct alias of pclmulqdq
* 2023/Jun/27 ver 6.69.2 add constexpr to `TypeT operator|` (thanks to Wunkolo)
* 2023/Mar/23 ver 6.69.1 add detection of xsave (thanks to Wunkolo)
* 2023/Feb/20 ver 6.69 util::Cpu supports AMD CPUs. support UINTR
* 2022/Dec/07 ver 6.68 support prefetchit{0,1}
* 2022/Nov/30 ver 6.67 support CMPccXADD
* 2022/Nov/25 ver 6.66 support RAO-INT
* 2022/Nov/22 ver 6.65 consider x32
* 2022/Nov/04 ver 6.64 some vmov* support addressing with mask
* 2022/Oct/06 ver 6.63 vpmadd52{h,l}uq support AVX-IFMA
* 2022/Oct/05 ver 6.63 support amx_fp16/avx_vnni_int8/avx_ne_convert and add setDefaultEncoding()
* 2022/Aug/15 ver 6.62 add serialize instruction
* 2022/Aug/02 ver 6.61.1 noexcept is supported by Visual Studio 2015 or later
* 2022/Jul/29 ver 6.61 fix exception of movzx eax, ah in 64-bit mode
* 2022/Jun/16 ver 6.60.2 fix detection of GFNI, VAES, and VPCLMULQDQ
* 2022/Jun/15 ver 6.60.1 fix link error of Xbyak::util::Cpu on Visual Studio with /O0 option
* 2022/Jun/06 ver 6.60 change the version format to avoid it going backward
* 2022/Jun/01 ver 6.06 refactor Cpu::Type class and improve MmapAllocator when XBYAK_USE_MEMFD is defined.
* 2022/Mar/20 ver 6.052 add Cpu::operator==()
* 2022/Mar/13 ver 6.051 fix compile error when XBYAK_NO_EXCEPTION is defined
* 2022/Mar/12 ver 6.05 add movdiri, movdir64b, clwb, cldemote
* 2022/Apr/22 ver 6.041 consider Android and mingw
* 2022/Apr/05 ver 6.04 add tpause, umonitor, umwait
* 2022/Mar/08 ver 6.03 MmapAllocator supports memfd with user-defined strings.
* 2022/Jan/28 ver 6.02 strict check the range of 32-bit dispacement
* 2021/Dec/14 ver 6.01 support T_FAR jump/call and retf
* 2021/Sep/14 ver 6.00 fully support AVX512-FP16
* 2021/Sep/09 ver 5.997 fix vrndscale* to support {sae}
* 2021/Sep/03 ver 5.996 fix v{add,sub,mul,div,max,min}{sd,ss} to support T_rd_sae.
* 2021/Aug/15 ver 5.995 add a label to /proc/self/maps if XBYAK_USE_MEMFD is defined on Linux
* 2021/Jun/17 ver 5.994 add alias of vcmpXX{ps,pd,ss,sd} with mask register
* 2021/Jun/06 ver 5.993 strict check of gather/scatter register combination
* 2021/May/09 ver 5.992 support endbr32 and endbr64
* 2020/Nov/16 ver 5.991 disable constexpr for gcc-5 with -std=c++-14
* 2020/Oct/19 ver 5.99 support VNNI instructions(Thanks to akharito)
* 2020/Oct/17 ver 5.98 support the form of [scale * reg]
* 2020/Sep/08 ver 5.97 replace uint32 with uint32_t etc.
* 2020/Aug/28 ver 5.95 some constructors of register classes support constexpr if C++14 or later
* 2020/Aug/04 ver 5.941 `CodeGenerator::reset()` calls `ClearError()`.
* 2020/Jul/28 ver 5.94 remove #include <winsock2.h> (only windows)
* 2020/Jul/21 ver 5.93 support exception-less mode
* 2020/Jun/30 ver 5.92 support Intel AMX instruction set (Thanks to nshustrov)
* 2020/Jun/22 ver 5.913 fix mov(r64, imm64) on 32-bit env with XBYAK64
* 2020/Jun/19 ver 5.912 define MAP_JIT on macOS regardless of Xcode version (Thanks to rsdubtso)
* 2020/May/10 ver 5.911 XBYAK_USE_MMAP_ALLOCATOR is defined unless XBYAK_DONT_USE_MMAP_ALLOCATOR is defined.
* 2020/Apr/20 ver 5.91 accept mask register k0 (it means no mask)
* 2020/Apr/09 ver 5.90 kmov{b,d,w,q} throws exception for an unsupported register
* 2020/Feb/26 ver 5.891 fix typo of type
* 2020/Jan/03 ver 5.89 fix error of vfpclasspd
* 2019/Dec/20 ver 5.88 fix compile error on Windows
* 2019/Dec/19 ver 5.87 add setDefaultJmpNEAR(), which deals with `jmp` of an undefined label as T_NEAR if no type is specified.
* 2019/Dec/13 ver 5.86 [changed] revert to the behavior before v5.84 if -fno-operator-names is defined (and() is available)
* 2019/Dec/07 ver 5.85 append MAP_JIT flag to mmap for macOS mojave or later
* 2019/Nov/29 ver 5.84 [changed] XBYAK_NO_OP_NAMES is defined unless XBYAK_USE_OP_NAMES is defined
* 2019/Oct/12 ver 5.83 exit(1) was removed
* 2019/Sep/23 ver 5.82 support monitorx, mwaitx, clzero (thanks to @MagurosanTeam)
* 2019/Sep/14 ver 5.81 support some generic mnemonics.
* 2019/Aug/01 ver 5.802 fix detection of AVX512_BF16 (thanks to vpirogov)
* 2019/May/27 support vp2intersectd, vp2intersectq (not tested)
* 2019/May/26 ver 5.80 support vcvtne2ps2bf16, vcvtneps2bf16, vdpbf16ps
* 2019/Apr/27 ver 5.79 vcmppd/vcmpps supports ptr_b(thanks to jkopinsky)
* 2019/Apr/15 ver 5.78 rewrite Reg::changeBit() (thanks to MerryMage)
* 2019/Mar/06 ver 5.77 fix number of cores that share LLC cache by densamoilov
* 2019/Jan/17 ver 5.76 add Cpu::getNumCores() by shelleygoel
* 2018/Oct/31 ver 5.751 recover Xbyak::CastTo for compatibility
* 2018/Oct/29 ver 5.75 unlink LabelManager from Label when msg is destroyed
* 2018/Oct/21 ver 5.74 support RegRip +/- int. Xbyak::CastTo is removed
* 2018/Oct/15 util::AddressFrame uses push/pop instead of mov
* 2018/Sep/19 ver 5.73 fix evex encoding of vpslld, vpslldq, vpsllw, etc for (reg, mem, imm8)
* 2018/Sep/19 ver 5.72 fix the encoding of vinsertps for disp8N(Thanks to petercaday)
* 2018/Sep/04 ver 5.71 L() returns a new label instance
* 2018/Aug/27 ver 5.70 support setProtectMode() and DontUseProtect for read/exec setting
* 2018/Aug/24 ver 5.68 fix wrong VSIB encoding with vector index >= 16(thanks to petercaday)
* 2018/Aug/14 ver 5.67 remove mutable in Address ; fix setCacheHierarchy for cloud vm
* 2018/Jul/26 ver 5.661 support mingw64
* 2018/Jul/24 ver 5.66 add CodeArray::PROTECT_RE to mode of protect()
* 2018/Jun/26 ver 5.65 fix push(qword [mem])
* 2018/Mar/07 ver 5.64 fix zero division in Cpu() on some cpu
* 2018/Feb/14 ver 5.63 fix Cpu::setCacheHierarchy() and fix EvexModifierZero for clang<3.9(thanks to mgouicem)
* 2018/Feb/13 ver 5.62 Cpu::setCacheHierarchy() by mgouicem and rsdubtso
* 2018/Feb/07 ver 5.61 vmov* supports mem{k}{z}(I forgot it)
* 2018/Jan/24 ver 5.601 add xword, yword, etc. into Xbyak::util namespace
* 2018/Jan/05 ver 5.60 support AVX-512 for Ice lake(319433-030.pdf)
* 2017/Aug/22 ver 5.53 fix mpx encoding, add bnd() prefix
* 2017/Aug/18 ver 5.52 fix align (thanks to MerryMage)
* 2017/Aug/17 ver 5.51 add multi-byte nop and align() uses it(thanks to inolen)
* 2017/Aug/08 ver 5.50 add mpx(thanks to magurosan)
* 2017/Aug/08 ver 5.45 add sha(thanks to magurosan)
* 2017/Aug/08 ver 5.44 add prefetchw(thanks to rsdubtso)
* 2017/Jul/12 ver 5.432 reduce warnings of PVS studio
* 2017/Jul/09 ver 5.431 fix hasRex() (no affect) (thanks to drillsar)
* 2017/May/14 ver 5.43 fix CodeGenerator::resetSize() (thanks to gibbed)
* 2017/May/13 ver 5.42 add movs{b,w,d,q}
* 2017/Jan/26 ver 5.41 add prefetchwt1 and support for scale == 0(thanks to rsdubtso)
* 2016/Dec/14 ver 5.40 add Label::getAddress() method to get the pointer specified by the label
* 2016/Dec/09 ver 5.34 fix handling of negative offsets when encoding disp8N(thanks to rsdubtso)
* 2016/Dec/08 ver 5.33 fix encoding of vpbroadcast{b,w,d,q}, vpinsr{b,w}, vpextr{b,w} for disp8N
* 2016/Dec/01 ver 5.32 rename __xgetbv() to _xgetbv() to support clang for Visual Studio(thanks to freiro)
* 2016/Nov/27 ver 5.31 rename AVX512_4VNNI to AVX512_4VNNIW
* 2016/Nov/27 ver 5.30 add AVX512_4VNNI, AVX512_4FMAPS instructions(thanks to rsdubtso)
* 2016/Nov/26 ver 5.20 add detection of AVX512_4VNNI and AVX512_4FMAPS(thanks to rsdubtso)
* 2016/Nov/20 ver 5.11 lost vptest for ymm(thanks to gregory38)
* 2016/Nov/20 ver 5.10 add addressing [rip+&var]
* 2016/Sep/29 ver 5.03 fix detection ERR_INVALID_OPMASK_WITH_MEMORY(thanks to PVS-Studio)
* 2016/Aug/15 ver 5.02 xbyak does not include xbyak_bin2hex.h
* 2016/Aug/15 ver 5.011 fix detection of version of gcc 5.4
* 2016/Aug/03 ver 5.01 disable omitted operand
* 2016/Jun/24 ver 5.00 support avx-512 instruction set
* 2016/Jun/13 avx-512 add mask instructions
* 2016/May/05 ver 4.91 add detection of AVX-512 to Xbyak::util::Cpu
* 2016/Mar/14 ver 4.901 comment to ready() function(thanks to skmp)
* 2016/Feb/04 ver 4.90 add jcc(const void *addr);
* 2016/Jan/30 ver 4.89 vpblendvb supports ymm reg(thanks to John Funnell)
* 2016/Jan/24 ver 4.88 lea, cmov supports 16-bit register(thanks to whyisthisfieldhere)
* 2015/Oct/05 ver 4.87 support segment selectors
* 2015/Aug/18 ver 4.86 fix [rip + label] addressing with immediate value(thanks to whyisthisfieldhere)
* 2015/Aug/10 ver 4.85 Address::operator==() is not correct(thanks to inolen)
* 2015/Jun/22 ver 4.84 call() support variadic template if available(thanks to randomstuff)
* 2015/Jun/16 ver 4.83 support movbe(thanks to benvanik)
* 2015/May/24 ver 4.82 support detection of F16C
* 2015/Apr/25 ver 4.81 fix the condition to throw exception for setSize(thanks to whyisthisfieldhere)
* 2015/Apr/22 ver 4.80 rip supports label(thanks to whyisthisfieldhere)
* 2015/Jar/28 ver 4.71 support adcx, adox, cmpxchg, rdseed, stac
* 2014/Oct/14 ver 4.70 support MmapAllocator
* 2014/Jun/13 ver 4.62 disable warning of VC2014
* 2014/May/30 ver 4.61 support bt, bts, btr, btc
* 2014/May/28 ver 4.60 support vcvtph2ps, vcvtps2ph
* 2014/Apr/11 ver 4.52 add detection of rdrand
* 2014/Mar/25 ver 4.51 remove state information of unreferenced labels
* 2014/Mar/16 ver 4.50 support new Label
* 2014/Mar/05 ver 4.40 fix wrong detection of BMI/enhanced rep on VirtualBox
* 2013/Dec/03 ver 4.30 support Reg::cvt8(), cvt16(), cvt32(), cvt64()
* 2013/Oct/16 ver 4.21 label support std::string
* 2013/Jul/30 ver 4.20 [break backward compatibility] split Reg32e class into RegExp(base+index*scale+disp) and Reg32e(means Reg32 or Reg64)
* 2013/Jul/04 ver 4.10 [break backward compatibility] change the type of Xbyak::Error from enum to a class
* 2013/Jun/21 ver 4.02 add putL(LABEL) function to put the address of the label
* 2013/Jun/21 ver 4.01 vpsllw, vpslld, vpsllq, vpsraw, vpsrad, vpsrlw, vpsrld, vpsrlq support (ymm, ymm, xmm). support vpbroadcastb, vpbroadcastw, vpbroadcastd, vpbroadcastq(thanks to Gabest).
* 2013/May/30 ver 4.00 support AVX2, VEX-encoded GPR-instructions
* 2013/Mar/27 ver 3.80 support mov(reg, "label");
* 2013/Mar/13 ver 3.76 add cqo(), jcxz(), jecxz(), jrcxz()
* 2013/Jan/15 ver 3.75 add setSize() to modify generated code
* 2013/Jan/12 ver 3.74 add CodeGenerator::reset() ; add Allocator::useProtect()
* 2013/Jan/06 ver 3.73 use unordered_map if possible
* 2012/Dec/04 ver 3.72 eax, ebx, ... are member variables of CodeGenerator(revert), Xbyak::util::eax, ... are static const.
* 2012/Nov/17 ver 3.71 and_(), or_(), xor_(), not_() are available if XBYAK_NO_OP_NAMES is not defined.
* 2012/Nov/17 change eax, ebx, ptr and so on in CodeGenerator as static member and alias of them are defined in Xbyak::util.
* 2012/Nov/09 ver 3.70 XBYAK_NO_OP_NAMES macro is added to use and_() instead of and() (thanks to Mattias)
* 2012/Nov/01 ver 3.62 add fwait/fnwait/finit/fninit
* 2012/Nov/01 ver 3.61 add fldcw/fstcw
* 2012/May/03 ver 3.60 change interface of Allocator
* 2012/Mar/23 ver 3.51 fix userPtr mode
* 2012/Mar/19 ver 3.50 support AutoGrow mode
* 2011/Nov/09 ver 3.05 fix bit property of rip addresing / support movsxd
* 2011/Aug/15 ver 3.04 fix dealing with imm8 such as add(dword [ebp-8], 0xda); (thanks to lolcat)
* 2011/Jun/16 ver 3.03 fix __GNUC_PREREQ macro for Mac gcc(thanks to t_teruya)
* 2011/Apr/28 ver 3.02 do not use xgetbv on Mac gcc
* 2011/May/24 ver 3.01 fix typo of OSXSAVE
* 2011/May/23 ver 3.00 add vcmpeqps and so on
* 2011/Feb/16 ver 2.994 beta add vmovq for 32-bit mode(I forgot it)
* 2011/Feb/16 ver 2.993 beta remove cvtReg to avoid thread unsafe
* 2011/Feb/10 ver 2.992 beta support one argument syntax for fadd like nasm
* 2011/Feb/07 ver 2.991 beta fix pextrw reg, xmm, imm(Thanks to Gabest)
* 2011/Feb/04 ver 2.99 beta support AVX
* 2010/Dec/08 ver 2.31 fix ptr [rip + 32bit offset], support rdtscp
* 2010/Oct/19 ver 2.30 support pclmulqdq, aesdec, aesdeclast, aesenc, aesenclast, aesimc, aeskeygenassist
* 2010/Jun/07 ver 2.29 fix call(<label>)
* 2010/Jun/17 ver 2.28 move some member functions to public
* 2010/Jun/01 ver 2.27 support encoding of mov(reg64, imm) like yasm(not nasm)
* 2010/May/24 ver 2.26 fix sub(rsp, 1000)
* 2010/Apr/26 ver 2.25 add jc/jnc(I forgot to implement them...)
* 2010/Apr/16 ver 2.24 change the prototype of rewrite() method
* 2010/Apr/15 ver 2.23 fix align() and xbyak_util.h for Mac
* 2010/Feb/16 ver 2.22 fix inLocalLabel()/outLocalLabel()
* 2009/Dec/09 ver 2.21 support cygwin(gcc 4.3.2)
* 2009/Nov/28 support a part of FPU
* 2009/Jun/25 fix mov(qword[rax], imm); (thanks to Martin)
* 2009/Mar/10 fix redundant REX.W prefix on jmp/call reg64
* 2009/Feb/24 add movq reg64, mmx/xmm; movq mmx/xmm, reg64
* 2009/Feb/13 movd(xmm7, dword[eax]) drops 0x66 prefix (thanks to Gabest)
* 2008/Dec/30 fix call in short relative address(thanks to kato san)
* 2008/Sep/18 support @@, @f, @b and localization of label(thanks to nobu-q san)
* 2008/Sep/18 support (ptr[rip + 32bit offset]) (thanks to Dango-Chu san)
* 2008/Jun/03 fix align(). mov(ptr[eax],1) throws ERR_MEM_SIZE_IS_NOT_SPECIFIED.
* 2008/Jun/02 support memory interface allocated by user
* 2008/May/26 fix protect() to avoid invalid setting(thanks to shinichiro_h san)
* 2008/Apr/30 add cmpxchg16b, cdqe
* 2008/Apr/29 support x64
* 2008/Apr/14 code refactoring
* 2008/Mar/12 add bsr/bsf
* 2008/Feb/14 fix output of sub eax, 1234 (thanks to Robert)
* 2007/Nov/5  support lock, xadd, xchg
* 2007/Nov/2  support SSSE3/SSE4 (thanks to Dango-Chu san)
* 2007/Feb/4  fix the bug that exception doesn't occur under the condition which the offset of jmp mnemonic without T_NEAR is over 127.
* 2007/Jan/21 fix the bug to create address like [disp] select smaller representation for mov (eax|ax|al, [disp])
* 2007/Jan/4  first version
