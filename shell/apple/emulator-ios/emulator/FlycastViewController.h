
#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import "iCade-iOS/iCadeReaderView.h"

enum IOSButton {
	IOS_BTN_A = 1,
	IOS_BTN_B = 2,
	IOS_BTN_X = 3,
	IOS_BTN_Y = 4,
	IOS_BTN_UP = 5,
	IOS_BTN_DOWN = 6,
	IOS_BTN_LEFT = 7,
	IOS_BTN_RIGHT = 8,
	IOS_BTN_MENU = 9,
	IOS_BTN_OPTIONS = 10,
	IOS_BTN_HOME = 11,
	IOS_BTN_L1 = 12,
	IOS_BTN_R1 = 13,
	IOS_BTN_L3 = 14,
	IOS_BTN_R3 = 15,
	IOS_BTN_L2 = 16,
	IOS_BTN_R2 = 17,

	IOS_BTN_MAX
};

@interface FlycastViewController : GLKViewController <iCadeEventDelegate>

- (void)handleKeyDown:(enum IOSButton)button;
- (void)handleKeyUp:(enum IOSButton)button;

@end
