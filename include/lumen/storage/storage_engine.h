#ifndef LUMEN_STORAGE_STORAGE_ENGINE_H
#define LUMEN_STORAGE_STORAGE_ENGINE_H

#include <lumen/storage/buffer_pool.h>
#include <lumen/storage/page.h>
#include <lumen/types.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lumen {

// Storage engine configuration
struct StorageConfig {
    std::string data_directory = "lumen_data";
    size_t page_size = kPageSize;
    size_t buffer_pool_size = 1024;  // Number of pages in buffer pool
    bool create_if_missing = true;
    bool error_if_exists = false;
    bool enable_wal = true;
    bool sync_on_commit = true;
    size_t max_open_files = 256;

    static StorageConfig default_config() {
        return StorageConfig{};
    }
};

// Database file metadata
struct DatabaseMetadata {
    uint32_t magic_number = 0x4C554D4E;  // "LUMN"
    uint32_t version = 1;
    uint32_t page_size = kPageSize;
    uint64_t page_count = 0;
    uint64_t free_page_count = 0;
    PageID first_free_page = kInvalidPageID;
    uint64_t creation_time = 0;
    uint64_t last_modified_time = 0;
    char reserved[456];  // Pad to 512 bytes

    static constexpr size_t kSize = 512;
};

static_assert(sizeof(DatabaseMetadata) == DatabaseMetadata::kSize,
              "DatabaseMetadata must be exactly 512 bytes");

// File handle wrapper for platform independence
class FileHandle {
   public:
    FileHandle() = default;
    explicit FileHandle(const std::filesystem::path& path, std::ios::openmode mode);
    ~FileHandle();

    // Non-copyable but movable
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&&) noexcept;
    FileHandle& operator=(FileHandle&&) noexcept;

    bool is_open() const {
        return file_.is_open();
    }
    void close();

    // File operations
    bool read(void* buffer, size_t size, size_t offset);
    bool write(const void* buffer, size_t size, size_t offset);
    bool sync();
    size_t size() const;
    bool truncate(size_t new_size);

    const std::filesystem::path& path() const {
        return path_;
    }

   private:
    std::filesystem::path path_;
    mutable std::fstream file_;
    mutable std::mutex mutex_;
};

// Main storage engine class
class StorageEngine : public std::enable_shared_from_this<StorageEngine> {
   public:
    explicit StorageEngine(const StorageConfig& config = StorageConfig::default_config());
    ~StorageEngine();

    // Non-copyable and non-movable
    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;
    StorageEngine(StorageEngine&&) = delete;
    StorageEngine& operator=(StorageEngine&&) = delete;

    // Initialization and shutdown
    bool open(const std::string& db_name);
    void close();
    bool is_open() const {
        return is_open_.load(std::memory_order_acquire);
    }

    // Page operations
    PageRef fetch_page(PageID page_id);
    PageRef new_page(PageType type = PageType::Data);
    bool delete_page(PageID page_id);
    bool flush_page(PageID page_id);
    void flush_all_pages();

    // Database operations
    bool create_database(const std::string& db_name);
    bool drop_database(const std::string& db_name);
    bool database_exists(const std::string& db_name) const;
    std::vector<std::string> list_databases() const;

    // Metadata access
    const DatabaseMetadata& metadata() const {
        return metadata_;
    }
    size_t page_count() const {
        return metadata_.page_count;
    }
    size_t free_page_count() const {
        return metadata_.free_page_count;
    }

    // Buffer pool access
    BufferPool* buffer_pool() {
        return buffer_pool_.get();
    }
    const BufferPool* buffer_pool() const {
        return buffer_pool_.get();
    }

    // Configuration
    const StorageConfig& config() const {
        return config_;
    }

    // File management
    std::filesystem::path database_path(const std::string& db_name) const;
    std::filesystem::path page_file_path(const std::string& db_name, PageID page_id) const;

   private:
    StorageConfig config_;
    std::unique_ptr<BufferPool> buffer_pool_;
    DatabaseMetadata metadata_;
    std::string current_database_;
    std::atomic<bool> is_open_{false};

    // File handles
    std::unique_ptr<FileHandle> metadata_file_;
    mutable std::shared_mutex file_handles_mutex_;
    std::unordered_map<PageID, std::unique_ptr<FileHandle>> page_files_;

    // Free page management
    mutable std::mutex free_pages_mutex_;
    std::vector<PageID> free_pages_;

    // Internal methods
    bool load_metadata();
    bool save_metadata();
    bool create_data_directory();
    FileHandle* get_or_create_page_file(PageID page_id);
    PageID allocate_page();
    void deallocate_page(PageID page_id);

   public:  // Made public for BufferPool integration
    // Page I/O
    std::shared_ptr<Page> read_page_from_disk(PageID page_id);
    bool write_page_to_disk(const Page& page);

   private:

    // Initialization
    bool initialize_new_database();
    bool open_existing_database();
};

// Storage engine factory
class StorageEngineFactory {
   public:
    static std::shared_ptr<StorageEngine> create(
        const StorageConfig& config = StorageConfig::default_config());
};

// Storage manager for managing multiple storage engines
class StorageManager {
   public:
    static StorageManager& instance();

    // Storage engine management
    std::shared_ptr<StorageEngine> create_engine(const std::string& name,
                                                  const StorageConfig& config);
    std::shared_ptr<StorageEngine> get_engine(const std::string& name);
    bool remove_engine(const std::string& name);
    std::vector<std::string> list_engines() const;

   private:
    StorageManager() = default;
    ~StorageManager() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<StorageEngine>> engines_;
};

}  // namespace lumen

#endif  // LUMEN_STORAGE_STORAGE_ENGINE_H