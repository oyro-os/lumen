//! Logging infrastructure for Lumen database

use log::Level;
use std::sync::Once;

static INIT: Once = Once::new();

/// Initialize the Lumen logging system
///
/// This function should be called once at the start of your application.
/// It sets up the logger with appropriate formatting and filtering.
pub fn init() {
    INIT.call_once(|| {
        let mut builder = env_logger::Builder::from_default_env();

        builder
            .format(|buf, record| {
                use std::io::Write;

                let timestamp = chrono::Utc::now().format("%Y-%m-%d %H:%M:%S%.3f");
                let level = record.level();
                let target = record.target();

                // Color the level based on severity
                let level_str = match level {
                    Level::Error => "\x1b[31mERROR\x1b[0m", // Red
                    Level::Warn => "\x1b[33mWARN\x1b[0m",   // Yellow
                    Level::Info => "\x1b[32mINFO\x1b[0m",   // Green
                    Level::Debug => "\x1b[36mDEBUG\x1b[0m", // Cyan
                    Level::Trace => "\x1b[37mTRACE\x1b[0m", // White
                };

                writeln!(
                    buf,
                    "{} [{}] {}: {}",
                    timestamp,
                    level_str,
                    target,
                    record.args()
                )
            })
            .filter_level(log::LevelFilter::Info) // Default to Info level
            .init();

        log::info!("Lumen logging system initialized");
    });
}

/// Initialize logging with a specific level
pub fn init_with_level(level: log::LevelFilter) {
    INIT.call_once(|| {
        let mut builder = env_logger::Builder::new();

        builder
            .format(|buf, record| {
                use std::io::Write;

                let timestamp = chrono::Utc::now().format("%Y-%m-%d %H:%M:%S%.3f");
                let level = record.level();
                let target = record.target();

                writeln!(
                    buf,
                    "{} [{}] {}: {}",
                    timestamp,
                    level,
                    target,
                    record.args()
                )
            })
            .filter_level(level)
            .init();

        log::info!("Lumen logging system initialized with level: {level:?}");
    });
}

/// Log an error message with Lumen context
#[macro_export]
macro_rules! lumen_error {
    ($($arg:tt)*) => {
        log::error!(target: "lumen", $($arg)*)
    };
}

/// Log a warning message with Lumen context
#[macro_export]
macro_rules! lumen_warn {
    ($($arg:tt)*) => {
        log::warn!(target: "lumen", $($arg)*)
    };
}

/// Log an info message with Lumen context
#[macro_export]
macro_rules! lumen_info {
    ($($arg:tt)*) => {
        log::info!(target: "lumen", $($arg)*)
    };
}

/// Log a debug message with Lumen context
#[macro_export]
macro_rules! lumen_debug {
    ($($arg:tt)*) => {
        log::debug!(target: "lumen", $($arg)*)
    };
}

/// Log a trace message with Lumen context
#[macro_export]
macro_rules! lumen_trace {
    ($($arg:tt)*) => {
        log::trace!(target: "lumen", $($arg)*)
    };
}

/// Performance timing helper
pub struct Timer {
    start: std::time::Instant,
    operation: String,
}

impl Timer {
    /// Start timing an operation
    pub fn start<S: Into<String>>(operation: S) -> Self {
        let operation = operation.into();
        lumen_debug!("Starting operation: {}", operation);
        Self {
            start: std::time::Instant::now(),
            operation,
        }
    }

    /// Get elapsed time without stopping the timer
    pub fn elapsed(&self) -> std::time::Duration {
        self.start.elapsed()
    }

    /// Stop the timer and log the elapsed time
    pub fn stop(self) -> std::time::Duration {
        let elapsed = self.start.elapsed();
        lumen_debug!("Operation '{}' completed in {:?}", self.operation, elapsed);
        elapsed
    }
}

impl Drop for Timer {
    fn drop(&mut self) {
        let elapsed = self.start.elapsed();
        if elapsed > std::time::Duration::from_millis(10) {
            lumen_warn!("Slow operation '{}' took {:?}", self.operation, elapsed);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use log::LevelFilter;

    #[test]
    fn test_logging_init() {
        // Test that we can initialize logging without panicking
        init_with_level(LevelFilter::Debug);

        // Test that we can use the logging macros
        lumen_info!("Test log message");
        lumen_debug!("Debug message with value: {}", 42);
        lumen_error!("Error message");
    }

    #[test]
    fn test_timer() {
        let timer = Timer::start("test operation");
        std::thread::sleep(std::time::Duration::from_millis(1));
        let elapsed = timer.stop();
        assert!(elapsed >= std::time::Duration::from_millis(1));
    }

    #[test]
    fn test_timer_elapsed() {
        let timer = Timer::start("test operation");
        std::thread::sleep(std::time::Duration::from_millis(1));
        let elapsed1 = timer.elapsed();
        std::thread::sleep(std::time::Duration::from_millis(1));
        let elapsed2 = timer.elapsed();

        assert!(elapsed2 > elapsed1);
        timer.stop(); // Clean up
    }
}
