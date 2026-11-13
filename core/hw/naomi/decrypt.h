#ifndef __DECRYPT_STV_H__
#define __DECRYPT_STV_H__

// read: where the chip reads its ciphertext words; the M2 cartridge when null
extern u16 cryptoDecrypt(u16 (*read)(u32 addr) = nullptr);
extern void cryptoReset();
extern void cyptoSetKey(u32 privKey);
extern void cyptoSetLowAddr(u16 val);
extern void cyptoSetHighAddr(u16 val);
extern void cyptoSetSubkey(u16 subKey);

#endif
