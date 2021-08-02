[![Build Status](https://github.com/herumi/xbyak/actions/workflows/main.yml/badge.svg)](https://github.com/herumi/xbyak/actions/workflows/main.yml)

# Xbyak 5.992 ; JIT assembler for x86(IA32), x64(AMD64, x86-64) by C++

## Abstract

Xbyak is a C++ header library that enables dynamically to assemble x86(IA32), x64(AMD64, x86-64) mnemonic.

The pronunciation of Xbyak is `kəi-bja-k`.
It is named from a Japanese word [開闢](https://translate.google.com/?hl=ja&sl=ja&tl=en&text=%E9%96%8B%E9%97%A2&op=translate), which means the beginning of the world.

## Feature
* header file only
* Intel/MASM like syntax
* fully support AVX-512

**Note**:
Use `and_()`, `or_()`, ... instead of `and()`, `or()`.
If you want to use them, then specify `-fno-operator-names` option to gcc/clang.

### News
- vnni instructions such as vpdpbusd supports vex encoding.
- (break backward compatibility) `push(byte, imm)` (resp. `push(word, imm)`) forces to cast `imm` to 8(resp. 16) bit.
- (Windows) `#include <winsock2.h>` has been removed from xbyak.h, so add it explicitly if you need it.
- support exception-less mode see. [Exception-less mode](#exception-less-mode)
- `XBYAK_USE_MMAP_ALLOCATOR` will be defined on Linux/macOS unless `XBYAK_DONT_USE_MMAP_ALLOCATOR` is defined.

### Supported OS

* Windows Xp, Vista, Windows 7, Windows 10(32bit, 64bit)
* Linux(32bit, 64bit)
* Intel macOS

### Supported Compilers

Almost C++03 or later compilers for x86/x64 such as Visual Studio, g++, clang++, Intel C++ compiler and g++ on mingw/cygwin.

## Install

The following files are necessary. Please add the path to your compile directory.

* xbyak.h
* xbyak_mnemonic.h
* xbyak_util.h

Linux:
```
make install
```

These files are copied into `/usr/local/include/xbyak`.

## How to use it

Inherit `Xbyak::CodeGenerator` class and make the class method.
```
#include <xbyak/xbyak.h>

struct Code : Xbyak::CodeGenerator {
    Code(int x)
    {
        mov(eax, x);
        ret();
    }
};
```
Or you can pass the instance of CodeGenerator without inheriting.
```
void genCode(Xbyak::CodeGenerator& code, int x) {
    using namespace Xbyak::util;
    code.mov(eax, x);
    code.ret();
}
```

Make an instance of the class and get the function
pointer by calling `getCode()` and call it.
```
Code c(5);
int (*f)() = c.getCode<int (*)()>();
printf("ret=%d\n", f()); // ret = 5
```

## Syntax
Similar to MASM/NASM syntax with parentheses.

```
NASM              Xbyak
mov eax, ebx  --> mov(eax, ebx);
inc ecx           inc(ecx);
ret           --> ret();
```

## Addressing
Use `qword`, `dword`, `word` and `byte` if it is necessary to specify the size of memory,
otherwise use `ptr`.

```
(ptr|qword|dword|word|byte) [base + index * (1|2|4|8) + displacement]
                            [rip + 32bit disp] ; x64 only

NASM                   Xbyak
mov eax, [ebx+ecx] --> mov(eax, ptr [ebx+ecx]);
mov al, [ebx+ecx]  --> mov(al, ptr [ebx + ecx]);
test byte [esp], 4 --> test(byte [esp], 4);
inc qword [rax]    --> inc(qword [rax]);
```
**Note**: `qword`, ... are member variables, then don't use `dword` as unsigned int type.

### How to use Selector (Segment Register)
```
mov eax, [fs:eax] --> putSeg(fs);
                      mov(eax, ptr [eax]);
mov ax, cs        --> mov(ax, cs);
```
**Note**: Segment class is not derived from `Operand`.

## AVX

```
vaddps(xmm1, xmm2, xmm3); // xmm1 <- xmm2 + xmm3
vaddps(xmm2, xmm3, ptr [rax]); // use ptr to access memory
vgatherdpd(xmm1, ptr [ebp + 256 + xmm2*4], xmm3);
```

**Note**:
If `XBYAK_ENABLE_OMITTED_OPERAND` is defined, then you can use two operand version for backward compatibility.
But the newer version will not support it.
```
vaddps(xmm2, xmm3); // xmm2 <- xmm2 + xmm3
```

## AVX-512

```
vaddpd zmm2, zmm5, zmm30                --> vaddpd(zmm2, zmm5, zmm30);
vaddpd xmm30, xmm20, [rax]              --> vaddpd(xmm30, xmm20, ptr [rax]);
vaddps xmm30, xmm20, [rax]              --> vaddps(xmm30, xmm20, ptr [rax]);
vaddpd zmm2{k5}, zmm4, zmm2             --> vaddpd(zmm2 | k5, zmm4, zmm2);
vaddpd zmm2{k5}{z}, zmm4, zmm2          --> vaddpd(zmm2 | k5 | T_z, zmm4, zmm2);
vaddpd zmm2{k5}{z}, zmm4, zmm2,{rd-sae} --> vaddpd(zmm2 | k5 | T_z, zmm4, zmm2 | T_rd_sae);
                                            vaddpd(zmm2 | k5 | T_z | T_rd_sae, zmm4, zmm2); // the position of `|` is arbitrary.
vcmppd k4{k3}, zmm1, zmm2, {sae}, 5     --> vcmppd(k4 | k3, zmm1, zmm2 | T_sae, 5);

vaddpd xmm1, xmm2, [rax+256]            --> vaddpd(xmm1, xmm2, ptr [rax+256]);
vaddpd xmm1, xmm2, [rax+256]{1to2}      --> vaddpd(xmm1, xmm2, ptr_b [rax+256]);
vaddpd ymm1, ymm2, [rax+256]{1to4}      --> vaddpd(ymm1, ymm2, ptr_b [rax+256]);
vaddpd zmm1, zmm2, [rax+256]{1to8}      --> vaddpd(zmm1, zmm2, ptr_b [rax+256]);
vaddps zmm1, zmm2, [rax+rcx*8+8]{1to16} --> vaddps(zmm1, zmm2, ptr_b [rax+rcx*8+8]);
vmovsd [rax]{k1}, xmm4                  --> vmovsd(ptr [rax] | k1, xmm4);

vcvtpd2dq xmm16, oword [eax+33]         --> vcvtpd2dq(xmm16, xword [eax+33]); // use xword for m128 instead of oword
                                            vcvtpd2dq(xmm16, ptr [eax+33]); // default xword
vcvtpd2dq xmm21, [eax+32]{1to2}         --> vcvtpd2dq(xmm21, ptr_b [eax+32]);
vcvtpd2dq xmm0, yword [eax+33]          --> vcvtpd2dq(xmm0, yword [eax+33]); // use yword for m256
vcvtpd2dq xmm19, [eax+32]{1to4}         --> vcvtpd2dq(xmm19, yword_b [eax+32]); // use yword_b to broadcast

vfpclassps k5{k3}, zword [rax+64], 5    --> vfpclassps(k5|k3, zword [rax+64], 5); // specify m512
vfpclasspd k5{k3}, [rax+64]{1to2}, 5    --> vfpclasspd(k5|k3, xword_b [rax+64], 5); // broadcast 64-bit to 128-bit
vfpclassps k5{k3}, [rax+64]{1to4}, 5    --> vfpclassps(k5|k3, yword_b [rax+64], 5); // broadcast 64-bit to 256-bit

vpdpbusd(xm0, xm1, xm2); // default encoding is EVEX
vpdpbusd(xm0, xm1, xm2, EvexEncoding); // same as the above
vpdpbusd(xm0, xm1, xm2, VexEncoding); // VEX encoding
```
### Remark
* `k1`, ..., `k7` are opmask registers.
  - `k0` is dealt as no mask.
  - e.g. `vmovaps(zmm0|k0, ptr[rax]);` and `vmovaps(zmm0|T_z, ptr[rax]);` are same to `vmovaps(zmm0, ptr[rax]);`.
* use `| T_z`, `| T_sae`, `| T_rn_sae`, `| T_rd_sae`, `| T_ru_sae`, `| T_rz_sae` instead of `,{z}`, `,{sae}`, `,{rn-sae}`, `,{rd-sae}`, `,{ru-sae}`, `,{rz-sae}` respectively.
* `k4 | k3` is different from `k3 | k4`.
* use `ptr_b` for broadcast `{1toX}`. X is automatically determined.
* specify `xword`/`yword`/`zword(_b)` for m128/m256/m512 if necessary.

## Label
Two kinds of Label are supported. (String literal and Label class).

### String literal
```
L("L1");
  jmp("L1");

  jmp("L2");
  ...
  a few mnemonics (8-bit displacement jmp)
  ...
L("L2");

  jmp("L3", T_NEAR);
  ...
  a lot of mnemonics (32-bit displacement jmp)
  ...
L("L3");
```

* Call `hasUndefinedLabel()` to verify your code has no undefined label.
* you can use a label for immediate value of mov like as `mov(eax, "L2")`.

### Support `@@`, `@f`, `@b` like MASM

```
L("@@"); // <A>
  jmp("@b"); // jmp to <A>
  jmp("@f"); // jmp to <B>
L("@@"); // <B>
  jmp("@b"); // jmp to <B>
  mov(eax, "@b");
  jmp(eax); // jmp to <B>
```

### Local label

Label symbols beginning with a period between `inLocalLabel()` and `outLocalLabel()`
are treated as a local label.
`inLocalLabel()` and `outLocalLabel()` can be nested.

```
void func1()
{
    inLocalLabel();
  L(".lp"); // <A> ; local label
    ...
    jmp(".lp"); // jmp to <A>
  L("aaa"); // global label <C>
    outLocalLabel();

    inLocalLabel();
  L(".lp"); // <B> ; local label
    func1();
    jmp(".lp"); // jmp to <B>
    inLocalLabel();
    jmp("aaa"); // jmp to <C>
}
```

### short and long jump
Xbyak deals with jump mnemonics of an undefined label as short jump if no type is specified.
So if the size between jmp and label is larger than 127 byte, then xbyak will cause an error.

```
jmp("short-jmp"); // short jmp
// small code
L("short-jmp");

jmp("long-jmp");
// long code
L("long-jmp"); // throw exception
```
Then specify T_NEAR for jmp.
```
jmp("long-jmp", T_NEAR); // long jmp
// long code
L("long-jmp");
```
Or call `setDefaultJmpNEAR(true);` once, then the default type is set to T_NEAR.
```
jmp("long-jmp"); // long jmp
// long code
L("long-jmp");
```

### Label class

`L()` and `jxx()` support Label class.

```
  Xbyak::Label label1, label2;
L(label1);
  ...
  jmp(label1);
  ...
  jmp(label2);
  ...
L(label2);
```

Use `putL` for jmp table
```
    Label labelTbl, L0, L1, L2;
    mov(rax, labelTbl);
    // rdx is an index of jump table
    jmp(ptr [rax + rdx * sizeof(void*)]);
L(labelTbl);
    putL(L0);
    putL(L1);
    putL(L2);
L(L0);
    ....
L(L1);
    ....
```

`assignL(dstLabel, srcLabel)` binds dstLabel with srcLabel.

```
  Label label2;
  Label label1 = L(); // make label1 ; same to Label label1; L(label1);
  ...
  jmp(label2); // label2 is not determined here
  ...
  assignL(label2, label1); // label2 <- label1
```
The `jmp` in the above code jumps to label1 assigned by `assignL`.

**Note**:
* srcLabel must be used in `L()`.
* dstLabel must not be used in `L()`.

`Label::getAddress()` returns the address specified by the label instance and 0 if not specified.
```
// not AutoGrow mode
Label  label;
assert(label.getAddress() == 0);
L(label);
assert(label.getAddress() == getCurr());
```

### Rip ; relative addressing
```
Label label;
mov(eax, ptr [rip + label]); // eax = 4
...

L(label);
dd(4);
```
```
int x;
...
  mov(eax, ptr[rip + &x]); // throw exception if the difference between &x and current position is larger than 2GiB
```

## Code size
The default max code size is 4096 bytes.
Specify the size in constructor of `CodeGenerator()` if necessary.

```
class Quantize : public Xbyak::CodeGenerator {
public:
  Quantize()
    : CodeGenerator(8192)
  {
  }
  ...
};
```

## User allocated memory

You can make jit code on prepared memory.

Call `setProtectModeRE` yourself to change memory mode if using the prepared memory.

```
uint8_t alignas(4096) buf[8192]; // C++11 or later

struct Code : Xbyak::CodeGenerator {
    Code() : Xbyak::CodeGenerator(sizeof(buf), buf)
    {
        mov(rax, 123);
        ret();
    }
};

int main()
{
    Code c;
    c.setProtectModeRE(); // set memory to Read/Exec
    printf("%d\n", c.getCode<int(*)()>()());
}
```

**Note**: See [sample/test0.cpp](sample/test0.cpp).

### AutoGrow

The memory region for jit is automatically extended if necessary when `AutoGrow` is specified in a constructor of `CodeGenerator`.

Call `ready()` or `readyRE()` before calling `getCode()` to fix jump address.
```
struct Code : Xbyak::CodeGenerator {
  Code()
    : Xbyak::CodeGenerator(<default memory size>, Xbyak::AutoGrow)
  {
     ...
  }
};
Code c;
// generate code for jit
c.ready(); // mode = Read/Write/Exec
```

**Note**:
* Don't use the address returned by `getCurr()` before calling `ready()` because it may be invalid address.

### Read/Exec mode
Xbyak set Read/Write/Exec mode to memory to run jit code.
If you want to use Read/Exec mode for security, then specify `DontSetProtectRWE` for `CodeGenerator` and
call `setProtectModeRE()` after generating jit code.

```
struct Code : Xbyak::CodeGenerator {
    Code()
        : Xbyak::CodeGenerator(4096, Xbyak::DontSetProtectRWE)
    {
        mov(eax, 123);
        ret();
    }
};

Code c;
c.setProtectModeRE();
...

```
Call `readyRE()` instead of `ready()` when using `AutoGrow` mode.
See [protect-re.cpp](sample/protect-re.cpp).

## Exception-less mode
If `XBYAK_NO_EXCEPTION` is defined, then gcc/clang can compile xbyak with `-fno-exceptions`.
In stead of throwing an exception, `Xbyak::GetError()` returns non-zero value (e.g. `ERR_BAD_ADDRESSING`) if there is something wrong.
The status will not be changed automatically, then you should reset it by `Xbyak::ClearError()`.
`CodeGenerator::reset()` calls `ClearError()`.

## Macro

* **XBYAK32** is defined on 32bit.
* **XBYAK64** is defined on 64bit.
* **XBYAK64_WIN** is defined on 64bit Windows(VC).
* **XBYAK64_GCC** is defined on 64bit gcc, cygwin.
* define **XBYAK_USE_OP_NAMES** on gcc with `-fno-operator-names` if you want to use `and()`, ....
* define **XBYAK_ENABLE_OMITTED_OPERAND** if you use omitted destination such as `vaddps(xmm2, xmm3);`(deprecated in the future).
* define **XBYAK_UNDEF_JNL** if Bessel function jnl is defined as macro.
* define **XBYAK_NO_EXCEPTION** for a compiler option `-fno-exceptions`.

## Sample

* [test0.cpp](sample/test0.cpp) ; tiny sample (x86, x64)
* [quantize.cpp](sample/quantize.cpp) ; JIT optimized quantization by fast division (x86 only)
* [calc.cpp](sample/calc.cpp) ; assemble and estimate a given polynomial (x86, x64)
* [bf.cpp](sample/bf.cpp) ; JIT brainfuck (x86, x64)

## License

modified new BSD License
http://opensource.org/licenses/BSD-3-Clause

## History
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

## Author
MITSUNARI Shigeo(herumi@nifty.com)

## Sponsors welcome
[GitHub Sponsor](https://github.com/sponsors/herumi)
