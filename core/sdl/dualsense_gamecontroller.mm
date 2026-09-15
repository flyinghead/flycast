/*
    This file is part of Flycast.
    Flycast is free software under the GNU General Public License, version 2
    or (at your option) any later version.
*/
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
#include "dualsense_gamecontroller.h"

@interface FlycastDualSenseNativeOutput : NSObject
@property (nonatomic, strong) GCController *controller;
@property (nonatomic, strong) CHHapticEngine *engine;
@property (nonatomic, strong) id<CHHapticPatternPlayer> player;
@property (nonatomic) BOOL driving;
@property (nonatomic, strong) NSDate *lastApply;
- (instancetype)initWithController:(GCController *)controller;
- (void)setDriving:(BOOL)enabled;
- (BOOL)rumble:(float)intensity duration:(uint32_t)durationMs;
- (void)update;
- (void)close;
@end

@implementation FlycastDualSenseNativeOutput
- (instancetype)initWithController:(GCController *)controller
{
    self = [super init];
    if (self) {
        _controller = controller;
        _engine = [controller.haptics createEngineWithLocality:GCHapticsLocalityDefault];
        _engine.autoShutdownEnabled = NO;
    }
    return self;
}

- (void)applyDrivingProfile
{
    GCDualSenseGamepad *gamepad = (GCDualSenseGamepad *)self.controller.extendedGamepad;
    if (![gamepad isKindOfClass:GCDualSenseGamepad.class])
        return;
    if (self.driving) {
        [gamepad.leftTrigger setModeSlopeFeedbackWithStartPosition:0.05f
            endPosition:0.95f startStrength:0.20f endStrength:0.90f];
        [gamepad.rightTrigger setModeSlopeFeedbackWithStartPosition:0.10f
            endPosition:0.95f startStrength:0.05f endStrength:0.55f];
    } else {
        [gamepad.leftTrigger setModeOff];
        [gamepad.rightTrigger setModeOff];
    }
    self.lastApply = [NSDate date];
}

- (void)setDriving:(BOOL)enabled
{
    if (self.driving == enabled)
        return;
    self.driving = enabled;
    [self applyDrivingProfile];
}

- (BOOL)rumble:(float)intensity duration:(uint32_t)durationMs
{
    if (!self.engine)
        return NO;
    NSError *error = nil;
    [self.player stopAtTime:0 error:nil];
    self.player = nil;
    if (intensity <= 0 || durationMs == 0)
        return YES;
    if (![self.engine startAndReturnError:&error])
        return NO;
    CHHapticEventParameter *gain = [[CHHapticEventParameter alloc]
        initWithParameterID:CHHapticEventParameterIDHapticIntensity value:intensity];
    CHHapticEvent *event = [[CHHapticEvent alloc]
        initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[gain]
        relativeTime:0 duration:durationMs / 1000.0];
    CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithEvents:@[event]
        parameters:@[] error:&error];
    if (!pattern)
        return NO;
    self.player = [self.engine createPlayerWithPattern:pattern error:&error];
    return self.player && [self.player startAtTime:0 error:&error];
}

- (void)update
{
    // Reapply only when the effect is active. Some controllers reset output
    // after a focus or transport change while SDL continues reading input.
    if (self.driving && [self.lastApply timeIntervalSinceNow] < -0.5)
        [self applyDrivingProfile];
}

- (void)close
{
    self.driving = NO;
    [self applyDrivingProfile];
    [self.player stopAtTime:0 error:nil];
    [self.engine stopWithCompletionHandler:nil];
}
@end

DualSenseGameControllerOutput::DualSenseGameControllerOutput() = default;

DualSenseGameControllerOutput::~DualSenseGameControllerOutput()
{
    if (native) {
        FlycastDualSenseNativeOutput *output = (__bridge FlycastDualSenseNativeOutput *)native;
        [output close];
        CFRelease(native);
    }
}

bool DualSenseGameControllerOutput::connect()
{
    for (GCController *controller in [GCController controllers]) {
        if (![controller.extendedGamepad isKindOfClass:GCDualSenseGamepad.class])
            continue;
        FlycastDualSenseNativeOutput *output =
            [[FlycastDualSenseNativeOutput alloc] initWithController:controller];
        native = (__bridge_retained void *)output;
        return true;
    }
    return false;
}

void DualSenseGameControllerOutput::setDrivingProfile(bool enabled)
{
    if (native)
        [(__bridge FlycastDualSenseNativeOutput *)native setDriving:enabled];
}

bool DualSenseGameControllerOutput::setRumble(float intensity, uint32_t durationMs)
{
    if (!native)
        return false;
    return [(__bridge FlycastDualSenseNativeOutput *)native rumble:intensity duration:durationMs];
}

void DualSenseGameControllerOutput::update()
{
    if (native)
        [(__bridge FlycastDualSenseNativeOutput *)native update];
}
