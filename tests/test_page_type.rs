//! Tests for page type enum

use lumen::storage::page_type::*;

#[test]
fn test_page_type_values() {
    assert_eq!(PageType::Free as u8, 0);
    assert_eq!(PageType::Leaf as u8, 1);
    assert_eq!(PageType::Internal as u8, 2);
    assert_eq!(PageType::Overflow as u8, 3);
}

#[test]
fn test_page_type_from_u8() {
    assert_eq!(PageType::try_from(0u8), Ok(PageType::Free));
    assert_eq!(PageType::try_from(1u8), Ok(PageType::Leaf));
    assert_eq!(PageType::try_from(2u8), Ok(PageType::Internal));
    assert_eq!(PageType::try_from(3u8), Ok(PageType::Overflow));
    assert!(PageType::try_from(255u8).is_err());
}

#[test]
fn test_page_type_invalid() {
    // Test invalid page type values
    for invalid in 4u8..=255u8 {
        assert!(PageType::try_from(invalid).is_err());
    }
}