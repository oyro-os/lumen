//! C FFI bindings for Lumen database
//!
//! This crate provides a C-compatible API for the Lumen database engine,
//! enabling integration with other languages like Dart, Swift, and Kotlin.

use std::ffi::CString;
use std::os::raw::{c_char, c_int};
use std::ptr;

/// FFI-safe result type
#[repr(C)]
pub struct LumenResult {
    /// Success/failure indicator (0 = success, non-zero = error)
    pub code: c_int,
    /// Error message (null if success)
    pub message: *mut c_char,
}

impl LumenResult {
    /// Create a success result
    fn success() -> Self {
        Self {
            code: 0,
            message: ptr::null_mut(),
        }
    }

    /// Create an error result
    #[allow(dead_code)]
    fn error(code: c_int, message: &str) -> Self {
        let c_message = CString::new(message)
            .unwrap_or_else(|_| CString::new("Invalid error message").unwrap());

        Self {
            code,
            message: c_message.into_raw(),
        }
    }
}

/// Get the Lumen library version
///
/// Returns a null-terminated string containing the version.
/// The caller must free the returned string with `lumen_free_string`.
///
/// # Safety
/// The returned pointer must be freed with `lumen_free_string`.
#[no_mangle]
pub extern "C" fn lumen_version() -> *mut c_char {
    CString::new(lumen::VERSION).unwrap().into_raw()
}

/// Get version components
///
/// # Safety
/// All output parameters must be valid pointers.
#[no_mangle]
pub unsafe extern "C" fn lumen_version_components(
    major: *mut u32,
    minor: *mut u32,
    patch: *mut u32,
) {
    if !major.is_null() {
        unsafe {
            *major = lumen::VERSION_MAJOR;
        }
    }
    if !minor.is_null() {
        unsafe {
            *minor = lumen::VERSION_MINOR;
        }
    }
    if !patch.is_null() {
        unsafe {
            *patch = lumen::VERSION_PATCH;
        }
    }
}

/// Initialize the Lumen logging system
///
/// This should be called once at application startup.
///
/// # Returns
/// A `LumenResult` indicating success or failure.
#[no_mangle]
pub extern "C" fn lumen_init_logging() -> LumenResult {
    lumen::common::logging::init();
    LumenResult::success()
}

/// Free a string allocated by Lumen
///
/// # Safety
/// The string must have been allocated by a Lumen FFI function.
/// After calling this function, the pointer is invalid and must not be used.
#[no_mangle]
pub unsafe extern "C" fn lumen_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s);
        }
    }
}

/// Free a LumenResult
///
/// This frees any allocated error message in the result.
///
/// # Safety
/// The result must have been returned by a Lumen FFI function.
#[no_mangle]
pub unsafe extern "C" fn lumen_free_result(result: *mut LumenResult) {
    if !result.is_null() {
        unsafe {
            let result = &mut *result;
            if !result.message.is_null() {
                let _ = CString::from_raw(result.message);
                result.message = ptr::null_mut();
            }
        }
    }
}

/// Test function to verify FFI is working
///
/// Returns a test message that should be freed with `lumen_free_string`.
///
/// # Safety
/// The returned pointer must be freed with `lumen_free_string`.
#[no_mangle]
pub extern "C" fn lumen_test_message() -> *mut c_char {
    CString::new("Hello from Lumen FFI!").unwrap().into_raw()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CStr;

    #[test]
    fn test_version_ffi() {
        let version_ptr = lumen_version();
        assert!(!version_ptr.is_null());

        let version_str = unsafe { CStr::from_ptr(version_ptr) };
        assert_eq!(version_str.to_str().unwrap(), "0.1.0");

        unsafe {
            lumen_free_string(version_ptr);
        }
    }

    #[test]
    fn test_version_components() {
        let mut major = 0u32;
        let mut minor = 0u32;
        let mut patch = 0u32;

        unsafe {
            lumen_version_components(&mut major, &mut minor, &mut patch);
        }

        assert_eq!(major, 0);
        assert_eq!(minor, 1);
        assert_eq!(patch, 0);
    }

    #[test]
    fn test_logging_init() {
        let result = lumen_init_logging();
        assert_eq!(result.code, 0);
        assert!(result.message.is_null());
    }

    #[test]
    fn test_message() {
        let msg_ptr = lumen_test_message();
        assert!(!msg_ptr.is_null());

        let msg_str = unsafe { CStr::from_ptr(msg_ptr) };
        assert_eq!(msg_str.to_str().unwrap(), "Hello from Lumen FFI!");

        unsafe {
            lumen_free_string(msg_ptr);
        }
    }
}
