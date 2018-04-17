reicast
===========
reicast is a multi-platform Sega Dreamcast emulator.

This is a developer-oriented resource, if you just want bins head over to http://reicast.com/

For development discussion, join #dreamcast in the r/EmuDev slack, https://slofile.com/slack/emudev

Caution
-------
The source is a mess, and dragons might eat your cat when you clone this project. We're working on cleaning things up, but don't hold your breath. Why don't you lend a hand?

Rebranding/(hard)forks
----------------
If you are interested into further porting/adapting/whatever, *please* don't fork off. I hate that. Really.

Let's try to keep everything under a single project :)

Contributing
------------
For small/one-off fixes a PR from a github fork is alright. For longer term collaboration we prefer to use namespaced branches in the form of `<username>/<whatever>` in the main repo. 

Before you work on something major, make sure to check the issue tracker to coordinate with other contributors, and open an issue to get feedback before doing big changes/PRs. It is always polite to check the history of the code you're working on and collaborate with the people that have worked on it. You can introduce yourself in [Meet the team](https://github.com/reicast/reicast-emulator/issues/1113).

Everything goes to master via PRs. Test builds are run automatically for both internal and external PRs, and generally should pass unless there's a really good reason for breakage.  You might want to check our [CLA](https://gist.github.com/skmp/920357e9d3a7733234ade1eb465367cc), which is required to have your changes merged.

If you are looking for somewhere to start, look for issues marked [good first issue](https://github.com/reicast/reicast-emulator/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) or [help wanted](https://github.com/reicast/reicast-emulator/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22)

Building for Android
--------------------
Tools required:
* Latest Android SDK
 - http://developer.android.com/sdk/index.html
* NDK r8b or newer
 - https://developer.android.com/tools/sdk/ndk/index.html
 - If are not using r9c+, comment the "NDK_TOOLCHAIN_VERSION := 4.8" in shell/android/jni/Application.mk and shell/android/xperia/jni/Application.mk
* Android 5.0.1 (API 21) & Android 2.3.1 (API 9)
 - http://developer.android.com/sdk/installing/adding-packages.html
 - note that API 9 is hidden (you must check to show obsolete in SDK manager)
* Ant
 - http://ant.apache.org/

From project root directory:
```
export ANDROID_NDK=/ # Type the full path to your NDK here

cd shell/android/

android update project -p . --target "android-21"

ant debug
```

Building for iOS
----------------
Tools required:
* Latest Xcode
 - https://developer.apple.com/xcode/downloads/
* An iOS device (reicast will not compile for the iOS Simulator)
* iOS 5.x ~ 7.x
* iOSOpenDev if developing without an official Apple certificate
 - http://iosopendev.com/download/

From project root directory:

```
cd shell/ios/

xcodebuild -configuration Release
```

Building for Linux
------------------
Requirements:
* build-essential
* libasound2
* libegl1-mesa-dev
* libgles2-mesa-dev
* libasound2-dev
* mesa-common-dev
* libgl1-mesa-dev

From project root directory:

```
cd shell/linux

make
```


Translations
------------
New and updated translations are always appreciated!
All we ask is that you not use “regional” phrases that may not be generally understood.

Translations can be submitted as a pull request


Development/Beta versions
-------------
| Platform                                           | Status | Downloads
| -------------------------------------------------- | -------------- | ---------
| ![Android](http://i.imgur.com/nK9exQe.jpg) Android | [![Build Status](https://travis-ci.org/reicast/reicast-emulator.svg?branch=master)](https://travis-ci.org/reicast/reicast-emulator) | [Reicast CI Builds](http://builds.reicast.com)
| ![iOS](http://i.imgur.com/6bvAUUj.png) iOS         | [![Build Status](https://app.ship.io/jobs/ttUMMV6QrHOy4_yx/build_status.png)](https://app.ship.io/dashboard#/jobs/9843/history) | *TODO*
| ![Windows](http://i.imgur.com/hAuMmjF.png) Windows | [![Build status](https://ci.appveyor.com/api/projects/status/353mwl73ki74tb58/branch/master?svg=true)](https://ci.appveyor.com/project/skmp/reicast-emulator/branch/master) |  [Reicast CI Builds](http://builds.reicast.com)
| ![Linux](http://i.imgur.com/19aAoQD.png) Linux     | [![wercker status](https://app.wercker.com/status/bcabca642a2de044c6f58203b975878b/s/master "wercker status")](https://app.wercker.com/project/bykey/bcabca642a2de044c6f58203b975878b) | *TODO*
| ![OSX](http://i.imgur.com/0YoI5Vm.png) OSX         | *TODO* | *TODO*


Additional builds (iOS & android) can be found at [angelxwind's](http://reicast.angelxwind.net/) buildbot and [Random Stuff "Daily/Nightly/Testing" PPA](https://launchpad.net/~random-stuff/+archive/ubuntu/ppa) (for Ubuntu).


Donations and stuff
-------------------
Well, glad you liked the project so far!

We're currently short on hardware.

If you would like to donate some devices, get in touch at team@reicast.com.
GLES3 capable stuff, some mainstream hardware and rarities would be extremely
appreciated.
Keep in mind we're located in Greece for the most part

This has been tested/developed on
* Galaxy tab 7.0 Plus
* LG P990
* Archos 10G9
* Some Chinese tablet
* OUYA
* Various development boards
* GCW Zero

We had to buy all of these, except the GCW Zero and a BeagleBone which were
donated (Thanks! You rock!)

Apart from that, we don't accept monetary donations right now.
We also don't plan to be releasing a premium version at any store.
Most of the project has been developed by drk||Raziel (aka, skmp, drk, Raz,
etc) but it has been based on the works of multiple people. It would be
extremely unfair to charge for it and get all the credit :)

We're planning for an indiegogo campaign later on to help with sustained
development and running costs, so follow @reicastdc on twitter for updates

Other Testing
-------------
Devices tested by the reicast team:
* Apple iPhone 4 GSM Rev0 (N90AP)
* Apple iPhone 4 CDMA (N92AP)
* Apple iPod touch 4 (N81AP)
* Apple iPod touch 3G (N18AP)
* Apple iPhone 3GS (N88AP)
* Apple iPhone 5s
* Apple iPad 3
* Sony Xperia X10a (es209ra)
* Amazon Kindle Fire HD 7 (tate-pvt-08)
* Nvidia Shield portable
* Nvidia Shield tablet
* Samsung Galaxy Note 4
* LG Nexus 5
* LG Nexus 5X
* Asus Nexus 7 (2013)

Team
----

You can check the currently active committers on [the pretty graphs page](https://github.com/reicast/reicast-emulator/graphs/contributors)

Our IRC channel is [#reicast @ chat.freenode.net](irc://chat.freenode.net/reicast).

The original reicast team consisted of drk||Raziel (mostly just writing code),
PsyMan (debugging/testing and everything else) and a little bit of gb_away


Special thanks
--------------
In previous iterations a lot of people have worked on this, notably David
Miller (aka, ZeZu), the nullDC team, friends from #pcsx2 and all over the world :)

[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/reicast/reicast-emulator/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

