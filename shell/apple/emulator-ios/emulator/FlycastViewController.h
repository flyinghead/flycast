
#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import "iCade-iOS/iCadeReaderView.h"

#include "ios_mouse.h"

@interface FlycastViewController : GLKViewController <iCadeEventDelegate>

- (void)handleKeyDown:(enum IOSButton)button;
- (void)handleKeyUp:(enum IOSButton)button;

@end
