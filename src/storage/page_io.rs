//! Page I/O operations for reading and writing pages to storage

use crate::common::error::Error;
use crate::storage::page::Page;
use crate::storage::page_constants::PAGE_SIZE;
use memmap2::MmapOptions;
use std::fs::File;
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::Path;

/// Calculate the byte offset for a given page ID
pub fn calculate_page_offset(page_id: u64) -> u64 {
    page_id * PAGE_SIZE as u64
}

/// Write a page to a file at the specified page ID
///
/// # Errors
///
/// Returns an error if the file seek or write operation fails
pub fn write_page_to_file(file: &mut File, page_id: u64, page: &Page) -> Result<(), Error> {
    let offset = calculate_page_offset(page_id);
    write_page_at_offset(file, offset, page)
}

/// Write a page to a file at the specified byte offset
///
/// # Errors
///
/// Returns an error if the file seek or write operation fails
pub fn write_page_at_offset(file: &mut File, offset: u64, page: &Page) -> Result<(), Error> {
    file.seek(SeekFrom::Start(offset))?;
    file.write_all(page.raw())?;
    Ok(())
}

/// Read a page from a file at the specified page ID
///
/// # Errors
///
/// Returns an error if the file seek or read operation fails, or if checksum verification fails
pub fn read_page_from_file(file: &mut File, page_id: u64) -> Result<Page, Error> {
    let offset = calculate_page_offset(page_id);
    let page = read_page_at_offset(file, offset)?;

    // Verify checksum
    if !page.verify_checksum() {
        return Err(Error::corruption(format!(
            "Checksum verification failed for page {page_id}"
        )));
    }

    Ok(page)
}

/// Read a page from a file at the specified byte offset
///
/// # Errors
///
/// Returns an error if the file seek or read operation fails, or if checksum verification fails
pub fn read_page_at_offset(file: &mut File, offset: u64) -> Result<Page, Error> {
    file.seek(SeekFrom::Start(offset))?;

    let mut page = Page::new();
    file.read_exact(page.raw_mut())?;

    // Verify checksum
    if !page.verify_checksum() {
        let page_id = offset / PAGE_SIZE as u64;
        return Err(Error::corruption(format!(
            "Checksum verification failed for page at offset {offset} (page_id: {page_id})"
        )));
    }

    Ok(page)
}

/// Read a page by page ID (convenience function)
///
/// # Errors
///
/// Returns an error if the file seek or read operation fails
pub fn read_page_by_id(file: &mut File, page_id: u64) -> Result<Page, Error> {
    read_page_from_file(file, page_id)
}

/// Write a page with explicit sync to ensure durability
///
/// # Errors
///
/// Returns an error if the write or sync operation fails
pub fn write_page_sync(file: &mut File, page_id: u64, page: &Page) -> Result<(), Error> {
    write_page_to_file(file, page_id, page)?;
    file.sync_all()?;
    Ok(())
}

/// Write a page using memory-mapped I/O
///
/// # Errors
///
/// Returns an error if file operations or memory mapping fails
pub fn write_page_mmap<P: AsRef<Path>>(path: P, page_id: u64, page: &Page) -> Result<(), Error> {
    let file = std::fs::OpenOptions::new()
        .read(true)
        .write(true)
        .open(path)?;

    let offset = calculate_page_offset(page_id);
    let len = offset + PAGE_SIZE as u64;

    // Ensure file is large enough
    if file.metadata()?.len() < len {
        file.set_len(len)?;
    }

    unsafe {
        let mut mmap = MmapOptions::new()
            .offset(offset)
            .len(PAGE_SIZE)
            .map_mut(&file)?;

        mmap.copy_from_slice(page.raw());
        mmap.flush()?;
    }

    Ok(())
}

/// Read a page using memory-mapped I/O
///
/// # Errors
///
/// Returns an error if file operations or memory mapping fails, or if checksum verification fails
pub fn read_page_mmap<P: AsRef<Path>>(path: P, page_id: u64) -> Result<Page, Error> {
    let file = File::open(path)?;
    let offset = calculate_page_offset(page_id);

    unsafe {
        let mmap = MmapOptions::new()
            .offset(offset)
            .len(PAGE_SIZE)
            .map(&file)?;

        let mut page = Page::new();
        page.raw_mut().copy_from_slice(&mmap);

        // Verify checksum
        if !page.verify_checksum() {
            return Err(Error::corruption(format!(
                "Checksum verification failed for page {page_id}"
            )));
        }

        Ok(page)
    }
}

/// Write a page using direct I/O (bypasses OS cache)
///
/// Note: Direct I/O requires:
/// - Page-aligned memory (handled by Page struct)
/// - File offset must be sector-aligned (usually 512 bytes)
/// - Transfer size must be sector-aligned
///
/// # Errors
///
/// Returns an error if file operations fail
#[cfg(target_os = "linux")]
pub fn write_page_direct<P: AsRef<Path>>(path: P, page_id: u64, page: &Page) -> Result<(), Error> {
    use std::os::unix::fs::OpenOptionsExt;

    let mut file = std::fs::OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(false)
        .custom_flags(libc::O_DIRECT)
        .open(path)?;

    write_page_to_file(&mut file, page_id, page)?;
    file.sync_all()?;
    Ok(())
}

/// Write a page using direct I/O (non-Linux fallback)
///
/// # Errors
///
/// Returns an error if file operations fail
#[cfg(not(target_os = "linux"))]
pub fn write_page_direct<P: AsRef<Path>>(path: P, page_id: u64, page: &Page) -> Result<(), Error> {
    // Fallback to regular I/O with sync on non-Linux platforms
    let mut file = std::fs::OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(false)
        .open(path)?;

    write_page_to_file(&mut file, page_id, page)?;
    file.sync_all()?;
    Ok(())
}

/// Read a page using direct I/O (bypasses OS cache)
///
/// # Errors
///
/// Returns an error if file operations fail, or if checksum verification fails
#[cfg(target_os = "linux")]
pub fn read_page_direct<P: AsRef<Path>>(path: P, page_id: u64) -> Result<Page, Error> {
    use std::os::unix::fs::OpenOptionsExt;

    let mut file = std::fs::OpenOptions::new()
        .read(true)
        .custom_flags(libc::O_DIRECT)
        .open(path)?;

    read_page_from_file(&mut file, page_id)
}

/// Read a page using direct I/O (non-Linux fallback)
///
/// # Errors
///
/// Returns an error if file operations fail, or if checksum verification fails
#[cfg(not(target_os = "linux"))]
pub fn read_page_direct<P: AsRef<Path>>(path: P, page_id: u64) -> Result<Page, Error> {
    // Fallback to regular I/O on non-Linux platforms
    let mut file = File::open(path)?;
    read_page_from_file(&mut file, page_id)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::storage::page_type::PageType;
    use tempfile::NamedTempFile;

    #[test]
    fn test_calculate_page_offset() {
        assert_eq!(calculate_page_offset(0), 0);
        assert_eq!(calculate_page_offset(1), PAGE_SIZE as u64);
        assert_eq!(calculate_page_offset(100), 100 * PAGE_SIZE as u64);
    }

    #[test]
    fn test_basic_write_read() -> Result<(), Error> {
        let temp_file = NamedTempFile::new()?;
        let mut file = File::create(temp_file.path())?;

        let mut page = Page::new();
        page.header_mut().page_type = PageType::Leaf;
        page.header_mut().page_id = 42;
        page.calculate_checksum()?;

        write_page_to_file(&mut file, 0, &page)?;

        let mut read_file = File::open(temp_file.path())?;
        let read_page = read_page_from_file(&mut read_file, 0)?;

        let page_type = read_page.header().page_type;
        let page_id = read_page.header().page_id;
        assert_eq!(page_type, PageType::Leaf);
        assert_eq!(page_id, 42);

        Ok(())
    }

    #[test]
    fn test_mmap_io() -> Result<(), Error> {
        let temp_file = NamedTempFile::new()?;

        // Ensure file exists and is large enough
        {
            let file = File::create(temp_file.path())?;
            file.set_len(PAGE_SIZE as u64)?;
        }

        let mut page = Page::new();
        page.header_mut().page_type = PageType::Internal;
        page.calculate_checksum()?;

        write_page_mmap(temp_file.path(), 0, &page)?;
        let read_page = read_page_mmap(temp_file.path(), 0)?;

        let page_type = read_page.header().page_type;
        assert_eq!(page_type, PageType::Internal);
        // Checksum is automatically verified during read

        Ok(())
    }
}
