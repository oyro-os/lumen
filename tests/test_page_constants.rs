//! Tests for page constants

use lumen::storage::page_constants::*;

#[test]
fn test_page_constants() {
    assert_eq!(PAGE_SIZE, 4096);
    assert_eq!(PAGE_HEADER_SIZE, 16);
    assert_eq!(PAGE_USABLE_SIZE, 4080);
}

#[test]
fn test_page_id_constants() {
    assert_eq!(INVALID_PAGE_ID, 0);
    assert_eq!(MAX_PAGE_ID, u32::MAX);
}

#[test]
fn test_page_size_is_power_of_two() {
    assert!(PAGE_SIZE.is_power_of_two());
    assert_eq!(PAGE_SIZE, 4096);
}
