// cheat.c
// generate DC cheat codes from c source

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define GDXCHEAT __attribute__((section("gdx.cheat")))
#define WRITE32(a, b) *((u32*)(a)) = (b)
#define READ8(a) *((u8*)(a))

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
