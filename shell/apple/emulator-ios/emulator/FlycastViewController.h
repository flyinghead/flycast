
#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import <GameController/GameController.h>
#import "iCade-iOS/iCadeReaderView.h"
#import "PadViewController.h"
#import "EmulatorView.h"

@interface FlycastViewController : GLKViewController <iCadeEventDelegate>

@property (nonatomic) iCadeReaderView* iCadeReader;
@property (nonatomic) GCController *gController __attribute__((weak_import));
@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@property (nonatomic, strong) PadViewController *padController;

@end
