/*
	Copyright 2021 flyinghead
	Copyright (c) 2014 Lounge Katt. All rights reserved.

	This file is part of Flycast.

	Flycast is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	Flycast is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
//
//  Created by Lounge Katt on 2/6/14.
//
#import "AppDelegate.h"
#import <AVFoundation/AVFoundation.h>

#include "emulator.h"
#include "log/LogManager.h"
#include "cfg/option.h"
#include "rend/gui.h"

static bool emulatorRunning;

@implementation AppDelegate {
	NSURL *openedURL;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
	// Allow audio playing AND recording
	AVAudioSession *session = [AVAudioSession sharedInstance];
	NSError *error = nil;
	[session setCategory:AVAudioSessionCategoryPlayAndRecord
			 withOptions:AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionDefaultToSpeaker
						| AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionAllowBluetoothA2DP
						| AVAudioSessionCategoryOptionAllowAirPlay
				   error:&error];
	if (error != nil)
		NSLog(@"AVAudioSession.setCategory:  %@", error);
	[session setActive:YES error:&error];
	if (error != nil)
		NSLog(@"AVAudioSession.setActive:  %@", error);

    return YES;
}
							
- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
	emulatorRunning = emu.running();
	if (emulatorRunning)
		emu.stop();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
	if (config::AutoSaveState && !settings.content.path.empty())
		dc_savestate(config::SavestateSlot);
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
	if (emulatorRunning)
	{
		emu.start();
		emulatorRunning = false;
	}
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
	flycast_term();
	LogManager::Shutdown();
}

- (BOOL)application:(UIApplication *)application openURL:(nonnull NSURL *)url options:(nonnull NSDictionary<UIApplicationOpenURLOptionsKey,id> *)options
{
	if (!url.fileURL)
		return false;
	if (openedURL != nil)
	{
		[openedURL stopAccessingSecurityScopedResource];
		openedURL = nil;
	}
	if ([url startAccessingSecurityScopedResource])
		openedURL = url;
	gui_state = GuiState::Closed;
	gui_start_game(url.fileSystemRepresentation);

	return true;
}

@end
