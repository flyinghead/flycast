# Native macOS library (preview)

Flycast's macOS Xcode build includes a SwiftUI library window. It lists the same games and cover art as the existing library, lets you search or add a games folder, and exposes basic video settings. Games open in full screen by default; turn off **Launch games in full screen** in Settings to open them in a window. The native interface follows the macOS language setting for English and Spanish. During a game, press **Command-Shift-B** to close it and return directly to the library. **Command-Shift-M** opens Flycast's in-game menu. Opening a game switches to Flycast's SDL renderer; the full settings panel remains available through **Ajustes → Abrir todos los ajustes de Flycast**.

On macOS 27 with Xcode 27:

```sh
cmake -S . -B build -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DUSE_VULKAN=OFF -DUSE_BREAKPAD=NO \
  -DCMAKE_XCODE_GENERATE_SCHEME=YES
open build/flycast.xcodeproj
```

Select the `flycast` scheme and **My Mac**, then Run. For a command-line build, run:

```sh
xcodebuild -project build/flycast.xcodeproj -scheme flycast \
  -configuration Release -destination 'platform=macOS,arch=arm64' build
open build/Release/Flycast.app
```

The native window requires macOS 13 or newer and an Xcode build. Xcode 27 may prompt to install its Metal Toolchain component; if so, run `xcodebuild -downloadComponent MetalToolchain` once. To build with the original library UI, reconfigure with `-DFLYCAST_MACOS_NATIVE_UI=OFF`.
