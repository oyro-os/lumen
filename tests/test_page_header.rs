//! Tests for page header structure

use bytemuck::bytes_of;
use lumen::storage::page_constants::*;
use lumen::storage::page_header::*;
use lumen::storage::page_type::PageType;

#[test]
fn test_page_header_size() {
    // Must be exactly 16 bytes as per plan/storage-format.md
    assert_eq!(std::mem::size_of::<PageHeader>(), 16);
    assert_eq!(std::mem::align_of::<PageHeader>(), 1); // packed struct
}

#[test]
fn test_page_header_field_offsets() {
    use std::mem::offset_of;

    // Check field offsets match plan/storage-format.md
    assert_eq!(offset_of!(PageHeader, page_id), 0);
    assert_eq!(offset_of!(PageHeader, page_type), 4);
    assert_eq!(offset_of!(PageHeader, flags), 5);
    assert_eq!(offset_of!(PageHeader, free_space), 6);
    assert_eq!(offset_of!(PageHeader, checksum), 8);
    assert_eq!(offset_of!(PageHeader, lsn), 12);
}

#[test]
fn test_page_header_zero_copy() {
    let mut buffer = [0u8; 16];
    let header = PageHeader {
        page_type: PageType::BTreeLeaf,
        flags: 0x42,
        free_space: 1024,
        page_id: 42,
        checksum: 0x12345678,
        lsn: 0x789ABCDE,
    };

    // Write using bytemuck
    buffer.copy_from_slice(bytes_of(&header));

    // Read using bytemuck
    let read_header: PageHeader = bytemuck::pod_read_unaligned(&buffer);

    // Copy fields to avoid unaligned access to packed struct
    let page_type = read_header.page_type;
    let flags = read_header.flags;
    let page_id = read_header.page_id;
    let checksum = read_header.checksum;
    let free_space = read_header.free_space;
    let lsn = read_header.lsn;

    assert_eq!(page_type, PageType::BTreeLeaf);
    assert_eq!(flags, 0x42);
    assert_eq!(page_id, 42);
    assert_eq!(checksum, 0x12345678);
    assert_eq!(free_space, 1024);
    assert_eq!(lsn, 0x789ABCDE);
}

#[test]
fn test_page_header_default() {
    let header = PageHeader::default();
    // Copy fields to avoid unaligned access to packed struct
    let page_type = header.page_type;
    let flags = header.flags;
    let free_space = header.free_space;
    let page_id = header.page_id;
    let checksum = header.checksum;
    let lsn = header.lsn;

    assert_eq!(page_type, PageType::Header); // Default to Header type
    assert_eq!(flags, 0);
    assert_eq!(free_space, PAGE_USABLE_SIZE as u16);
    assert_eq!(page_id, INVALID_PAGE_ID);
    assert_eq!(checksum, 0);
    assert_eq!(lsn, 0);
}

#[test]
fn test_page_header_flags() {
    let mut header = PageHeader::default();

    // Test dirty flag
    assert!(!header.is_dirty());
    header.set_dirty(true);
    assert!(header.is_dirty());
    header.set_dirty(false);
    assert!(!header.is_dirty());

    // Test pinned flag
    assert!(!header.is_pinned());
    header.set_pinned(true);
    assert!(header.is_pinned());
    header.set_pinned(false);
    assert!(!header.is_pinned());

    // Test both flags independently
    header.set_dirty(true);
    header.set_pinned(true);
    assert!(header.is_dirty());
    assert!(header.is_pinned());

    header.set_dirty(false);
    assert!(!header.is_dirty());
    assert!(header.is_pinned());
}

#[test]
fn test_page_header_new() {
    let header = PageHeader::new(PageType::BTreeInternal, 999);

    // Copy fields to avoid unaligned access
    let page_type = header.page_type;
    let page_id = header.page_id;
    let flags = header.flags;
    let free_space = header.free_space;
    let checksum = header.checksum;
    let lsn = header.lsn;

    assert_eq!(page_type, PageType::BTreeInternal);
    assert_eq!(page_id, 999);
    assert_eq!(flags, 0);
    assert_eq!(free_space, PAGE_USABLE_SIZE as u16);
    assert_eq!(checksum, 0);
    assert_eq!(lsn, 0);
}
