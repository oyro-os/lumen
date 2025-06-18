# Toolchain file for cross-compiling to Windows from Linux/macOS using MinGW-w64

# Target system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MinGW-w64 toolchain prefix
if(DEFINED ENV{MINGW_PREFIX})
    set(MINGW_PREFIX $ENV{MINGW_PREFIX})
else()
    # Try common prefixes
    find_program(MINGW_CC NAMES 
        x86_64-w64-mingw32-gcc
        x86_64-w64-mingw32-gcc-posix
        x86_64-w64-mingw32-gcc-win32
        x86_64-pc-mingw32-gcc
        mingw64-gcc
    )
    if(MINGW_CC)
        get_filename_component(MINGW_PREFIX ${MINGW_CC} NAME)
        string(REGEX REPLACE "-gcc.*$" "" MINGW_PREFIX ${MINGW_PREFIX})
    else()
        set(MINGW_PREFIX "x86_64-w64-mingw32")
    endif()
endif()

# Compilers
set(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

# Target environment
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX})

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Windows-specific flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")

# Force static linking to avoid DLL dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-all-symbols")

# Windows doesn't have RPATH
set(CMAKE_SKIP_RPATH TRUE)