# Compiler flags and optimization settings

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Common flags for GCC and Clang
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-sign-compare
        -Wno-nan-infinity-disabled
        -fstrict-aliasing
    )
    
    # Debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG -DLUMEN_DEBUG")
    
    # Release flags with aggressive optimization
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -flto -fomit-frame-pointer")
    
    # Platform specific optimizations
    if(LUMEN_PLATFORM STREQUAL "android")
        add_compile_options(-ffunction-sections -fdata-sections)
        add_link_options(-Wl,--gc-sections)
    endif()
    
    # Architecture specific SIMD flags
    if(LUMEN_ARCH STREQUAL "x86_64")
        add_compile_options(-march=native -mavx2 -mfma)
        add_compile_definitions(LUMEN_SIMD_AVX2)
    elseif(LUMEN_ARCH STREQUAL "arm64")
        add_compile_options(-march=armv8-a+simd)
        add_compile_definitions(LUMEN_SIMD_NEON)
    endif()
    
    # Security hardening flags
    add_compile_options(
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
    )
    
    # Linux-specific security hardening
    if(LUMEN_PLATFORM STREQUAL "linux" AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_link_options(-Wl,-z,relro,-z,now)
    endif()
    
elseif(MSVC)
    # Windows/MSVC flags
    add_compile_options(
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /Zc:inline
        /Zc:throwingNew
    )
    
    # Debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /MDd /DDEBUG /DLUMEN_DEBUG")
    
    # Release flags with link-time optimization
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /MD /DNDEBUG /GL")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
    
    # Architecture specific flags
    if(LUMEN_ARCH STREQUAL "x64")
        add_compile_options(/arch:AVX2)
        add_compile_definitions(LUMEN_SIMD_AVX2)
    endif()
    
    # Security flags
    add_compile_options(/GS /sdl)
endif()

# Coverage flags
if(COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

# Define allocator type
add_compile_definitions(LUMEN_ALLOCATOR_${LUMEN_ALLOCATOR})