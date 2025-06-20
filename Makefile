# Lumen Database Makefile - Rust Edition
# Shortcuts for common development tasks

# Default target
.DEFAULT_GOAL := help

# Variables
ROOT_DIR := $(shell pwd)
TARGET_DIR := target
DIST_DIR := dist
CORES := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CARGO := cargo
CARGO_FLAGS := --jobs $(CORES)

# Colors for output
GREEN := \033[0;32m
YELLOW := \033[0;33m
RED := \033[0;31m
NC := \033[0m # No Color

# Help target
help:
	@echo "$(GREEN)Lumen Database - Make Targets$(NC)"
	@echo ""
	@echo "$(YELLOW)Development:$(NC)"
	@echo "  make build         - Debug build"
	@echo "  make release       - Release build for distribution"
	@echo "  make test          - Run all tests"
	@echo "  make test-verbose  - Run tests with detailed output"
	@echo "  make bench         - Run benchmarks"
	@echo "  make doc           - Generate documentation"
	@echo "  make coverage      - Generate coverage report"
	@echo "  make clean         - Clean build directory"
	@echo "  make distclean     - Clean everything"
	@echo ""
	@echo "$(YELLOW)Platform Builds:$(NC)"
	@echo "  make macos         - Build for macOS (current arch)"
	@echo "  make ios           - Build for iOS"
	@echo "  make android       - Build for Android"
	@echo "  make linux         - Build for Linux"
	@echo "  make windows       - Build for Windows"
	@echo ""
	@echo "$(YELLOW)Cross Compilation:$(NC)"
	@echo "  make cross-setup   - Install all cross-compilation targets"
	@echo "  make cross-all     - Cross-compile for all platforms from Linux"
	@echo "  make macos-arm64   - Build for macOS ARM64"
	@echo "  make macos-x86_64  - Build for macOS x86_64"
	@echo "  make ios-arm64     - Build for iOS ARM64"
	@echo "  make ios-sim       - Build for iOS Simulator"
	@echo "  make windows-x64   - Build for Windows x64"
	@echo "  make linux-arm64   - Build for Linux ARM64"
	@echo ""
	@echo "$(YELLOW)CI/CD:$(NC)"
	@echo "  make ci-test       - Run CI tests"
	@echo "  make ci-all        - Run all CI checks locally"
	@echo "  make ci-format-check - Check formatting (CI mode)"
	@echo "  make ci-clippy     - Run clippy (CI mode)"
	@echo "  make ci-coverage   - Generate coverage report"
	@echo "  make ci-security   - Run security audit"
	@echo "  make format        - Format code with rustfmt"
	@echo "  make lint          - Run clippy"
	@echo "  make check         - Check + format + lint + test"
	@echo ""
	@echo "$(YELLOW)Utilities:$(NC)"
	@echo "  make install       - Install binary"
	@echo "  make package       - Create distribution package"

# Development targets
build:
	@echo "$(GREEN)Building Lumen (Debug)...$(NC)"
	@$(CARGO) build $(CARGO_FLAGS)
	@echo "$(GREEN)Build complete!$(NC)"

release:
	@echo "$(GREEN)Building Lumen (Release)...$(NC)"
	@$(CARGO) build --release $(CARGO_FLAGS)
	@mkdir -p $(DIST_DIR)
	@cp target/release/liblumen.* $(DIST_DIR)/ 2>/dev/null || true
	@echo "$(GREEN)Release build complete! Check target/release/$(NC)"

test:
	@echo "$(GREEN)Running tests...$(NC)"
	@$(CARGO) test $(CARGO_FLAGS) --workspace
	@echo "$(GREEN)All tests passed!$(NC)"

test-verbose:
	@echo "$(GREEN)Running tests (verbose)...$(NC)"
	@$(CARGO) test $(CARGO_FLAGS) -- --nocapture --test-threads=1

coverage:
	@echo "$(GREEN)Generating coverage report...$(NC)"
	@command -v cargo-llvm-cov >/dev/null 2>&1 || cargo install cargo-llvm-cov
	@cargo llvm-cov --all-features --workspace --html --output-dir target/coverage
	@cargo llvm-cov --all-features --workspace --text
	@echo ""
	@echo "$(GREEN)HTML report: target/coverage/html/index.html$(NC)"
	@echo "$(GREEN)Text report shown above$(NC)"

bench:
	@echo "$(GREEN)Running benchmarks...$(NC)"
	@$(CARGO) bench $(CARGO_FLAGS)

doc:
	@echo "$(GREEN)Generating documentation...$(NC)"
	@$(CARGO) doc --no-deps --open

# Platform-specific builds
macos:
	@echo "$(GREEN)Building for macOS (native)...$(NC)"
	@$(CARGO) build --release $(CARGO_FLAGS)

macos-universal:
	@echo "$(GREEN)Building universal macOS binary...$(NC)"
	@$(CARGO) build --release --target aarch64-apple-darwin
	@$(CARGO) build --release --target x86_64-apple-darwin
	@lipo -create target/aarch64-apple-darwin/release/liblumen.dylib \
		target/x86_64-apple-darwin/release/liblumen.dylib \
		-output target/release/liblumen-universal.dylib

ios:
	@echo "$(GREEN)Building for iOS...$(NC)"
	@cargo install cargo-lipo 2>/dev/null || true
	@rustup target add aarch64-apple-ios x86_64-apple-ios 2>/dev/null || true
	@cargo lipo --release
	@echo "$(GREEN)iOS build complete!$(NC)"

android:
	@echo "$(GREEN)Building for Android...$(NC)"
	@cargo install cargo-ndk 2>/dev/null || true
	@rustup target add aarch64-linux-android armv7-linux-androideabi 2>/dev/null || true
	@cargo ndk -t armeabi-v7a -t arm64-v8a -o jniLibs build --release
	@echo "$(GREEN)Android build complete!$(NC)"

linux:
	@echo "$(GREEN)Building for Linux...$(NC)"
	@$(CARGO) build --release $(CARGO_FLAGS)
	@echo "$(GREEN)Linux build complete!$(NC)"

windows:
	@echo "$(GREEN)Building for Windows...$(NC)"
	@rustup target add x86_64-pc-windows-gnu 2>/dev/null || true
	@cargo build --release --target x86_64-pc-windows-gnu
	@echo "$(GREEN)Windows build complete!$(NC)"

# Cross-compilation setup and targets
cross-setup:
	@echo "$(GREEN)Installing cross-compilation toolchains...$(NC)"
	@echo "$(YELLOW)Installing Rust targets...$(NC)"
	@rustup target add x86_64-apple-darwin aarch64-apple-darwin
	@rustup target add aarch64-apple-ios x86_64-apple-ios
	@rustup target add x86_64-pc-windows-gnu x86_64-pc-windows-msvc
	@rustup target add aarch64-linux-android armv7-linux-androideabi
	@rustup target add aarch64-unknown-linux-gnu
	@echo "$(YELLOW)Installing cargo tools...$(NC)"
	@cargo install cargo-lipo 2>/dev/null || true
	@cargo install cargo-ndk 2>/dev/null || true
	@echo "$(GREEN)Cross-compilation setup complete!$(NC)"
	@echo "$(YELLOW)NOTE: For Windows cross-compilation from Linux, install mingw-w64:$(NC)"
	@echo "  Ubuntu/Debian: sudo apt install mingw-w64"
	@echo "  Fedora/RHEL: sudo dnf install mingw64-gcc"
	@echo "  Arch: sudo pacman -S mingw-w64-gcc"

cross-all: cross-setup
	@echo "$(GREEN)Cross-compiling for all platforms...$(NC)"
	@mkdir -p $(DIST_DIR)/{linux,macos,ios,windows,android}
	@$(MAKE) macos-arm64
	@$(MAKE) macos-x86_64  
	@$(MAKE) ios-arm64
	@$(MAKE) ios-sim
	@$(MAKE) windows-x64
	@$(MAKE) linux-arm64
	@$(MAKE) android
	@echo "$(GREEN)All cross-compilation targets complete!$(NC)"
	@echo "$(YELLOW)Output directory: $(DIST_DIR)$(NC)"

# macOS targets (requires macOS host)
macos-arm64:
	@echo "$(GREEN)Building for macOS ARM64...$(NC)"
	@echo "$(YELLOW)NOTE: This target requires a macOS host machine$(NC)"
	@rustup target add aarch64-apple-darwin 2>/dev/null || true
	@cargo build --release --target aarch64-apple-darwin $(CARGO_FLAGS)
	@mkdir -p $(DIST_DIR)/macos/arm64
	@cp target/aarch64-apple-darwin/release/liblumen_ffi.* $(DIST_DIR)/macos/arm64/ 2>/dev/null || true
	@echo "$(GREEN)macOS ARM64 build complete!$(NC)"

macos-x86_64:
	@echo "$(GREEN)Building for macOS x86_64...$(NC)"
	@echo "$(YELLOW)NOTE: This target requires a macOS host machine$(NC)"
	@rustup target add x86_64-apple-darwin 2>/dev/null || true
	@cargo build --release --target x86_64-apple-darwin $(CARGO_FLAGS)
	@mkdir -p $(DIST_DIR)/macos/x86_64
	@cp target/x86_64-apple-darwin/release/liblumen_ffi.* $(DIST_DIR)/macos/x86_64/ 2>/dev/null || true
	@echo "$(GREEN)macOS x86_64 build complete!$(NC)"

# iOS targets (requires macOS host)
ios-arm64:
	@echo "$(GREEN)Building for iOS ARM64...$(NC)"
	@echo "$(YELLOW)NOTE: This target requires a macOS host machine$(NC)"
	@rustup target add aarch64-apple-ios 2>/dev/null || true
	@cargo build --release --target aarch64-apple-ios $(CARGO_FLAGS)
	@mkdir -p $(DIST_DIR)/ios/arm64
	@cp target/aarch64-apple-ios/release/liblumen_ffi.a $(DIST_DIR)/ios/arm64/ 2>/dev/null || true
	@echo "$(GREEN)iOS ARM64 build complete!$(NC)"

ios-sim:
	@echo "$(GREEN)Building for iOS Simulator...$(NC)"
	@echo "$(YELLOW)NOTE: This target requires a macOS host machine$(NC)"
	@rustup target add x86_64-apple-ios 2>/dev/null || true
	@cargo build --release --target x86_64-apple-ios $(CARGO_FLAGS)
	@mkdir -p $(DIST_DIR)/ios/simulator
	@cp target/x86_64-apple-ios/release/liblumen_ffi.a $(DIST_DIR)/ios/simulator/ 2>/dev/null || true
	@echo "$(GREEN)iOS Simulator build complete!$(NC)"

# Windows targets
windows-x64:
	@echo "$(GREEN)Building for Windows x64...$(NC)"
	@rustup target add x86_64-pc-windows-gnu 2>/dev/null || true
	@cargo build --release --target x86_64-pc-windows-gnu $(CARGO_FLAGS)
	@mkdir -p $(DIST_DIR)/windows/x64
	@cp target/x86_64-pc-windows-gnu/release/lumen_ffi.* $(DIST_DIR)/windows/x64/ 2>/dev/null || true
	@echo "$(GREEN)Windows x64 build complete!$(NC)"

# Linux targets
linux-arm64:
	@echo "$(GREEN)Building for Linux ARM64...$(NC)"
	@rustup target add aarch64-unknown-linux-gnu 2>/dev/null || true
	@cargo build --release --target aarch64-unknown-linux-gnu $(CARGO_FLAGS)
	@mkdir -p $(DIST_DIR)/linux/arm64
	@cp target/aarch64-unknown-linux-gnu/release/liblumen_ffi.* $(DIST_DIR)/linux/arm64/ 2>/dev/null || true
	@echo "$(GREEN)Linux ARM64 build complete!$(NC)"


# CI/CD targets
ci-test:
	@echo "$(GREEN)Running CI tests...$(NC)"
	@$(MAKE) check
	@$(MAKE) test
	@echo "$(GREEN)CI tests complete!$(NC)"

# Individual CI checks (for debugging CI failures locally)
ci-format-check:
	@echo "$(GREEN)Checking code formatting...$(NC)"
	@cargo fmt --all -- --check

ci-clippy:
	@echo "$(GREEN)Running clippy with CI settings...$(NC)"
	@cargo clippy --all-targets --all-features -- -D warnings

ci-check:
	@echo "$(GREEN)Checking compilation...$(NC)"
	@cargo check --all-targets --all-features

ci-test-verbose:
	@echo "$(GREEN)Running tests (CI mode)...$(NC)"
	@cargo test --workspace --verbose

ci-doc-test:
	@echo "$(GREEN)Running doc tests...$(NC)"
	@cargo test --doc --all-features

ci-coverage:
	@echo "$(GREEN)Generating coverage report (requires cargo-llvm-cov)...$(NC)"
	@command -v cargo-llvm-cov >/dev/null 2>&1 || cargo install cargo-llvm-cov
	@cargo llvm-cov --all-features --workspace --lcov --output-path lcov.info
	@cargo llvm-cov --all-features --workspace --text --output-path coverage.txt
	@echo "$(GREEN)Coverage report saved to coverage.txt$(NC)"
	@echo ""
	@cat coverage.txt
	@echo ""
	@echo "$(GREEN)Coverage summary:$(NC)"
	@grep "TOTAL" coverage.txt || echo "No total coverage found"

ci-security:
	@echo "$(GREEN)Running security audit (requires cargo-audit)...$(NC)"
	@command -v cargo-audit >/dev/null 2>&1 || cargo install cargo-audit
	@cargo audit

ci-all:
	@echo "$(GREEN)Running all CI checks locally...$(NC)"
	@$(MAKE) ci-format-check
	@$(MAKE) ci-clippy
	@$(MAKE) ci-check
	@$(MAKE) ci-test-verbose
	@$(MAKE) ci-doc-test
	@echo "$(GREEN)All CI checks passed locally!$(NC)"

format:
	@echo "$(GREEN)Formatting code...$(NC)"
	@$(CARGO) fmt
	@echo "$(GREEN)Code formatted!$(NC)"

format-check:
	@echo "$(GREEN)Checking code format...$(NC)"
	@$(CARGO) fmt -- --check
	@echo "$(GREEN)Code format check passed!$(NC)"

lint:
	@echo "$(GREEN)Running clippy...$(NC)"
	@$(CARGO) clippy -- -D warnings
	@echo "$(GREEN)Lint check complete!$(NC)"

check:
	@echo "$(GREEN)Running cargo check...$(NC)"
	@$(CARGO) check
	@$(MAKE) format
	@$(MAKE) lint
	@$(MAKE) test
	@echo "$(GREEN)All checks passed!$(NC)"

# Installation
install: release
	@echo "$(GREEN)Installing Lumen...$(NC)"
	@$(CARGO) install --path .
	@echo "$(GREEN)Installation complete!$(NC)"

package: release
	@echo "$(GREEN)Creating package...$(NC)"
	@mkdir -p $(DIST_DIR)
	@tar czf $(DIST_DIR)/lumen-$(shell cargo metadata --no-deps --format-version 1 | jq -r '.packages[0].version').tar.gz \
		-C target/release \
		liblumen.a liblumen.so liblumen.dylib 2>/dev/null || true
	@echo "$(GREEN)Package created in $(DIST_DIR)$(NC)"

# Cleaning
clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	@$(CARGO) clean

distclean: clean
	@echo "$(YELLOW)Cleaning everything...$(NC)"
	@rm -rf $(DIST_DIR)
	@rm -rf $(TARGET_DIR)
	@rm -f Cargo.lock
	@echo "$(GREEN)Clean complete!$(NC)"

# Quick shortcuts
t: test
b: build
r: release
c: clean

.PHONY: help build release test test-verbose bench doc coverage \
        macos macos-universal ios android linux windows \
        cross-setup cross-all macos-arm64 macos-x86_64 ios-arm64 ios-sim windows-x64 linux-arm64 \
        ci-test ci-all ci-format-check ci-clippy ci-check ci-test-verbose ci-doc-test ci-coverage ci-security \
        format format-check lint check install package clean distclean t b r c
