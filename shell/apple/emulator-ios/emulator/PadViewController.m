//
//  PadViewController.m
//  reicast-ios
//
//  Created by Lounge Katt on 8/25/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import "PadViewController.h"
#import "EmulatorViewController.h"

@interface PadViewController ()

@end

@implementation PadViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view from its nib.
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)showController:(UIView *)parentView
{
	[parentView addSubview:self.view];
}

- (void)hideController {
	[self.view removeFromSuperview];
}

- (BOOL)isControllerVisible {
	if (self.view.window != nil) {
		return YES;
	}
	return NO;
}

- (IBAction)keycode:(id)sender
{
	ViewController *emulatorView = [[ViewController alloc] init];
	[emulatorView handleKeycode:(UIButton*)sender];
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
	self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
	if (self) {
		// Custom initialization
	}
	return self;
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
