# CI Architecture

## Overview

Lumen uses a Linux-centric CI/CD pipeline that leverages cross-compilation to build for all supported platforms from a single host OS. This approach provides several benefits:

- **Cost Efficiency**: Only need Linux runners (cheaper than macOS/Windows runners)
- **Consistency**: All builds use the same base environment
- **Speed**: Parallel cross-compilation is faster than sequential native builds
- **Simplicity**: Single build configuration to maintain

## CI Jobs

### 1. Test Suite (`test`)
- **Runs on**: ubuntu-latest
- **Rust versions**: stable, beta, nightly
- **Purpose**: Run all tests, check formatting, run clippy
- **Coverage**: 29 tests across unit, integration, and FFI

### 2. Code Coverage (`coverage`)
- **Runs on**: ubuntu-latest
- **Tool**: cargo-llvm-cov
- **Output**: LCOV format for Codecov integration
- **Requirement**: 90%+ coverage

### 3. Security Audit (`security`)
- **Runs on**: ubuntu-latest
- **Tool**: cargo-audit
- **Purpose**: Check for known vulnerabilities

### 4. Performance Benchmarks (`benchmark`)
- **Runs on**: ubuntu-latest
- **Tool**: Criterion.rs
- **Trigger**: Only on push to main branch

### 5. Cross-Compilation (`cross-compile`)
- **Runs on**: ubuntu-latest
- **Targets**:
  - macOS: x86_64, aarch64
  - iOS: ARM64, x86_64 simulator
  - Windows: x86_64 (mingw-w64)
  - Linux: ARM64, x86_64 musl
  - Android: ARM64, ARMv7
- **Tool**: cross-rs for most targets, native cargo for Windows

### 6. Release (`release`)
- **Runs on**: ubuntu-latest
- **Trigger**: Git tags (v*)
- **Output**: Compressed archives for each platform

### 7. CI Summary (`ci-summary`)
- **Purpose**: Ensure all required checks pass
- **Dependencies**: All other jobs must succeed

## Cross-Compilation Strategy

### Tool Selection
- **cross-rs**: Used for most non-native targets
  - Provides Docker containers with pre-configured toolchains
  - Handles complex cross-compilation scenarios
- **Native cargo**: Used for Windows (with mingw-w64)
  - Simpler setup for GNU toolchain

### Platform-Specific Notes

#### macOS/iOS
- Requires osxcross or cross-rs Docker images
- Builds both x86_64 and ARM64 architectures
- iOS builds static libraries only

#### Windows
- Uses mingw-w64 for cross-compilation
- Produces both DLL and static libraries
- Alternative: Could use wine for testing

#### Android
- Supports both ARM64 and ARMv7
- Requires Android NDK (handled by cross-rs)
- Produces .so files for JNI

#### Linux
- ARM64 cross-compilation for embedded devices
- musl builds for maximum portability
- Static linking preferred

## Artifacts

Each successful build produces:
```
lumen-<target>/
├── liblumen_ffi.{so,dylib,dll,a}  # Platform library
├── lumen_ffi.lib                   # Windows import lib
└── lumen.h                         # C header file
```

## Local Development

Developers can replicate CI builds locally:

```bash
# Install all cross-compilation targets
make cross-setup

# Build for all platforms
make cross-all

# Build for specific platform
make macos-arm64
make windows-x64
make linux-arm64
```

## Future Enhancements

1. **Caching**: Implement sccache for faster builds
2. **Matrix Testing**: Test against multiple dependency versions
3. **Performance Tracking**: Store benchmark results over time
4. **Binary Size Tracking**: Monitor library size growth
5. **Integration Tests**: Run FFI tests with actual language bindings