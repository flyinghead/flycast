//
// from https://github.com/ulwanski/md5
//
#pragma once

typedef unsigned int MD5_u32;

struct MD5_CTX {
	MD5_u32 lo, hi;
	MD5_u32 a, b, c, d;
	unsigned char buffer[64];
	MD5_u32 block[16];
};

void MD5_Init(MD5_CTX *ctx);
void MD5_Update(MD5_CTX *ctx, const void *buf, unsigned long len);
void MD5_Final(unsigned char digest[16], MD5_CTX *ctx);
