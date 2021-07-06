// cheat.c
// generate DC cheat codes from c source

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define GDXCHEAT __attribute__((section("gdx.cheat")))
#define READ8(a) *((u8*)(a))
#define READ16(a) *((u16*)(a))
#define READ32(a) *((u32*)(a))
#define WRITE8(a, b) *((u8*)(a)) = (b)
#define WRITE16(a, b) *((u16*)(a)) = (b)
#define WRITE32(a, b) *((u32*)(a)) = (b)

int GDXCHEAT gdx_cheat_disk2_PlShotTurnSet_PlAttackSet(int param1, int param2, char param3) {
    u8 msid = READ8(param1 + 0x1f02);
    u8 wpid = READ8(param1 + 0x1f03);
    switch (msid) {
        case 3: // 旧ザク
        case 4: // ザク
        case 5: // シャアザク
        case 6: // グフ
        case 13: // アッガイ
            if (wpid == 0) return 0;
        case 19: // 陸ガン
        case 20: // 陸GM
            if (wpid == 1) return 0;
    }
    return ((int (*)(int, int, char)) 0x0c06f6f2)(param1, param2, param3);
}

void GDXCHEAT gdx_cheat_disk2_DamageSet(void) {
    // There is difference DC from AC version, but I don't know what has changed.
    // Following addresses are also fix difference.

    /*
    WriteMem16_nommu(0x8c16d9c6, 9);
    WriteMem16_nommu(0x8c16d972, 9);
    WriteMem16_nommu(0x8c16d9a8, 9);
    WriteMem16_nommu(0x8c16d9b4, 9);

    WriteMem32_nommu(0x8c05cf28, this func);
    WriteMem32_nommu(0x8c194958, this func);
    WriteMem32_nommu(0x8c195e04, this func);
     */

    u8 prev_55d[4];
    for (int i = 0; i < 4; ++i) {
        u32 pwork = 0x0c3d1cd4 + i * 0x2000;
        if (pwork[0]) {
            prev_55d[i] = READ8(pwork + 0x055d);
            if (((READ16(pwork + 0x023c) & 8) == 0) && (READ8(pwork + 0x0230) == 0)) {
                WRITE8(pwork + 0x055d, 0);
            } else {
                WRITE8(pwork + 0x055d, 1);
            }
        }
    }

    ((void(*)()) 0x0c16d7d0)();

    for (int i = 0; i < 4; ++i) {
        if (pwork[0]) {
            u32 pwork = 0x0c3d1cd4 + i * 0x2000;
            WRITE8(pwork + 0x055d, prev_55d[i]);
        }
    }
}

