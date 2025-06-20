# Lumen Database

[![CI](https://github.com/oyro-os/lumen/actions/workflows/ci.yml/badge.svg)](https://github.com/rock-ai/lumen/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/badge/coverage-87%25-brightgreen)](https://github.com/rock-ai/lumen/actions)
[![Rust](https://img.shields.io/badge/rust-1.84%2B-blue.svg)](https://www.rust-lang.org)
[![License](https://img.shields.io/badge/license-MIT%2FApache--2.0-blue)](LICENSE)

Lumen is a high-performance embedded database written in Rust, designed for extreme efficiency and reliability.

## Key Features

- **Extreme Performance**: Handle 1 billion records with just 100MB RAM
- **Zero Failure Tolerance**: Automatic recovery from any crash state
- **Laravel-Inspired API**: Clean, intuitive, and developer-friendly
- **Cross-Platform**: Native support for macOS, iOS, Android, Windows, and Linux
- **Memory Safety**: Written in Rust for guaranteed memory safety
- **Zero-Cost FFI**: Efficient bindings for Dart, Swift, Kotlin, and more

## Why Rust?

- **Performance**: Matches or exceeds C performance with zero-cost abstractions
- **Safety**: Memory safety guarantees eliminate entire classes of bugs
- **Concurrency**: Fearless concurrency for better multi-threaded performance
- **Modern Tooling**: Cargo, rustfmt, and clippy provide superior developer experience

## Design Goals

1. **Memory Efficiency**: Optimized for minimal memory usage (target: 10-100MB)
2. **Read Performance**: Sub-millisecond queries on billion-record datasets
3. **Reliability**: ACID compliance with automatic crash recovery
4. **Simplicity**: Intuitive API that's easy to learn and use
5. **Portability**: Single codebase for all platforms

## Quick Start

### Prerequisites

- Rust 1.70+ (install from [rustup.rs](https://rustup.rs/))
- macOS, Linux, or Windows

### Building

```bash
# Clone the repository
git clone https://github.com/yourusername/lumen.git
cd lumen

# Build debug version
make build

# Build release version
make release

# Run tests
make test

# Run benchmarks
make bench
```

### Example Usage

```rust
use lumen::{Storage, Database};

// Create or open a database
let storage = Storage::create("myapp.lumen")?;
let db = storage.db("main")?;

// Create a table
db.schema().create("users", |table| {
    table.id();
    table.string("name");
    table.string("email").unique();
    table.timestamp("created_at");
});

// Insert data
db.table("users")?
    .insert()
    .values(vec![
        ("name", "John Doe"),
        ("email", "john@example.com"),
    ])
    .execute()?;

// Query data
let users = db.table("users")?
    .select(vec!["id", "name", "email"])
    .where_("email", "=", "john@example.com")
    .get()?;
```

## Architecture

Lumen uses a sophisticated architecture optimized for performance:

- **Page-based storage**: 4KB pages with direct memory mapping
- **B+Tree indexes**: Nodes ARE pages for zero-copy access
- **Write-Ahead Logging**: Ensures durability and crash recovery
- **Adaptive memory management**: Scales from 10MB to available RAM
- **Lock-free data structures**: For maximum concurrency

## Development

```bash
# Format code
make format

# Run linter
make lint

# Run all checks
make check

# Generate documentation
make doc

# Clean build artifacts
make clean
```

## Platform Support

- **macOS**: Universal binary (Apple Silicon + Intel)
- **iOS**: Universal framework
- **Android**: AAR with JNI bindings
- **Windows**: Native DLL
- **Linux**: Shared library

## License

Lumen is dual-licensed under MIT and Apache 2.0. Choose whichever license works best for you.

## Contributing

Contributions are welcome! Please read our [Contributing Guide](CONTRIBUTING.md) for details.

## Status

This project is currently in active development. See [ROADMAP.md](ROADMAP.md) for the development plan.
