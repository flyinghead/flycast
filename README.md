reicast
===========
reicast is a sega dreamcast emulator

This is developer oriented resource, if you just want bins head over to http://reicast.com

Caution
-------
The source is a mess, and I need sleep.
We had to discover some interesting arm bugs on the cleaned/refactored branch right after the release.
Expect a much improved changed folder/make structure soon.

Rebranding/forks
----------------
If you are interested into further porting/adapting/whatever, *please* don't fork off. I hate that. Really.

Let's try to keep everything under a single project :)

To build for android
--------------------
Tools required:
* Latest Android SDK and NDK (duh)
* Ant

From project root directory:
```
cd shell\android
android update project -p .

ndk-build -j 4

ant debug
```

Beta versions
-------------------
Compiled test versions are available at recast.loungekatt.com

Donations and stuff
-------------------
Well, glad you liked the project so far!

We're currently short on hardware

If you would like to donate some devices, get in touch at team@reicast.com. GLES3 capable stuff, some mainstream hardware and rarities would be extremely appreciated.
Keep in mind we're located in Greece for the most part

This has been tested/developed on
* Galaxy tab 7.0 Plus
* LG P990
* Archos 10G9
* Some Chinese tablet
* OUYA
* Various development boards
* gcw zero

We had to buy all of these, except the gcw zero and a beaglebone which were donated (thanks! You rock!)

Apart from that, we don't accept monetary donations right now.
We also don't plan to be releasing a premium version at any store.
Most of the project has been developed by drk||Raziel (aka, skmp, drk, Raz, etc) but it has been based on the 
works of multiple people. It would be extremely unfair to charge for it and get all the credit :)


We're planning for an indiegogo campaign later on to help with sustained development and running costs, so follow @reicastdc on twitter for updates


Team
----

You can check the currently active committers on [the pretty graphs page](https://github.com/reicast/reicast-emulator/graphs/contributors)

Chat on freenode #reicast. 

The original reicast team consisted of drk||Raziel (mostly just writing code), PsyMan (debugging/testing and everything else) and a little bit of gb_away


Special thanks
--------------
In previous iterations a lot of people have worked on this, notably David Miller (aka, ZeZu), the nullDC team, friends from #pcsx2 and all over the world :)


[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/reicast/reicast-emulator/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

