//! Page type enumeration for different page kinds in the storage layer

use crate::common::error::Error;

/// Page type enumeration - must be u8 for storage
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum PageType {
    /// Free page - available for allocation
    Free = 0,
    /// Leaf page - contains actual data
    Leaf = 1,
    /// Internal page - contains keys and child page references
    Internal = 2,
    /// Overflow page - for large values that don't fit in a single page
    Overflow = 3,
}

impl TryFrom<u8> for PageType {
    type Error = Error;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(PageType::Free),
            1 => Ok(PageType::Leaf),
            2 => Ok(PageType::Internal),
            3 => Ok(PageType::Overflow),
            _ => Err(Error::InvalidPageType(value)),
        }
    }
}

impl PageType {
    /// Check if this is a B+Tree page (leaf or internal)
    pub fn is_btree_page(&self) -> bool {
        matches!(self, PageType::Leaf | PageType::Internal)
    }

    /// Check if this is a free page
    pub fn is_free(&self) -> bool {
        matches!(self, PageType::Free)
    }

    /// Check if this is an overflow page
    pub fn is_overflow(&self) -> bool {
        matches!(self, PageType::Overflow)
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
        assert_eq!(PageType::Free as u8, 0);
        assert_eq!(PageType::Leaf as u8, 1);
        assert_eq!(PageType::Internal as u8, 2);
        assert_eq!(PageType::Overflow as u8, 3);
    }

    #[test]
    fn test_page_type_from_u8() {
        assert_eq!(PageType::try_from(0u8).unwrap(), PageType::Free);
        assert_eq!(PageType::try_from(1u8).unwrap(), PageType::Leaf);
        assert_eq!(PageType::try_from(2u8).unwrap(), PageType::Internal);
        assert_eq!(PageType::try_from(3u8).unwrap(), PageType::Overflow);
    }

    #[test]
    fn test_page_type_from_u8_invalid() {
        for invalid in 4u8..=255u8 {
            match PageType::try_from(invalid) {
                Err(Error::InvalidPageType(val)) => assert_eq!(val, invalid),
                _ => panic!("Expected InvalidPageType error for value {invalid}"),
            }
        }
    }

    #[test]
    fn test_page_type_helper_methods() {
        assert!(PageType::Leaf.is_btree_page());
        assert!(PageType::Internal.is_btree_page());
        assert!(!PageType::Free.is_btree_page());
        assert!(!PageType::Overflow.is_btree_page());

        assert!(PageType::Free.is_free());
        assert!(!PageType::Leaf.is_free());

        assert!(PageType::Overflow.is_overflow());
        assert!(!PageType::Leaf.is_overflow());
    }
}
