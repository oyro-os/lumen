//! Integration test framework for Lumen database
//!
//! This module provides utilities for testing multiple components together
//! and ensuring they work correctly as a system.

use lumen::common::test_utils::{TempDir, init_test_logging};
use lumen::common::{Error, Result};
use std::path::Path;

/// Integration test environment
pub struct TestEnvironment {
    /// Temporary directory for test files
    pub temp_dir: TempDir,
    /// Database path within temp directory
    pub db_path: std::path::PathBuf,
}

impl TestEnvironment {
    /// Create a new test environment
    pub fn new() -> Result<Self> {
        init_test_logging();
        
        let temp_dir = TempDir::new()?;
        let db_path = temp_dir.path().join("test.lumen");
        
        Ok(Self {
            temp_dir,
            db_path,
        })
    }

    /// Get the database path
    pub fn db_path(&self) -> &Path {
        &self.db_path
    }

    /// Get the temp directory path
    pub fn temp_path(&self) -> &Path {
        self.temp_dir.path()
    }

    /// Create a test file with given content
    pub fn create_test_file(&self, name: &str, content: &[u8]) -> Result<std::path::PathBuf> {
        let file_path = self.temp_dir.path().join(name);
        std::fs::write(&file_path, content)
            .map_err(|e| Error::io(format!("Failed to create test file: {}", e)))?;
        Ok(file_path)
    }

    /// Verify a file exists and has expected content
    pub fn verify_file_content(&self, name: &str, expected: &[u8]) -> Result<()> {
        let file_path = self.temp_dir.path().join(name);
        let content = std::fs::read(&file_path)
            .map_err(|e| Error::io(format!("Failed to read test file: {}", e)))?;
        
        if content != expected {
            return Err(Error::invalid_input(format!(
                "File content mismatch. Expected {} bytes, got {} bytes",
                expected.len(),
                content.len()
            )));
        }
        
        Ok(())
    }
}

impl Default for TestEnvironment {
    fn default() -> Self {
        Self::new().expect("Failed to create test environment")
    }
}

/// Helper for testing error conditions
pub struct ErrorTester;

impl ErrorTester {
    /// Test that a function returns a specific error type
    pub fn assert_error_type<T: std::fmt::Debug, F>(func: F, expected_predicate: fn(&Error) -> bool)
    where
        F: FnOnce() -> Result<T>,
    {
        let result = func();
        assert!(result.is_err(), "Expected error, got success");
        
        let error = result.unwrap_err();
        assert!(
            expected_predicate(&error),
            "Error type mismatch. Got: {:?}",
            error
        );
    }

    /// Test that a function returns an IO error
    pub fn assert_io_error<F>(func: F)
    where
        F: FnOnce() -> Result<()>,
    {
        Self::assert_error_type(func, |e| e.is_io());
    }

    /// Test that a function returns a corruption error
    pub fn assert_corruption_error<F>(func: F)
    where
        F: FnOnce() -> Result<()>,
    {
        Self::assert_error_type(func, |e| e.is_corruption());
    }
}

/// Multi-component test helper
pub struct ComponentTester {
    env: TestEnvironment,
}

impl ComponentTester {
    /// Create a new component tester
    pub fn new() -> Result<Self> {
        Ok(Self {
            env: TestEnvironment::new()?,
        })
    }

    /// Get the test environment
    #[allow(dead_code)]
    pub fn env(&self) -> &TestEnvironment {
        &self.env
    }

    /// Test component interaction
    pub fn test_interaction<F>(&self, test_fn: F) -> Result<()>
    where
        F: FnOnce(&TestEnvironment) -> Result<()>,
    {
        test_fn(&self.env)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_environment_creation() {
        let env = TestEnvironment::new().expect("Should create test environment");
        
        // Test that temp directory exists
        assert!(env.temp_path().exists());
        assert!(env.temp_path().is_dir());
        
        // Test that db path is set correctly
        assert!(env.db_path().parent().unwrap().exists());
        assert_eq!(env.db_path().file_name().unwrap(), "test.lumen");
    }

    #[test]
    fn test_file_operations() {
        let env = TestEnvironment::new().expect("Should create test environment");
        
        let test_content = b"Hello, Lumen!";
        let file_path = env.create_test_file("test.txt", test_content)
            .expect("Should create test file");
        
        assert!(file_path.exists());
        
        env.verify_file_content("test.txt", test_content)
            .expect("Should verify file content");
    }

    #[test]
    fn test_error_tester() {
        // Test IO error detection
        ErrorTester::assert_io_error(|| -> Result<()> {
            Err(Error::io("Test IO error"))
        });

        // Test corruption error detection
        ErrorTester::assert_corruption_error(|| -> Result<()> {
            Err(Error::corruption("Test corruption"))
        });
    }

    #[test]
    fn test_component_tester() {
        let tester = ComponentTester::new().expect("Should create component tester");
        
        tester.test_interaction(|env| {
            // Test that we can use the environment
            assert!(env.temp_path().exists());
            Ok(())
        }).expect("Should run interaction test");
    }
}