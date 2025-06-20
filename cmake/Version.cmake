# Version configuration for Lumen Database

# Set version numbers
set(LUMEN_VERSION_MAJOR 0)
set(LUMEN_VERSION_MINOR 1)
set(LUMEN_VERSION_PATCH 0)
set(LUMEN_VERSION "${LUMEN_VERSION_MAJOR}.${LUMEN_VERSION_MINOR}.${LUMEN_VERSION_PATCH}")

# Git information (optional)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE LUMEN_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE LUMEN_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(LUMEN_GIT_HASH "unknown")
    set(LUMEN_GIT_DESCRIBE "unknown")
endif()

# Build timestamp
string(TIMESTAMP LUMEN_BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)

# Configure version header
configure_file(
    "${CMAKE_SOURCE_DIR}/include/lumen/version.h.in"
    "${CMAKE_BINARY_DIR}/include/lumen/version.h"
    @ONLY
)

# Add configured header to include directories
include_directories("${CMAKE_BINARY_DIR}/include")

# Print version information
message(STATUS "Lumen Database version: ${LUMEN_VERSION}")
if(LUMEN_GIT_HASH)
    message(STATUS "Git commit: ${LUMEN_GIT_HASH}")
endif()