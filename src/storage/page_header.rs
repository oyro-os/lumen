//! Page header structure - exactly 16 bytes at the beginning of each page
//! MUST match plan/storage-format.md specification

use crate::storage::page_constants::{PageId, INVALID_PAGE_ID, PAGE_USABLE_SIZE};
use crate::storage::page_type::PageType;
use bytemuck::{Pod, Zeroable};

/// Page header - exactly 16 bytes as specified in plan/storage-format.md
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C, packed(1))]
pub struct PageHeader {
    /// Page number (4 bytes)
    pub page_id: u32,
    /// Page type enum (1 byte)
    pub page_type: PageType,
    /// Page-specific flags (1 byte)
    pub flags: u8,
    /// Bytes of free space (2 bytes)
    pub free_space: u16,
    /// CRC32 of page content (4 bytes)
    pub checksum: u32,
    /// Log sequence number (4 bytes)
    pub lsn: u32,
    // Total: 16 bytes (exactly as specified)
}

// SAFETY: PageHeader is a POD type with no padding or invalid values
// All fields are plain data types with no special requirements
unsafe impl Pod for PageHeader {}
unsafe impl Zeroable for PageHeader {}

impl Default for PageHeader {
    #[allow(clippy::cast_possible_truncation)]
    fn default() -> Self {
        Self {
            page_id: INVALID_PAGE_ID,
            page_type: PageType::Header, // Default to Header type
            flags: 0,
            free_space: PAGE_USABLE_SIZE as u16, // PAGE_USABLE_SIZE is 4080, fits in u16
            checksum: 0,
            lsn: 0,
        }
    }
}

impl PageHeader {
    /// Create a new page header with the given type and ID
    pub fn new(page_type: PageType, page_id: PageId) -> Self {
        Self {
            page_id,
            page_type,
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

        // Check field offsets match plan/storage-format.md
        assert_eq!(offset_of!(PageHeader, page_id), 0);
        assert_eq!(offset_of!(PageHeader, page_type), 4);
        assert_eq!(offset_of!(PageHeader, flags), 5);
        assert_eq!(offset_of!(PageHeader, free_space), 6);
        assert_eq!(offset_of!(PageHeader, checksum), 8);
        assert_eq!(offset_of!(PageHeader, lsn), 12);
        assert_eq!(std::mem::size_of::<PageHeader>(), 16);
    }

    #[test]
    fn test_page_header_size() {
        // Ensure the header is exactly 16 bytes as per plan/storage-format.md
        assert_eq!(std::mem::size_of::<PageHeader>(), 16);
    }

    #[test]
    fn test_page_header_pod() {
        // Test that we can safely cast to/from bytes
        let header = PageHeader::new(PageType::BTreeLeaf, 42);
        let bytes = bytes_of(&header);
        assert_eq!(bytes.len(), 16);

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
