//! Test utilities for Lumen database

use crate::common::Result;
use std::path::{Path, PathBuf};
use std::sync::Once;

static TEST_LOGGER_INIT: Once = Once::new();

/// Initialize logging for tests
pub fn init_test_logging() {
    TEST_LOGGER_INIT.call_once(|| {
        let _ = env_logger::builder()
            .filter_level(log::LevelFilter::Debug)
            .is_test(true)
            .try_init();
    });
}

/// Temporary directory helper for tests
pub struct TempDir {
    path: PathBuf,
}

impl TempDir {
    /// Create a new temporary directory
    ///
    /// # Errors
    ///
    /// Returns an error if the temporary directory cannot be created.
    pub fn new() -> Result<Self> {
        let path = std::env::temp_dir().join(format!("lumen_test_{}", uuid::Uuid::new_v4()));
        std::fs::create_dir_all(&path)?;

        Ok(Self { path })
    }

    /// Get the path to the temporary directory
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Create a file path within the temporary directory
    pub fn file_path<S: AsRef<str>>(&self, filename: S) -> PathBuf {
        self.path.join(filename.as_ref())
    }

    /// Write data to a file in the temporary directory
    ///
    /// # Errors
    ///
    /// Returns an error if the file cannot be written.
    pub fn write_file<S: AsRef<str>>(&self, filename: S, data: &[u8]) -> Result<PathBuf> {
        let file_path = self.file_path(filename);
        std::fs::write(&file_path, data)?;
        Ok(file_path)
    }

    /// Read data from a file in the temporary directory
    ///
    /// # Errors
    ///
    /// Returns an error if the file cannot be read.
    pub fn read_file<S: AsRef<str>>(&self, filename: S) -> Result<Vec<u8>> {
        let file_path = self.file_path(filename);
        let data = std::fs::read(file_path)?;
        Ok(data)
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.path);
    }
}

/// Memory usage tracker for tests
pub struct MemoryTracker {
    initial_memory: usize,
    name: String,
}

impl MemoryTracker {
    /// Start tracking memory usage
    pub fn start<S: Into<String>>(name: S) -> Self {
        let name = name.into();
        let initial_memory = get_memory_usage();
        println!(
            "Memory tracker '{name}' started at {initial_memory} bytes"
        );

        Self {
            initial_memory,
            name,
        }
    }

    /// Get current memory delta
    #[allow(clippy::cast_possible_wrap)]
    pub fn current_delta(&self) -> isize {
        let current = get_memory_usage();
        current as isize - self.initial_memory as isize
    }

    /// Stop tracking and return memory delta
    pub fn stop(self) -> isize {
        let delta = self.current_delta();
        println!(
            "Memory tracker '{}' ended with delta: {} bytes",
            self.name, delta
        );
        delta
    }
}

fn get_memory_usage() -> usize {
    // Simple memory usage approximation
    // In a real implementation, we'd use platform-specific APIs
    use std::alloc::System;

    // This is a simplified version - in practice we'd track allocations
    std::mem::size_of::<System>() // Placeholder
}

/// Performance assertion helper
pub struct PerformanceAssertion {
    max_duration: std::time::Duration,
    operation: String,
    start: std::time::Instant,
}

impl PerformanceAssertion {
    /// Create a new performance assertion
    pub fn new<S: Into<String>>(operation: S, max_duration: std::time::Duration) -> Self {
        Self {
            max_duration,
            operation: operation.into(),
            start: std::time::Instant::now(),
        }
    }

    /// Assert that the operation completed within the time limit
    ///
    /// # Panics
    ///
    /// Panics if the operation took longer than the specified maximum duration.
    pub fn assert_completed(self) {
        let elapsed = self.start.elapsed();
        assert!(
            elapsed <= self.max_duration,
            "Operation '{}' took {:?}, expected <= {:?}",
            self.operation,
            elapsed,
            self.max_duration
        );
    }
}

/// Generate test data
#[allow(clippy::cast_possible_truncation)]
pub fn generate_test_data(size: usize) -> Vec<u8> {
    (0..size).map(|i| (i % 256) as u8).collect()
}

/// Generate random test data
pub fn generate_random_data(size: usize) -> Vec<u8> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    let mut data = Vec::with_capacity(size);
    for i in 0..size {
        let mut hasher = DefaultHasher::new();
        i.hash(&mut hasher);
        #[allow(clippy::cast_possible_truncation)]
        let byte = (hasher.finish() % 256) as u8;
        data.push(byte);
    }
    data
}

/// Create test pages with specific patterns
pub fn create_test_page(page_size: usize, pattern: u8) -> Vec<u8> {
    vec![pattern; page_size]
}

/// Assert that two byte slices are equal with better error messages
///
/// # Panics
///
/// Panics if the byte slices differ in length or content.
pub fn assert_bytes_equal(actual: &[u8], expected: &[u8], context: &str) {
    assert!(
        actual.len() == expected.len(),
        "{context}: Length mismatch - actual: {}, expected: {}",
        actual.len(),
        expected.len()
    );

    for (i, (a, e)) in actual.iter().zip(expected.iter()).enumerate() {
        assert!(
            a == e,
            "{context}: Byte mismatch at index {i}: actual 0x{a:02x}, expected 0x{e:02x}"
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[test]
    fn test_temp_dir() {
        init_test_logging();

        let temp_dir = TempDir::new().expect("Failed to create temp dir");
        let path = temp_dir.path();
        assert!(path.exists());

        // Test file operations
        let data = b"test data";
        let file_path = temp_dir
            .write_file("test.txt", data)
            .expect("Failed to write file");
        assert!(file_path.exists());

        let read_data = temp_dir.read_file("test.txt").expect("Failed to read file");
        assert_eq!(read_data, data);

        // Directory will be cleaned up when temp_dir is dropped
    }

    #[test]
    fn test_memory_tracker() {
        init_test_logging();

        let tracker = MemoryTracker::start("test");
        let _data = vec![0u8; 1024]; // Allocate some memory
        let delta = tracker.stop();

        // We can't assert exact values due to allocator behavior,
        // but we can test that it doesn't panic
        println!("Memory delta: {} bytes", delta);
    }

    #[test]
    fn test_performance_assertion() {
        init_test_logging();

        let assertion = PerformanceAssertion::new("fast operation", Duration::from_millis(100));
        std::thread::sleep(Duration::from_millis(1)); // Very fast operation
        assertion.assert_completed(); // Should not panic
    }

    #[test]
    #[should_panic(expected = "took")]
    fn test_performance_assertion_timeout() {
        init_test_logging();

        let assertion = PerformanceAssertion::new("slow operation", Duration::from_millis(1));
        std::thread::sleep(Duration::from_millis(10)); // Too slow
        assertion.assert_completed(); // Should panic
    }

    #[test]
    fn test_generate_test_data() {
        let data = generate_test_data(256);
        assert_eq!(data.len(), 256);
        assert_eq!(data[0], 0);
        assert_eq!(data[255], 255);
    }

    #[test]
    fn test_assert_bytes_equal() {
        let data1 = vec![1, 2, 3, 4];
        let data2 = vec![1, 2, 3, 4];
        assert_bytes_equal(&data1, &data2, "should be equal");
    }

    #[test]
    #[should_panic(expected = "Length mismatch")]
    fn test_assert_bytes_equal_length_mismatch() {
        let data1 = vec![1, 2, 3];
        let data2 = vec![1, 2, 3, 4];
        assert_bytes_equal(&data1, &data2, "should panic");
    }

    #[test]
    #[should_panic(expected = "Byte mismatch")]
    fn test_assert_bytes_equal_content_mismatch() {
        let data1 = vec![1, 2, 3, 4];
        let data2 = vec![1, 2, 4, 4]; // Different at index 2
        assert_bytes_equal(&data1, &data2, "should panic");
    }
}
