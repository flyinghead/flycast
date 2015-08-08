//
//  EmulatorView.m
//  emulator
//
//  Created by admin on 1/18/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import "EmulatorView.h"

#include "types.h"

extern u16 kcode[4];
extern u32 vks[4];
extern s8 joyx[4],joyy[4];
extern u8 rt[4],lt[4];

#define key_CONT_A           (1 << 2)
#define key_CONT_START       (1 << 3)
#define key_CONT_DPAD_LEFT   (1 << 6)

int dpad_or_btn = 0;

@implementation EmulatorView

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect {
    // Drawing code
}
*/

-(void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    
    if (dpad_or_btn &1)
        kcode[0] &= ~(key_CONT_START|key_CONT_A);
    else
        kcode[0] &= ~(key_CONT_DPAD_LEFT);
}

-(void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    
    //	[event allTouches];
    
    if (dpad_or_btn &1)
        kcode[0] |= (key_CONT_START|key_CONT_A);
    else
        kcode[0] |= (key_CONT_DPAD_LEFT);
    
    dpad_or_btn++;
}
@end
