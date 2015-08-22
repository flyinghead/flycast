//
//  VirtualViewController.h
//  
//
//  Created by Lounge Katt on 8/19/15.
//
//

#import <UIKit/UIKit.h>

@interface VirtualViewController : UIViewController

@property (nonatomic, strong) IBOutlet UIButton* img_dpad_l;
@property (nonatomic, strong) IBOutlet UIButton* img_dpad_r;
@property (nonatomic, strong) IBOutlet UIButton* img_dpad_u;
@property (nonatomic, strong) IBOutlet UIButton* img_dpad_d;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_a;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_b;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_x;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_y;
@property (nonatomic, strong) IBOutlet UIButton* img_vjoy;
@property (nonatomic, strong) IBOutlet UIButton* img_lt;
@property (nonatomic, strong) IBOutlet UIButton* img_rt;
@property (nonatomic, strong) IBOutlet UIButton* img_start;

- (void) showController:(UIView *)parentView;
- (void) hideController;
- (BOOL) pollController;

@end
