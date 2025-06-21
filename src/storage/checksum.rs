//! CRC32 checksum implementation for page integrity

use crate::common::error::Error;
use crate::storage::page_constants::PAGE_SIZE;
use crc32fast::Hasher;

/// Calculate CRC32 checksum for data
pub fn calculate_crc32(data: &[u8]) -> u32 {
    let mut hasher = Hasher::new();
    hasher.update(data);
    hasher.finalize()
}

/// Calculate CRC32 checksum excluding the checksum field itself
///
/// In the 16-byte header (per plan/storage-format.md):
/// - `page_id(4)` + `page_type(1)` + `flags(1)` + `free_space(2)` = 8 bytes
/// - checksum(4) at bytes 8-11
/// - lsn(4) at bytes 12-15
///
/// We hash everything except the checksum field to allow verification.
///
/// # Errors
///
/// Returns `Error::InvalidInput` if `page_data` is not exactly `PAGE_SIZE` bytes
pub fn calculate_page_checksum(page_data: &[u8]) -> Result<u32, Error> {
    if page_data.len() != PAGE_SIZE {
        return Err(Error::InvalidInput(format!(
            "Invalid page size: expected {}, got {}",
            PAGE_SIZE,
            page_data.len()
        )));
    }

    let mut hasher = Hasher::new();

    // Hash everything except the checksum field (bytes 8-11 in header)
    hasher.update(&page_data[0..8]); // Before checksum
    hasher.update(&page_data[12..]); // After checksum (lsn + all page data)

    Ok(hasher.finalize())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_crc32_empty_data() {
        let data = [];
        let checksum = calculate_crc32(&data);
        assert_eq!(checksum, 0); // CRC32 of empty data is 0
    }

    #[test]
    fn test_crc32_known_value() {
        // Test vector from CRC32 specification
        let data = b"The quick brown fox jumps over the lazy dog";
        let checksum = calculate_crc32(data);
        assert_eq!(checksum, 0x414F_A339);
    }

    #[test]
    fn test_page_checksum_correct_size() {
        let mut page_data = vec![0u8; PAGE_SIZE];
        page_data[0] = 0x42;

        let result = calculate_page_checksum(&page_data);
        assert!(result.is_ok());
    }

    #[test]
    fn test_page_checksum_wrong_size() {
        let page_data = vec![0u8; 1024];
        let result = calculate_page_checksum(&page_data);
        assert!(matches!(result, Err(Error::InvalidInput(_))));
    }

    #[test]
    fn test_checksum_field_excluded() {
        let mut page1 = vec![0u8; PAGE_SIZE];
        let mut page2 = vec![0u8; PAGE_SIZE];

        // Make pages identical except for checksum field
        for i in 0..PAGE_SIZE {
            if !(8..12).contains(&i) {
                #[allow(clippy::cast_possible_truncation)]
                let val = (i % 256) as u8; // i % 256 is always 0-255, safe to cast
                page1[i] = val;
                page2[i] = val;
            }
        }

        // Set different values in checksum field (bytes 8-11 in 16-byte header)
        page1[8..12].copy_from_slice(&[0xFF, 0xFF, 0xFF, 0xFF]);
        page2[8..12].copy_from_slice(&[0x00, 0x00, 0x00, 0x00]);

        let checksum1 = calculate_page_checksum(&page1).unwrap();
        let checksum2 = calculate_page_checksum(&page2).unwrap();

        assert_eq!(checksum1, checksum2);
    }
}
