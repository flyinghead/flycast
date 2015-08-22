//
//  PathsViewController.m
//  emulator
//
//  Created by Karen Tsai on 2014/3/5.
//  Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.
//

#import "PathsViewController.h"
#import "SWRevealViewController.h"
#import "EmulatorViewController.h"

@interface PathsViewController ()

@end

@implementation PathsViewController

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.title = @"Paths";
    
    // Set the side bar button action. When it's tapped, it'll show up the sidebar.
    _sidebarButton.target = self.revealViewController;
    _sidebarButton.action = @selector(revealToggle:);

    // Set the gesture
    [self.view addGestureRecognizer:self.revealViewController.panGestureRecognizer];
    // Uncomment the following line to preserve selection between presentations.
    // self.clearsSelectionOnViewWillAppear = NO;
 
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
	
	self.diskImages = [[NSMutableArray alloc] init];
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documents = [paths objectAtIndex:0];
	NSArray *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:documents error:NULL];
	NSArray *filters = @[[NSPredicate predicateWithFormat:@"self ENDSWITH '.chd'"],
						 [NSPredicate predicateWithFormat:@"self ENDSWITH '.gdi'"],
						 [NSPredicate predicateWithFormat:@"self ENDSWITH '.cdi'"]];
	NSPredicate *compoundPredicate = [NSCompoundPredicate andPredicateWithSubpredicates:filters];
	self.diskImages = [NSMutableArray arrayWithArray:[files filteredArrayUsingPredicate:compoundPredicate]];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

#pragma mark - Table view data source

-(NSInteger)numberOfSectionsInTableView: (UITableView*)tableView
{
	return 1;
}

-(NSInteger)tableView: (UITableView *)tableView numberOfRowsInSection: (NSInteger)section
{
	return [self.diskImages count];
}

-(NSString*)tableView: (UITableView*)tableView titleForHeaderInSection: (NSInteger)section
{
	return @"";
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath {
	return 160;
	// Assign the specific cell height to prevent issues with custom size
}

-(UITableViewCell*)tableView: (UITableView*)tableView cellForRowAtIndexPath: (NSIndexPath*)indexPath
{
	static NSString *CellIdentifier = @"Cell";
	
	UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier: CellIdentifier];
	if(cell == nil)
	{
		cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier: CellIdentifier];
	}
	
	assert(indexPath.row < [self.diskImages count]);
	NSString* imagePath = [self.diskImages objectAtIndex: indexPath.row];
	
	cell.textLabel.text = [imagePath lastPathComponent];
	
	return cell;
}

-(void)tableView: (UITableView*)tableView didSelectRowAtIndexPath: (NSIndexPath*)indexPath
{
	[self performSegueWithIdentifier: @"emulatorView" sender: self];
}

@end
