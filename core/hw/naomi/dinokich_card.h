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
 * Dinosaur King (China), Love & Berry 3 (China) - card payload codec (DES + odd-digit interleave).
 */
#pragma once
#include "types.h"

namespace systemsp
{

/* ----- 56-byte decrypted plaintext layout ----- */
#pragma pack(push, 1)
typedef struct {
    u8  serial_prefix[4];   /* plain[0x00..0x04] */
    u16 games_played;       /* plain[0x04..0x06]  LE; +=1 each play */
    u16 prev_credits;       /* plain[0x06..0x08]  LE; backup before decrement */
    u8  tag[11];            /* plain[0x08..0x13] */
    u16 credits;            /* plain[0x13..0x15]  LE; play counter, decremented */
    u8  tail[30];           /* plain[0x15..0x33] */
    u8  pad[3];             /* plain[0x33..0x36]  random pad bytes from writer */
    u8  bufout[2];          /* plain[0x36..0x38]  card-check value */
} dinokich_plain_t;
#pragma pack(pop)

/* ----- DES-ECB primitives -----
 * NOTE: this is a MODIFIED DES. Standard FIPS-46 structure (IP/FP/PC1/PC2/E/P
 * + rotation schedule), but the S-box has two tweaked nibbles matching the
 * in-game table at 0x8c1bbeb8 (anti-cloning). A stock DES library will
 * mis-decrypt blocks that exercise those entries. See SBOX in dinokich_card.c.
 */
void dinokich_des_encrypt(const u8 *plain, size_t len,
                          const u8 key[8], u8 *cipher);
void dinokich_des_decrypt(const u8 *cipher, size_t len,
                          const u8 key[8], u8 *plain);

/* ----- Odd-digit interleave / deinterleave -----
 *
 * Splits / joins a 64-byte card block into:
 *   - 8 bytes of DES key   (key)
 *   - 56 bytes of DES cipher (cipher)
 *
 * The game's deinterleaver classifies each ciphertext byte by atoi() of its
 * ASCII character: bytes whose value is an odd digit ('1','3','5','7','9')
 * mark positions where a key byte follows.  The 7 cipher chunks therefore
 * carry 0..N key bytes each.  If the total is less than 8, the leftover key
 * bytes are stored verbatim at the tail of the 64-byte block.
 *
 * Both functions always succeed (the leftover fallback handles all inputs).
 */
void dinokich_deinterleave(const u8 in[64],
                           u8       key_out[8],
                           u8       cipher_out[56]);
void dinokich_interleave(const u8 cipher_in[56],
                         const u8 key_in[8],
                         u8       out[64]);

/* ----- High-level codec ----- */

/* Decode a 64-byte block read from card offset 0x40 into the plaintext
 * struct above. The 8-byte DES key extracted from the block is also returned
 * so the caller can later re-encrypt with the same key.
 */
void dinokich_decode_card(const u8 card[64],
                          dinokich_plain_t *out,
                          u8           key_out[8]);

/* Inverse: encode a plaintext + key into the 64-byte card block. */
void dinokich_encode_card(const dinokich_plain_t *in,
                          const u8           key[8],
                          u8                 card[64]);

/* Replay of the in-game per-play flow (FUN_8c169e7e). Returns 0 on success,
 * -7 if not enough credits. */
int dinokich_play_credits(u8 card[64], u16 decrement);

/* Sum of bytes (signed-byte accumulator, returned as int). Used by the
 * "extended verification" path at initSmartCardReader step 7.
 */
int dinokich_sum_bytes(const u8 *data, size_t len);

/* ----- MD5 (non-standard) - used by the UART scrambling protocol ----- */
typedef struct {
    u32 state[4];
    u64 bit_count;
    u8  buffer[64];
} dinokich_md5_ctx;

void dinokich_md5_init(dinokich_md5_ctx *ctx);
void dinokich_md5_update(dinokich_md5_ctx *ctx, const u8 *data, size_t len);
void dinokich_md5_final(dinokich_md5_ctx *ctx, u8 digest[16]);
void dinokich_md5(const u8 *data, size_t len, u8 digest[16]);

/* The protocol's payload obfuscation: cycles a 16-byte digest derived from
 * the 2-byte salt across `len` bytes and XORs in place / into out.
 */
void dinokich_md5_scramble(const u8 salt[2],
                           const u8 *in, size_t len,
                           u8 *out);

}	// namespace systemsp
