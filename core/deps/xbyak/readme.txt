
    C++用x86(IA-32), x64(AMD64, x86-64) JITアセンブラ Xbyak 7.23.1

-----------------------------------------------------------------------------
◎概要

これはx86, x64(AMD64, x86-64)のマシン語命令を生成するC++のクラスライブラリです。
プログラム実行時に動的にアセンブルすることが可能です。

-----------------------------------------------------------------------------
◎特徴

・ヘッダファイルオンリー
    xbyak.hをインクルードするだけですぐ利用することができます。
    C++の枠組み内で閉じているため、外部アセンブラは不要です。
    32bit/64bit両対応です。
    対応ニーモニック:特権命令除くx86, MMX/MMX2/SSE/SSE2/SSE3/SSSE3/SSE4/FPU(一部)/AVX/AVX2/FMA/AVX-512/APX/AVX10.2

・Windows Xp(32bit, 64bit), Windows 7/Linux(32bit, 64bit)/Intel Mac対応
    Windows Xp, Windows 7上ではVC2008, VC2010, VC2012
    Linux (kernel 3.8)上ではgcc 4.7.3, clang 3.3
    Intel Mac
    などで動作確認をしています。

※ and, orなどの代わりにand_, or_を使用してください。
and, orなどを使いたい場合は-fno-operator-namesをgcc/clangに指定してください。

-----------------------------------------------------------------------------
◎準備
xbyak.h
xbyak_bin2hex.h
これらを同一のパスに入れてインクルードパスに追加してください。

Linuxではmake installで/usr/local/include/xbyakにコピーされます。
-----------------------------------------------------------------------------
◎下位互換性の破れ
* push byte, immまたはpush word, immが下位8bit, 16bitにキャストした値を使うように変更。
* (Windows) `<winsock2.h>`をincludeしなくなったので必要なら明示的にincludeしてください。
* XBYAK_USE_MMAP_ALLOCATORがデフォルトで有効になりました。従来の方式にする場合はXBYAK_DONT_USE_MMAP_ALLOCATORを定義してください。
* Xbyak::Errorの型をenumからclassに変更
** 従来のenumの値をとるにはintにキャストしてください。
* (古い)Reg32eクラスを(新しい)Reg32eとRegExpに分ける。
** (新しい)Reg32eはReg32かReg64
** (新しい)RegExpは'Reg32e + (Reg32e|Xmm|Ymm) * scale + disp'の型

-----------------------------------------------------------------------------
◎新機能

APX/AVX10.2対応

例外なしモード追加
XBYAK_NO_EXCEPTIONを定義してコンパイルするとgcc/clangで-fno-exceptionsオプションでコンパイルできます。
エラーは例外の代わりに`Xbyak::GetError()`で通達されます。
この値が0でなければ何か問題が発生しています。
この値は自動的に変更されないので`Xbyak::ClearError()`でリセットしてください。
`CodeGenerator::reset()`は`ClearError()`を呼びます。

MmapAllocator追加
これはUnix系OSでのみの仕様です。XBYAK_USE_MMAP_ALLOCATORを使うと利用できます。
デフォルトのAllocatorはメモリ確保時にposix_memalignを使います。
この領域に対するmprotectはmap countを減らします。
map countの最大値は/proc/sys/vm/max_map_countに書かれています。
デフォルトでは3万個ほどのXbyak::CodeGeneratorインスタンスを生成するとエラーになります。
test/mprotect_test.cppで確認できます。
これを避けるためにはmmapを使うMmapAllocatorを使ってください。


AutoGrowモード追加
これはメモリ伸長を動的に行うモードです。
今まではXbyak::CodeGenerator()に渡したメモリサイズを超えると例外が発生して
いましたが、このモードでは内部でメモリを再確保して伸長します。
ただし、getCode()を呼び出す前にジャンプ命令のアドレス解決をするためにready()
関数を呼ぶ必要があります。

次のように使います。

 struct Code : Xbyak::CodeGenerator {
   Code()
     : Xbyak::CodeGenerator(<default memory size>, Xbyak::AutoGrow)
   {
      ...
   }
 };
 Code c;
 c.ready(); // この呼び出しを忘れてはいけない

注意1. ready()を呼んで確定するまではgetCurr()で得たポインタは無効化されている
可能性があります。getSize()でoffsetを保持しておきready()のあとにgetCode()を
呼び出してからgetCode() + offsetで新しいポインタを取得してください。

注意2. AutoGrowモードでは64bitモードの相対アドレッシング[rip]は非サポートです。

-----------------------------------------------------------------------------
◎文法

Xbyak::CodeGeneratorクラスを継承し、そのクラスメソッド内でx86, x64アセンブラを
記述します。そのメソッドを呼び出した後、getCode()メソッドを呼び出し、その戻
り値を自分が使いたい関数ポインタに変換して利用します。アセンブルエラーは例外
により通知されます(cf. main.cpp)。

・基本的にnasmの命令で括弧をつければよいです。

mov eax, ebx  --> mov(eax, ebx);
inc ecx           inc(ecx);
ret           --> ret();

・アドレッシング

(ptr|dword|word|byte) [base + index * (1|2|4|8) + displacement]
                      [rip + 32bit disp] ; x64 only
という形で指定します。サイズを指定する必要がない限りptrを使えばよいです。

セレクター(セグメントレジスタ)をサポートしました。
(注意)セグメントレジスタはOperandを継承していません。

mov eax, [fs:eax]  --> putSeg(fs); mov(eax, ptr [eax]);
mov ax, cs         --> mov(ax, cs);

mov eax, [ebx+ecx] --> mov (eax, ptr[ebx+ecx]);
test byte [esp], 4 --> test (byte [esp], 4);

(注意) dword, word, byteはメンバ変数です。従ってたとえばunsigned intの
つもりでdwordをtypedefしないでください。

・AVX

FMAについては簡略表記を導入するか検討中です(アイデア募集中)。

vaddps(xmm1, xmm2, xmm3); // xmm1 <- xmm2 + xmm3
vaddps(xmm2, xmm3, ptr [rax]); // メモリアクセスはptrで

vfmadd231pd(xmm1, xmm2, xmm3); // xmm1 <- (xmm2 * xmm3) + xmm1

*注意*
デスティネーションの省略形はサポートされなくなりました。

vaddps(xmm2, xmm3); // xmm2 <- xmm2 + xmm3

XBYAK_ENABLE_OMITTED_OPERANDを定義すると使えますが、将来はそれも非サポートになるでしょう。

・AVX-512

vaddpd zmm2, zmm5, zmm30                --> vaddpd(zmm2, zmm5, zmm30);
vaddpd xmm30, xmm20, [rax]              --> vaddpd(xmm30, xmm20, ptr [rax]);
vaddps xmm30, xmm20, [rax]              --> vaddps(xmm30, xmm20, ptr [rax]);
vaddpd zmm2{k5}, zmm4, zmm2             --> vaddpd(zmm2 | k5, zmm4, zmm2);
vaddpd zmm2{k5}{z}, zmm4, zmm2          --> vaddpd(zmm2 | k5 | T_z, zmm4, zmm2);
vaddpd zmm2{k5}{z}, zmm4, zmm2,{rd-sae} --> vaddpd(zmm2 | k5 | T_z, zmm4, zmm2 | T_rd_sae);
                                            vaddpd(zmm2 | k5 | T_z | T_rd_sae, zmm4, zmm2); // the position of `|` is arbitrary.
vcmppd k4{k3}, zmm1, zmm2, {sae}, 5     --> vcmppd(k4 | k3, zmm1, zmm2 | T_sae, 5);

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
vfpclassps k5{k3}, [rax+64]{1to4}, 5    --> vfpclassps(k5|k3, xword_b [rax+64], 5); // broadcast 64-bit to 256-bit

vpdpbusd(xm0, xm1, xm2); // default encoding is EVEX
vpdpbusd(xm0, xm1, xm2, EvexEncoding); // same as the above
vpdpbusd(xm0, xm1, xm2, VexEncoding); // VEX encoding
setDefaultEncoding(VexEncoding); // default encoding is VEX
vpdpbusd(xm0, xm1, xm2); // VEX encoding
注意
* k1, ..., k7 は新しいopmaskレジスタです。
* z, sae, rn-sae, rd-sae, ru-sae, rz-saeの代わりにT_z, T_sae, T_rn_sae, T_rd_sae, T_ru_sae, T_rz_saeを使ってください。
* `k4 | k3`と`k3 | k4`は意味が異なります。
* {1toX}の代わりにptr_bを使ってください。Xは自動的に決まります。
* 一部の命令はメモリサイズを指定するためにxword/yword/zword(_b)を使ってください。
* setDefaultEncoding()でencoding省略時のEVEX/VEXを設定できます。

・ラベル

L(文字列);
で定義します。ジャンプするときはその文字列を指定します。後方参照も可能ですが、
相対アドレスが8ビットに収まらない場合はT_NEARをつけないと実行時に例外が発生
します。
mov(eax, "L2");の様にラベルが表すアドレスをmovの即値として使えます。

・hasUndefinedLabel()を呼び出して真ならジャンプ先が存在しないことを示します。
コードを見直してください。

L("L1");
    jmp ("L1");

    jmp ("L2");
    ...
    少しの命令の場合。
    ...
L("L2");

    jmp ("L3", T_NEAR);
    ...
    沢山の命令がある場合
    ...
L("L3");

<応用編>

1. MASMライクな@@, @f, @bをサポート

  L("@@"); // <A>
  jmp("@b"); // jmp to <A>
  jmp("@f"); // jmp to <B>
  L("@@"); // <B>
  jmp("@b"); // jmp to <B>
  mov(eax, "@b");
  jmp(eax); // jmp to <B>

2. ラベルの局所化

ピリオドで始まるラベルをinLocalLabel(), outLocalLabel()で挟むことで局所化できます。
inLocalLabel(), outLocalLabel()は入れ子にすることができます。

void func1()
{
    inLocalLabel();
   L(".lp"); // <A> ; ローカルラベル
    ...
    jmp(".lp"); // jmpt to <A>
   L("aaa"); // グローバルラベル
    outLocalLabel();
}

void func2()
{
    inLocalLabel();
    L(".lp"); // <B> ; ローカルラベル
    func1();
    jmp(".lp"); // jmp to <B>
    outLocalLabel();
}

上記サンプルではinLocalLabel(), outLocalLabel()が無いと、
".lp"ラベルの二重定義エラーになります。

3. 新しいLabelクラスによるジャンプ命令

ジャンプ先を文字列による指定だけでなくラベルクラスを使えるようになりました。

      Label label1, label2;
    L(label1);
      ...
      jmp(label1);
      ...
      jmp(label2);
      ...
    L(label2);

更にラベルの割り当てを行うassignL(dstLabel, srcLabel)という命令も追加されました。

      Label label2;
    Label label1 = L(); // Label label1; L(label1);と同じ意味
      ...
      jmp(label2);
      ...
      assignL(label2, label1);

上記jmp命令はlabel1にジャンプします。

制限
* srcLabelはL()により飛び先が確定していないといけません。
* dstLabelはL()により飛び先が確定していてはいけません。

ラベルは`getAddress()`によりそのアドレスを取得できます。
未定義のときは0が返ります。
```
// not AutoGrow mode
Label label;
assert(label.getAddress(), 0);
L(label);
assert(label.getAddress(), getCurr());
```

4. farジャンプ

`jmp(mem, T_FAR)`, `call(mem, T_FAR)`, `retf()`をサポートします。
サイズを明示するために`ptr`の代わりに`word|dword|qword`を利用してください。

32bit
```
jmp(word[eax], T_FAR);  // jmp m16:16(FF /5)
jmp(dword[eax], T_FAR); // jmp m16:32(FF /5)
```

64bit
```
jmp(word[rax], T_FAR);  // jmp m16:16(FF /5)
jmp(dword[rax], T_FAR); // jmp m16:32(FF /5)
jmp(qword[rax], T_FAR); // jmp m16:64(REX.W FF /5)
```

・Xbyak::CodeGenerator()コンストラクタインタフェース

@param maxSize [in] コード生成最大サイズ(デフォルト4096byte)
@param userPtr [in] ユーザ指定メモリ

CodeGenerator(size_t maxSize = DEFAULT_MAX_CODE_SIZE, void *userPtr = 0);

デフォルトコードサイズは4096(=DEFAULT_MAX_CODE_SIZE)バイトです。
それより大きなコードを生成する場合はCodeGenerator()のコンストラクタに指定してください。

class Quantize : public Xbyak::CodeGenerator {
public:
    Quantize()
        : CodeGenerator(8192)
    {
    }
    ...
};

またユーザ指定メモリをコード生成最大サイズと共に指定すると、CodeGeneratorは
指定されたメモリ上にバイト列を生成します。

補助関数として指定されたアドレスの実行属性を変更するCodeArray::protect()と
与えられたポインタからアライメントされたポインタを取得するCodeArray::getAlignedAddress()
も用意しました。詳細はsample/test0.cppのuse memory allocated by userを参考に
してください。

/**
    change exec permission of memory
    @param addr [in] buffer address
    @param size [in] buffer size
    @param canExec [in] true(enable to exec), false(disable to exec)
    @return true(success), false(failure)
*/
bool CodeArray::protect(const void *addr, size_t size, bool canExec);

/**
    get aligned memory pointer
*/
uint8 *CodeArray::getAlignedAddress(uint8 *addr, size_t alignedSize = ALIGN_SIZE);

・read/execモード
デフォルトのCodeGeneratorはコンストラクト時にJIT用の領域をread/write/execモードに設定して利用します。
コード生成時はread/writeでコード実行時にはread/execにしたい場合、次のようにしてください。

struct Code : Xbyak::CodeGenerator {
    Code()
        : Xbyak::CodeGenerator(4096, Xbyak::DontUseProtect) // JIT領域をread/writeのままコード生成
    {
        mov(eax, 123);
        ret();
    }
};

Code c;
c.setProtectModeRE(); // read/execモードに変更
// JIT領域を実行

AutoGrowの場合はreadyの代わりにreadyRE()を読んでください。

struct Code : Xbyak::CodeGenerator {
    Code()
        : Xbyak::CodeGenerator(4096, Xbyak::AutoGrow) // JIT領域をread/writeのままコード生成
    {
        mov(eax, 123);
        ret();
    }
};

Code c;
c.readyRE(); // read/exeモードに変更
// JIT領域を実行

setProtectModeRW()を呼ぶと領域が元のread/execモードに戻ります。


その他詳細は各種サンプルを参照してください。
-----------------------------------------------------------------------------
◎マクロ

32bit環境上でコンパイルするとXBYAK32が、64bit環境上でコンパイルするとXBYAK64が
定義されます。さらに64bit環境上ではWindows(VC)ならXBYAK64_WIN、cygwin, gcc上では
XBYAK64_GCCが定義されます。

-----------------------------------------------------------------------------
◎使用例

test0.cpp ; 簡単な例(x86, x64)
quantize.cpp ; 割り算のJITアセンブルによる量子化の高速化(x86)
calc.cpp ; 与えられた多項式をアセンブルして実行(x86, x64)
           boost(http://www.boost.org/)が必要
bf.cpp ; JIT Brainfuck(x86, x64)

-----------------------------------------------------------------------------
◎ライセンス

修正された新しいBSDライセンスに従います。
http://opensource.org/licenses/BSD-3-Clause

sample/{echo,hello}.bfは http://www.kmonos.net/alang/etc/brainfuck.php から
いただきました。

-----------------------------------------------------------------------------
◎履歴

2025/02/07 ver 7.23.1 StackFrame::close()の挙動を元に戻す
2025/02/07 ver 7.23 レジスタサイズのチェック厳密化・16ビット側値の範囲改善・StackFrame::close()の仕様変更。push/pop with APX修正。
2024/11/11 ver 7.22 Reg::cvt{128,256,512}(). xed 2024.11.04でテスト
2024/10/31 ver 7.21 SSE命令のXMMレジスタのチェックを厳密化
2024/10/17 ver 7.20.1 AVX10.2 rev 2.0仕様書の変更に追従
2024/10/15 ver 7.20 setDefaultEncoding/setDefaultEncodingAVX10の仕様確定
2024/10/15 ver 7.11 AVX10.2完全サポート
2024/10/13 ver 7.10 AVX10 integer and fp16 vnni, mediaの新命令対応. setDefaultEncodingの拡張.
2024/10/10 ver 7.09.1 vpcompressbとvpcompresswの名前修正
2024/10/08 ver 7.09 AVX10.2のYMMレジスタの埋め込み丸め対応
2024/10/07 ver 7.08 rdfabaseなどサポート
2024/08/29 ver 7.07.1 xchgの仕様をnasm 2.16.03の挙動に合わせる。
2024/06/11 ver 7.07 xresldtrk/xsusldtrkサポート
2024/03/07 ver 7.06 util::Cpuのキャッシュ判定周りがAMD CPU対応
2024/02/11 ver 7.05.1 util::CpuのextractBit()とautoGrowモードでのalign()の修正
2024/01/03 ver 7.05 APX対応RAO-INT
2023/12/28 ver 7.04 2バイトオペコードのrex2対応
2023/12/26 ver 7.03 dfvのデフォルト値を0に設定
2023/12/20 ver 7.02 SHA*のAPX対応
2023/12/19 ver 7.01 AESKLE, WIDE_KL, KEYLOCKER, KEYLOCKER_WIDE対応 APX10/APX判定対応
2023/12/01 ver 7.00 APX対応
2023/08/07 ver 6.73 sha512/sm3/sm4/avx-vnni-int16追加
2023/08/02 ver 6.72 xabort, xbegin, xend追加
2023/07/27 ver 6.71 Allocatorでhuge pageを考慮する。
2023/07/05 ver 6.70 vpclmulqdqのailas追加
2023/06/27 ver 6.69.2 `TypeT operator|`にconstexpr追加(thanks to Wunkolo)
2023/03/23 ver 6.69.1 xsave判定追加(thanks to Wunkolo)
2023/02/20 ver 6.69 util::CpuがAMD対応 UINTR命令対応
2022/12/07 ver 6.68 prefetchit{0,1}サポート
2022/11/30 ver 6.67 CMPccXADDサポート
2022/11/25 ver 6.66 RAO-INTサポート
2022/11/22 ver 6.65 x32動作確認
2022/11/04 ver 6.64 vmov*命令をmaskつきアドレッシング対応修正
2022/10/06 ver 6.63 AVX-IFMA用のvpmadd52{h,l}uq対応
2022/10/05          amx_fp16/avx_vnni_int8/avx_ne_convertt対応とsetDefaultEncoding()追加
2022/09/15 ver 6.62 serialize追加
2022/08/02 ver 6.61.1 noexceptはVisual Studio 2015以降対応
2022/07/29 ver 6.61 movzx eax, ahがエラーになるのを修正
2022/06/16 ver 6.60.2 GFNI, VAES, VPCLMULQDQの判定修正
2022/06/15 ver 6.60.1 Visual Studio /O0でXbyak::util::Cpuがリンクエラーになるのに対応
2022/06/06 ver 6.60 バージョンのつけ方を数値が戻らないように変更
2022/06/01 ver 6.06 Cpu::TypeクラスのリファクタリングとXBYAK_USE_MEMFDが定義されたときのMmapAllocatorの改善
2022/05/20 ver 6.052 Cpu::operator==()を正しく定義
2022/05/13 ver 6.051 XYBAK_NO_EXCEPTIONを定義したときのCpuクラスのコンパイルエラー修正
2022/05/12 ver 6.05 movdiri, movdir64b, clwb, cldemoteを追加
2022/04/05 ver 6.04 tpause, umonitor, umwaitを追加
2022/03/08 ver 6.03 MmapAllocatorがmemfd用のユーザ定義文字列をサポート
2022/01/28 ver 6.02 dispacementの32bit範囲チェックの厳密化
2021/12/14 ver 6.01 T_FAR jump/callとretfをサポート
2021/09/14 ver 6.00 AVX512-FP16を完全サポート
2021/09/09 ver 5.997 vrndscale*を{sae}をサポートするよう修正
2021/09/03 ver 5.996 v{add,sub,mul,div,max,min}{sd,ss}をT_rd_saeなどをサポートするよう修正
2021/08/15 ver 5.995 Linux上でXBYAK_USE_MEMFDが定義されたなら/proc/self/mapsにラベル追加
2021/06/17 ver 5.994 マスクレジスタ用のvcmpXX{ps,pd,ss,sd}のalias追加
2021/06/06 ver 5.993 gather/scatterのレジスタの組み合わせの厳密なチェック
2021/05/09 ver 5.992 endbr32とendbr64のサポート
2020/11/16 ver 5.991 g++-5のC++14でconstexpr機能の抑制
2020/10/19 ver 5.99 VNNI命令サポート(Thanks to akharito)
2020/10/17 ver 5.98 [scale * reg]のサポート
2020/09/08 ver 5.97 uint32などをuint32_tに置換
2020/08/28 ver 5.95 レジスタクラスのコンストラクタがconstexprに対応(C++14以降)
2020/08/04 ver 5.941 `CodeGenerator::reset()`が`ClearError()`を呼ぶように変更
2020/07/28 ver 5.94 #include <winsock2.h>の削除 (only windows)
2020/07/21 ver 5.93 例外なしモード追加
2020/06/30 ver 5.92 Intel AMX命令サポート (Thanks to nshustrov)
2020/06/19 ver 5.913 32ビット環境でXBYAK64を定義したときのmov(r64, imm64)を修正
2020/06/19 ver 5.912 macOSの古いXcodeでもMAP_JITを有効にする(Thanks to rsdubtso)
2020/05/10 ver 5.911 Linux/macOSでXBYAK_USE_MMAP_ALLOCATORがデフォルト有効になる
2020/04/20 ver 5.91 マスクレジスタk0を受け入れる(マスクをしない)
2020/04/09 ver 5.90 kmov{b,w,d,q}がサポートされないレジスタを受けると例外を投げる
2020/02/26 ver 5.891 zm0のtype修正
2020/01/03 ver 5.89 vfpclasspdの処理エラー修正
2019/12/20 ver 5.88 Windowsでのコンパイルエラー修正
2019/12/19 ver 5.87 未定義ラベルへのjmp命令のデフォルト挙動をT_NEARにするsetDefaultJmpNEAR()を追加
2019/12/13 ver 5.86 [変更] -fno-operator-namesが指定されたときは5.84以前の挙動に戻す
2019/12/07 ver 5.85 mmapにMAP_JITフラグを追加(macOS mojave以上)
2019/11/29 ver 5.84 [変更] XBYAK_USE_OP_NAMESが定義されていない限りXBYAK_NO_OP_NAMESが定義されるように変更
2019/10/12 ver 5.83 exit(1)の除去
2019/09/23 ver 5.82 monitorx, mwaitx, clzero対応 (thanks to MagurosanTeam)
2019/09/14 ver 5.81 いくつかの一般命令をサポート
2019/08/01 ver 5.802 AVX512_BF16判定修正 (thanks to vpirogov)
2019/05/27 support vp2intersectd, vp2intersectq (not tested)
2019/05/26 ver 5.80 support vcvtne2ps2bf16, vcvtneps2bf16, vdpbf16ps
2019/04/27 ver 5.79 vcmppd/vcmppsのptr_b対応忘れ(thanks to jkopinsky)
2019/04/15 ver 5.78 Reg::changeBit()のリファクタリング(thanks to MerryMage)
2019/03/06 ver 5.77 LLCキャッシュを共有数CPU数の修整(by densamoilov)
2019/01/17 ver 5.76 Cpu::getNumCores()追加(by shelleygoel)
2018/10/31 ver 5.751 互換性のためにXbyak::CastToの復元
2018/10/29 ver 5.75 LabelManagerのデストラクタでLabelから参照を切り離す
2018/10/21 ver 5.74 RegRip +/intの形をサポート Xbyak::CastToを削除
2018/10/15 util::StackFrameでmovの代わりにpush/popを使う
2018/09/19 ver 5.73 vpslld, vpslldq, vpsllwなどの(reg, mem, imm8)に対するevexエンコーディング修整
2018/09/19 ver 5.72 fix the encoding of vinsertps for disp8N(Thanks to petercaday)
2018/08/27 ver 5.71 新しいlabelインスタンスを返すL()を追加
2018/08/27 ver 5.70 read/exec設定のためのsetProtectMode()とDontUseProtectの追加
2018/08/24 ver 5.68 indexが16以上のVSIBエンコーディングのバグ修正(thanks to petercaday)
2018/08/14 ver 5.67 Addressクラス内のmutableを削除 ; fix setCacheHierarchy for cloud vm
2018/07/26 ver 5.661 mingw64対応
2018/07/24 ver 5.66 protect()のmodeにCodeArray::PROTECT_REを追加
2018/06/26 ver 5.65 fix push(qword [mem])
2018/03/07 ver 5.64 Cpu()の中でzero divisionが出ることがあるのを修正
2018/02/14 ver 5.63 Cpu::setCacheHierarchy()の修正とclang<3.9のためのEvexModifierZero修正(thanks to mgouicem)
2018/02/13 ver 5.62 Cpu::setCacheHierarchy() by mgouicem and rsdubtso
2018/02/07 ver 5.61 vmov*がmem{k}{z}形式対応(忘れてた)
2018/01/24 ver 5.601 xword, ywordなどをXbyak::util名前空間に追加
2018/01/05 ver 5.60 Ice lake系命令対応(319433-030.pdf)
2017/08/22 ver 5.53 mpxエンコーディングバグ修正, bnd()プレフィクス追加
2017/08/18 ver 5.52 align修正(thanks to MerryMage)
2017/08/17 ver 5.51 multi-byte nop追加 align()はそれを使用する(thanks to inolen)
2017/08/08 ver 5.50 mpx追加(thanks to magurosan)
2017/08/08 ver 5.45 sha追加(thanks to magurosan)
2017/08/08 ver 5.44 prefetchw追加(thanks to rsdubtso)
2017/07/12 ver 5.432 PVS-studioの警告を減らす
2017/07/09 ver 5.431 hasRex()修正 (影響なし) (thanks to drillsar)
2017/05/14 ver 5.43 CodeGenerator::resetSize()修正(thanks to gibbed)
2017/05/13 ver 5.42 movs{b,w,d,q}追加
2017/01/26 ver 5.41 prefetcwt1追加とscale == 0対応(thanks to rsdubtso)
2016/12/14 ver 5.40 Labelが示すアドレスを取得するLabel::getAddress()追加
2016/12/07 ver 5.34 disp8N時の負のオフセット処理の修正(thanks to rsdubtso)
2016/12/06 ver 5.33 disp8N時のvpbroadcast{b,w,d,q}, vpinsr{b,w}, vpextr{b,w}のバグ修正
2016/12/01 ver 5.32 clang for Visual Studioサポートのために__xgetbv()を_xgetbv()に変更(thanks to freiro)
2016/11/27 ver 5.31 AVX512_4VNNIをAVX512_4VNNIWに変更
2016/11/27 ver 5.30 AVX512_4VNNI, AVX512_4FMAPS命令の追加(thanks to rsdubtso)
2016/11/26 ver 5.20 AVX512_4VNNIとAVX512_4FMAPSの判定追加(thanks to rsdubtso)
2016/11/20 ver 5.11 何故か消えていたvptest for ymm追加(thanks to gregory38)
2016/11/20 ver 5.10 [rip+&var]の形のアドレッシング追加
2016/09/29 ver 5.03 ERR_INVALID_OPMASK_WITH_MEMORYの判定ミス修正(thanks to PVS-Studio)
2016/08/15 ver 5.02 xbyak_bin2hex.hをincludeしない
2016/08/15 ver 5.011 gcc 5.4のバージョン取得ミスの修正
2016/08/03 ver 5.01 AVXの省略表記非サポート
2016/07/24 ver 5.00 avx-512フルサポート
2016/06/13 avx-512 opmask命令サポート
2016/05/05 ver 4.91 AVX-512命令の検出サポート
2016/03/14 ver 4.901 ready()関数にコメント加筆(thanks to skmp)
2016/02/04 ver 4.90 条件分岐命令にjcc(const void *addr);のタイプを追加
2016/01/30 ver 4.89 vpblendvbがymmレジスタをサポートしていなかった(thanks to John Funnell)
2016/01/24 ver 4.88 lea, cmovの16bitレジスタ対応(thanks to whyisthisfieldhere)
2015/08/16 ver 4.87 セグメントセレクタに対応
2015/08/16 ver 4.86 [rip + label]アドレッシングで即値を使うと壊れる(thanks to whyisthisfieldhere)
2015/08/10 ver 4.85 Address::operator==()が間違っている(thanks to inolen)
2015/07/22 ver 4.84 call()がvariadic template対応
2015/05/24 ver 4.83 mobveサポート(thanks to benvanik)
2015/05/24 ver 4.82 F16Cが使えるかどうかの判定追加
2015/04/25 ver 4.81 setSizeが例外を投げる条件を修正(thanks to whyisthisfieldhere)
2015/04/22 ver 4.80 rip相対でLabelのサポート(thanks to whyisthisfieldhere)
2015/01/28 ver 4.71 adcx, adox, cmpxchg, rdseed, stacのサポート
2014/10/14 ver 4.70 MmapAllocatorのサポート
2014/06/13 ver 4.62 VC2014で警告抑制
2014/05/30 ver 4.61 bt, bts, btr, btcのサポート
2014/05/28 ver 4.60 vcvtph2ps, vcvtps2phのサポート
2014/04/11 ver 4.52 rdrandの判定追加
2014/03/25 ver 4.51 参照されなくなったラベルの状態を削除する
2014/03/16 ver 4.50 新しいラベルクラスのサポート
2014/03/05 ver 4.40 VirtualBox上でBMI/enhanced repのサポート判定を間違うことがあるのを修正
2013/12/03 ver 4.30 Reg::cvt8(), cvt16(), cvt32()のサポート
2013/10/16 ver 4.21 ラベルでstd::stringを受け付ける。
2013/07/30 ver 4.20 [break backward compatibility] 従来のReg32eクラスをアドレッシング用のRegExpとReg32, Reg64を表すReg32eに分離
2013/07/04 ver 4.10 [break backward compatibility] Xbyak::Errorの型をenumからclassに変更
2013/06/21 ver 4.02 LABELの指すアドレスを書き込むputL(LABEL)関数の追加。
2013/06/21 ver 4.01 vpsllw, vpslld, vpsllq, vpsraw, vpsrad, vpsrlw, vpsrld, vpsrlq support (ymm, ymm, xmm)
                    support vpbroadcastb, vpbroadcastw, vpbroadcastd, vpbroadcastq(thanks to Gabest)
2013/05/30 ver 4.00 AVX2, VEX-encoded GPR-instructionをサポート
2013/03/27 ver 3.80 mov(reg, "label");をサポート
2013/03/13 ver 3.76 cqo, jcxz, jecxz, jrcxz追加
2013/01/15 ver 3.75 生成されたコードを修正するためにsetSize()を追加
2013/01/12 ver 3.74 CodeGenerator::reset()とAllocator::useProtect()を追加
2013/01/06 ver 3.73 可能ならunordered_mapを使う
2012/12/04 ver 3.72 eaxなどをCodeGeneratorのメンバ変数に戻す. Xbyak::util::eaxはstatic const変数
2012/11/17 ver 3.71 and_(), or_(), xor_(), not_()をXBYAK_NO_OP_NAMESが定義されていないときでも使えるようにした
2012/11/17 CodeGeneratorのeax, ecx, ptrなどのメンバ変数をstaticにし、const参照をXbyak::utilにも定義
2012/11/09 ver 3.70 and()をand_()にするためのマクロXBYAK_NO_OP_NAMESを追加(thanks to Mattias)
2012/11/01 ver 3.62 add fwait/fnwait/finit/fninit
2012/11/01 ver 3.61 add fldcw/fstcw
2012/05/03 ver 3.60 Allocatorクラスのインタフェースを変更
2012/03/23 ver 3.51 userPtrモードがバグったのを修正
2012/03/19 ver 3.50 AutoGrowモードサポート
2011/11/09 ver 3.05 rip相対の64bitサイズ以外の扱いのバグ修正 / movsxdサポート
2011/08/15 ver 3.04 add(dword [ebp-8], 0xda);などにおけるimm8の扱いのバグ修正(thanks to lolcat)
2011/06/16 ver 3.03 Macのgcc上での__GNUC_PREREQがミスってたのを修正(thanks to t_teruya)
2011/04/28 ver 3.02 Macのgcc上ではxgetbvをdisable
2011/03/24 ver 3.01 fix typo of OSXSAVE
2011/03/23 ver 3.00  vcmpeqpsなどを追加
2011/02/16 ver 2.994 beta add vmovq for 32-bit mode(I forgot it)
2011/02/16 ver 2.993 beta remove cvtReg to avoid thread unsafe
2011/02/10 ver 2.992 beta support one argument syntax for fadd like nasm
2011/02/07 ver 2.991 beta fix pextrw reg, xmm, imm(Thanks to Gabest)
2011/02/04 ver 2.99 beta support AVX
2010/12/08 ver 2.31 fix ptr [rip + 32bit offset], support rtdscp
2010/10/19 ver 2.30 support pclmulqdq, aesdec, aesdeclast, aesenc, aesenclast, aesimc, aeskeygenassist
2010/07/07 ver 2.29 fix call(<label>)
2010/06/17 ver 2.28 move some member functions to public
2010/06/01 ver 2.27 support encoding of mov(reg64, imm) like yasm(not nasm)
2010/05/24 ver 2.26 fix sub(rsp, 1000)
2010/04/26 ver 2.25 add jc/jnc(I forgot to implement them...)
2010/04/16 ver 2.24 change the prototype of rewrite() method
2010/04/15 ver 2.23 fix align() and xbyak_util.h for Mac
2010/02/16 ver 2.22 fix inLocalLabel()/outLocalLabel()
2009/12/09 ver 2.21 support cygwin(gcc 4.3.2)
2009/11/28 ver 2.20 FPUの一部命令サポート
2009/06/25 ver 2.11 64bitモードでの mov(qword[rax], imm); 修正(thanks to Martinさん)
2009/03/10 ver 2.10 jmp/call reg64の冗長なREG.W削除
2009/02/24 ver 2.09 movq reg64, mmx/xmm; movq mmx/xmm, reg64追加
2009/02/13 ver 2.08 movd(xmm7, dword[eax])が0x66を落とすバグ修正(thanks to Gabestさん)
2008/12/30 ver 2.07 call()の相対アドレスが8bit以下のときのバグ修正(thanks to katoさん)
2008/09/18 ver 2.06 @@, @f, @bとラベルの局所化機能追加(thanks to nobu-qさん)
2008/09/18 ver 2.05 ptr [rip + 32bit offset]サポート(thanks to 団子厨(Dango-Chu)さん)
2008/06/03 ver 2.04 align()のポカミス修正。mov(ptr[eax],1);などをエラーに
2008/06/02 ver 2.03 ユーザ定義メモリインタフェースサポート
2008/05/26 ver 2.02 protect()(on Linux)で不正な設定になることがあるのを修正(thanks to sinichiro_hさん)
2008/04/30 ver 2.01 cmpxchg16b, cdqe追加
2008/04/29 ver 2.00 x86/x64-64版公開
2008/04/25 ver 1.90 x64版β公開
2008/04/18 ver 1.12 コード整理
2008/04/14 ver 1.11 コード整理
2008/03/12 ver 1.10 bsf/bsr追加(忘れていた)
2008/02/14 ver 1.09 sub eax, 1234が16bitモードで出力されていたのを修正(thanks to Robertさん)
2007/11/05 ver 1.08 lock, xadd, xchg追加
2007/11/02 ver 1.07 SSSE3/SSE4対応(thanks to 団子厨(Dango-Chu)さん)
2007/09/25 ver 1.06 call((int)関数ポインタ); jmp((int)関数ポインタ);のサポート
2007/08/04 ver 1.05 細かい修正
2007/02/04 後方へのジャンプでT_NEARをつけないときに8bit相対アドレスに入らない
           場合に例外が発生しないバグの修正
2007/01/21 [disp]の形のアドレス生成のバグ修正
           mov (eax|ax|al, [disp]); mov([disp], eax|ax|al);の短い表現選択
2007/01/17 webページ作成
2007/01/04 公開開始

-----------------------------------------------------------------------------
◎著作権者

光成滋生(MITSUNARI Shigeo, herumi@nifty.com)
