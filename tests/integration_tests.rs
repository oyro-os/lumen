//! Integration tests for Lumen database
//!
//! These tests verify that multiple components work together correctly.

mod integration;

use integration::{ComponentTester, ErrorTester, TestEnvironment};
use lumen::{
    common::{logging, Error},
    VERSION,
};

#[test]
fn test_version_and_logging_integration() {
    let env = TestEnvironment::new().expect("Should create test environment");

    // Test that version is accessible
    assert_eq!(VERSION, "0.1.0");

    // Log some test messages (logging already initialized by test environment)
    lumen::lumen_info!("Integration test starting in {:?}", env.temp_path());
    lumen::lumen_debug!("Version: {}", VERSION);
}

#[test]
fn test_error_system_integration() {
    let _env = TestEnvironment::new().expect("Should create test environment");

    // Test error creation and propagation
    let io_error = Error::io("Test IO error");
    assert!(io_error.is_io());
    assert!(io_error.is_recoverable());

    let corruption_error = Error::corruption("Test corruption");
    assert!(corruption_error.is_corruption());
    assert!(!corruption_error.is_recoverable());

    // Test error conversion
    let std_error = std::io::Error::new(std::io::ErrorKind::NotFound, "File not found");
    let lumen_error: Error = std_error.into();
    assert!(lumen_error.is_io());
}

#[test]
fn test_basic_functionality_integration() {
    let _env = TestEnvironment::new().expect("Should create test environment");

    // Test basic library functionality
    assert_eq!(VERSION, "0.1.0");

    // Test that we can create and use errors
    let test_error = Error::io("Integration test error");
    assert!(test_error.is_io());
    assert!(test_error.is_recoverable());

    // Test logging functionality (already initialized by test environment)
    lumen::lumen_info!("Integration test completed successfully");
}

#[test]
fn test_component_interaction() {
    let tester = ComponentTester::new().expect("Should create component tester");

    tester
        .test_interaction(|env| {
            // Test that we can create files and verify them
            let test_data = b"Lumen integration test data";
            let _file_path = env.create_test_file("integration_test.dat", test_data)?;

            env.verify_file_content("integration_test.dat", test_data)?;

            // Test error handling integration
            ErrorTester::assert_io_error(|| -> lumen::Result<()> {
                env.verify_file_content("nonexistent.txt", b"test")?;
                Ok(())
            });

            Ok(())
        })
        .expect("Should run component interaction test");
}

#[test]
fn test_timer_integration() {
    let _env = TestEnvironment::new().expect("Should create test environment");

    // Test timer functionality
    let timer = logging::Timer::start("integration_test_timer");

    // Simulate some work
    std::thread::sleep(std::time::Duration::from_millis(1));

    let elapsed = timer.stop();
    assert!(elapsed >= std::time::Duration::from_millis(1));
}
