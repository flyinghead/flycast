# Flycast

[![Android CI](https://github.com/flyinghead/flycast/actions/workflows/android.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/android.yml)
[![C/C++ CI](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml)
[![Nintendo Switch CI](https://github.com/flyinghead/flycast/actions/workflows/switch.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/switch.yml)
[![Windows UWP CI](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml)
[![BSD CI](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml)

<img src="shell/linux/flycast.png" alt="flycast logo" width="150"/>

**Flycast** is a multi-platform Sega Dreamcast, Naomi, Naomi 2, and Atomiswave emulator derived from [**reicast**](https://github.com/skmp/reicast-emulator).

Information about configuration and supported features can be found on [**TheArcadeStriker's flycast wiki**](https://github.com/TheArcadeStriker/flycast-wiki/wiki).

Join us on our [**Discord server**](https://discord.gg/X8YWP8w) for a chat.

## Downloads ![android](https://flyinghead.github.io/flycast-builds/android.jpg) ![windows](https://flyinghead.github.io/flycast-builds/windows.png) ![linux](https://flyinghead.github.io/flycast-builds/ubuntu.png) ![apple](https://flyinghead.github.io/flycast-builds/apple.png) ![switch](https://flyinghead.github.io/flycast-builds/switch.png) ![xbox](https://flyinghead.github.io/flycast-builds/xbox.png)

Get builds for your system from the [**builds page**](https://flyinghead.github.io/flycast-builds/) or [**GitHub Releases**](https://github.com/flyinghead/flycast/releases).

- **Latest master builds:** regular builds from the `master` branch with recent fixes and updates.
- **Nightly dev builds:** experimental builds with the latest features and changes.
- **Stable tagged releases:** versioned release builds published on GitHub Releases.

Automated test results are available from the builds page as well.

## Install

### Android ![android](https://flyinghead.github.io/flycast-builds/android.jpg)

Install Flycast from [**Google Play**](https://play.google.com/store/apps/details?id=com.flycast.emulator).

### Flatpak (Linux ![ubuntu logo](https://flyinghead.github.io/flycast-builds/ubuntu.png))

1. [Set up Flatpak](https://www.flatpak.org/setup/).

2. Install Flycast from [Flathub](https://flathub.org/apps/details/org.flycast.Flycast):

`flatpak install -y org.flycast.Flycast`

3. Run Flycast:

`flatpak run org.flycast.Flycast`

### Homebrew (macOS ![apple logo](https://flyinghead.github.io/flycast-builds/apple.png))

1. [Set up Homebrew](https://brew.sh).

2. Install Flycast via Homebrew:

`brew install --cask flycast`

### iOS

Due to persistent harassment from an iOS user, support for this platform has been dropped.

### Xbox One/Series ![xbox logo](https://flyinghead.github.io/flycast-builds/xbox.png)

Grab the latest build from [**the builds page**](https://flyinghead.github.io/flycast-builds/), or the [**GitHub Actions**](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml). Then install it using the **Xbox Device Portal**.

## Build from source

### macOS

Right-click the bootstrap script and choose **Open**:

`shell/apple/generate_xcode_project.command`

### Windows

Double-click the bootstrap script:

`shell\windows\generate_vs_project.bat`

### Linux

#### Dependencies

- **C/C++ compiler toolchain** (e.g. `gcc`/`g++`)
- **CMake**
- **make**
- **libcurl** (development headers)
- **libudev** (development headers)
- **SDL2** (development headers)
- **Graphics API**: Vulcan, OpenGL

#### Build

```
$ git clone --recursive https://github.com/flyinghead/flycast.git
$ cd flycast
$ mkdir build && cd build
$ cmake ..
$ make
```

## Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/flycast.svg)](https://repology.org/project/flycast/versions)
