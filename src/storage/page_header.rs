//! Page header structure - exactly 32 bytes at the beginning of each page

use crate::storage::page_constants::{PageId, INVALID_PAGE_ID, PAGE_USABLE_SIZE};
use crate::storage::page_type::PageType;
use bytemuck::{Pod, Zeroable};

/// Page header - exactly 32 bytes, zero-copy serializable
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C, packed(1))]
pub struct PageHeader {
    /// Page type (1 byte)
    pub page_type: PageType,
    /// Page flags (1 byte)
    pub flags: u8,
    /// Free space in the page (2 bytes)
    pub free_space: u16,
    /// This page's ID (4 bytes)
    pub page_id: PageId,
    /// Next page in chain (4 bytes)
    pub next_page: PageId,
    /// Log sequence number for recovery (8 bytes)
    pub lsn: u64,
    /// CRC32 checksum (4 bytes)
    pub checksum: u32,
    /// Reserved for future use (4 bytes)
    pub reserved: u32,
    /// Padding to ensure 32 bytes total (4 bytes)
    #[allow(dead_code)] // Required for struct size
    padding: [u8; 4],
    // Total: 32 bytes
}

// SAFETY: PageHeader is a POD type with no padding or invalid values
// All fields are plain data types with no special requirements
unsafe impl Pod for PageHeader {}
unsafe impl Zeroable for PageHeader {}

impl Default for PageHeader {
    #[allow(clippy::cast_possible_truncation)]
    fn default() -> Self {
        Self {
            page_type: PageType::Free,
            flags: 0,
            free_space: PAGE_USABLE_SIZE as u16, // PAGE_USABLE_SIZE is 4064, fits in u16
            page_id: INVALID_PAGE_ID,
            next_page: INVALID_PAGE_ID,
            lsn: 0,
            checksum: 0,
            reserved: 0,
            padding: [0; 4],
        }
    }
}

impl PageHeader {
    /// Create a new page header with the given type and ID
    pub fn new(page_type: PageType, page_id: PageId) -> Self {
        Self {
            page_type,
            page_id,
            ..Default::default()
        }
    }

    /// Check if the page is marked as dirty (needs to be written)
    pub fn is_dirty(&self) -> bool {
        self.flags & 0x01 != 0
    }

    /// Mark the page as dirty
    pub fn set_dirty(&mut self, dirty: bool) {
        if dirty {
            self.flags |= 0x01;
        } else {
            self.flags &= !0x01;
        }
    }

    /// Check if the page is pinned in memory
    pub fn is_pinned(&self) -> bool {
        self.flags & 0x02 != 0
    }

    /// Set the pinned flag
    pub fn set_pinned(&mut self, pinned: bool) {
        if pinned {
            self.flags |= 0x02;
        } else {
            self.flags &= !0x02;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bytemuck::{bytes_of, from_bytes};

    #[test]
    fn test_field_offsets() {
        use std::mem::offset_of;

        // Check field offsets
        println!("page_type offset: {}", offset_of!(PageHeader, page_type));
        println!("flags offset: {}", offset_of!(PageHeader, flags));
        println!("free_space offset: {}", offset_of!(PageHeader, free_space));
        println!("page_id offset: {}", offset_of!(PageHeader, page_id));
        println!("next_page offset: {}", offset_of!(PageHeader, next_page));
        println!("lsn offset: {}", offset_of!(PageHeader, lsn));
        println!("checksum offset: {}", offset_of!(PageHeader, checksum));
        println!("reserved offset: {}", offset_of!(PageHeader, reserved));
        println!("Total size: {}", std::mem::size_of::<PageHeader>());
    }

    #[test]
    fn test_page_header_size() {
        // Ensure the header is exactly 32 bytes
        assert_eq!(std::mem::size_of::<PageHeader>(), 32);
    }

    #[test]
    fn test_page_header_pod() {
        // Test that we can safely cast to/from bytes
        let header = PageHeader::new(PageType::Leaf, 42);
        let bytes = bytes_of(&header);
        assert_eq!(bytes.len(), 32);

        let header2: &PageHeader = from_bytes(bytes);
        assert_eq!(header, *header2);
    }

    #[test]
    fn test_page_header_flags() {
        let mut header = PageHeader::default();

        assert!(!header.is_dirty());
        header.set_dirty(true);
        assert!(header.is_dirty());

        assert!(!header.is_pinned());
        header.set_pinned(true);
        assert!(header.is_pinned());

        // Both flags should be independent
        assert!(header.is_dirty());
        assert!(header.is_pinned());

        header.set_dirty(false);
        assert!(!header.is_dirty());
        assert!(header.is_pinned());
    }
}
