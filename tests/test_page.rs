//! Tests for page structure

use lumen::storage::page::*;
use lumen::storage::page_constants::*;
use lumen::storage::page_type::PageType;

#[test]
fn test_page_creation() {
    let page = Page::new();
    assert_eq!(page.size(), PAGE_SIZE);
    assert_eq!(page.header().page_type, PageType::Free);
}

#[test]
fn test_page_header_access() {
    let mut page = Page::new();
    page.header_mut().page_type = PageType::Leaf;
    page.header_mut().page_id = 100;

    // Copy fields to avoid unaligned access to packed struct
    let page_type = page.header().page_type;
    let page_id = page.header().page_id;

    assert_eq!(page_type, PageType::Leaf);
    assert_eq!(page_id, 100);
}

#[test]
fn test_page_data_access() {
    let mut page = Page::new();
    page.data_mut()[0] = 0x42;
    assert_eq!(page.data()[0], 0x42);
}

#[test]
fn test_page_alignment() {
    let page = Page::new();
    let ptr = page.raw().as_ptr() as usize;
    assert_eq!(ptr % 4096, 0); // Page must be 4KB aligned
}

#[test]
fn test_page_initialization() {
    let page = Page::new();
    // Check that header is zeroed (matches Default::default())
    // Copy fields to avoid unaligned access to packed struct
    let page_type = page.header().page_type;
    let page_id = page.header().page_id;
    let lsn = page.header().lsn;

    assert_eq!(page_type, PageType::Free);
    assert_eq!(page_id, INVALID_PAGE_ID);
    assert_eq!(lsn, 0);

    // Check that data area is zeroed
    for &byte in page.data() {
        assert_eq!(byte, 0);
    }
}

#[test]
fn test_page_raw_access() {
    let mut page = Page::new();
    assert_eq!(page.raw().len(), PAGE_SIZE);
    assert_eq!(page.raw_mut().len(), PAGE_SIZE);

    // Write through raw and read through header/data
    page.raw_mut()[0] = PageType::Leaf as u8;
    assert_eq!(page.header().page_type, PageType::Leaf);

    page.raw_mut()[PAGE_HEADER_SIZE] = 0xAB;
    assert_eq!(page.data()[0], 0xAB);
}

#[test]
fn test_page_sizes() {
    let page = Page::new();
    assert_eq!(page.size(), PAGE_SIZE);
    assert_eq!(page.usable_size(), PAGE_USABLE_SIZE);
    assert_eq!(page.data().len(), PAGE_USABLE_SIZE);
}
