#include <lumen/storage/single_file_storage.h>
#include <lumen/common/logging.h>

#include <cstring>
#include <iostream>

namespace lumen {

// CRC32 table for checksum calculation
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table() {
    if (crc32_table_initialized)
        return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_initialized = true;
}

static uint32_t crc32(const void* data, size_t length) {
    init_crc32_table();

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

SingleFileStorage::SingleFileStorage(const SingleFileStorageConfig& config) : config_(config) {
    // Don't create buffer pool in constructor as 'this' is not fully constructed yet
    // It will be created when opening/creating the database
}

SingleFileStorage::~SingleFileStorage() {
    if (is_open()) {
        close();
    }
}

bool SingleFileStorage::create() {
    if (std::filesystem::exists(config_.database_path)) {
        if (config_.error_if_exists) {
            return false;
        }
        // Open existing database
        return open();
    }

    // Create parent directory if needed
    auto parent_path = std::filesystem::path(config_.database_path).parent_path();
    if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
        std::filesystem::create_directories(parent_path);
    }

    // Open file for writing
    db_file_.open(config_.database_path,
                  std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if (!db_file_.is_open()) {
        return false;
    }

    // Initialize header
    header_ = HeaderPage();
    header_.page_count = 1;  // Just the header page initially

    // Write header page
    if (!write_header()) {
        db_file_.close();
        std::filesystem::remove(config_.database_path);
        return false;
    }

    // Grow file to initial size
    size_t initial_pages = (config_.initial_size_mb * 1024 * 1024) / kPageSize;
    if (initial_pages > 1) {
        std::lock_guard<std::mutex> lock(free_pages_mutex_);
        if (!grow_file(initial_pages)) {
            db_file_.close();
            std::filesystem::remove(config_.database_path);
            return false;
        }
    }

    // Create buffer pool now that storage is initialized
    if (!buffer_pool_) {
        buffer_pool_ = std::make_unique<BufferPool>(config_.buffer_pool_size, this);
    }

    is_open_.store(true);
    return true;
}

bool SingleFileStorage::open() {
    if (is_open()) {
        return true;
    }

    if (!std::filesystem::exists(config_.database_path)) {
        if (config_.create_if_missing) {
            return create();
        }
        return false;
    }

    // Open existing file
    db_file_.open(config_.database_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!db_file_.is_open()) {
        return false;
    }

    // Read and verify header
    if (!read_header()) {
        db_file_.close();
        return false;
    }

    // Verify magic number
    if (std::memcmp(header_.magic, "LUMENDB", 8) != 0) {
        db_file_.close();
        return false;
    }

    // Verify page size
    if (header_.page_size != kPageSize) {
        db_file_.close();
        return false;
    }

    // Load free page list
    if (header_.free_list_head != kInvalidPageID) {
        std::lock_guard<std::mutex> lock(free_pages_mutex_);
        free_page_list_.clear();
        
        PageID current_page = header_.free_list_head;
        size_t loaded_count = 0;
        
        // Traverse the free page linked list
        while (current_page != kInvalidPageID && loaded_count < header_.free_pages) {
            // Read the free page
            auto page = read_page_from_disk(current_page);
            if (page) {
                free_page_list_.push_back(current_page);
                loaded_count++;
                
                // Free pages store the next free page ID in their first 4 bytes
                if (kPageSize >= sizeof(PageID)) {
                    std::memcpy(&current_page, page->data(), sizeof(PageID));
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        
        // Update free page count if mismatch
        if (loaded_count != header_.free_pages) {
            header_.free_pages = loaded_count;
            write_header();
        }
    }

    // Create buffer pool now that storage is initialized
    if (!buffer_pool_) {
        buffer_pool_ = std::make_unique<BufferPool>(config_.buffer_pool_size, this);
    }

    is_open_.store(true);
    return true;
}

void SingleFileStorage::close() {
    if (!is_open()) {
        return;
    }

    // Flush all dirty pages
    flush_all_pages();

    // Update and write header
    write_header();

    // Close files
    if (db_file_.is_open()) {
        db_file_.close();
    }
    if (wal_file_.is_open()) {
        wal_file_.close();
    }

    is_open_.store(false);
}

PageRef SingleFileStorage::fetch_page(PageID page_id) {
    if (!is_open() || page_id >= header_.page_count) {
        return PageRef();
    }

    return buffer_pool_->fetch_page(page_id);
}

PageRef SingleFileStorage::new_page(PageTypeV2 type) {
    if (!is_open()) {
        return PageRef();
    }

    PageID page_id = allocate_page();
    if (page_id == kInvalidPageID) {
        return PageRef();
    }

    // Create new page in buffer pool with our pre-allocated ID
    PageRef page = buffer_pool_->new_page(page_id, static_cast<PageType>(type));
    if (!page) {
        deallocate_page(page_id);
        return PageRef();
    }

    // Page is already initialized with correct type and marked dirty
    return page;
}

bool SingleFileStorage::delete_page(PageID page_id) {
    if (!is_open() || page_id == 0 || page_id >= header_.page_count) {
        return false;
    }

    // Remove from buffer pool
    buffer_pool_->delete_page(page_id);

    // Add to free list
    deallocate_page(page_id);

    return true;
}

bool SingleFileStorage::flush_page(PageID page_id) {
    if (!is_open()) {
        return false;
    }

    return buffer_pool_->flush_page(page_id);
}

void SingleFileStorage::flush_all_pages() {
    if (!is_open()) {
        return;
    }

    buffer_pool_->flush_all_pages();
}

bool SingleFileStorage::read_header() {
    std::lock_guard<std::mutex> lock(file_mutex_);

    db_file_.seekg(0, std::ios::beg);
    db_file_.read(reinterpret_cast<char*>(&header_), sizeof(HeaderPage));

    if (!db_file_.good()) {
        return false;
    }

    // Verify header checksum
    uint64_t calculated_checksum = calculate_header_checksum();
    if (header_.header_checksum != 0 && header_.header_checksum != calculated_checksum) {
        // Checksum mismatch - file may be corrupted
        // For now, update the checksum to match (recovery mode)
        header_.header_checksum = calculated_checksum;
        write_header();
    }

    return true;
}

bool SingleFileStorage::write_header() {
    std::lock_guard<std::mutex> lock(file_mutex_);

    // Update header checksum (zero it first, then calculate)
    uint64_t saved_checksum = header_.header_checksum;
    header_.header_checksum = 0;
    header_.header_checksum = calculate_header_checksum();

    db_file_.seekp(0, std::ios::beg);
    db_file_.write(reinterpret_cast<const char*>(&header_), sizeof(HeaderPage));
    db_file_.flush();

    return db_file_.good();
}

bool SingleFileStorage::grow_file(size_t new_page_count) {
    if (new_page_count <= header_.page_count) {
        return true;
    }

    std::lock_guard<std::mutex> lock(file_mutex_);

    // Seek to end and write zeros
    db_file_.seekp(0, std::ios::end);

    std::vector<char> zero_page(kPageSize, 0);
    size_t pages_to_add = new_page_count - header_.page_count;

    for (size_t i = 0; i < pages_to_add; ++i) {
        db_file_.write(zero_page.data(), kPageSize);
        if (!db_file_.good()) {
            return false;
        }
    }

    // Update free page list (caller must hold free_pages_mutex_ if needed)
    for (PageID id = header_.page_count; id < new_page_count; ++id) {
        free_page_list_.push_back(id);
    }
    header_.free_pages += pages_to_add;

    header_.page_count = new_page_count;
    header_.file_size = new_page_count * kPageSize;

    return true;
}

PageID SingleFileStorage::allocate_page() {
    std::lock_guard<std::mutex> lock(free_pages_mutex_);

    // Try to get from free list
    if (!free_page_list_.empty()) {
        PageID page_id = free_page_list_.back();
        free_page_list_.pop_back();
        header_.free_pages--;
        return page_id;
    }

    LOG_DEBUG << "allocate_page: No free pages, growing file from " 
              << header_.page_count << " pages";

    // Need to grow file
    size_t new_count = header_.page_count * 2;  // Double the size
    if (new_count < header_.page_count + 64) {
        new_count = header_.page_count + 64;  // Add at least 64 pages
    }

    LOG_DEBUG << "allocate_page: Growing to " << new_count << " pages";

    if (!grow_file(new_count)) {
        LOG_ERROR << "allocate_page: grow_file failed!";
        return kInvalidPageID;
    }

    LOG_DEBUG << "allocate_page: After grow, free_page_list size: " << free_page_list_.size();

    // Should have free pages now
    if (!free_page_list_.empty()) {
        PageID page_id = free_page_list_.back();
        free_page_list_.pop_back();
        header_.free_pages--;
        LOG_DEBUG << "allocate_page: Allocated page " << page_id;
        return page_id;
    }

    LOG_ERROR << "allocate_page: Still no free pages after grow!";
    return kInvalidPageID;
}

void SingleFileStorage::deallocate_page(PageID page_id) {
    std::lock_guard<std::mutex> lock(free_pages_mutex_);
    free_page_list_.push_back(page_id);
    header_.free_pages++;
}

std::shared_ptr<Page> SingleFileStorage::read_page_from_disk(PageID page_id) {
    if (page_id >= header_.page_count) {
        return nullptr;
    }

    auto page = std::make_shared<Page>(page_id);

    std::lock_guard<std::mutex> lock(file_mutex_);

    // Seek to page location
    size_t offset = static_cast<size_t>(page_id) * kPageSize;
    db_file_.seekg(offset, std::ios::beg);

    // Read entire page
    db_file_.read(static_cast<char*>(page->data()), kPageSize);

    if (!db_file_.good()) {
        return nullptr;
    }

    // Verify checksum if not header page
    if (page_id > 0) {
        PageHeaderV2* header = reinterpret_cast<PageHeaderV2*>(page->data());
        if (!verify_checksum(*header, page->data())) {
            // Checksum mismatch
            return nullptr;
        }
    }

    return page;
}

bool SingleFileStorage::write_page_to_disk(const Page& page) {
    PageID page_id = page.page_id();
    if (page_id >= header_.page_count) {
        return false;
    }

    std::lock_guard<std::mutex> lock(file_mutex_);

    // Update checksum if not header page
    if (page_id > 0) {
        // Need to cast away const to update checksum
        void* page_data = const_cast<void*>(page.data());
        PageHeaderV2* header = reinterpret_cast<PageHeaderV2*>(page_data);
        header->checksum =
            calculate_checksum(static_cast<const char*>(page.data()) + PageHeaderV2::kSize,
                               kPageSize - PageHeaderV2::kSize);
    }

    // Seek to page location
    size_t offset = static_cast<size_t>(page_id) * kPageSize;
    db_file_.seekp(offset, std::ios::beg);

    // Write entire page
    db_file_.write(static_cast<const char*>(page.data()), kPageSize);

    if (config_.sync_on_commit) {
        db_file_.flush();
    }

    return db_file_.good();
}

uint32_t SingleFileStorage::calculate_checksum(const void* data, size_t size) const {
    return crc32(data, size);
}

bool SingleFileStorage::verify_checksum(const PageHeaderV2& header, const void* page_data) const {
    uint32_t calculated = calculate_checksum(
        static_cast<const char*>(page_data) + PageHeaderV2::kSize, kPageSize - PageHeaderV2::kSize);
    return calculated == header.checksum;
}

uint64_t SingleFileStorage::calculate_header_checksum() const {
    // Calculate checksum of header excluding the checksum fields themselves
    size_t checksum_offset = offsetof(HeaderPage, header_checksum);
    uint64_t checksum = static_cast<uint64_t>(crc32(&header_, checksum_offset));
    
    // Also include the portion after the checksum fields
    size_t after_checksum_offset = offsetof(HeaderPage, features);
    size_t remaining_size = sizeof(HeaderPage) - after_checksum_offset;
    checksum ^= static_cast<uint64_t>(crc32(
        reinterpret_cast<const char*>(&header_) + after_checksum_offset, remaining_size));
    
    return checksum;
}

// Factory implementation
std::shared_ptr<SingleFileStorage> SingleFileStorageFactory::create(
    const SingleFileStorageConfig& config) {
    return std::make_shared<SingleFileStorage>(config);
}

}  // namespace lumen