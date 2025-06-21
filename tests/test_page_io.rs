//! Tests for page I/O operations

use lumen::storage::page::Page;
use lumen::storage::page_constants::PAGE_SIZE;
use lumen::storage::page_io::*;
use lumen::storage::page_type::PageType;
use std::fs::{self, File};
use std::io::Write as IoWrite;
use tempfile::NamedTempFile;

#[test]
fn test_page_write_and_read() -> Result<(), Box<dyn std::error::Error>> {
    let temp_file = NamedTempFile::new()?;
    let mut file = File::create(temp_file.path())?;

    let mut write_page = Page::new();
    write_page.header_mut().page_type = PageType::BTreeLeaf;
    write_page.header_mut().page_id = 42;
    write_page.data_mut()[0] = 0xFF;
    write_page.calculate_checksum()?;

    write_page_to_file(&mut file, 0, &write_page)?;

    let mut read_file = File::open(temp_file.path())?;
    let read_page = read_page_from_file(&mut read_file, 0)?;

    // Checksum is automatically verified during read
    let page_type = read_page.header().page_type;
    let page_id = read_page.header().page_id;
    assert_eq!(page_type, PageType::BTreeLeaf);
    assert_eq!(page_id, 42);
    assert_eq!(read_page.data()[0], 0xFF);

    Ok(())
}

#[test]
fn test_page_io_multiple_pages() -> Result<(), Box<dyn std::error::Error>> {
    let temp_file = NamedTempFile::new()?;
    let mut file = File::create(temp_file.path())?;

    // Write multiple pages
    for i in 0..5 {
        let mut page = Page::new();
        page.header_mut().page_type = PageType::BTreeLeaf;
        page.header_mut().page_id = i;
        page.data_mut()[0] = i as u8;
        page.calculate_checksum()?;

        write_page_to_file(&mut file, i as u64, &page)?;
    }

    // Read them back
    let mut read_file = File::open(temp_file.path())?;
    for i in 0..5 {
        let page = read_page_from_file(&mut read_file, i as u64)?;
        // Checksum is automatically verified during read
        let page_id = page.header().page_id;
        assert_eq!(page_id, i);
        assert_eq!(page.data()[0], i as u8);
    }

    Ok(())
}

#[test]
fn test_page_io_memory_mapped() -> Result<(), Box<dyn std::error::Error>> {
    let temp_file = NamedTempFile::new()?;
    let file = File::create(temp_file.path())?;
    file.set_len(PAGE_SIZE as u64)?; // Ensure file is large enough
    drop(file); // Close file before memory mapping

    let mut write_page = Page::new();
    write_page.header_mut().page_type = PageType::BTreeInternal;
    write_page.header_mut().page_id = 100;
    write_page.calculate_checksum()?;

    // Test memory-mapped I/O
    write_page_mmap(temp_file.path(), 0, &write_page)?;
    let read_page = read_page_mmap(temp_file.path(), 0)?;

    // Checksum is automatically verified during read
    let page_type = read_page.header().page_type;
    let page_id = read_page.header().page_id;
    assert_eq!(page_type, PageType::BTreeInternal);
    assert_eq!(page_id, 100);

    Ok(())
}

#[test]
fn test_page_io_offset_calculations() -> Result<(), Box<dyn std::error::Error>> {
    let temp_file = NamedTempFile::new()?;
    let mut file = File::create(temp_file.path())?;

    // Write pages at specific offsets
    let page_ids = [10, 20, 30];
    for &id in &page_ids {
        let mut page = Page::new();
        page.header_mut().page_id = id;
        page.calculate_checksum()?;

        let offset = calculate_page_offset(id as u64);
        write_page_at_offset(&mut file, offset, &page)?;
    }

    // Read using page ID
    let mut read_file = File::open(temp_file.path())?;
    for &id in &page_ids {
        let page = read_page_by_id(&mut read_file, id as u64)?;
        // Checksum is automatically verified during read
        let page_id = page.header().page_id;
        assert_eq!(page_id, id);
    }

    Ok(())
}

#[test]
fn test_page_io_direct() -> Result<(), Box<dyn std::error::Error>> {
    let temp_file = NamedTempFile::new()?;

    let mut write_page = Page::new();
    write_page.header_mut().page_type = PageType::BTreeLeaf;
    write_page.header_mut().page_id = 77;
    write_page.data_mut()[100] = 0xAB;
    write_page.calculate_checksum()?;

    // Test direct I/O (O_DIRECT on Linux)
    write_page_direct(temp_file.path(), 0, &write_page)?;
    let read_page = read_page_direct(temp_file.path(), 0)?;

    // Checksum is automatically verified during read
    let page_id = read_page.header().page_id;
    assert_eq!(page_id, 77);
    assert_eq!(read_page.data()[100], 0xAB);

    Ok(())
}

#[test]
fn test_page_io_sync() -> Result<(), Box<dyn std::error::Error>> {
    let temp_file = NamedTempFile::new()?;
    let mut file = File::create(temp_file.path())?;

    let mut page = Page::new();
    page.header_mut().page_type = PageType::FreeList;
    page.calculate_checksum()?;

    // Test synchronized write
    write_page_sync(&mut file, 0, &page)?;

    // Verify data was written
    let mut read_file = File::open(temp_file.path())?;
    let read_page = read_page_from_file(&mut read_file, 0)?;

    // Checksum is automatically verified during read
    let page_type = read_page.header().page_type;
    assert_eq!(page_type, PageType::FreeList);

    Ok(())
}

#[test]
fn test_page_io_corruption_detection() -> Result<(), Box<dyn std::error::Error>> {
    let temp_file = NamedTempFile::new()?;

    // Write a valid page
    {
        let mut file = File::create(temp_file.path())?;
        let mut page = Page::new();
        page.header_mut().page_type = PageType::BTreeLeaf;
        page.calculate_checksum()?;
        write_page_to_file(&mut file, 0, &page)?;
    }

    // Corrupt the file
    {
        let mut file = fs::OpenOptions::new().write(true).open(temp_file.path())?;
        file.write_all(&[0xFF])?; // Corrupt first byte
    }

    // Try to read - should detect corruption and return error
    let mut file = File::open(temp_file.path())?;
    let result = read_page_from_file(&mut file, 0);

    // With automatic checksum verification, read should fail
    assert!(result.is_err());
    if let Err(e) = result {
        assert!(e.is_corruption());
    }

    Ok(())
}

#[test]
fn test_page_io_file_too_small() {
    let temp_file = NamedTempFile::new().unwrap();
    let file = File::create(temp_file.path()).unwrap();
    file.set_len(100).unwrap(); // Too small for a page
    drop(file);

    let mut file = File::open(temp_file.path()).unwrap();
    let result = read_page_from_file(&mut file, 0);
    assert!(result.is_err());
}
