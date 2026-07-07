#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "$script_dir/../../"

MOLTENVK_TAG="v1.4.2-pre.973bb08"
MOLTENVK_ROOT="$PWD/build/deps/MoltenVK-${MOLTENVK_TAG}"

moltenvk_root=""
moltenvk_dylib=""

install_homebrew() {
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    if [ -x /opt/homebrew/bin/brew ]; then
        echo >> ~/.zprofile
        echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/opt/homebrew/bin/brew shellenv)"
    else
        echo >> ~/.zprofile
        echo 'eval "$(/usr/local/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/usr/local/bin/brew shellenv)"
    fi
}

ensure_homebrew() {
    if command -v brew >/dev/null 2>&1; then
        return
    fi

    read -p $'\033[1mHomebrew is required to install missing build tools.\033[0m Type \033[1;32my\033[0m to install it now: ' b
    if [ "$b" = "y" ]; then
        install_homebrew
    else
        exit 1
    fi
}

init_submodules() {
    if [ -d .git ]; then
        if git rev-parse --verify HEAD >/dev/null 2>&1; then
            git submodule update --init --recursive
        else
            git fetch --depth 1 origin master
            git reset --hard FETCH_HEAD
            git submodule update --init --recursive
        fi
    else
        git init
        git remote add origin https://github.com/flyinghead/flycast.git
        git fetch --depth 1 origin master
        git reset --hard FETCH_HEAD
        git submodule update --init --recursive
    fi
}

find_molten_vk() {
    local candidate=""

    if [ ! -d "$MOLTENVK_ROOT" ]; then
        return 1
    fi

    while IFS= read -r candidate; do
        case "$candidate" in
            */macOS/*|*/macos-*) ;;
            *) continue ;;
        esac

        moltenvk_root="$MOLTENVK_ROOT"
        moltenvk_dylib="$candidate"
        return 0
    done < <(/usr/bin/find "$MOLTENVK_ROOT" -type f -name libMoltenVK.dylib)

    return 1
}

download_molten_vk() {
    local extract_tmp="$MOLTENVK_ROOT.tmp"
    local archive="$PWD/build/deps/MoltenVK-macos-${MOLTENVK_TAG}.tar"
    local url="https://github.com/vkedwardli/MoltenVK/releases/download/${MOLTENVK_TAG}/MoltenVK-macos.tar"

    mkdir -p "$(dirname "$archive")"
    curl -fL "$url" -o "$archive"
    rm -rf "$extract_tmp"
    mkdir -p "$extract_tmp"
    tar -xf "$archive" -C "$extract_tmp"
    rm -rf "$MOLTENVK_ROOT"
    mv "$extract_tmp" "$MOLTENVK_ROOT"

    if ! find_molten_vk; then
        echo "MoltenVK ${MOLTENVK_TAG} was downloaded, but macOS libMoltenVK.dylib was not found." >&2
        exit 1
    fi
}

if ! command -v cmake >/dev/null 2>&1; then
    read -p $'\033[1mCMake is required.\033[0m Type \033[1;32my\033[0m to install it with Homebrew: ' c
    if [ "$c" = "y" ]; then
        ensure_homebrew
        brew install cmake
    else
        exit 1
    fi
fi

xcode_app="$(find /Applications -maxdepth 1 -type d -name 'Xcode*.app' | head -n 1)"
if [ -z "$xcode_app" ]; then
    case "$(sw_vers -productVersion)" in
        26.[2-9]*|26.[1-9][0-9]*|2[7-9].*|[3-9][0-9].*)
            xcode_version="latest"
            ;;
        15.[6-9]*|15.[1-9][0-9]*)
            xcode_version="26.3"
            ;;
        15.[3-5]*)
            xcode_version="16.4"
            ;;
        15.2*)
            xcode_version="16.3"
            ;;
        14.[5-9]*|14.[1-9][0-9]*)
            xcode_version="16.2"
            ;;
        14.*)
            xcode_version="15.4"
            ;;
        13.[5-9]*|13.[1-9][0-9]*)
            xcode_version="15.1"
            ;;
        13.*)
            xcode_version="14.2"
            ;;
        12.[5-9]*|12.[1-9][0-9]*)
            xcode_version="14.2"
            ;;
        12.*)
            xcode_version="13.1"
            ;;
        11.[3-9]*|11.[1-9][0-9]*)
            xcode_version="13.1"
            ;;
        10.15.[4-9]*|10.15.[1-9][0-9]*|10.15)
            xcode_version="12.5"
            ;;
        *)
            echo 'This macOS version is too old to build Flycast.' >&2
            exit 1
            ;;
    esac
    echo $'\033[1mFull Xcode is required.\033[0m Generate an Xcode project with the full Xcode app, not Command Line Tools.' >&2
    echo 'Install Xcode from the Mac App Store: https://apps.apple.com/us/app/xcode/id497799835' >&2
    if [ "$xcode_version" = "latest" ]; then
        read -p $'Or type \033[1;32my\033[0m to install the latest Xcode with Homebrew (you will be prompted to sign in with your Apple account): ' c
    else
        read -p $'Or type \033[1;32my\033[0m to install Xcode '"$xcode_version"$' with Homebrew (you will be prompted to sign in with your Apple account): ' c
    fi
    if [ "$c" = "y" ]; then
        ensure_homebrew
        brew install xcodes
        if [ "$xcode_version" = "latest" ]; then
            xcodes install --latest --no-superuser
        else
            xcodes install "$xcode_version" --no-superuser
        fi
        xcode_app="$(find /Applications -maxdepth 1 -type d -name 'Xcode*.app' | head -n 1)"
    else
        exit 1
    fi
fi

if [ "$(xcode-select -p 2>/dev/null || true)" != "$xcode_app/Contents/Developer" ]; then
    sudo xcode-select -s "$xcode_app/Contents/Developer"
    sudo xcodebuild -runFirstLaunch
fi

if [ ! -f core/deps/glslang/CMakeLists.txt ]; then
    echo 'Git submodules are missing.' >&2
    read -p $'Type \033[1;32my\033[0m to initialize git and fetch submodules recursively (ZIP checkout files will be reset to the upstream master snapshot): ' g
    if [ "$g" = "y" ]; then
        init_submodules
    else
        exit 1
    fi
fi

echo "1) macOS ($(uname -m))"
echo "2) iOS"
read -p "Choose your target platform: " x

if [ "${x:-1}" = "2" ]; then
    if [ "$(uname -m)" = "arm64" ]; then
        option="-DCMAKE_SYSTEM_NAME=iOS"
    else
        option="-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
    fi
    lldbinitfolder="emulator-ios"
    echo 'Building iOS xcodeproj for debugging'
    echo 'Remove CODE_SIGNING_ALLOWED=NO in Build Settings if you are using your Apple Developer Certificate for signing'
else
    if find_molten_vk; then
        echo "Using MoltenVK from $moltenvk_root"
        use_vulkan="ON"
    else
        echo $'\033[1mMoltenVK is required for Vulkan.\033[0m'
        echo "Download URL: https://github.com/vkedwardli/MoltenVK/releases/download/${MOLTENVK_TAG}/MoltenVK-macos.tar"
        read -p $'Type \033[1;32my\033[0m to download MoltenVK from the configured release URL: ' v
        if [ "$v" = "y" ]; then
            download_molten_vk
            echo "Using MoltenVK from $moltenvk_root"
            use_vulkan="ON"
        else
            echo 'Vulkan will be disabled' >&2
            use_vulkan="OFF"
        fi
    fi

    option="-DCMAKE_OSX_ARCHITECTURES=$(uname -m) -DUSE_VULKAN=$use_vulkan"
    if [ "$use_vulkan" = "ON" ]; then
        option+=" -DMOLTENVK_DYLIB=$moltenvk_dylib"
    fi
    lldbinitfolder="emulator-osx"
    echo 'Building macOS xcodeproj for debugging'
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release $option -DUSE_BREAKPAD=NO -DCMAKE_XCODE_GENERATE_SCHEME=YES -G "Xcode"

nl=$'\n'
/usr/bin/sed -i '' -E "s/launchStyle/customLLDBInitFile = \"\$(SRCROOT)\/shell\/apple\/\\${lldbinitfolder}\/LLDBInitFile\"\\${nl}launchStyle/g" build/flycast.xcodeproj/xcshareddata/xcschemes/flycast.xcscheme
open build/flycast.xcodeproj
printf '\033[1;32m%s\033[0m\n' 'Once the project opens in Xcode, press Cmd+R to build and run.'
