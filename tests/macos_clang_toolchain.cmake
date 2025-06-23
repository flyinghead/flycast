set(CMAKE_SYSTEM_NAME Darwin)

# Specify the C compiler
set(CMAKE_C_COMPILER /Applications/Xcode16.2.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang)

# Specify the CXX compiler
set(CMAKE_CXX_COMPILER /Applications/Xcode16.2.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++)


# Specify the target architecture
set(CMAKE_OSX_ARCHITECTURES arm64)

# Set the sysroot (optional, but good practice for cross-compilation or specific SDKs)
set(CMAKE_OSX_SYSROOT /Applications/Xcode16.2.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

# Set the deployment target (optional, but good practice)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13") # Or your desired minimum macOS version
