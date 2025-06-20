# Lumen Database - Developer Guide

## Quick Start

### Prerequisites

- Rust 1.70+ (install via [rustup](https://rustup.rs/))
- Git
- Clang/LLVM (for some dependencies)

### Clone and Build

```bash
git clone https://github.com/yourusername/lumen.git
cd lumen
cargo build
```

### Run Tests

```bash
cargo test                    # Run all tests
cargo test --workspace       # Run tests for all crates
cargo test integration       # Run integration tests only
```

### Run Benchmarks

```bash
cargo bench                   # Run performance benchmarks
```

## Project Structure

```
lumen/
├── src/                      # Main Rust library
│   ├── lib.rs               # Library entry point
│   ├── common/              # Common utilities
│   │   ├── error.rs         # Error handling
│   │   ├── logging.rs       # Logging infrastructure
│   │   └── test_utils.rs    # Test utilities
│   ├── storage/             # Storage layer (Phase 2+)
│   ├── index/               # Index implementations (Phase 4+)
│   ├── query/               # Query engine (Phase 6+)
│   └── types/               # Type definitions (Phase 3+)
├── lumen-ffi/               # C FFI bindings
│   ├── src/lib.rs           # FFI implementation
│   ├── build.rs             # C header generation
│   └── include/lumen.h      # Generated C header
├── tests/                   # Integration tests
│   ├── integration/         # Integration test framework
│   └── integration_tests.rs # Integration test cases
├── benches/                 # Performance benchmarks
├── docs/                    # Documentation
└── Cargo.toml               # Workspace configuration
```

## Development Workflow

### 1. Code Style

We use standard Rust formatting:

```bash
cargo fmt                     # Format code
cargo fmt --check            # Check formatting without changing
```

### 2. Code Quality

We enforce strict linting:

```bash
cargo clippy                  # Run clippy linter
cargo clippy -- -D warnings  # Treat warnings as errors
```

### 3. Testing Strategy

#### Unit Tests
- Located in `#[cfg(test)]` modules within source files
- Test individual functions and modules
- Run with `cargo test`

#### Integration Tests
- Located in `tests/` directory
- Test multiple components working together
- Use the integration test framework in `tests/integration/`

#### Benchmarks
- Located in `benches/` directory
- Use Criterion.rs for performance testing
- Run with `cargo bench`

### 4. Documentation

Generate and view documentation:

```bash
cargo doc --open              # Generate and open docs
cargo doc --all-features     # Include all feature flags
```

Write documentation using Rust doc comments:

```rust
/// This function does something important
/// 
/// # Arguments
/// 
/// * `arg` - Description of the argument
/// 
/// # Returns
/// 
/// Description of what is returned
/// 
/// # Example
/// 
/// ```
/// use lumen::example_function;
/// let result = example_function(42);
/// ```
pub fn example_function(arg: i32) -> String {
    // implementation
}
```

## FFI Development

### Building FFI Bindings

```bash
cargo build -p lumen-ffi      # Build FFI crate
```

This generates:
- `target/debug/liblumen_ffi.dylib` (macOS) or `.so` (Linux)
- `target/debug/liblumen_ffi.a` (static library)
- `lumen-ffi/include/lumen.h` (C header)

### Testing FFI

```bash
cargo test -p lumen-ffi       # Run FFI tests
```

### Using from C

```c
#include "lumen.h"
#include <stdio.h>

int main() {
    // Initialize logging
    struct LumenResult result = lumen_init_logging();
    if (result.code != 0) {
        printf("Failed to initialize logging\\n");
        return 1;
    }
    
    // Get version
    char* version = lumen_version();
    printf("Lumen version: %s\\n", version);
    lumen_free_string(version);
    
    return 0;
}
```

## Error Handling

Lumen uses a comprehensive error system:

```rust
use lumen::{Result, Error};

fn example_function() -> Result<String> {
    // Use custom error constructors
    if some_condition {
        return Err(Error::invalid_input("Bad parameter"));
    }
    
    // Automatic conversion from std errors
    let content = std::fs::read_to_string("file.txt")?;
    
    Ok(content)
}

// Check error types
let result = example_function();
if let Err(error) = result {
    if error.is_io() {
        println!("I/O error: {}", error);
    }
    
    if error.is_recoverable() {
        // Retry logic
    }
}
```

## Logging

Use Lumen's structured logging:

```rust
use lumen::{lumen_info, lumen_debug, lumen_error};

// Initialize logging (once per application)
lumen::common::logging::init();

// Log messages with different levels
lumen_info!("Operation started");
lumen_debug!("Processing {} items", count);
lumen_error!("Operation failed: {}", error);

// Performance timing
let timer = lumen::common::logging::Timer::start("expensive_operation");
// ... do work ...
let elapsed = timer.stop(); // Automatically logs timing
```

## Testing Best Practices

### Use Test Utilities

```rust
use lumen::common::test_utils::{TempDir, init_test_logging};

#[test]
fn test_file_operations() {
    init_test_logging();
    
    let temp_dir = TempDir::new().expect("Should create temp dir");
    let file_path = temp_dir.path().join("test.dat");
    
    // Test implementation...
}
```

### Integration Testing

```rust
use crate::integration::{TestEnvironment, ComponentTester};

#[test]
fn test_component_interaction() {
    let tester = ComponentTester::new().expect("Should create tester");
    
    tester.test_interaction(|env| {
        // Test multiple components together
        let test_data = b"test data";
        env.create_test_file("test.dat", test_data)?;
        env.verify_file_content("test.dat", test_data)?;
        Ok(())
    }).expect("Should pass integration test");
}
```

## Performance Considerations

### Benchmarking

```rust
use criterion::{criterion_group, criterion_main, Criterion};

fn benchmark_function(c: &mut Criterion) {
    c.bench_function("my_function", |b| {
        b.iter(|| {
            // Code to benchmark
        })
    });
}

criterion_group!(benches, benchmark_function);
criterion_main!(benches);
```

### Memory Management

- Use `#[repr(C)]` for FFI-compatible structs
- Leverage Rust's ownership system for memory safety
- Use `bytemuck` for zero-copy operations where possible
- Monitor memory usage with test utilities

## Common Commands

```bash
# Development
cargo build                   # Debug build
cargo build --release        # Release build (optimized)
cargo test                   # Run all tests
cargo bench                  # Run benchmarks

# Code Quality
cargo fmt                    # Format code
cargo clippy                 # Lint code
cargo check                  # Fast type checking

# Documentation
cargo doc --open             # Generate and view docs

# Workspace
cargo build --workspace      # Build all crates
cargo test --workspace       # Test all crates

# FFI
cargo build -p lumen-ffi     # Build FFI bindings
cargo test -p lumen-ffi      # Test FFI bindings

# Coverage (with cargo-llvm-cov)
cargo llvm-cov --html        # Generate HTML coverage report
```

## IDE Setup

### VS Code

Recommended extensions:
- rust-analyzer
- CodeLLDB (for debugging)
- Even Better TOML

### Settings

Add to `.vscode/settings.json`:

```json
{
    "rust-analyzer.cargo.features": "all",
    "rust-analyzer.checkOnSave.command": "clippy"
}
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `cargo test --workspace`
5. Run linting: `cargo clippy -- -D warnings`
6. Format code: `cargo fmt`
7. Submit a pull request

### Commit Message Format

```
type(scope): brief description

Longer description if needed.

- List specific changes
- Use present tense
- Keep first line under 50 characters
```

Examples:
- `feat(storage): add page-based storage system`
- `fix(error): improve error message clarity`
- `docs(guide): add FFI usage examples`

## Troubleshooting

### Build Issues

**Problem**: `cbindgen` fails to generate headers
**Solution**: Ensure you have a recent version of cbindgen:
```bash
cargo install cbindgen --force
```

**Problem**: Tests fail with logging initialization errors
**Solution**: Use `init_test_logging()` instead of `logging::init()` in tests

**Problem**: FFI tests don't compile
**Solution**: Make sure you're in the workspace root and run:
```bash
cargo build --workspace
```

### Performance Issues

- Use `cargo build --release` for performance testing
- Profile with `cargo flamegraph` (requires cargo-flamegraph)
- Check memory usage with test utilities

### Getting Help

- Check existing issues on GitHub
- Read the API documentation: `cargo doc --open`
- Look at integration tests for usage examples