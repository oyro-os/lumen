//! Lumen - High-performance embedded database
//!
//! Lumen is a high-performance embedded database designed for:
//! - 1 billion records with 100MB RAM constraint
//! - Zero-failure tolerance
//! - Laravel-inspired clean, readable API
//! - Extreme read performance optimization
//! - Cross-platform: macOS, iOS, Android, Windows, Linux

#![warn(missing_docs)]
#![warn(rust_2018_idioms)]
#![warn(clippy::all)]
#![warn(clippy::pedantic)]
#![allow(clippy::module_name_repetitions)]
#![allow(clippy::must_use_candidate)]

// Core modules
pub mod common;
pub mod index;
pub mod query;
pub mod storage;
pub mod types;

// Re-exports for convenience
pub use common::{Error, Result};

/// Version information
pub const VERSION_MAJOR: u32 = 0;
/// Version information
pub const VERSION_MINOR: u32 = 1;
/// Version information  
pub const VERSION_PATCH: u32 = 0;
/// Version string
pub const VERSION: &str = "0.1.0";

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version() {
        assert_eq!(VERSION, "0.1.0");
        assert_eq!(VERSION_MAJOR, 0);
        assert_eq!(VERSION_MINOR, 1);
        assert_eq!(VERSION_PATCH, 0);
    }
}
