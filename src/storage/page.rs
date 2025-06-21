//! Page structure - a 4KB aligned byte array with typed header access

use crate::common::error::Error;
use crate::storage::page_constants::{PAGE_HEADER_SIZE, PAGE_SIZE, PAGE_USABLE_SIZE};
use crate::storage::page_header::PageHeader;

/// Page - 4KB aligned byte array with typed header access
#[repr(C, align(4096))]
pub struct Page {
    buffer: [u8; PAGE_SIZE],
}

impl Page {
    /// Create a new zero-initialized page
    pub fn new() -> Self {
        let mut page = Self {
            buffer: [0; PAGE_SIZE],
        };
        // Initialize header to default
        *page.header_mut() = PageHeader::default();
        page
    }

    /// Get page header (read-only)
    pub fn header(&self) -> &PageHeader {
        // SAFETY: We're reading exactly the first PAGE_HEADER_SIZE bytes of the buffer
        // as a PageHeader. This is safe because:
        // 1. PageHeader is Pod (plain old data) with no invalid bit patterns
        // 2. The buffer is properly aligned (4096 bytes)
        // 3. We're reading exactly PAGE_HEADER_SIZE bytes which matches PageHeader size
        unsafe { &*(self.buffer.as_ptr().cast::<PageHeader>()) }
    }

    /// Get page header (mutable)
    pub fn header_mut(&mut self) -> &mut PageHeader {
        // SAFETY: We're reading exactly the first PAGE_HEADER_SIZE bytes of the buffer
        // as a PageHeader. This is safe because:
        // 1. PageHeader is Pod (plain old data) with no invalid bit patterns
        // 2. The buffer is properly aligned (4096 bytes)
        // 3. We're reading exactly PAGE_HEADER_SIZE bytes which matches PageHeader size
        unsafe { &mut *(self.buffer.as_mut_ptr().cast::<PageHeader>()) }
    }

    /// Get page data area (read-only)
    pub fn data(&self) -> &[u8] {
        &self.buffer[PAGE_HEADER_SIZE..]
    }

    /// Get page data area (mutable)
    pub fn data_mut(&mut self) -> &mut [u8] {
        &mut self.buffer[PAGE_HEADER_SIZE..]
    }

    /// Get raw page buffer (read-only)
    pub fn raw(&self) -> &[u8] {
        &self.buffer
    }

    /// Get raw page buffer (mutable)
    pub fn raw_mut(&mut self) -> &mut [u8] {
        &mut self.buffer
    }

    /// Page size in bytes
    pub fn size(&self) -> usize {
        PAGE_SIZE
    }

    /// Usable data size in bytes
    pub fn usable_size(&self) -> usize {
        PAGE_USABLE_SIZE
    }

    /// Calculate and store page checksum
    ///
    /// # Errors
    ///
    /// Returns an error if the page size is invalid (should never happen with Page struct)
    pub fn calculate_checksum(&mut self) -> Result<(), Error> {
        let checksum = crate::storage::checksum::calculate_page_checksum(&self.buffer)?;
        self.header_mut().checksum = checksum;
        Ok(())
    }

    /// Verify page checksum
    pub fn verify_checksum(&self) -> bool {
        match crate::storage::checksum::calculate_page_checksum(&self.buffer) {
            Ok(calculated) => {
                // Copy checksum value to avoid unaligned access
                let stored_checksum = self.header().checksum;
                calculated == stored_checksum
            }
            Err(_) => false,
        }
    }

    /// Check if page is corrupted
    pub fn is_corrupted(&self) -> bool {
        !self.verify_checksum()
    }
}

impl Default for Page {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::storage::page_type::PageType;

    #[test]
    fn test_page_alignment() {
        // Verify the Page struct is properly aligned
        assert_eq!(std::mem::align_of::<Page>(), 4096);
    }

    #[test]
    fn test_page_size() {
        assert_eq!(std::mem::size_of::<Page>(), PAGE_SIZE);
    }

    #[test]
    fn test_header_cast_safety() {
        let mut page = Page::new();

        // Ensure we can safely cast to/from header
        page.header_mut().page_id = 12345;
        page.header_mut().page_type = PageType::Leaf;

        // Copy values to avoid unaligned access
        let page_id = page.header().page_id;
        let page_type = page.header().page_type;

        assert_eq!(page_id, 12345);
        assert_eq!(page_type, PageType::Leaf);

        // Verify the raw bytes match
        let raw_page_type = page.buffer[0];
        assert_eq!(raw_page_type, PageType::Leaf as u8);
    }
}
