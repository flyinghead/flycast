script_dir=$(dirname "$0")
cd $script_dir/../../

echo "1) macOS ($(uname -m))"
echo "2) iOS"
read -p "Choose your target platform: " x

if [ $x -eq 2 ]; then
    option="-DCMAKE_SYSTEM_NAME=iOS"
    lldbinitfolder="emulator-ios"
    echo 'Building iOS xcodeproj for debugging'
    echo 'Remove CODE_SIGNING_ALLOWED=NO in Build Settings if you are using your Apple Developer Certificate for signing'
else
    option="-DCMAKE_OSX_ARCHITECTURES=$(uname -m)"
    lldbinitfolder="emulator-osx"
    echo 'Building macOS xcodeproj for debugging'
fi

if [[ -z "${VULKAN_SDK}" ]]; then
    echo 'Warning: VULKAN_SDK is not set in environment variable'
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release $option -DCMAKE_XCODE_GENERATE_SCHEME=YES -G "Xcode"

nl=$'\n'
sed -i '' -E "s/launchStyle/customLLDBInitFile = \"\$(SRCROOT)\/shell\/apple\/\\${lldbinitfolder}\/LLDBInitFile\"\\${nl}launchStyle/g" build/flycast.xcodeproj/xcshareddata/xcschemes/flycast.xcscheme
open build/flycast.xcodeproj
