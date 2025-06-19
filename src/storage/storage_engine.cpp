#include <lumen/storage/storage_engine.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace lumen {

// FileHandle implementation
FileHandle::FileHandle(const std::filesystem::path& path, std::ios::openmode mode)
    : path_(path) {
    // Ensure parent directory exists
    std::filesystem::create_directories(path_.parent_path());
    
    file_.open(path_, mode | std::ios::binary);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open file: " + path_.string());
    }
}

FileHandle::~FileHandle() {
    close();
}

FileHandle::FileHandle(FileHandle&& other) noexcept
    : path_(std::move(other.path_)), file_(std::move(other.file_)) {
    other.path_.clear();
}

FileHandle& FileHandle::operator=(FileHandle&& other) noexcept {
    if (this != &other) {
        close();
        path_ = std::move(other.path_);
        file_ = std::move(other.file_);
        other.path_.clear();
    }
    return *this;
}

void FileHandle::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
}

bool FileHandle::read(void* buffer, size_t size, size_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open()) {
        return false;
    }

    file_.seekg(offset, std::ios::beg);
    if (!file_.good()) {
        return false;
    }

    file_.read(static_cast<char*>(buffer), size);
    return file_.gcount() == static_cast<std::streamsize>(size);
}

bool FileHandle::write(const void* buffer, size_t size, size_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open()) {
        return false;
    }

    file_.clear();  // Clear any error flags
    file_.seekp(offset, std::ios::beg);
    if (!file_.good()) {
        return false;
    }

    file_.write(static_cast<const char*>(buffer), size);
    file_.flush();  // Ensure data is written
    return file_.good();
}

bool FileHandle::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open()) {
        return false;
    }

    file_.flush();
    return file_.good();
}

size_t FileHandle::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open()) {
        return 0;
    }

    file_.seekg(0, std::ios::end);
    return static_cast<size_t>(file_.tellg());
}

bool FileHandle::truncate(size_t new_size) {
    // Platform-specific implementation would go here
    // For now, we'll just return false as truncation requires OS-specific calls
    return false;
}

// StorageEngine implementation
StorageEngine::StorageEngine(const StorageConfig& config)
    : config_(config), buffer_pool_(BufferPoolFactory::create()) {
    std::memset(&metadata_, 0, sizeof(metadata_));
    metadata_.magic_number = 0x4C554D4E;  // "LUMN"
    metadata_.version = 1;
    metadata_.page_size = config_.page_size;

    // Note: Buffer pool will be connected to storage engine after construction
}

StorageEngine::~StorageEngine() {
    close();
}

bool StorageEngine::open(const std::string& db_name) {
    if (is_open_.load(std::memory_order_acquire)) {
        return false;  // Already open
    }

    current_database_ = db_name;
    std::filesystem::path db_path = database_path(db_name);

    // Create data directory if needed
    if (!create_data_directory()) {
        return false;
    }

    // Check if metadata file exists (more reliable than checking directory)
    std::filesystem::path metadata_path = db_path / "metadata.db";
    bool exists = std::filesystem::exists(metadata_path);

    if (exists && config_.error_if_exists) {
        return false;
    }

    if (!exists && !config_.create_if_missing) {
        return false;
    }

    // Create database directory if needed
    if (!exists) {
        std::error_code ec;
        if (!std::filesystem::create_directories(db_path, ec)) {
            return false;
        }
    }

    // Open or create database
    bool success = exists ? open_existing_database() : initialize_new_database();
    if (success) {
        is_open_.store(true, std::memory_order_release);
    }

    return success;
}

void StorageEngine::close() {
    if (!is_open_.load(std::memory_order_acquire)) {
        return;
    }

    // Flush all pages
    flush_all_pages();

    // Save metadata
    save_metadata();

    // Close all file handles
    {
        std::unique_lock<std::shared_mutex> lock(file_handles_mutex_);
        page_files_.clear();
    }

    // Close metadata file
    metadata_file_.reset();

    // Clear state
    current_database_.clear();
    free_pages_.clear();
    is_open_.store(false, std::memory_order_release);
}

PageRef StorageEngine::fetch_page(PageID page_id) {
    if (!is_open()) {
        return PageRef{};
    }

    // Check if page is in free list (deleted)
    {
        std::lock_guard<std::mutex> lock(free_pages_mutex_);
        if (std::find(free_pages_.begin(), free_pages_.end(), page_id) != free_pages_.end()) {
            return PageRef{};  // Page is deleted
        }
    }

    // Try buffer pool first
    PageRef page = buffer_pool_->fetch_page(page_id);
    if (page) {
        return page;
    }

    // Page not in buffer pool, load from disk
    auto loaded_page = read_page_from_disk(page_id);
    if (!loaded_page) {
        return PageRef{};
    }

    // Add to buffer pool and return
    // Note: Buffer pool will handle eviction if needed
    return buffer_pool_->fetch_page(page_id);
}

PageRef StorageEngine::new_page(PageType type) {
    if (!is_open()) {
        return PageRef{};
    }

    // Create page via buffer pool - buffer pool will assign page ID
    PageRef page = buffer_pool_->new_page(type);
    if (!page) {
        return PageRef{};
    }

    // Update metadata
    metadata_.page_count++;
    save_metadata();

    return page;
}

bool StorageEngine::delete_page(PageID page_id) {
    if (!is_open()) {
        return false;
    }

    // Try to remove from buffer pool
    buffer_pool_->delete_page(page_id);

    // Add to free list
    deallocate_page(page_id);

    // Delete the page file
    std::filesystem::path page_path = page_file_path(current_database_, page_id);
    std::error_code ec;
    std::filesystem::remove(page_path, ec);

    // Update metadata
    save_metadata();

    return true;
}

bool StorageEngine::flush_page(PageID page_id) {
    if (!is_open()) {
        return false;
    }

    return buffer_pool_->flush_page(page_id);
}

void StorageEngine::flush_all_pages() {
    if (!is_open()) {
        return;
    }

    buffer_pool_->flush_all_pages();
}

bool StorageEngine::create_database(const std::string& db_name) {
    std::filesystem::path db_path = database_path(db_name);

    // Check if already exists
    if (std::filesystem::exists(db_path)) {
        return false;
    }

    // Create database directory
    std::error_code ec;
    if (!std::filesystem::create_directories(db_path, ec)) {
        return false;
    }

    return true;
}

bool StorageEngine::drop_database(const std::string& db_name) {
    if (current_database_ == db_name && is_open()) {
        close();
    }

    std::filesystem::path db_path = database_path(db_name);
    std::error_code ec;
    return std::filesystem::remove_all(db_path, ec) > 0;
}

bool StorageEngine::database_exists(const std::string& db_name) const {
    return std::filesystem::exists(database_path(db_name));
}

std::vector<std::string> StorageEngine::list_databases() const {
    std::vector<std::string> databases;
    std::filesystem::path data_dir(config_.data_directory);

    if (!std::filesystem::exists(data_dir)) {
        return databases;
    }

    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.is_directory()) {
            databases.push_back(entry.path().filename().string());
        }
    }

    return databases;
}

std::filesystem::path StorageEngine::database_path(const std::string& db_name) const {
    return std::filesystem::path(config_.data_directory) / db_name;
}

std::filesystem::path StorageEngine::page_file_path(const std::string& db_name,
                                                     PageID page_id) const {
    // Use a simple naming scheme: pages are stored in subdirectories
    // to avoid having too many files in one directory
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(8) << page_id;
    std::string page_str = ss.str();

    // Create subdirectories based on page ID (e.g., 00/00/00000001.page)
    std::filesystem::path db_path = database_path(db_name);
    std::filesystem::path page_path = db_path / page_str.substr(0, 2) / page_str.substr(2, 2) /
                                      (page_str + ".page");

    return page_path;
}

bool StorageEngine::load_metadata() {
    if (!metadata_file_) {
        return false;
    }

    DatabaseMetadata temp_metadata;
    if (!metadata_file_->read(&temp_metadata, sizeof(temp_metadata), 0)) {
        return false;
    }

    // Validate metadata
    if (temp_metadata.magic_number != 0x4C554D4E) {
        return false;
    }

    if (temp_metadata.version != 1) {
        return false;
    }

    if (temp_metadata.page_size != config_.page_size) {
        return false;
    }

    metadata_ = temp_metadata;

    // Load free pages
    if (metadata_.first_free_page != kInvalidPageID) {
        // TODO: Load free page list from disk
    }

    return true;
}

bool StorageEngine::save_metadata() {
    if (!metadata_file_) {
        return false;
    }

    // Update modification time
    auto now = std::chrono::system_clock::now();
    metadata_.last_modified_time =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    bool write_result = metadata_file_->write(&metadata_, sizeof(metadata_), 0);
    bool sync_result = metadata_file_->sync();
    
    return write_result && sync_result;
}

bool StorageEngine::create_data_directory() {
    std::filesystem::path data_dir(config_.data_directory);

    if (std::filesystem::exists(data_dir)) {
        return true;
    }

    std::error_code ec;
    return std::filesystem::create_directories(data_dir, ec);
}

FileHandle* StorageEngine::get_or_create_page_file(PageID page_id) {
    std::unique_lock<std::shared_mutex> lock(file_handles_mutex_);

    auto it = page_files_.find(page_id);
    if (it != page_files_.end()) {
        return it->second.get();
    }

    // Create page file path and ensure directories exist
    std::filesystem::path page_path = page_file_path(current_database_, page_id);
    std::filesystem::create_directories(page_path.parent_path());

    // Open or create the file
    auto file_handle = std::make_unique<FileHandle>(
        page_path, std::ios::in | std::ios::out | std::ios::app);

    FileHandle* handle_ptr = file_handle.get();
    page_files_[page_id] = std::move(file_handle);

    return handle_ptr;
}

PageID StorageEngine::allocate_page() {
    std::lock_guard<std::mutex> lock(free_pages_mutex_);

    PageID page_id;

    if (!free_pages_.empty()) {
        // Reuse a free page
        page_id = free_pages_.back();
        free_pages_.pop_back();
        metadata_.free_page_count--;
    } else {
        // Allocate a new page
        page_id = static_cast<PageID>(metadata_.page_count + 1);
    }

    return page_id;
}

void StorageEngine::deallocate_page(PageID page_id) {
    std::lock_guard<std::mutex> lock(free_pages_mutex_);
    free_pages_.push_back(page_id);
    metadata_.free_page_count++;
}

std::shared_ptr<Page> StorageEngine::read_page_from_disk(PageID page_id) {
    FileHandle* file_handle = get_or_create_page_file(page_id);
    if (!file_handle) {
        return nullptr;
    }

    // Check if file exists and has correct size
    size_t file_size = file_handle->size();
    if (file_size == 0) {
        return nullptr;  // Page doesn't exist on disk
    }

    if (file_size != kPageSize) {
        return nullptr;  // Corrupted page file
    }

    // Read page data
    std::vector<char> buffer(kPageSize);
    if (!file_handle->read(buffer.data(), kPageSize, 0)) {
        return nullptr;
    }

    // Create page from data
    auto page = PageFactory::load_page(page_id, buffer.data());
    return std::shared_ptr<Page>(std::move(page));
}

bool StorageEngine::write_page_to_disk(const Page& page) {
    FileHandle* file_handle = get_or_create_page_file(page.page_id());
    if (!file_handle) {
        return false;
    }

    // Serialize page to buffer
    std::vector<char> buffer(kPageSize);
    page.serialize_to(buffer.data());

    // Write to disk
    if (!file_handle->write(buffer.data(), kPageSize, 0)) {
        return false;
    }

    // Sync if configured
    if (config_.sync_on_commit) {
        return file_handle->sync();
    }

    return true;
}

bool StorageEngine::initialize_new_database() {
    // Database directory should already exist from open()
    std::filesystem::path db_path = database_path(current_database_);
    if (!std::filesystem::exists(db_path)) {
        return false;
    }

    // Initialize metadata
    auto now = std::chrono::system_clock::now();
    metadata_.creation_time =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    metadata_.last_modified_time = metadata_.creation_time;

    // Create metadata file
    std::filesystem::path metadata_path = db_path / "metadata.db";
    try {
        metadata_file_ = std::make_unique<FileHandle>(metadata_path,
                                                      std::ios::in | std::ios::out | std::ios::trunc);
    } catch (const std::exception& e) {
        return false;
    }

    // Save initial metadata
    return save_metadata();
}

bool StorageEngine::open_existing_database() {
    std::filesystem::path db_path = database_path(current_database_);
    std::filesystem::path metadata_path = db_path / "metadata.db";

    // Open metadata file
    if (!std::filesystem::exists(metadata_path)) {
        return false;
    }

    metadata_file_ =
        std::make_unique<FileHandle>(metadata_path, std::ios::in | std::ios::out);

    // Load metadata
    return load_metadata();
}

// StorageEngineFactory implementation
std::shared_ptr<StorageEngine> StorageEngineFactory::create(const StorageConfig& config) {
    auto engine = std::make_shared<StorageEngine>(config);
    // Connect buffer pool to storage engine
    if (engine->buffer_pool()) {
        engine->buffer_pool()->set_storage_engine(engine);
    }
    return engine;
}

// StorageManager implementation
StorageManager& StorageManager::instance() {
    static StorageManager instance;
    return instance;
}

std::shared_ptr<StorageEngine> StorageManager::create_engine(const std::string& name,
                                                              const StorageConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = engines_.find(name);
    if (it != engines_.end()) {
        return it->second;
    }

    auto engine = StorageEngineFactory::create(config);
    engines_[name] = engine;
    return engine;
}

std::shared_ptr<StorageEngine> StorageManager::get_engine(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = engines_.find(name);
    return (it != engines_.end()) ? it->second : nullptr;
}

bool StorageManager::remove_engine(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return engines_.erase(name) > 0;
}

std::vector<std::string> StorageManager::list_engines() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(engines_.size());

    for (const auto& [name, engine] : engines_) {
        names.push_back(name);
    }

    return names;
}

}  // namespace lumen