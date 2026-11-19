# Run with Xcode and the iPhoneOS SDK on macOS.
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_SYSROOT iphoneos)
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Target architecture")
