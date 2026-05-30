/*
	Copyright 2026 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 * Dinosaur King (China) - card payload codec + MD5 (implementation).
 *
 * Pair with dinokich_card.h. The DES tables, byte-bit conventions and
 * deinterleaver logic mirror the in-game routines verified at
 * 0x8c170458 / 0x8c170888 / 0x8c17049c / 0x8c16e408 / 0x8c16e2da in the
 * Naomi memory dump.
 */

#include "dinokich_card.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace systemsp
{

/* =================================================================== */
/*  Modified DES (matches the Naomi card reader exactly)               */
/*                                                                     */
/*  This is a faithful 1:1 port of the in-game routines                */
/*  (FUN_8c170458/888/49c/696/9a2). It is NOT textbook DES:            */
/*    - all state lives in little-endian 32-bit words, bits addressed  */
/*      MSB-first via the RevPowerOf2 mask (RP(i) = 1<<(31-(i&31)));    */
/*    - C/D and round-key halves are packed in the TOP bits of words;  */
/*    - the PC2 D-half is indexed rp[PC2[i]-28] (a 1-off quirk vs the  */
/*      textbook -29) which is preserved verbatim;                     */
/*    - two S-box nibbles deviate from FIPS-46 (see SBOX below).        */
/*  Verified: decrypt(cipher1,key1) == plain1 byte-for-byte.           */
/*  The permutation tables (IP/FP/E/P/PC1/PC2) are standard FIPS-46.    */
/* =================================================================== */

static const u8 IP[64] = {
    58,50,42,34,26,18,10, 2, 60,52,44,36,28,20,12, 4,
    62,54,46,38,30,22,14, 6, 64,56,48,40,32,24,16, 8,
    57,49,41,33,25,17, 9, 1, 59,51,43,35,27,19,11, 3,
    61,53,45,37,29,21,13, 5, 63,55,47,39,31,23,15, 7
};
static const u8 FP[64] = {
    40, 8,48,16,56,24,64,32, 39, 7,47,15,55,23,63,31,
    38, 6,46,14,54,22,62,30, 37, 5,45,13,53,21,61,29,
    36, 4,44,12,52,20,60,28, 35, 3,43,11,51,19,59,27,
    34, 2,42,10,50,18,58,26, 33, 1,41, 9,49,17,57,25
};
static const u8 E[48] = {
    32, 1, 2, 3, 4, 5,  4, 5, 6, 7, 8, 9,
     8, 9,10,11,12,13, 12,13,14,15,16,17,
    16,17,18,19,20,21, 20,21,22,23,24,25,
    24,25,26,27,28,29, 28,29,30,31,32, 1
};
static const u8 P[32] = {
    16, 7,20,21,29,12,28,17, 1,15,23,26, 5,18,31,10,
     2, 8,24,14,32,27, 3, 9,19,13,30, 6,22,11, 4,25
};
static const u8 PC1[56] = {
    57,49,41,33,25,17, 9,  1,58,50,42,34,26,18,
    10, 2,59,51,43,35,27, 19,11, 3,60,52,44,36,
    63,55,47,39,31,23,15,  7,62,54,46,38,30,22,
    14, 6,61,53,45,37,29, 21,13, 5,28,20,12, 4
};
static const u8 PC2[48] = {
    14,17,11,24, 1, 5, 3,28,15, 6,21,10,
    23,19,12, 4,26, 8,16, 7,27,20,13, 2,
    41,52,31,37,47,55,30,40,51,45,33,48,
    44,49,39,56,34,53,46,42,50,36,29,32
};
static const u8 LSHIFT[16] = { 1,1,2,2, 2,2,2,2, 1,2,2,2, 2,2,2,1 };
/*
 * S-boxes. These are the FIPS-46 boxes EXCEPT for two deliberately tweaked
 * nibbles found in the in-game table at 0x8c1bbeb8 (an anti-cloning measure):
 *
 *   - S2, 6-bit input v=15 (row1,col7): 14 -> 15   (SBOX[1][23], byte @0x8c1bbf07)
 *   - S4, 6-bit input v=43 (row3,col5):  1 -> 10   (SBOX[3][53], byte @0x8c1bbfa3)
 *
 * Because of these, a stock DES library will mis-decrypt any block whose
 * ciphertext exercises those two entries. The deviations are marked <-- below.
 * IP/FP/PC1/PC2/E/P and the rotation schedule are all standard.
 */
static const u8 SBOX[8][64] = {
{14, 4,13, 1, 2,15,11, 8, 3,10, 6,12, 5, 9, 0, 7,
  0,15, 7, 4,14, 2,13, 1,10, 6,12,11, 9, 5, 3, 8,
  4, 1,14, 8,13, 6, 2,11,15,12, 9, 7, 3,10, 5, 0,
 15,12, 8, 2, 4, 9, 1, 7, 5,11, 3,14,10, 0, 6,13},
{15, 1, 8,14, 6,11, 3, 4, 9, 7, 2,13,12, 0, 5,10,
  3,13, 4, 7,15, 2, 8,15,12, 0, 1,10, 6, 9,11, 5,  /* [23]:14->15 <-- */
  0,14, 7,11,10, 4,13, 1, 5, 8,12, 6, 9, 3, 2,15,
 13, 8,10, 1, 3,15, 4, 2,11, 6, 7,12, 0, 5,14, 9},
{10, 0, 9,14, 6, 3,15, 5, 1,13,12, 7,11, 4, 2, 8,
 13, 7, 0, 9, 3, 4, 6,10, 2, 8, 5,14,12,11,15, 1,
 13, 6, 4, 9, 8,15, 3, 0,11, 1, 2,12, 5,10,14, 7,
  1,10,13, 0, 6, 9, 8, 7, 4,15,14, 3,11, 5, 2,12},
{ 7,13,14, 3, 0, 6, 9,10, 1, 2, 8, 5,11,12, 4,15,
 13, 8,11, 5, 6,15, 0, 3, 4, 7, 2,12, 1,10,14, 9,
 10, 6, 9, 0,12,11, 7,13,15, 1, 3,14, 5, 2, 8, 4,
  3,15, 0, 6,10,10,13, 8, 9, 4, 5,11,12, 7, 2,14},  /* [53]:1->10 <-- */
{ 2,12, 4, 1, 7,10,11, 6, 8, 5, 3,15,13, 0,14, 9,
 14,11, 2,12, 4, 7,13, 1, 5, 0,15,10, 3, 9, 8, 6,
  4, 2, 1,11,10,13, 7, 8,15, 9,12, 5, 6, 3, 0,14,
 11, 8,12, 7, 1,14, 2,13, 6,15, 0, 9,10, 4, 5, 3},
{12, 1,10,15, 9, 2, 6, 8, 0,13, 3, 4,14, 7, 5,11,
 10,15, 4, 2, 7,12, 9, 5, 6, 1,13,14, 0,11, 3, 8,
  9,14,15, 5, 2, 8,12, 3, 7, 0, 4,10, 1,13,11, 6,
  4, 3, 2,12, 9, 5,15,10,11,14, 1, 7, 6, 0, 8,13},
{ 4,11, 2,14,15, 0, 8,13, 3,12, 9, 7, 5,10, 6, 1,
 13, 0,11, 7, 4, 9, 1,10,14, 3, 5,12, 2,15, 8, 6,
  1, 4,11,13,12, 3, 7,14,10,15, 6, 8, 0, 5, 9, 2,
  6,11,13, 8, 1, 4,10, 7, 9, 5, 0,15,14, 2, 3,12},
{13, 2, 8, 4, 6,15,11, 1,10, 9, 3,14, 5, 0,12, 7,
  1,15,13, 8,10, 3, 7, 4,12, 5, 6,11, 0,14, 9, 2,
  7,11, 4, 1, 9,12,14, 2, 0, 6,10,13,15, 3, 5, 8,
  2, 1,14, 7, 4,10, 8,13,15,12, 9, 0, 3, 5, 6,11}
};

/* RevPowerOf2 mask: bit i (MSB-first within a 32-bit word), period 32 so the
 * same index works for both halves of a 64-bit/48-bit quantity. */
static inline u32 RP(int i){ return 1u << (31 - (i & 31)); }
/* Wrap-mask for the C/D rotate: top `shift` bits of a word. */
static const u32 DES_WRAPMASK[3] = { 0u, 0x80000000u, 0xc0000000u };

/* little-endian load/store of a 32-bit word (matches the SH-4 in-place ops) */
static inline u32 des_ld(const u8 *b){
    return (u32)b[0] | ((u32)b[1]<<8) | ((u32)b[2]<<16) | ((u32)b[3]<<24);
}
static inline void des_st(u8 *b, u32 w){
    b[0]=(u8)w; b[1]=(u8)(w>>8); b[2]=(u8)(w>>16); b[3]=(u8)(w>>24);
}

/* round keys, 2 words each (top 24 bits used) -- FUN_8c170888 storage */
typedef struct { u32 rk[16][2]; } des_ks_t;

/* FUN_8c1709a2: rotate C,D for this round and emit the round key. */
static void des_ks_round(u32 *C, u32 *D, int round, des_ks_t *ks){
    int s = LSHIFT[round];
    u32 tC = DES_WRAPMASK[s] & *C, tD = DES_WRAPMASK[s] & *D;
    if (s == 1) { tC >>= 27; tD >>= 27; } else { tC >>= 26; tD >>= 26; }
    tC &= 0xfffffff0u; tD &= 0xfffffff0u;
    *C = (*C << s) | tC;
    *D = (*D << s) | tD;
    u32 w0 = 0, w1 = 0;
    for (int i = 0; i < 48; ++i) {
        if (i < 24) { if (RP(PC2[i]-1)  & *C) w0 |= RP(i);      }
        else        { if (RP(PC2[i]-28) & *D) w1 |= RP(i-24);   } /* -28: in-game quirk */
    }
    ks->rk[round][0] = w0;
    ks->rk[round][1] = w1;
}

/* FUN_8c170888: PC-1 then 16 round keys. PC1[0..27]=C half, PC1[28..55]=D half. */
static void des_key_schedule(const u8 key[8], des_ks_t *ks){
    u32 k0 = des_ld(key), k1 = des_ld(key+4), C = 0, D = 0;
    for (int i = 0; i < 28; ++i) {
        u8 v = PC1[i];      u32 s0 = (v < 0x21) ? k0 : k1; if (RP(v-1) & s0) C |= RP(i);
        u8 w = PC1[28+i];   u32 s1 = (w < 0x21) ? k0 : k1; if (RP(w-1) & s1) D |= RP(i);
    }
    for (int r = 0; r < 16; ++r) des_ks_round(&C, &D, r, ks);
}

/* FUN_8c170696: Feistel round, updates (L=*p1, R=*p2). */
static void des_feistel(u32 *p1, u32 *p2, int round, const des_ks_t *ks){
    u32 R = *p2, e0 = 0, e1 = 0;
    for (int i = 0; i < 48; ++i) {
        if (i < 24) { if (RP(E[i]-1) & R) e0 |= RP(i);    }
        else        { if (RP(E[i]-1) & R) e1 |= RP(i-24); }
    }
    e0 ^= ks->rk[round][0];
    e1 ^= ks->rk[round][1];
    u8 g[8];
    g[0]=(e0>>26)&0x3f; g[1]=(e0>>20)&0x3f; g[2]=(e0>>14)&0x3f; g[3]=(e0>>8)&0x3f;
    g[4]=(e1>>26)&0x3f; g[5]=(e1>>20)&0x3f; g[6]=(e1>>14)&0x3f; g[7]=(e1>>8)&0x3f;
    /* SBOX is stored row-major (SBOX[s][row*16+col]); map the raw 6-bit value
     * g = b5b4b3b2b1b0 to (row = b5*2|b0, col = middle 4 bits). */
    u32 out = 0;
    for (int i = 0; i < 8; ++i) {
        int v = g[i], idx = (((v>>4)&2)|(v&1))*16 + ((v>>1)&0xF);
        out = (i < 7) ? ((out | SBOX[i][idx]) << 4) : (out | SBOX[7][idx]);
    }
    u32 per = 0;
    for (int k = 0; k < 32; ++k) if (RP(P[k]-1) & out) per |= RP(k);
    *p2 = per ^ *p1;
    *p1 = R;
}

/* FUN_8c17049c: process one 8-byte block in place. mode 0 = encrypt, 1 = decrypt. */
static void des_block(u8 *io, const des_ks_t *ks, int mode){
    u32 w0 = des_ld(io), w1 = des_ld(io+4), l0 = 0, l1 = 0;
    for (int i = 0; i < 64; ++i) {
        u8 v = IP[i]; u32 src = (v < 0x21) ? w0 : w1;
        if (i < 32) { if (RP(v-1) & src) l0 |= RP(i); }
        else        { if (RP(v-1) & src) l1 |= RP(i); }
    }
    u32 L = l0, R = l1;
    if (mode == 0) for (int r = 0;  r < 16; ++r) des_feistel(&L, &R, r, ks);
    else           for (int r = 15; r >= 0; --r) des_feistel(&L, &R, r, ks);
    u32 t = L; L = R; R = t;                /* final L/R exchange */
    u32 f0 = 0, f1 = 0;
    for (int i = 0; i < 64; ++i) {
        u8 v = FP[i]; u32 src = (v < 0x21) ? L : R;
        if (i < 32) { if (RP(v-1) & src) f0 |= RP(i); }
        else        { if (RP(v-1) & src) f1 |= RP(i); }
    }
    des_st(io, f0); des_st(io+4, f1);
}

void dinokich_des_encrypt(const u8 *plain, size_t len, const u8 key[8], u8 *cipher){
    des_ks_t ks; des_key_schedule(key, &ks);
    memcpy(cipher, plain, len);
    for (size_t i = 0; i < len; i += 8) des_block(cipher + i, &ks, 0);
}
void dinokich_des_decrypt(const u8 *cipher, size_t len, const u8 key[8], u8 *plain){
    des_ks_t ks; des_key_schedule(key, &ks);
    memcpy(plain, cipher, len);
    for (size_t i = 0; i < len; i += 8) des_block(plain + i, &ks, 1);
}

/* =================================================================== */
/*  Odd-digit deinterleaver / interleaver                              */
/* =================================================================== */

static int digit_value(u8 c){ char d[2] = { (char)c, 0 }; return atoi(d); }

void dinokich_deinterleave(const u8 in[64], u8 key_out[8], u8 cipher_out[56])
{
    int outIdx = 0, keyUsed = 0, keyLeft = 8, oddCnt = 0;
    for (int i = 1; i <= 64 - 8; ++i) {
        if (digit_value(in[(8 - keyLeft) + i - 1]) & 1) oddCnt++;
        if ((i & 7) == 0 && (i / 8) > 0) {
            memcpy(cipher_out + outIdx, in + (8 - keyLeft) + outIdx, 8);
            if (oddCnt > keyLeft) oddCnt = keyLeft;
            memcpy(key_out + keyUsed, in + (8 - keyLeft) + i, oddCnt);
            keyUsed += oddCnt;
            keyLeft -= oddCnt;
            outIdx  += 8;
            oddCnt   = 0;
        }
    }
    /* Game's fallback: any unspent key bytes are read from the tail. */
    if (keyLeft > 0)
        memcpy(key_out + keyUsed, in + 64 - keyLeft, keyLeft);
}

void dinokich_interleave(const u8 cipher_in[56], const u8 key_in[8], u8 out[64])
{
    memset(out, 0, 64);
    int outPos = 0, keyUsed = 0, keyLeft = 8;
    for (int chunk = 0; chunk < 7; ++chunk) {
        memcpy(out + outPos, cipher_in + chunk * 8, 8);
        int odd = 0;
        for (int j = 0; j < 8; ++j)
            if (digit_value(cipher_in[chunk * 8 + j]) & 1) odd++;
        if (odd > keyLeft) odd = keyLeft;
        outPos += 8;
        memcpy(out + outPos, key_in + keyUsed, odd);
        outPos  += odd;
        keyUsed += odd;
        keyLeft -= odd;
    }
    /* Game's fallback: leftover key bytes go at the tail. */
    if (keyLeft > 0)
        memcpy(out + 64 - keyLeft, key_in + keyUsed, keyLeft);
}

/* =================================================================== */
/*  High-level codec                                                   */
/* =================================================================== */

void dinokich_decode_card(const u8 card[64], dinokich_plain_t *out, u8 key_out[8])
{
    u8 cipher[56];
    dinokich_deinterleave(card, key_out, cipher);
    dinokich_des_decrypt(cipher, 56, key_out, (u8 *)out);
}

void dinokich_encode_card(const dinokich_plain_t *in, const u8 key[8], u8 card[64])
{
    u8 cipher[56];
    dinokich_des_encrypt((const u8 *)in, 56, key, cipher);
    dinokich_interleave(cipher, key, card);
}

int dinokich_play_credits(u8 card[64], u16 decrement)
{
    dinokich_plain_t p; u8 key[8];
    dinokich_decode_card(card, &p, key);
    if (p.credits < decrement) return -7;
    p.games_played += 1;
    p.prev_credits  = p.credits;
    p.credits      -= decrement;
    dinokich_encode_card(&p, key, card);
    return 0;
}

int dinokich_sum_bytes(const u8 *data, size_t len)
{
    int sum = 0;
    for (size_t i = 0; i < len; ++i) sum += (int8_t)data[i];
    return sum;
}

/* =================================================================== */
/*  hacked MD5 used by dinokich card reader                            */
/* =================================================================== */

#define MD5_F(x,y,z) (((x) & (y)) | ((~x) & (z)))
#define MD5_G(x,y,z) (((x) & (z)) | ((y) & (~z)))
#define MD5_H(x,y,z) ((x) ^ (y) ^ (z))
#define MD5_I(x,y,z) ((y) ^ ((x) | (~z)))
#define MD5_ROL(x,n) (((x) << (n)) | ((x) >> (32 - (n))))

#define MD5_STEP(f,a,b,c,d,x,t,s) \
    (a) += f((b),(c),(d)) + (x) + (t); \
    (a) = MD5_ROL((a), (s)); \
    (a) += (b);

static void md5_transform(u32 state[4], const u8 block[64])
{
    u32 a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0; i < 16; ++i)
        x[i] = (u32)block[i*4] | ((u32)block[i*4+1]<<8) |
               ((u32)block[i*4+2]<<16) | ((u32)block[i*4+3]<<24);

    // This isn't a standard MD5 implementation and it is probably intentional.
    // The shift values of the first round are normally 7, 12, 17, 22 (in this order)
    MD5_STEP(MD5_F,a,b,c,d,x[ 0],0xd76aa478,12);
    MD5_STEP(MD5_F,d,a,b,c,x[ 1],0xe8c7b756, 7);
    MD5_STEP(MD5_F,c,d,a,b,x[ 2],0x242070db,17);
    MD5_STEP(MD5_F,b,c,d,a,x[ 3],0xc1bdceee,22);
    MD5_STEP(MD5_F,a,b,c,d,x[ 4],0xf57c0faf,12);
    MD5_STEP(MD5_F,d,a,b,c,x[ 5],0x4787c62a, 7);
    MD5_STEP(MD5_F,c,d,a,b,x[ 6],0xa8304613,17);
    MD5_STEP(MD5_F,b,c,d,a,x[ 7],0xfd469501,22);
    MD5_STEP(MD5_F,a,b,c,d,x[ 8],0x698098d8,12);
    MD5_STEP(MD5_F,d,a,b,c,x[ 9],0x8b44f7af, 7);
    MD5_STEP(MD5_F,c,d,a,b,x[10],0xffff5bb1,17);
    MD5_STEP(MD5_F,b,c,d,a,x[11],0x895cd7be,22);
    MD5_STEP(MD5_F,a,b,c,d,x[12],0x6b901122,12);
    MD5_STEP(MD5_F,d,a,b,c,x[13],0xfd987193, 7);
    MD5_STEP(MD5_F,c,d,a,b,x[14],0xa679438e,17);
    MD5_STEP(MD5_F,b,c,d,a,x[15],0x49b40821,22);

    MD5_STEP(MD5_G,a,b,c,d,x[ 1],0xf61e2562, 5);
    MD5_STEP(MD5_G,d,a,b,c,x[ 6],0xc040b340, 9);
    MD5_STEP(MD5_G,c,d,a,b,x[11],0x265e5a51,14);
    MD5_STEP(MD5_G,b,c,d,a,x[ 0],0xe9b6c7aa,20);
    MD5_STEP(MD5_G,a,b,c,d,x[ 5],0xd62f105d, 5);
    MD5_STEP(MD5_G,d,a,b,c,x[10],0x02441453, 9);
    MD5_STEP(MD5_G,c,d,a,b,x[15],0xd8a1e681,14);
    MD5_STEP(MD5_G,b,c,d,a,x[ 4],0xe7d3fbc8,20);
    MD5_STEP(MD5_G,a,b,c,d,x[ 9],0x21e1cde6, 5);
    MD5_STEP(MD5_G,d,a,b,c,x[14],0xc33707d6, 9);
    MD5_STEP(MD5_G,c,d,a,b,x[ 3],0xf4d50d87,14);
    MD5_STEP(MD5_G,b,c,d,a,x[ 8],0x455a14ed,20);
    MD5_STEP(MD5_G,a,b,c,d,x[13],0xa9e3e905, 5);
    MD5_STEP(MD5_G,d,a,b,c,x[ 2],0xfcefa3f8, 9);
    MD5_STEP(MD5_G,c,d,a,b,x[ 7],0x676f02d9,14);
    MD5_STEP(MD5_G,b,c,d,a,x[12],0x8d2a4c8a,20);

    MD5_STEP(MD5_H,a,b,c,d,x[ 5],0xfffa3942, 4);
    MD5_STEP(MD5_H,d,a,b,c,x[ 8],0x8771f681,11);
    MD5_STEP(MD5_H,c,d,a,b,x[11],0x6d9d6122,16);
    MD5_STEP(MD5_H,b,c,d,a,x[14],0xfde5380c,23);
    MD5_STEP(MD5_H,a,b,c,d,x[ 1],0xa4beea44, 4);
    MD5_STEP(MD5_H,d,a,b,c,x[ 4],0x4bdecfa9,11);
    MD5_STEP(MD5_H,c,d,a,b,x[ 7],0xf6bb4b60,16);
    MD5_STEP(MD5_H,b,c,d,a,x[10],0xbebfbc70,23);
    MD5_STEP(MD5_H,a,b,c,d,x[13],0x289b7ec6, 4);
    MD5_STEP(MD5_H,d,a,b,c,x[ 0],0xeaa127fa,11);
    MD5_STEP(MD5_H,c,d,a,b,x[ 3],0xd4ef3085,16);
    MD5_STEP(MD5_H,b,c,d,a,x[ 6],0x04881d05,23);
    MD5_STEP(MD5_H,a,b,c,d,x[ 9],0xd9d4d039, 4);
    MD5_STEP(MD5_H,d,a,b,c,x[12],0xe6db99e5,11);
    MD5_STEP(MD5_H,c,d,a,b,x[15],0x1fa27cf8,16);
    MD5_STEP(MD5_H,b,c,d,a,x[ 2],0xc4ac5665,23);

    MD5_STEP(MD5_I,a,b,c,d,x[ 0],0xf4292244, 6);
    MD5_STEP(MD5_I,d,a,b,c,x[ 7],0x432aff97,10);
    MD5_STEP(MD5_I,c,d,a,b,x[14],0xab9423a7,15);
    MD5_STEP(MD5_I,b,c,d,a,x[ 5],0xfc93a039,21);
    MD5_STEP(MD5_I,a,b,c,d,x[12],0x655b59c3, 6);
    MD5_STEP(MD5_I,d,a,b,c,x[ 3],0x8f0ccc92,10);
    MD5_STEP(MD5_I,c,d,a,b,x[10],0xffeff47d,15);
    MD5_STEP(MD5_I,b,c,d,a,x[ 1],0x85845dd1,21);
    MD5_STEP(MD5_I,a,b,c,d,x[ 8],0x6fa87e4f, 6);
    MD5_STEP(MD5_I,d,a,b,c,x[15],0xfe2ce6e0,10);
    MD5_STEP(MD5_I,c,d,a,b,x[ 6],0xa3014314,15);
    MD5_STEP(MD5_I,b,c,d,a,x[13],0x4e0811a1,21);
    MD5_STEP(MD5_I,a,b,c,d,x[ 4],0xf7537e82, 6);
    MD5_STEP(MD5_I,d,a,b,c,x[11],0xbd3af235,10);
    MD5_STEP(MD5_I,c,d,a,b,x[ 2],0x2ad7d2bb,15);
    MD5_STEP(MD5_I,b,c,d,a,x[ 9],0xeb86d391,21);

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

void dinokich_md5_init(dinokich_md5_ctx *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->bit_count = 0;
}

void dinokich_md5_update(dinokich_md5_ctx *ctx, const u8 *data, size_t len)
{
    size_t idx = (size_t)((ctx->bit_count >> 3) & 0x3F);
    ctx->bit_count += (u64)len << 3;
    size_t part = 64 - idx;
    size_t i = 0;
    if (len >= part) {
        memcpy(ctx->buffer + idx, data, part);
        md5_transform(ctx->state, ctx->buffer);
        for (i = part; i + 63 < len; i += 64) md5_transform(ctx->state, data + i);
        idx = 0;
    }
    memcpy(ctx->buffer + idx, data + i, len - i);
}

void dinokich_md5_final(dinokich_md5_ctx *ctx, u8 digest[16])
{
    u8 bits[8];
    for (int i = 0; i < 8; ++i) bits[i] = (u8)(ctx->bit_count >> (i*8));

    size_t idx = (size_t)((ctx->bit_count >> 3) & 0x3F);
    size_t pad_len = idx < 56 ? 56 - idx : 120 - idx;
    static const u8 pad[64] = { 0x80, 0 };
    dinokich_md5_update(ctx, pad, pad_len);
    dinokich_md5_update(ctx, bits, 8);

    for (int i = 0; i < 4; ++i) {
        digest[i*4+0] = (u8)(ctx->state[i]      );
        digest[i*4+1] = (u8)(ctx->state[i] >>  8);
        digest[i*4+2] = (u8)(ctx->state[i] >> 16);
        digest[i*4+3] = (u8)(ctx->state[i] >> 24);
    }
}

void dinokich_md5(const u8 *data, size_t len, u8 digest[16])
{
    dinokich_md5_ctx ctx;
    dinokich_md5_init(&ctx);
    dinokich_md5_update(&ctx, data, len);
    dinokich_md5_final(&ctx, digest);
}

void dinokich_md5_scramble(const u8 salt[2], const u8 *in, size_t len, u8 *out)
{
    u8 digest[16];
    dinokich_md5(salt, 2, digest);
    for (size_t i = 0; i < len; ++i)
    	out[i] = in[i] ^ digest[i % 16];
}

}	// namespace systemsp
