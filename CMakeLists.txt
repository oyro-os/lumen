cmake_minimum_required(VERSION 3.20)
project(lumen VERSION 1.0.0 LANGUAGES CXX)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_STATIC "Build static library" ON)
option(BUILD_SHARED_DEBUG "Build shared debug library" ON)
option(BUILD_SHARED_RELEASE "Build shared release library" ON)
option(COVERAGE "Enable coverage reporting" OFF)
option(USE_CLANG "Force use of Clang compiler" OFF)
option(LUMEN_ALLOCATOR "Memory allocator to use" "MIMALLOC")
set_property(CACHE LUMEN_ALLOCATOR PROPERTY STRINGS "SYSTEM" "JEMALLOC" "TCMALLOC" "MIMALLOC")

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force Clang if requested
if(USE_CLANG)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
endif()

# Module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include modules
include(Platform)
include(CompilerFlags)
include(Dependencies)

# Source files (starting with minimal C API)
set(LUMEN_SOURCES
    src/lumen_c_api.cpp
    src/types.cpp
    src/memory/allocator.cpp
    src/storage/page.cpp
)

# Public headers
set(LUMEN_PUBLIC_HEADERS
    include/lumen/lumen.h
    include/lumen/types.h
    include/lumen/memory/allocator.h
    include/lumen/storage/page.h
)

# Include directories
include_directories(include)

# Libraries
if(BUILD_STATIC)
    add_library(lumen_static STATIC ${LUMEN_SOURCES})
    set_target_properties(lumen_static PROPERTIES
        OUTPUT_NAME lumen
        POSITION_INDEPENDENT_CODE ON
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH}
    )
    target_compile_definitions(lumen_static PRIVATE LUMEN_STATIC_LIB)
endif()

if(BUILD_SHARED_DEBUG)
    add_library(lumen_shared_debug SHARED ${LUMEN_SOURCES})
    set_target_properties(lumen_shared_debug PROPERTIES
        OUTPUT_NAME lumen_debug
        DEBUG_POSTFIX ""
    )
    target_compile_definitions(lumen_shared_debug PRIVATE LUMEN_DEBUG LUMEN_SHARED_LIB)
    # Debug libraries stay in build directory, not dist/
endif()

if(BUILD_SHARED_RELEASE)
    add_library(lumen_shared_release SHARED ${LUMEN_SOURCES})
    set_target_properties(lumen_shared_release PROPERTIES
        OUTPUT_NAME lumen
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH}
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist/${LUMEN_PLATFORM}/${LUMEN_ARCH}
    )
    target_compile_definitions(lumen_shared_release PRIVATE LUMEN_SHARED_LIB)
endif()

# Set allocator compile definitions
if(LUMEN_ALLOCATOR STREQUAL "MIMALLOC")
    if(BUILD_STATIC)
        target_compile_definitions(lumen_static PRIVATE LUMEN_USE_MIMALLOC)
    endif()
    if(BUILD_SHARED_DEBUG)
        target_compile_definitions(lumen_shared_debug PRIVATE LUMEN_USE_MIMALLOC)
    endif()
    if(BUILD_SHARED_RELEASE)
        target_compile_definitions(lumen_shared_release PRIVATE LUMEN_USE_MIMALLOC)
    endif()
elseif(LUMEN_ALLOCATOR STREQUAL "JEMALLOC")
    if(BUILD_STATIC)
        target_compile_definitions(lumen_static PRIVATE LUMEN_USE_JEMALLOC)
    endif()
    if(BUILD_SHARED_DEBUG)
        target_compile_definitions(lumen_shared_debug PRIVATE LUMEN_USE_JEMALLOC)
    endif()
    if(BUILD_SHARED_RELEASE)
        target_compile_definitions(lumen_shared_release PRIVATE LUMEN_USE_JEMALLOC)
    endif()
elseif(LUMEN_ALLOCATOR STREQUAL "TCMALLOC")
    if(BUILD_STATIC)
        target_compile_definitions(lumen_static PRIVATE LUMEN_USE_TCMALLOC)
    endif()
    if(BUILD_SHARED_DEBUG)
        target_compile_definitions(lumen_shared_debug PRIVATE LUMEN_USE_TCMALLOC)
    endif()
    if(BUILD_SHARED_RELEASE)
        target_compile_definitions(lumen_shared_release PRIVATE LUMEN_USE_TCMALLOC)
    endif()
endif()

# Link allocator libraries
if(LUMEN_ALLOCATOR STREQUAL "JEMALLOC")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JEMALLOC REQUIRED jemalloc)
    if(BUILD_STATIC)
        target_link_libraries(lumen_static ${JEMALLOC_LIBRARIES})
    endif()
    if(BUILD_SHARED_DEBUG)
        target_link_libraries(lumen_shared_debug ${JEMALLOC_LIBRARIES})
    endif()
    if(BUILD_SHARED_RELEASE)
        target_link_libraries(lumen_shared_release ${JEMALLOC_LIBRARIES})
    endif()
elseif(LUMEN_ALLOCATOR STREQUAL "TCMALLOC")
    find_library(TCMALLOC_LIB tcmalloc)
    if(BUILD_STATIC)
        target_link_libraries(lumen_static ${TCMALLOC_LIB})
    endif()
    if(BUILD_SHARED_DEBUG)
        target_link_libraries(lumen_shared_debug ${TCMALLOC_LIB})
    endif()
    if(BUILD_SHARED_RELEASE)
        target_link_libraries(lumen_shared_release ${TCMALLOC_LIB})
    endif()
elseif(LUMEN_ALLOCATOR STREQUAL "MIMALLOC")
    find_package(mimalloc QUIET)
    if(mimalloc_FOUND)
        if(BUILD_STATIC)
            target_link_libraries(lumen_static mimalloc)
        endif()
        if(BUILD_SHARED_DEBUG)
            target_link_libraries(lumen_shared_debug mimalloc)
        endif()
        if(BUILD_SHARED_RELEASE)
            target_link_libraries(lumen_shared_release mimalloc)
        endif()
    else()
        message(STATUS "mimalloc not found, using system allocator")
        set(LUMEN_ALLOCATOR "SYSTEM")
    endif()
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Coverage
if(COVERAGE)
    include(Coverage)
    setup_coverage(lumen_tests)
endif()

# Installation
include(Install)