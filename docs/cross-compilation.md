# Cross-Compilation Guide

This guide explains how to cross-compile Lumen for all supported platforms from a Linux development machine.

## Quick Start

```bash
# Install all targets and tools
make cross-setup

# Build for all platforms
make cross-all
```

Output will be in `dist/` directory organized by platform and architecture.

## Supported Targets

| Platform | Architecture | Target Triple | Output |
|----------|-------------|---------------|---------|
| macOS | ARM64 | `aarch64-apple-darwin` | `.dylib`, `.a` |
| macOS | x86_64 | `x86_64-apple-darwin` | `.dylib`, `.a` |
| iOS | ARM64 | `aarch64-apple-ios` | `.a` |
| iOS | Simulator | `x86_64-apple-ios` | `.a` |
| Windows | x64 | `x86_64-pc-windows-gnu` | `.dll`, `.lib` |
| Linux | ARM64 | `aarch64-unknown-linux-gnu` | `.so`, `.a` |
| Android | ARM64 | `aarch64-linux-android` | `.so` |
| Android | ARMv7 | `armv7-linux-androideabi` | `.so` |

## Prerequisites

### Ubuntu/Debian

```bash
# Windows cross-compilation
sudo apt update
sudo apt install mingw-w64 gcc-mingw-w64

# Linux ARM64 cross-compilation  
sudo apt install gcc-aarch64-linux-gnu

# Optional: macOS cross-compilation (requires osxcross)
# See: https://github.com/tpoechtrager/osxcross
```

### Fedora/RHEL

```bash
# Windows cross-compilation
sudo dnf install mingw64-gcc mingw64-winpthreads-static

# Linux ARM64 cross-compilation
sudo dnf install gcc-aarch64-linux-gnu
```

### Arch Linux

```bash
# Windows cross-compilation
sudo pacman -S mingw-w64-gcc

# Linux ARM64 cross-compilation
sudo pacman -S aarch64-linux-gnu-gcc
```

## Individual Platform Builds

### macOS

```bash
# ARM64 (Apple Silicon)
make macos-arm64

# x86_64 (Intel)
make macos-x86_64

# Universal binary (both architectures) - macOS only
make macos-universal
```

### iOS

```bash
# Device (ARM64)
make ios-arm64

# Simulator (x86_64)
make ios-sim

# Universal iOS framework - requires Xcode
cargo lipo --release
```

### Windows

```bash
# x64
make windows-x64

# Note: Requires mingw-w64 toolchain
```

### Linux

```bash
# Native
make linux

# ARM64 cross-compile
make linux-arm64
```

### Android

```bash
# All Android architectures
make android

# Individual architectures
cargo ndk -t arm64-v8a build --release
cargo ndk -t armeabi-v7a build --release
```

## Advanced Cross-Compilation

### Environment Variables

```bash
# Custom Android NDK path
export ANDROID_NDK_ROOT=/path/to/ndk

# Custom macOS SDK path (for osxcross)
export OSXCROSS_ROOT=/path/to/osxcross

# Custom Windows toolchain
export CC_x86_64_pc_windows_gnu=x86_64-w64-mingw32-gcc
```

### Manual Target Installation

```bash
# Add specific targets
rustup target add aarch64-apple-darwin
rustup target add x86_64-pc-windows-gnu

# List all available targets
rustup target list
```

### Custom Linkers

Edit `.cargo/config.toml` to specify custom linkers:

```toml
[target.x86_64-pc-windows-gnu]
linker = "x86_64-w64-mingw32-gcc"
ar = "x86_64-w64-mingw32-ar"
```

## Platform-Specific Limitations

### Apple Platforms (macOS/iOS)

**Important**: Apple targets (macOS and iOS) cannot be cross-compiled from Linux using standard tools due to:
- Proprietary SDK requirements
- Code signing requirements
- Lack of Docker images for `cross` tool

**Solutions**:
1. **Use macOS CI runners**: GitHub Actions provides macOS runners for building Apple targets
2. **Local development**: Build on actual macOS hardware
3. **OSXCross**: Advanced users can set up osxcross, but it requires:
   - Apple Developer account
   - Manual SDK extraction
   - Complex setup process
   - May violate Apple's license terms

**Recommendation**: Use GitHub Actions with macOS runners for Apple builds in CI/CD.

## Troubleshooting

### Common Issues

**Windows: "linker not found"**
```bash
# Install mingw-w64
sudo apt install mingw-w64
```

**macOS: "SDK not found"**
```bash
# Install osxcross or use macOS machine
# See macOS cross-compilation section above
```

**Android: "NDK not found"**
```bash
# Install Android NDK
cargo install cargo-ndk
# Set ANDROID_NDK_ROOT environment variable
```

**ARM64: "cross-compiler not found"**
```bash
# Install ARM64 cross-compiler
sudo apt install gcc-aarch64-linux-gnu
```

### Verification

Test that cross-compiled binaries work:

```bash
# Check binary architecture
file target/aarch64-apple-darwin/release/liblumen_ffi.dylib

# Test Windows binary (if Wine is installed)
wine target/x86_64-pc-windows-gnu/release/lumen_ffi.exe

# Test with qemu (for ARM binaries)
qemu-aarch64 target/aarch64-unknown-linux-gnu/release/lumen_ffi
```

## CI/CD Integration

### GitHub Actions

```yaml
name: Cross Compilation
on: [push, pull_request]

jobs:
  cross-compile:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target:
          - aarch64-apple-darwin
          - x86_64-apple-darwin
          - x86_64-pc-windows-gnu
          - aarch64-unknown-linux-gnu
    
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
        with:
          targets: ${{ matrix.target }}
      
      - name: Install cross-compilation tools
        run: make cross-setup
      
      - name: Build
        run: cargo build --release --target ${{ matrix.target }}
```

## Distribution

After cross-compilation, distribute platform-specific packages:

```
dist/
├── macos/
│   ├── arm64/liblumen_ffi.dylib
│   └── x86_64/liblumen_ffi.dylib
├── ios/
│   ├── arm64/liblumen_ffi.a
│   └── simulator/liblumen_ffi.a
├── windows/
│   └── x64/lumen_ffi.dll
├── linux/
│   └── arm64/liblumen_ffi.so
└── android/
    ├── arm64-v8a/liblumen_ffi.so
    └── armeabi-v7a/liblumen_ffi.so
```

Each directory contains the FFI library and generated C headers for that platform.