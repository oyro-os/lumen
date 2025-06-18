#ifndef LUMEN_STORAGE_PAGE_H
#define LUMEN_STORAGE_PAGE_H

#include <lumen/memory/allocator.h>
#include <lumen/types.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>

namespace lumen {

// Forward declarations
class BufferPool;
class PageManager;

// Page types for different storage purposes
enum class PageType : uint8_t {
    Free = 0,      // Free page
    Meta = 1,      // Metadata page
    Data = 2,      // Data page
    Index = 3,     // Index page
    Overflow = 4,  // Overflow page for large data
    WAL = 5,       // Write-ahead log page
    Directory = 6  // Directory page for page management
};

// Page header structure (fixed size at beginning of each page)
struct PageHeader {
    PageID page_id;              // Unique page identifier
    PageType page_type;          // Type of page
    uint8_t flags;               // Page flags (dirty, pinned, etc.)
    uint16_t free_space_offset;  // Offset to start of free space
    uint16_t free_space_size;    // Size of free space
    uint16_t slot_count;         // Number of slots used
    uint32_t checksum;           // Page checksum for integrity
    TransactionID lsn;           // Log sequence number for recovery

    static constexpr size_t kSize = 32;  // Fixed header size

    PageHeader()
        : page_id(kInvalidPageID),
          page_type(PageType::Free),
          flags(0),
          free_space_offset(kSize),
          free_space_size(kPageSize - kSize),
          slot_count(0),
          checksum(0),
          lsn(0) {}
};

static_assert(sizeof(PageHeader) <= PageHeader::kSize, "PageHeader too large");

// Page flags
namespace PageFlags {
constexpr uint8_t kDirty = 0x01;    // Page has been modified
constexpr uint8_t kPinned = 0x02;   // Page is pinned in memory
constexpr uint8_t kLocked = 0x04;   // Page is locked for updates
constexpr uint8_t kDeleted = 0x08;  // Page is marked for deletion
}  // namespace PageFlags

// Slot directory entry for variable-length records
struct SlotEntry {
    uint16_t offset;  // Offset from start of page
    uint16_t length;  // Length of the record

    bool is_free() const {
        return offset == 0 && length == 0;
    }
    void mark_free() {
        offset = 0;
        length = 0;
    }
};

// Page class representing a single page in storage
class Page {
   public:
    explicit Page(PageID page_id);
    ~Page();

    // Non-copyable and non-movable (due to mutex)
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&) = delete;
    Page& operator=(Page&&) = delete;

    // Basic accessors
    PageID page_id() const {
        return header_.page_id;
    }
    PageType page_type() const {
        return header_.page_type;
    }
    void set_page_type(PageType type) {
        header_.page_type = type;
        mark_dirty();
    }

    // Page state
    bool is_dirty() const {
        return header_.flags & PageFlags::kDirty;
    }
    bool is_pinned() const {
        return header_.flags & PageFlags::kPinned;
    }
    bool is_locked() const {
        return header_.flags & PageFlags::kLocked;
    }
    bool is_deleted() const {
        return header_.flags & PageFlags::kDeleted;
    }

    void mark_dirty() {
        header_.flags |= PageFlags::kDirty;
    }
    void mark_clean() {
        header_.flags &= ~PageFlags::kDirty;
    }
    void set_pinned(bool pinned) {
        if (pinned) {
            header_.flags |= PageFlags::kPinned;
        } else {
            header_.flags &= ~PageFlags::kPinned;
        }
    }

    // Free space management
    uint16_t free_space_size() const {
        return header_.free_space_size;
    }
    uint16_t free_space_offset() const {
        return header_.free_space_offset;
    }

    // Slot management
    uint16_t slot_count() const {
        return header_.slot_count;
    }
    SlotEntry* get_slot(uint16_t slot_id);
    const SlotEntry* get_slot(uint16_t slot_id) const;

    // Record operations
    SlotID insert_record(const void* data, size_t size);
    bool update_record(SlotID slot_id, const void* data, size_t size);
    bool delete_record(SlotID slot_id);
    const void* get_record(SlotID slot_id, size_t* size = nullptr) const;

    // Page data access
    void* data() {
        return data_.get();
    }
    const void* data() const {
        return data_.get();
    }
    char* raw_data() {
        return reinterpret_cast<char*>(data_.get());
    }
    const char* raw_data() const {
        return reinterpret_cast<const char*>(data_.get());
    }

    // Serialization
    void serialize_to(void* buffer) const;
    void deserialize_from(const void* buffer);

    // Integrity checking
    uint32_t calculate_checksum() const;
    bool verify_checksum() const;
    void update_checksum();

    // LSN management for recovery
    TransactionID get_lsn() const {
        return header_.lsn;
    }
    void set_lsn(TransactionID lsn) {
        header_.lsn = lsn;
        mark_dirty();
    }

    // Page compaction
    void compact();

    // Lock management
    void read_lock() {
        mutex_.lock_shared();
    }
    void read_unlock() {
        mutex_.unlock_shared();
    }
    void write_lock() {
        mutex_.lock();
    }
    void write_unlock() {
        mutex_.unlock();
    }

   private:
    PageHeader header_;
    std::unique_ptr<void, void (*)(void*)> data_;
    mutable std::shared_mutex mutex_;

    // Internal helpers
    SlotEntry* slot_directory() {
        return reinterpret_cast<SlotEntry*>(raw_data() + PageHeader::kSize);
    }
    const SlotEntry* slot_directory() const {
        return reinterpret_cast<const SlotEntry*>(raw_data() + PageHeader::kSize);
    }

    size_t slot_directory_size() const {
        return header_.slot_count * sizeof(SlotEntry);
    }
    size_t available_space() const;
    void defragment();
    SlotID find_free_slot();

    // Header synchronization
    void sync_header_to_data();
    void sync_header_from_data();
};

// RAII page lock guards
class PageReadLock {
   public:
    explicit PageReadLock(Page& page) : page_(page) {
        page_.read_lock();
    }
    ~PageReadLock() {
        page_.read_unlock();
    }

   private:
    Page& page_;
};

class PageWriteLock {
   public:
    explicit PageWriteLock(Page& page) : page_(page) {
        page_.write_lock();
    }
    ~PageWriteLock() {
        page_.write_unlock();
    }

   private:
    Page& page_;
};

// Page reference counting for buffer pool management
class PageRef {
   public:
    PageRef() = default;
    explicit PageRef(std::shared_ptr<Page> page) : page_(std::move(page)) {}

    Page* operator->() {
        return page_.get();
    }
    const Page* operator->() const {
        return page_.get();
    }
    Page& operator*() {
        return *page_;
    }
    const Page& operator*() const {
        return *page_;
    }

    explicit operator bool() const {
        return page_ != nullptr;
    }
    Page* get() {
        return page_.get();
    }
    const Page* get() const {
        return page_.get();
    }

    void reset() {
        page_.reset();
    }

   private:
    std::shared_ptr<Page> page_;
};

// Page factory for creating pages with proper initialization
class PageFactory {
   public:
    static std::unique_ptr<Page> create_page(PageID page_id, PageType type = PageType::Data);
    static std::unique_ptr<Page> load_page(PageID page_id, const void* data);
};

}  // namespace lumen

#endif  // LUMEN_STORAGE_PAGE_H