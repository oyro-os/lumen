//! Page constants and fundamental types for the storage layer

/// Page size in bytes - must be power of 2 and >= 4KB
pub const PAGE_SIZE: usize = 4096;

/// Page header size in bytes - MUST match plan/storage-format.md
pub const PAGE_HEADER_SIZE: usize = 16;

/// Usable space in page after header
pub const PAGE_USABLE_SIZE: usize = PAGE_SIZE - PAGE_HEADER_SIZE;

/// Page ID type - supports 16TB databases (4KB * 2^32)
pub type PageId = u32;

/// Invalid page ID sentinel value
pub const INVALID_PAGE_ID: PageId = 0;

/// Maximum valid page ID
pub const MAX_PAGE_ID: PageId = u32::MAX;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_page_constants() {
        assert_eq!(PAGE_SIZE, 4096);
        assert_eq!(PAGE_HEADER_SIZE, 16);
        assert_eq!(PAGE_USABLE_SIZE, 4080);
    }

    #[test]
    fn test_page_id_constants() {
        assert_eq!(INVALID_PAGE_ID, 0);
        assert_eq!(MAX_PAGE_ID, u32::MAX);
    }

    #[test]
    fn test_page_size_is_power_of_two() {
        assert!(PAGE_SIZE.is_power_of_two());
        // PAGE_SIZE is a constant 4096, so this would always be true
        // Just verify the actual value instead
        assert_eq!(PAGE_SIZE, 4096);
    }

    #[test]
    fn test_page_size_calculation() {
        // Ensure our constants are consistent
        assert_eq!(PAGE_HEADER_SIZE + PAGE_USABLE_SIZE, PAGE_SIZE);
    }

    #[test]
    fn test_database_size_calculation() {
        // With 32-bit page IDs and 4KB pages, we can address:
        // u32::MAX * 4KB = 17592186040320 bytes (approximately 16TB)
        let max_database_size = u64::from(MAX_PAGE_ID) * (PAGE_SIZE as u64);
        assert_eq!(max_database_size, 17_592_186_040_320);

        // This is approximately 16TB (actually 16TB - 4096 bytes)
        let sixteen_tb = 16u64 * 1024 * 1024 * 1024 * 1024;
        assert_eq!(sixteen_tb, 17_592_186_044_416);
        assert_eq!(sixteen_tb - max_database_size, 4096); // Exactly one page less
    }
}
