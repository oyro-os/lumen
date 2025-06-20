#pragma once

// Lumen Database - High-Performance Embedded Database

// Include generated version information
#include <lumen/version.h>

// Platform detection
#ifdef _WIN32
#define LUMEN_PLATFORM_WINDOWS
#ifdef _WIN64
#define LUMEN_PLATFORM_WIN64
#else
#define LUMEN_PLATFORM_WIN32
#endif
#elif __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define LUMEN_PLATFORM_MACOS
#elif TARGET_OS_IPHONE
#define LUMEN_PLATFORM_IOS
#endif
#elif __ANDROID__
#define LUMEN_PLATFORM_ANDROID
#elif __linux__
#define LUMEN_PLATFORM_LINUX
#else
#error "Unknown platform"
#endif

// Export/Import macros
#ifdef LUMEN_SHARED_LIBRARY
#ifdef LUMEN_BUILDING_LIBRARY
#ifdef LUMEN_PLATFORM_WINDOWS
#define LUMEN_API __declspec(dllexport)
#else
#define LUMEN_API __attribute__((visibility("default")))
#endif
#else
#ifdef LUMEN_PLATFORM_WINDOWS
#define LUMEN_API __declspec(dllimport)
#else
#define LUMEN_API
#endif
#endif
#else
#define LUMEN_API
#endif

// C++ standard requirements
#ifdef __cplusplus
#if __cplusplus < 202002L
#error "Lumen requires C++20 or later"
#endif
#endif

// Standard includes
#include <cstddef>
#include <cstdint>

namespace lumen {

// Forward declarations
class Status;
template<typename T>
class Result;
class Database;

}  // namespace lumen

// Public C++ API includes
#include <lumen/common/logging.h>
#include <lumen/common/status.h>

// C API
#ifdef __cplusplus
extern "C" {
#endif

// Version information
LUMEN_API const char* lumen_version_string();
LUMEN_API int lumen_version_major();
LUMEN_API int lumen_version_minor();
LUMEN_API int lumen_version_patch();

#ifdef __cplusplus
}
#endif