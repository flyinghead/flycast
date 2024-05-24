# Flycast

[![Android CI](https://github.com/flyinghead/flycast/actions/workflows/android.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/android.yml)
[![C/C++ CI](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml)
[![Nintendo Switch CI](https://github.com/flyinghead/flycast/actions/workflows/switch.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/switch.yml)
[![Windows UWP CI](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml)
[![BSD CI](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml)

![flycast logo](https://github.com/flyinghead/flycast/raw/master/shell/linux/flycast.png)

**Flycast** is a multi-platform Sega Dreamcast, Naomi, Naomi 2, and Atomiswave emulator derived from [**reicast**](https://github.com/skmp/reicast-emulator).

Information about configuration and supported features can be found on [**TheArcadeStriker's flycast wiki**](https://github.com/TheArcadeStriker/flycast-wiki/wiki).

Join us on our [**Discord server**](https://discord.gg/X8YWP8w) for a chat. 

## Install

### Flatpak (Linux ![ubuntu logo](https://flyinghead.github.io/flycast-builds/ubuntu.png))

1. [Set up Flatpak](https://www.flatpak.org/setup/).

2. Install Flycast from [Flathub](https://flathub.org/apps/details/org.flycast.Flycast):

`flatpak install -y org.flycast.Flycast`

3. Run Flycast:

`flatpak run org.flycast.Flycast`

### Homebrew (MacOS ![apple logo](https://flyinghead.github.io/flycast-builds/apple.png))

1. [Set up Homebrew](https://brew.sh).

2. Install Flycast via Homebrew:

`brew install --cask flycast`

### Xbox One/Series ![xbox logo](https://flyinghead.github.io/flycast-builds/xbox.png)

Grab the latest build from [**the builds page**](https://flyinghead.github.io/flycast-builds/), or the [**GitHub Actions**](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml). Then install it using the **Xbox Device Portal**.

### Binaries ![android](https://flyinghead.github.io/flycast-builds/android.jpg) ![windows](https://flyinghead.github.io/flycast-builds/windows.png) ![linux](https://flyinghead.github.io/flycast-builds/ubuntu.png) ![apple](https://flyinghead.github.io/flycast-builds/apple.png) ![switch](https://flyinghead.github.io/flycast-builds/switch.png) ![xbox](https://flyinghead.github.io/flycast-builds/xbox.png)

Get fresh builds for your system [**on the builds page**](https://flyinghead.github.io/flycast-builds/).

**New:** Now automated test results are available as well. 

### Build instructions:
```
$ git clone --recursive https://github.com/flyinghead/flycast.git
$ cd flycast
$ mkdir build && cd build
$ cmake ..
$ make
```
