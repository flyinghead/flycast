//
//  Created by Lounge Katt on 8/25/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import "PadViewController.h"
#import "EmulatorView.h"

@interface PadViewController ()

@end

@implementation PadViewController

- (void)viewDidLoad {
    [super viewDidLoad];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)showController:(UIView *)parentView
{
	[parentView addSubview:self.view];
}

- (void)hideController
{
	[self.view removeFromSuperview];
}

- (BOOL)isControllerVisible {
	if (self.view.window != nil) {
		return YES;
	}
	return NO;
}

- (void)setControlOutput:(EmulatorView *)output
{
	self.handler = output;
}

- (IBAction)keycodeDown:(id)sender
{
	[self.handler handleKeyDown:(UIButton*)sender];
}

- (IBAction)keycodeUp:(id)sender
{
	[self.handler handleKeyUp:(UIButton*)sender];
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
	self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
	if (self) {
		// Custom initialization
	}
	return self;
}

@end
