//! Page type enumeration for different page kinds in the storage layer

use crate::common::error::Error;

/// Page type enumeration - MUST match plan/storage-format.md exactly
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum PageType {
    /// Header page - database file header (page 0)
    Header = 0x01,
    /// Table metadata page - stores table definitions
    TableMetadata = 0x02,
    /// Data page - stores actual row data
    Data = 0x03,
    /// B+Tree internal page - contains keys and child page references
    BTreeInternal = 0x04,
    /// B+Tree leaf page - contains keys and values
    BTreeLeaf = 0x05,
    /// Vector index page - for vector similarity search
    VectorIndex = 0x06,
    /// Overflow page - for large values that don't fit in a single page
    Overflow = 0x07,
    /// Free list page - tracks free pages
    FreeList = 0x08,
    /// Bloom filter page - for fast existence checks
    BloomFilter = 0x09,
}

impl TryFrom<u8> for PageType {
    type Error = Error;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0x01 => Ok(PageType::Header),
            0x02 => Ok(PageType::TableMetadata),
            0x03 => Ok(PageType::Data),
            0x04 => Ok(PageType::BTreeInternal),
            0x05 => Ok(PageType::BTreeLeaf),
            0x06 => Ok(PageType::VectorIndex),
            0x07 => Ok(PageType::Overflow),
            0x08 => Ok(PageType::FreeList),
            0x09 => Ok(PageType::BloomFilter),
            _ => Err(Error::InvalidPageType(value)),
        }
    }
}

impl PageType {
    /// Check if this is a B+Tree page (leaf or internal)
    pub fn is_btree_page(&self) -> bool {
        matches!(self, PageType::BTreeLeaf | PageType::BTreeInternal)
    }

    /// Check if this is a free list page
    pub fn is_free_list(&self) -> bool {
        matches!(self, PageType::FreeList)
    }

    /// Check if this is an overflow page
    pub fn is_overflow(&self) -> bool {
        matches!(self, PageType::Overflow)
    }

    /// Check if this is a data page
    pub fn is_data(&self) -> bool {
        matches!(self, PageType::Data)
    }

    /// Check if this is a metadata page (header or table metadata)
    pub fn is_metadata(&self) -> bool {
        matches!(self, PageType::Header | PageType::TableMetadata)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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
            match PageType::try_from(invalid) {
                Err(Error::InvalidPageType(val)) => assert_eq!(val, invalid),
                _ => panic!("Expected InvalidPageType error for value {invalid}"),
            }
        }
    }

    #[test]
    fn test_page_type_helper_methods() {
        // Test is_btree_page
        assert!(PageType::BTreeLeaf.is_btree_page());
        assert!(PageType::BTreeInternal.is_btree_page());
        assert!(!PageType::Header.is_btree_page());
        assert!(!PageType::Data.is_btree_page());
        assert!(!PageType::Overflow.is_btree_page());

        // Test is_free_list
        assert!(PageType::FreeList.is_free_list());
        assert!(!PageType::BTreeLeaf.is_free_list());

        // Test is_overflow
        assert!(PageType::Overflow.is_overflow());
        assert!(!PageType::BTreeLeaf.is_overflow());

        // Test is_data
        assert!(PageType::Data.is_data());
        assert!(!PageType::BTreeLeaf.is_data());

        // Test is_metadata
        assert!(PageType::Header.is_metadata());
        assert!(PageType::TableMetadata.is_metadata());
        assert!(!PageType::Data.is_metadata());
        assert!(!PageType::BTreeLeaf.is_metadata());
    }
}
