/*
    This file is part of Flycast.
    Flycast is free software under the GNU General Public License, version 2
    or (at your option) any later version.
*/
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
void FlycastNativeObserveLibrary(bool enabled);
void FlycastNativeWatchContentPaths(void);
char *FlycastNativeGamesJSON(bool enqueueArtwork);
char *FlycastNativeContentPathsJSON(void);
void FlycastNativeFree(char *value);
void FlycastNativeLaunch(const char *path);
bool FlycastNativeLaunchFullscreen(void);
void FlycastNativeSetLaunchFullscreen(bool enabled);
void FlycastNativeAddContentPath(const char *path);
bool FlycastNativeWidescreen(void);
void FlycastNativeSetWidescreen(bool enabled);
bool FlycastNativeWidescreenHacks(void);
void FlycastNativeSetWidescreenHacks(bool enabled);
int FlycastNativeResolution(void);
void FlycastNativeSetResolution(int height);
void FlycastNativeAdvancedSettings(void);
void FlycastNativeQuit(void);
#ifdef __cplusplus
}
#endif
