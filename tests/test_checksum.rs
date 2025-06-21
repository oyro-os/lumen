//! Tests for checksum functionality

use lumen::storage::checksum::*;
use lumen::storage::page::Page;
use lumen::storage::page_constants::PAGE_SIZE;
use lumen::storage::page_type::PageType;

#[test]
fn test_crc32_implementation() {
    // Test with known CRC32 values
    let test_string = b"123456789";
    let checksum = calculate_crc32(test_string);
    assert_eq!(checksum, 0xCBF4_3926); // Known CRC32 value
}

#[test]
fn test_empty_page_checksum() {
    let buffer = [0u8; 4096];
    let checksum = calculate_crc32(&buffer);
    assert_ne!(checksum, 0); // Even empty page has non-zero checksum
}

#[test]
fn test_single_bit_detection() {
    let buffer1 = [0u8; 4096];
    let mut buffer2 = [0u8; 4096];

    // Flip single bit
    buffer2[2048] = 0x01;

    let checksum1 = calculate_crc32(&buffer1);
    let checksum2 = calculate_crc32(&buffer2);
    assert_ne!(checksum1, checksum2); // CRC32 detects single bit errors
}

#[test]
fn test_page_checksum_excludes_checksum_field() {
    let mut buffer1 = [0u8; PAGE_SIZE];
    let mut buffer2 = [0u8; PAGE_SIZE];

    // Set different checksum values at bytes 20-23
    buffer1[20] = 0xFF;
    buffer1[21] = 0xFF;
    buffer1[22] = 0xFF;
    buffer1[23] = 0xFF;

    buffer2[20] = 0x00;
    buffer2[21] = 0x00;
    buffer2[22] = 0x00;
    buffer2[23] = 0x00;

    // Checksums should be the same since we exclude the checksum field
    let checksum1 = calculate_page_checksum(&buffer1).unwrap();
    let checksum2 = calculate_page_checksum(&buffer2).unwrap();
    assert_eq!(checksum1, checksum2);
}

#[test]
fn test_page_checksum_invalid_size() {
    let buffer = [0u8; 1024]; // Wrong size
    let result = calculate_page_checksum(&buffer);
    assert!(result.is_err());
}

#[test]
fn test_corruption_detection() {
    let mut page = Page::new();
    page.header_mut().page_type = PageType::Leaf;
    page.calculate_checksum().unwrap();

    // Verify checksum is correct
    assert!(page.verify_checksum());

    // Simulate corruption
    page.data_mut()[100] ^= 0xFF;

    assert!(!page.verify_checksum()); // Detects corruption
}

#[test]
fn test_page_checksum_calculate_and_verify() {
    let mut page = Page::new();
    page.header_mut().page_type = PageType::Leaf;
    page.data_mut()[0] = 0x42;

    page.calculate_checksum().unwrap();
    assert!(page.verify_checksum());

    page.data_mut()[0] = 0x43; // Corrupt data
    assert!(!page.verify_checksum());
}

#[test]
fn test_page_checksum_empty_page() {
    let mut page = Page::new();
    page.calculate_checksum().unwrap();
    assert!(page.verify_checksum());
}

#[test]
fn test_page_is_corrupted() {
    let mut page = Page::new();
    page.calculate_checksum().unwrap();
    assert!(!page.is_corrupted());

    // Corrupt the page
    page.data_mut()[500] = 0xDE;
    assert!(page.is_corrupted());
}
