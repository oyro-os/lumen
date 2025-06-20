//! Tests for page header structure

use lumen::storage::page_header::*;
use lumen::storage::page_type::PageType;
use lumen::storage::page_constants::*;
use bytemuck::bytes_of;

#[test]
fn test_page_header_size() {
    assert_eq!(std::mem::size_of::<PageHeader>(), 32);
    assert_eq!(std::mem::align_of::<PageHeader>(), 1); // packed struct
}

#[test]
fn test_page_header_zero_copy() {
    let mut buffer = [0u8; 32];
    let header = PageHeader {
        page_type: PageType::Leaf,
        flags: 0x42,
        free_space: 1024,
        page_id: 42,
        next_page: 100,
        lsn: 0x123456789ABCDEF0,
        checksum: 0x12345678,
        reserved: 0,
        _padding: [0; 4],
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
    
    assert_eq!(page_type, PageType::Leaf);
    assert_eq!(flags, 0x42);
    assert_eq!(page_id, 42);
    assert_eq!(checksum, 0x12345678);
}

#[test]
fn test_page_header_default() {
    let header = PageHeader::default();
    // Copy fields to avoid unaligned access to packed struct
    let page_type = header.page_type;
    let flags = header.flags;
    let free_space = header.free_space;
    let page_id = header.page_id;
    let next_page = header.next_page;
    let lsn = header.lsn;
    let checksum = header.checksum;
    let reserved = header.reserved;
    
    assert_eq!(page_type, PageType::Free);
    assert_eq!(flags, 0);
    assert_eq!(free_space, PAGE_USABLE_SIZE as u16);
    assert_eq!(page_id, INVALID_PAGE_ID);
    assert_eq!(next_page, INVALID_PAGE_ID);
    assert_eq!(lsn, 0);
    assert_eq!(checksum, 0);
    assert_eq!(reserved, 0);
}