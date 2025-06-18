# Platform detection and configuration

# Detect platform and architecture
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LUMEN_PLATFORM "macos")
    # Use CMAKE_SYSTEM_PROCESSOR for actual host architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(LUMEN_ARCH "arm64")
    else()
        set(LUMEN_ARCH "x86_64")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(LUMEN_PLATFORM "ios")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(LUMEN_ARCH "arm64")
    else()
        set(LUMEN_ARCH "x86_64")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(LUMEN_PLATFORM "android")
    set(LUMEN_ARCH ${CMAKE_ANDROID_ARCH_ABI})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LUMEN_PLATFORM "windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LUMEN_ARCH "x64")
    else()
        set(LUMEN_ARCH "x86")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LUMEN_PLATFORM "linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(LUMEN_ARCH "arm64")
    else()
        set(LUMEN_ARCH "x86_64")
    endif()
endif()

message(STATUS "Building for: ${LUMEN_PLATFORM}/${LUMEN_ARCH}")

# Set output directories only for release libraries
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH})
endif()

# Tests and benchmarks stay in build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Create dist directories
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH})