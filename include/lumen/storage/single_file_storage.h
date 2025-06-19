#ifndef LUMEN_STORAGE_SINGLE_FILE_STORAGE_H
#define LUMEN_STORAGE_SINGLE_FILE_STORAGE_H

#include <lumen/storage/buffer_pool.h>
#include <lumen/storage/page.h>
#include <lumen/types.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

namespace lumen {

// Page types as defined in storage-format.md
enum class PageTypeV2 : uint8_t {
    HEADER = 0x01,
    TABLE_METADATA = 0x02,
    DATA = 0x03,
    BTREE_INTERNAL = 0x04,
    BTREE_LEAF = 0x05,
    VECTOR_INDEX = 0x06,
    OVERFLOW_PAGE = 0x07,  // Renamed to avoid macro conflict
    FREE_LIST = 0x08,
    BLOOM_FILTER = 0x09
};

// Header page structure (4KB)
struct HeaderPage {
    // Magic and version (16 bytes)
    char magic[8];       // "LUMENDB\0"
    uint32_t version;    // 0x00010000 (1.0.0)
    uint32_t page_size;  // 4096

    // File info (32 bytes)
    uint64_t file_size;     // Total file size
    uint64_t page_count;    // Total pages
    uint64_t free_pages;    // Number of free pages
    uint64_t wal_sequence;  // WAL sequence number

    // Root pages (64 bytes)
    uint32_t metadata_root;   // Root of metadata B+tree
    uint32_t table_root;      // Root of table B+tree
    uint32_t free_list_head;  // Head of free page list
    uint32_t reserved[13];    // Future use

    // Checksums (16 bytes)
    uint64_t header_checksum;
    uint64_t file_checksum;

    // Feature flags (32 bytes)
    uint64_t features;  // Enabled features
    uint64_t flags;     // Runtime flags
    uint64_t reserved2[2];

    // Padding to 4KB
    char padding[3936];

    HeaderPage() {
        std::memset(this, 0, sizeof(*this));
        std::memcpy(magic, "LUMENDB", 8);
        version = 0x00010000;
        page_size = kPageSize;
        metadata_root = kInvalidPageID;
        table_root = kInvalidPageID;
        free_list_head = kInvalidPageID;
    }
};

static_assert(sizeof(HeaderPage) == kPageSize, "HeaderPage must be exactly 4KB");

// Common page header for all pages (16 bytes)
struct PageHeaderV2 {
    uint32_t page_id;     // Page number
    uint8_t type;         // PageTypeV2
    uint8_t flags;        // Page-specific flags
    uint16_t free_space;  // Bytes of free space
    uint32_t checksum;    // CRC32 of page content
    uint32_t lsn;         // Log sequence number

    static constexpr size_t kSize = 16;
};

static_assert(sizeof(PageHeaderV2) == PageHeaderV2::kSize, "PageHeaderV2 size mismatch");

// Single-file storage configuration
struct SingleFileStorageConfig {
    std::string database_path = "lumen.db";
    std::string wal_path = "lumen.wal";
    size_t buffer_pool_size = 256;  // Number of pages (256 * 4KB = 1MB)
    bool create_if_missing = true;
    bool error_if_exists = false;
    bool enable_wal = true;
    bool sync_on_commit = true;
    size_t initial_size_mb = 1;  // Initial file size

    static SingleFileStorageConfig default_config() {
        return SingleFileStorageConfig{};
    }
};

// Single-file storage engine implementation
class SingleFileStorage : public std::enable_shared_from_this<SingleFileStorage>,
                          public IStorageBackend {
   public:
    explicit SingleFileStorage(
        const SingleFileStorageConfig& config = SingleFileStorageConfig::default_config());
    ~SingleFileStorage();

    // Non-copyable and non-movable
    SingleFileStorage(const SingleFileStorage&) = delete;
    SingleFileStorage& operator=(const SingleFileStorage&) = delete;
    SingleFileStorage(SingleFileStorage&&) = delete;
    SingleFileStorage& operator=(SingleFileStorage&&) = delete;

    // Database operations
    bool create();
    bool open();
    void close();
    bool is_open() const {
        return is_open_.load();
    }

    // Page operations
    PageRef fetch_page(PageID page_id);
    PageRef new_page(PageTypeV2 type);
    bool delete_page(PageID page_id);
    bool flush_page(PageID page_id);
    void flush_all_pages();

    // Metadata access
    const HeaderPage& header() const {
        return header_;
    }
    size_t page_count() const {
        return header_.page_count;
    }
    size_t free_page_count() const {
        return header_.free_pages;
    }

    // Buffer pool access
    BufferPool* buffer_pool() {
        return buffer_pool_.get();
    }
    const BufferPool* buffer_pool() const {
        return buffer_pool_.get();
    }

    // Configuration
    const SingleFileStorageConfig& config() const {
        return config_;
    }

   private:
    SingleFileStorageConfig config_;
    std::unique_ptr<BufferPool> buffer_pool_;
    HeaderPage header_;
    std::atomic<bool> is_open_{false};

    // File handle
    mutable std::mutex file_mutex_;
    std::fstream db_file_;
    std::fstream wal_file_;

    // Free page management
    mutable std::mutex free_pages_mutex_;
    std::vector<PageID> free_page_list_;

    // Internal methods
    bool read_header();
    bool write_header();
    bool grow_file(size_t new_page_count);
    PageID allocate_page();
    void deallocate_page(PageID page_id);
    uint64_t calculate_header_checksum() const;

    // Page I/O (implements IStorageBackend)
    std::shared_ptr<Page> read_page_from_disk(PageID page_id) override;
    bool write_page_to_disk(const Page& page) override;

    // Checksum calculation
    uint32_t calculate_checksum(const void* data, size_t size) const;
    bool verify_checksum(const PageHeaderV2& header, const void* page_data) const;
};

// Factory for creating single-file storage
class SingleFileStorageFactory {
   public:
    static std::shared_ptr<SingleFileStorage> create(
        const SingleFileStorageConfig& config = SingleFileStorageConfig::default_config());
};

}  // namespace lumen

#endif  // LUMEN_STORAGE_SINGLE_FILE_STORAGE_H