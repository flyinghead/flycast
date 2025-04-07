
# Xbyak 7.23.1 [![Badge Build]][Build Status]

*A JIT assembler for x86/x64 architectures supporting advanced instruction sets up to AVX10.2*

## Menu

- [Install]
- [Usage]
- [Changelog]

## Abstract

Xbyak is a C++ header-only library that enables dynamic assembly of x86/x64 instructions using mnemonics.

The pronunciation of Xbyak is `/kʌɪbjæk/` (kai-byak).

The name is derived from the Japanese word [開闢](https://translate.google.com/?hl=ja&sl=ja&tl=en&text=%E9%96%8B%E9%97%A2&op=translate) (kaibyaku), which means "the beginning of the world" or "creation".

## Features
- Header-only library
- Intel/MASM-like syntax
- Full support for AVX-512, APX, and AVX10.2

**Note**:
Use `and_()`, `or_()`, ... instead of `and()`, `or()`.
If you want to use them, then specify `-fno-operator-names` option to gcc/clang.

### Derived Projects
- [Xbyak_aarch64](https://github.com/fujitsu/xbyak_aarch64/) : for AArch64
- [Xbyak_riscv](https://github.com/herumi/xbyak_riscv) : for RISC-V

### News

- Support AVX10.2
- Support xresldtrk/xsusldtrk
- Support RAO-INT for APX
- Support AVX10 detection, AESKLE, WIDE_KL, KEYLOCKER, KEYLOCKER_WIDE
- Support APX except for a few instructions
- Add amx_fp16/avx_vnni_int8/avx_ne_convert/avx-ifma
- Add movdiri, movdir64b, clwb, cldemote
- WAITPKG instructions (tpause, umonitor, umwait) are supported.
- MmapAllocator supports memfd with user-defined strings. see sample/memfd.cpp
- Strictly check address offset disp32 in a signed 32-bit integer. e.g., `ptr[(void*)0xffffffff]` causes an error.
  - Define `XBYAK_OLD_DISP_CHECK` if you need an old check, but the option will be remoevd.
- Add `jmp(mem, T_FAR)`, `call(mem, T_FAR)` `retf()` for far absolute indirect jump.
- VNNI instructions such as vpdpbusd supports vex encoding.
- (Break backward compatibility) `push(byte, imm)` (resp. `push(word, imm)`) forces to cast `imm` to 8(resp. 16) bit.
- (Windows) `#include <winsock2.h>` has been removed from xbyak.h, so add it explicitly if you need it.
- Support exception-less mode see. [Exception-less mode](#exception-less-mode)
- `XBYAK_USE_MMAP_ALLOCATOR` will be defined on Linux/macOS unless `XBYAK_DONT_USE_MMAP_ALLOCATOR` is defined.

### Supported OS

- Windows (Xp, Vista, 7, 10, 11) (32 / 64 bit)
- Linux (32 / 64 bit)
- macOS (Intel CPU)

### Supported Compilers

Almost C++03 or later compilers for x86/x64 such as Visual Studio, g++, clang++, Intel C++ compiler and g++ on mingw/cygwin.

## License

[BSD-3-Clause License](http://opensource.org/licenses/BSD-3-Clause)

## Author

#### 光成滋生 Mitsunari Shigeo
 [GitHub](https://github.com/herumi) | [Website (Japanese)](http://herumi.in.coocan.jp/) | [herumi@nifty.com](mailto:herumi@nifty.com)

## Sponsors welcome
[GitHub Sponsor](https://github.com/sponsors/herumi)

<!----------------------------------------------------------------------------->

[Badge Build]: https://github.com/herumi/xbyak/actions/workflows/main.yml/badge.svg
[Build Status]: https://github.com/herumi/xbyak/actions/workflows/main.yml

[License]: COPYRIGHT

[Changelog]: doc/changelog.md
[Install]: doc/install.md
[Usage]: doc/usage.md

