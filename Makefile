# Lumen Database Makefile
# Shortcuts for common development tasks

# Default target
.DEFAULT_GOAL := help

# Variables
BUILD_DIR := build
DIST_DIR := dist
CORES := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CMAKE := cmake
CTEST := ctest

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
	@echo "  make build         - Debug build with tests"
	@echo "  make release       - Release build for distribution"
	@echo "  make test          - Run all tests"
	@echo "  make test-verbose  - Run tests with detailed output"
	@echo "  make coverage      - Generate coverage report"
	@echo "  make clean         - Clean build directory"
	@echo "  make distclean     - Clean everything (build + dist)"
	@echo ""
	@echo "$(YELLOW)Platform Builds:$(NC)"
	@echo "  make macos         - Build for macOS (current arch)"
	@echo "  make ios           - Build for iOS"
	@echo "  make android       - Build for Android"
	@echo "  make linux         - Build for Linux"
	@echo "  make windows       - Build for Windows"
	@echo ""
	@echo "$(YELLOW)Cross Compilation:$(NC)"
	@echo "  make macos-arm64   - Build for macOS ARM64"
	@echo "  make macos-x86_64  - Build for macOS x86_64"
	@echo "  make ios-arm64     - Build for iOS ARM64"
	@echo "  make ios-sim       - Build for iOS Simulator"
	@echo ""
	@echo "$(YELLOW)CI/CD:$(NC)"
	@echo "  make ci-test       - Run CI tests"
	@echo "  make format        - Format code with clang-format"
	@echo "  make lint          - Run clang-tidy"
	@echo "  make check         - Format + lint + test"
	@echo ""
	@echo "$(YELLOW)Utilities:$(NC)"
	@echo "  make benchmark     - Run benchmarks"
	@echo "  make install       - Install libraries"
	@echo "  make package       - Create distribution package"

# Development targets
build: clean
	@echo "$(GREEN)Building Lumen (Debug)...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && CC=$${CC:-clang} CXX=$${CXX:-clang++} $(CMAKE) .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)Build complete!$(NC)"

release: clean
	@echo "$(GREEN)Building Lumen (Release)...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && CC=$${CC:-clang} CXX=$${CXX:-clang++} $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)Release build complete! Check dist/ directory$(NC)"

test: build
	@echo "$(GREEN)Running tests...$(NC)"
	@cd $(BUILD_DIR) && ./bin/lumen_tests
	@echo "$(GREEN)All tests passed!$(NC)"

test-verbose: build
	@echo "$(GREEN)Running tests (verbose)...$(NC)"
	@cd $(BUILD_DIR) && ./bin/lumen_tests --gtest_color=yes --gtest_output=json:test_results.json

coverage: clean
	@echo "$(GREEN)Building with coverage...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && CC=$${CC:-clang} CXX=$${CXX:-clang++} $(CMAKE) .. -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@cd $(BUILD_DIR) && $(MAKE) coverage
	@echo "$(GREEN)Coverage report: $(BUILD_DIR)/coverage/index.html$(NC)"

benchmark: build
	@echo "$(GREEN)Running benchmarks...$(NC)"
	@cd $(BUILD_DIR) && ./bin/lumen_benchmarks

# Platform-specific builds
macos: macos-$(shell uname -m)

macos-arm64: clean
	@echo "$(GREEN)Building for macOS ARM64...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_OSX_ARCHITECTURES=arm64 -DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)macOS ARM64 build complete!$(NC)"

macos-x86_64: clean
	@echo "$(GREEN)Building for macOS x86_64...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_OSX_ARCHITECTURES=x86_64 -DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)macOS x86_64 build complete!$(NC)"

ios: ios-arm64

ios-arm64: clean
	@echo "$(GREEN)Building for iOS ARM64...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/iOS.cmake \
		-DIOS_PLATFORM=OS -DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)iOS ARM64 build complete!$(NC)"

ios-sim: clean
	@echo "$(GREEN)Building for iOS Simulator...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/iOS.cmake \
		-DIOS_PLATFORM=SIMULATOR -DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)iOS Simulator build complete!$(NC)"

android: clean
	@echo "$(GREEN)Building for Android...$(NC)"
	@if [ -z "$$ANDROID_NDK_HOME" ] && [ -z "$$ANDROID_NDK_ROOT" ]; then \
		echo "$(RED)Error: ANDROID_NDK_HOME or ANDROID_NDK_ROOT not set$(NC)"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/Android.cmake \
		-DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)Android build complete!$(NC)"

linux: clean
	@echo "$(GREEN)Building for Linux...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && CC=$${CC:-clang} CXX=$${CXX:-clang++} $(CMAKE) .. \
		-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)Linux build complete!$(NC)"

windows: windows-cross

# Detect Visual Studio version for Windows builds
ifeq ($(OS),Windows_NT)
    VS_VERSION := $(shell cmake --help | grep -o "Visual Studio [0-9][0-9] [0-9][0-9][0-9][0-9]" | head -1 || echo "Visual Studio 16 2019")
else
    VS_VERSION := Visual Studio 16 2019
endif

windows-debug: clean
	@echo "$(GREEN)Building for Windows (Debug) using $(VS_VERSION)...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -G "$(VS_VERSION)" -A x64 -DBUILD_TESTS=ON
	@cd $(BUILD_DIR) && cmake --build . --config Debug --parallel
	@echo "$(GREEN)Windows Debug build complete!$(NC)"

windows-release: clean
	@echo "$(GREEN)Building for Windows (Release) using $(VS_VERSION)...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -G "$(VS_VERSION)" -A x64 -DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && cmake --build . --config Release --parallel
	@echo "$(GREEN)Windows Release build complete!$(NC)"

windows-test: windows-debug
	@echo "$(GREEN)Running Windows tests...$(NC)"
	@cd $(BUILD_DIR) && ./bin/Debug/lumen_tests.exe

# Cross-compilation for Windows from Unix
windows-cross: clean
	@echo "$(GREEN)Cross-compiling for Windows x64...$(NC)"
	@if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then \
		echo "$(RED)MinGW-w64 not found. Install with:$(NC)"; \
		echo "  Ubuntu/Debian: sudo apt-get install mingw-w64"; \
		echo "  macOS: brew install mingw-w64"; \
		echo "  Arch: sudo pacman -S mingw-w64"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/Windows.cmake \
		-DBUILD_TESTS=OFF
	@cd $(BUILD_DIR) && $(MAKE) -j$(CORES)
	@echo "$(GREEN)Windows cross-compilation complete!$(NC)"

# CI/CD targets
ci-test:
	@echo "$(GREEN)Running CI tests...$(NC)"
	@$(MAKE) clean
	@$(MAKE) build
	@$(MAKE) test
	@$(MAKE) lint || true
	@echo "$(GREEN)CI tests complete!$(NC)"

format:
	@echo "$(GREEN)Formatting code...$(NC)"
	@if command -v clang-format >/dev/null 2>&1; then \
		find src include tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i; \
		echo "$(GREEN)Code formatted!$(NC)"; \
	else \
		echo "$(YELLOW)clang-format not found. Install with: brew install clang-format$(NC)"; \
		exit 1; \
	fi

format-check:
	@echo "$(GREEN)Checking code format...$(NC)"
	@if command -v clang-format >/dev/null 2>&1; then \
		find src include tests -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror; \
		echo "$(GREEN)Code format check passed!$(NC)"; \
	else \
		echo "$(YELLOW)clang-format not found. Install with: brew install clang-format$(NC)"; \
		exit 1; \
	fi

lint:
	@echo "$(GREEN)Running clang-tidy...$(NC)"
	@if command -v clang-tidy >/dev/null 2>&1; then \
		mkdir -p $(BUILD_DIR); \
		cd $(BUILD_DIR) && CC=$${CC:-clang} CXX=$${CXX:-clang++} $(CMAKE) .. -DCMAKE_BUILD_TYPE=Debug; \
		find src -name "*.cpp" | xargs clang-tidy -p $(BUILD_DIR); \
	else \
		echo "$(YELLOW)clang-tidy not found. Install with: brew install llvm$(NC)"; \
		echo "$(YELLOW)Skipping lint check$(NC)"; \
	fi

check: format lint test
	@echo "$(GREEN)All checks passed!$(NC)"

# Installation
install: release
	@echo "$(GREEN)Installing Lumen...$(NC)"
	@cd $(BUILD_DIR) && $(MAKE) install
	@echo "$(GREEN)Installation complete!$(NC)"

package: release
	@echo "$(GREEN)Creating package...$(NC)"
	@cd $(BUILD_DIR) && $(MAKE) package
	@echo "$(GREEN)Package created in $(BUILD_DIR)$(NC)"

# Cleaning
clean:
	@echo "$(YELLOW)Cleaning build directory...$(NC)"
	@rm -rf $(BUILD_DIR)

distclean: clean
	@echo "$(YELLOW)Cleaning everything...$(NC)"
	@rm -rf $(DIST_DIR)
	@rm -rf third_party
	@find . -name "*.o" -delete
	@find . -name "*.a" -delete
	@find . -name "*.so" -delete
	@find . -name "*.dylib" -delete
	@echo "$(GREEN)Clean complete!$(NC)"

# Quick shortcuts
t: test
b: build
r: release
c: clean

.PHONY: help build release test test-verbose coverage benchmark \
        macos macos-arm64 macos-x86_64 ios ios-arm64 ios-sim \
        android linux windows windows-debug windows-release windows-test \
        ci-test format lint check install package clean distclean t b r c