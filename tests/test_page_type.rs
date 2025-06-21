//! Tests for page type enum

use lumen::storage::page_type::*;

#[test]
fn test_page_type_size() {
    // PageType must be exactly 1 byte for the page header
    assert_eq!(std::mem::size_of::<PageType>(), 1);
}

#[test]
fn test_page_type_values() {
    // Must match plan/storage-format.md exactly
    assert_eq!(PageType::Header as u8, 0x01);
    assert_eq!(PageType::TableMetadata as u8, 0x02);
    assert_eq!(PageType::Data as u8, 0x03);
    assert_eq!(PageType::BTreeInternal as u8, 0x04);
    assert_eq!(PageType::BTreeLeaf as u8, 0x05);
    assert_eq!(PageType::VectorIndex as u8, 0x06);
    assert_eq!(PageType::Overflow as u8, 0x07);
    assert_eq!(PageType::FreeList as u8, 0x08);
    assert_eq!(PageType::BloomFilter as u8, 0x09);
}

#[test]
fn test_page_type_from_u8() {
    assert_eq!(PageType::try_from(0x01u8).unwrap(), PageType::Header);
    assert_eq!(PageType::try_from(0x02u8).unwrap(), PageType::TableMetadata);
    assert_eq!(PageType::try_from(0x03u8).unwrap(), PageType::Data);
    assert_eq!(PageType::try_from(0x04u8).unwrap(), PageType::BTreeInternal);
    assert_eq!(PageType::try_from(0x05u8).unwrap(), PageType::BTreeLeaf);
    assert_eq!(PageType::try_from(0x06u8).unwrap(), PageType::VectorIndex);
    assert_eq!(PageType::try_from(0x07u8).unwrap(), PageType::Overflow);
    assert_eq!(PageType::try_from(0x08u8).unwrap(), PageType::FreeList);
    assert_eq!(PageType::try_from(0x09u8).unwrap(), PageType::BloomFilter);
}

#[test]
fn test_page_type_from_u8_invalid() {
    // Test invalid values: 0 and anything > 9
    assert!(PageType::try_from(0u8).is_err());
    for invalid in 10u8..=255u8 {
        assert!(PageType::try_from(invalid).is_err());
    }
}

#[test]
fn test_page_type_helper_methods() {
    // Test is_btree_page
    assert!(PageType::BTreeLeaf.is_btree_page());
    assert!(PageType::BTreeInternal.is_btree_page());
    assert!(!PageType::Header.is_btree_page());
    assert!(!PageType::TableMetadata.is_btree_page());
    assert!(!PageType::Data.is_btree_page());
    assert!(!PageType::VectorIndex.is_btree_page());
    assert!(!PageType::Overflow.is_btree_page());
    assert!(!PageType::FreeList.is_btree_page());
    assert!(!PageType::BloomFilter.is_btree_page());

    // Test is_free_list
    assert!(PageType::FreeList.is_free_list());
    assert!(!PageType::Header.is_free_list());
    assert!(!PageType::BTreeLeaf.is_free_list());

    // Test is_overflow
    assert!(PageType::Overflow.is_overflow());
    assert!(!PageType::Header.is_overflow());
    assert!(!PageType::BTreeLeaf.is_overflow());

    // Test is_data
    assert!(PageType::Data.is_data());
    assert!(!PageType::Header.is_data());
    assert!(!PageType::BTreeLeaf.is_data());

    // Test is_metadata
    assert!(PageType::Header.is_metadata());
    assert!(PageType::TableMetadata.is_metadata());
    assert!(!PageType::Data.is_metadata());
    assert!(!PageType::BTreeLeaf.is_metadata());
    assert!(!PageType::Overflow.is_metadata());
}
