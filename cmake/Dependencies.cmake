include(FetchContent)

# Function to fetch latest release from GitHub API
function(fetch_github_latest_release REPO_OWNER REPO_NAME OUTPUT_TAG OUTPUT_URL)
    set(API_URL "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases/latest")

    file(DOWNLOAD ${API_URL} ${CMAKE_BINARY_DIR}/release_info.json
        STATUS download_status
        TIMEOUT 30
    )

    list(GET download_status 0 status_code)
    if(status_code EQUAL 0)
        file(READ ${CMAKE_BINARY_DIR}/release_info.json release_json)

        # Parse JSON for tag_name and tarball_url
        string(REGEX MATCH "\"tag_name\":\\s*\"([^\"]+)\"" _ "${release_json}")
        set(${OUTPUT_TAG} ${CMAKE_MATCH_1} PARENT_SCOPE)

        string(REGEX MATCH "\"tarball_url\":\\s*\"([^\"]+)\"" _ "${release_json}")
        set(${OUTPUT_URL} ${CMAKE_MATCH_1} PARENT_SCOPE)

        message(STATUS "Found latest release: ${CMAKE_MATCH_1}")
    else()
        message(WARNING "Failed to fetch latest release info, using fallback")
        set(${OUTPUT_TAG} "" PARENT_SCOPE)
        set(${OUTPUT_URL} "" PARENT_SCOPE)
    endif()
endfunction()

# Google Test (fetch latest release)
if(BUILD_TESTS)
    message(STATUS "Fetching latest Google Test release...")

    fetch_github_latest_release("google" "googletest" GTEST_TAG GTEST_URL)

    if(GTEST_TAG)
        FetchContent_Declare(
            googletest
            URL ${GTEST_URL}
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/googletest-${GTEST_TAG}
        )
    else()
        # Fallback to known good version
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
            URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/googletest-v1.14.0
            DOWNLOAD_EXTRACT_TIMESTAMP NO
        )
    endif()

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# mimalloc (fetch latest release)
if(LUMEN_ALLOCATOR STREQUAL "MIMALLOC")
    message(STATUS "Fetching latest mimalloc release...")

    fetch_github_latest_release("microsoft" "mimalloc" MIMALLOC_TAG MIMALLOC_URL)

    if(MIMALLOC_TAG)
        FetchContent_Declare(
            mimalloc
            URL ${MIMALLOC_URL}
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/mimalloc-${MIMALLOC_TAG}
        )
    else()
        # Fallback to known good version
        FetchContent_Declare(
            mimalloc
            URL https://github.com/microsoft/mimalloc/archive/refs/tags/v2.1.2.tar.gz
            URL_HASH SHA256=2b1bff6f717f9725c70bf8d79e4786da13de8a270059e4ba0bdd262ae7be46eb
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/mimalloc-v2.1.2
        )
    endif()

    set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(mimalloc)
endif()

# Android NDK handling
if(ANDROID)
    message(STATUS "Android build detected...")

    # Use environment variable first
    if(DEFINED ENV{ANDROID_NDK_HOME})
        set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK_HOME})
        message(STATUS "Using NDK from environment: ${CMAKE_ANDROID_NDK}")
    elseif(DEFINED ENV{ANDROID_NDK_ROOT})
        set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK_ROOT})
        message(STATUS "Using NDK from environment: ${CMAKE_ANDROID_NDK}")
    else()
        message(WARNING "ANDROID_NDK_HOME or ANDROID_NDK_ROOT not set. Please set one of these environment variables.")
    endif()
endif()
