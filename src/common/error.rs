//! Error handling for Lumen database

use std::fmt;

/// Common result type for Lumen operations
pub type Result<T> = std::result::Result<T, Error>;

/// Main error type for Lumen database operations
#[derive(Debug, Clone, PartialEq)]
pub enum Error {
    /// I/O operation failed
    Io(String),
    /// Database corruption detected
    Corruption(String),
    /// Invalid input or arguments
    InvalidInput(String),
    /// Resource not found
    NotFound(String),
    /// Operation would exceed memory limits
    OutOfMemory,
    /// Transaction conflict or deadlock
    TransactionConflict(String),
    /// Internal database error
    Internal(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Io(msg) => write!(f, "I/O error: {msg}"),
            Error::Corruption(msg) => write!(f, "Database corruption: {msg}"),
            Error::InvalidInput(msg) => write!(f, "Invalid input: {msg}"),
            Error::NotFound(msg) => write!(f, "Not found: {msg}"),
            Error::OutOfMemory => write!(f, "Out of memory"),
            Error::TransactionConflict(msg) => write!(f, "Transaction conflict: {msg}"),
            Error::Internal(msg) => write!(f, "Internal error: {msg}"),
        }
    }
}

impl std::error::Error for Error {}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Self {
        Error::Io(err.to_string())
    }
}

impl From<std::fmt::Error> for Error {
    fn from(err: std::fmt::Error) -> Self {
        Error::Internal(err.to_string())
    }
}

impl Error {
    /// Create an I/O error
    pub fn io<S: Into<String>>(msg: S) -> Self {
        Error::Io(msg.into())
    }

    /// Create a corruption error
    pub fn corruption<S: Into<String>>(msg: S) -> Self {
        Error::Corruption(msg.into())
    }

    /// Create an invalid input error
    pub fn invalid_input<S: Into<String>>(msg: S) -> Self {
        Error::InvalidInput(msg.into())
    }

    /// Create a not found error
    pub fn not_found<S: Into<String>>(msg: S) -> Self {
        Error::NotFound(msg.into())
    }

    /// Create a transaction conflict error
    pub fn transaction_conflict<S: Into<String>>(msg: S) -> Self {
        Error::TransactionConflict(msg.into())
    }

    /// Create an internal error
    pub fn internal<S: Into<String>>(msg: S) -> Self {
        Error::Internal(msg.into())
    }

    /// Check if this is an I/O error
    pub fn is_io(&self) -> bool {
        matches!(self, Error::Io(_))
    }

    /// Check if this is a corruption error
    pub fn is_corruption(&self) -> bool {
        matches!(self, Error::Corruption(_))
    }

    /// Check if this is a not found error
    pub fn is_not_found(&self) -> bool {
        matches!(self, Error::NotFound(_))
    }

    /// Check if this is a recoverable error
    pub fn is_recoverable(&self) -> bool {
        match self {
            Error::Io(_)
            | Error::TransactionConflict(_)
            | Error::InvalidInput(_)
            | Error::NotFound(_) => true,
            Error::Corruption(_) | Error::OutOfMemory | Error::Internal(_) => false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_creation() {
        let io_err = Error::io("File not accessible");
        assert!(io_err.is_io());
        assert!(io_err.is_recoverable());

        let corruption_err = Error::corruption("Invalid page checksum");
        assert!(corruption_err.is_corruption());
        assert!(!corruption_err.is_recoverable());

        let not_found_err = Error::not_found("Table does not exist");
        assert!(not_found_err.is_not_found());
        assert!(not_found_err.is_recoverable());
    }

    #[test]
    fn test_error_display() {
        let error = Error::io("Connection lost");
        assert_eq!(error.to_string(), "I/O error: Connection lost");

        let error = Error::OutOfMemory;
        assert_eq!(error.to_string(), "Out of memory");
    }

    #[test]
    fn test_error_from_std_io() {
        let io_error = std::io::Error::new(std::io::ErrorKind::NotFound, "File not found");
        let lumen_error: Error = io_error.into();
        assert!(lumen_error.is_io());
    }

    #[test]
    fn test_result_type() {
        fn might_fail() -> Result<String> {
            Ok("Success".to_string())
        }

        fn will_fail() -> Result<String> {
            Err(Error::invalid_input("Bad parameter"))
        }

        assert!(might_fail().is_ok());
        assert!(will_fail().is_err());
    }
}
