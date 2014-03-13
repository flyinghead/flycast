reicast
===========
reicast is a multi-platform Sega Dreamcast emulator.

This is a developer-oriented resource, if you just want bins head over to http://reicast.com/

Caution
-------
The source is a mess, and I need sleep.
We had to discover some interesting arm bugs on the cleaned/refactored branch right after the release.
Expect a much improved changed folder/make structure soon.

Rebranding/forks
----------------
If you are interested into further porting/adapting/whatever, *please* don't fork off. I hate that. Really.

Let's try to keep everything under a single project :)

Building for Android
--------------------
Tools required:
* Latest Android SDK
 - http://developer.android.com/sdk/index.html
* NDK r8b or newer
 - https://developer.android.com/tools/sdk/ndk/index.html
 - If are not using r9c+, comment the "NDK_TOOLCHAIN_VERSION := 4.8" in shell/android/jni/Application.mk and shell/android/xperia/jni/Application.mk
* Android 4.4 (API 19) & Android 2.3.1 (API 9)
 - http://developer.android.com/sdk/installing/adding-packages.html
* Ant
 - http://ant.apache.org/

From project root directory:
```
export ANDROID_NDK=/ # Type the full path to your NDK here

cd shell/android/

android update project -p . --target "android-19"

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

Translations
------------
New and updated translations are always appreciated!
All we ask is that you not use “regional” phrases that may not be generally understood.

Translations can be submitted as a pull request


Beta versions
-------------
Automated Git builds can be found at http://reicast.angelxwind.net/ for iOS and Android.

Donations and stuff
-------------------
Well, glad you liked the project so far!

We're currently short on hardware.

If you would like to donate some devices, get in touch at team@reicast.com. GLES3 capable stuff, some mainstream hardware and rarities would be extremely appreciated.
Keep in mind we're located in Greece for the most part

This has been tested/developed on
* Galaxy tab 7.0 Plus
* LG P990
* Archos 10G9
* Some Chinese tablet
* OUYA
* Various development boards
* GCW Zero

We had to buy all of these, except the GCW Zero and a BeagleBone which were donated (Thanks! You rock!)

Apart from that, we don't accept monetary donations right now.
We also don't plan to be releasing a premium version at any store.
Most of the project has been developed by drk||Raziel (aka, skmp, drk, Raz, etc) but it has been based on the 
works of multiple people. It would be extremely unfair to charge for it and get all the credit :)

We're planning for an indiegogo campaign later on to help with sustained development and running costs, so follow @reicastdc on twitter for updates

Other Testing
-------------
These devices are tested by Karen/angelXwind:
* Apple iPhone 4 GSM Rev0 (N90AP)
* Apple iPhone 4 CDMA (N92AP)
* Apple iPod touch 4 (N81AP)
* Apple iPod touch 3G (N18AP)
* Apple iPhone 3GS (N88AP)
* Sony Xperia X10a (es209ra)
* Amazon Kindle Fire HD 7 (tate-pvt-08)

These devices are tested by contributors regularly:
* Nvidia Shield
* Nexus 5 / 7
* Xperia Play

Team
----

You can check the currently active committers on [the pretty graphs page](https://github.com/reicast/reicast-emulator/graphs/contributors)

Our IRC channel is [#reicast @ chat.freenode.net](irc://chat.freenode.net/reicast). 

The original reicast team consisted of drk||Raziel (mostly just writing code), PsyMan (debugging/testing and everything else) and a little bit of gb_away


Special thanks
--------------
In previous iterations a lot of people have worked on this, notably David Miller (aka, ZeZu), the nullDC team, friends from #pcsx2 and all over the world :)

[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/reicast/reicast-emulator/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

