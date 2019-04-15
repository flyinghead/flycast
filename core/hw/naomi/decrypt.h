#ifndef __DECRYPT_STV_H__
#define __DECRYPT_STV_H__

extern u16 cryptoDecrypt();
extern void cryptoReset();
extern void cyptoSetKey(u32 privKey);
extern void cyptoSetLowAddr(u16 val);
extern void cyptoSetHighAddr(u16 val);
extern void cyptoSetSubkey(u16 subKey);

#endif
