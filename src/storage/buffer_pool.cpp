#include <lumen/storage/buffer_pool.h>
#include <lumen/storage/storage_engine.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace lumen {

// Frame implementation
void Frame::update_access_time() {
    auto now = std::chrono::steady_clock::now();
    last_access_time.store(
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count(),
        std::memory_order_relaxed);
}

// LRU Eviction Policy
LRUEvictionPolicy::LRUEvictionPolicy(size_t pool_size)
    : pool_size_(pool_size), access_times_(pool_size, 0) {}

FrameID LRUEvictionPolicy::select_victim(const std::vector<Frame>& frames) {
    std::lock_guard<std::mutex> lock(mutex_);

    FrameID victim = kInvalidFrameID;
    uint64_t oldest_time = UINT64_MAX;

    for (size_t i = 0; i < frames.size(); ++i) {
        const auto& frame = frames[i];
        if (!frame.is_pinned() && frame.page) {
            uint64_t access_time = frame.last_access_time.load(std::memory_order_relaxed);
            if (access_time < oldest_time) {
                oldest_time = access_time;
                victim = static_cast<FrameID>(i);
            }
        }
    }

    return victim;
}

void LRUEvictionPolicy::access_frame(FrameID frame_id) {
    if (frame_id >= pool_size_)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    access_times_[frame_id] =
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

void LRUEvictionPolicy::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(access_times_.begin(), access_times_.end(), 0);
}

// Clock Eviction Policy
ClockEvictionPolicy::ClockEvictionPolicy(size_t pool_size)
    : pool_size_(pool_size), reference_bits_(pool_size) {
    for (auto& bit : reference_bits_) {
        bit.store(false, std::memory_order_relaxed);
    }
}

FrameID ClockEvictionPolicy::select_victim(const std::vector<Frame>& frames) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t start_hand = clock_hand_.load(std::memory_order_relaxed);
    size_t current = start_hand;

    // First pass: look for frames with reference bit not set
    do {
        const auto& frame = frames[current];
        if (!frame.is_pinned() && frame.page) {
            bool expected = true;
            if (reference_bits_[current].compare_exchange_weak(expected, false,
                                                               std::memory_order_relaxed)) {
                // Reference bit was set, clear it and continue
            } else {
                // Reference bit was not set, this is our victim
                clock_hand_.store((current + 1) % pool_size_, std::memory_order_relaxed);
                return static_cast<FrameID>(current);
            }
        }
        current = (current + 1) % pool_size_;
    } while (current != start_hand);

    // Second pass: if all reference bits were set, choose any unpinned frame
    current = start_hand;
    do {
        const auto& frame = frames[current];
        if (!frame.is_pinned() && frame.page) {
            // Clear reference bit and select this frame
            reference_bits_[current].store(false, std::memory_order_relaxed);
            clock_hand_.store((current + 1) % pool_size_, std::memory_order_relaxed);
            return static_cast<FrameID>(current);
        }
        current = (current + 1) % pool_size_;
    } while (current != start_hand);

    // If we can't find any victim, return invalid
    return kInvalidFrameID;
}

void ClockEvictionPolicy::access_frame(FrameID frame_id) {
    if (frame_id >= pool_size_)
        return;
    reference_bits_[frame_id].store(true, std::memory_order_relaxed);
}

void ClockEvictionPolicy::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& bit : reference_bits_) {
        bit.store(false, std::memory_order_relaxed);
    }
    clock_hand_.store(0, std::memory_order_relaxed);
}

// BufferPool implementation
BufferPool::BufferPool(size_t pool_size, std::unique_ptr<EvictionPolicy> eviction_policy)
    : pool_size_(pool_size), frames_(pool_size), eviction_policy_(std::move(eviction_policy)) {
    if (pool_size_ == 0) {
        throw std::invalid_argument("Buffer pool size must be greater than 0");
    }

    // Initialize free frame list
    free_frames_.reserve(pool_size_);
    for (size_t i = 0; i < pool_size_; ++i) {
        free_frames_.push_back(static_cast<FrameID>(i));
    }

    // Create default eviction policy if none provided
    if (!eviction_policy_) {
        eviction_policy_ = std::make_unique<ClockEvictionPolicy>(pool_size_);
    }
}

BufferPool::~BufferPool() {
    flush_all_pages();
}

PageRef BufferPool::fetch_page(PageID page_id) {
    if (page_id == kInvalidPageID) {
        return PageRef{};
    }

    stats_.page_requests.fetch_add(1, std::memory_order_relaxed);

    // First check if page is already in buffer pool
    {
        std::shared_lock<std::shared_mutex> lock(table_mutex_);
        auto it = page_table_.find(page_id);
        if (it != page_table_.end()) {
            FrameID frame_id = it->second;
            Frame& frame = frames_[frame_id];

            std::shared_lock<std::shared_mutex> frame_lock(frame.mutex);
            if (frame.page && frame.page->page_id() == page_id) {
                frame.pin();
                frame.update_access_time();
                update_frame_access(frame_id);
                stats_.page_hits.fetch_add(1, std::memory_order_relaxed);
                return PageRef{frame.page};
            }
        }
    }

    stats_.page_misses.fetch_add(1, std::memory_order_relaxed);

    // Page not in buffer pool, need to load it
    std::shared_ptr<Page> page = read_page_from_storage(page_id);
    if (!page) {
        return PageRef{};
    }

    // Find a frame to place the page
    FrameID frame_id = find_free_frame();
    if (frame_id == kInvalidFrameID) {
        frame_id = evict_frame();
        if (frame_id == kInvalidFrameID) {
            return PageRef{};  // Could not evict any frame
        }
    }

    Frame& frame = frames_[frame_id];

    // Install page in frame
    {
        std::unique_lock<std::shared_mutex> frame_lock(frame.mutex);
        std::unique_lock<std::shared_mutex> table_lock(table_mutex_);

        frame.page = page;
        frame.is_dirty.store(false, std::memory_order_relaxed);
        frame.pin();
        frame.update_access_time();

        page_table_[page_id] = frame_id;
    }

    update_frame_access(frame_id);
    return PageRef{page};
}

bool BufferPool::unpin_page(PageID page_id, bool is_dirty) {
    std::shared_lock<std::shared_mutex> lock(table_mutex_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }

    FrameID frame_id = it->second;
    Frame& frame = frames_[frame_id];

    std::shared_lock<std::shared_mutex> frame_lock(frame.mutex);
    if (!frame.page || frame.page->page_id() != page_id) {
        return false;
    }

    if (is_dirty) {
        frame.is_dirty.store(true, std::memory_order_relaxed);
    }

    frame.unpin();
    return true;
}

bool BufferPool::delete_page(PageID page_id) {
    std::unique_lock<std::shared_mutex> lock(table_mutex_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;  // Page not in buffer pool, nothing to do
    }

    FrameID frame_id = it->second;
    Frame& frame = frames_[frame_id];

    std::unique_lock<std::shared_mutex> frame_lock(frame.mutex);
    if (frame.is_pinned()) {
        return false;  // Cannot delete pinned page
    }

    // Remove from page table and free the frame
    page_table_.erase(it);
    frame.page.reset();
    frame.is_dirty.store(false, std::memory_order_relaxed);
    frame.pin_count.store(0, std::memory_order_relaxed);

    std::lock_guard<std::mutex> free_lock(free_frames_mutex_);
    free_frames_.push_back(frame_id);
    frame_available_.notify_one();

    return true;
}

PageRef BufferPool::new_page(PageType type) {
    PageID page_id = next_page_id_.fetch_add(1, std::memory_order_relaxed);

    // Find a frame for the new page
    FrameID frame_id = find_free_frame();
    if (frame_id == kInvalidFrameID) {
        frame_id = evict_frame();
        if (frame_id == kInvalidFrameID) {
            return PageRef{};
        }
    }

    // Create new page
    auto unique_page = PageFactory::create_page(page_id, type);
    std::shared_ptr<Page> page = std::move(unique_page);

    // Mark the page as dirty since it's a new page
    page->mark_dirty();

    Frame& frame = frames_[frame_id];

    // Install page in frame
    {
        std::unique_lock<std::shared_mutex> frame_lock(frame.mutex);
        std::unique_lock<std::shared_mutex> table_lock(table_mutex_);

        frame.page = page;
        frame.is_dirty.store(true, std::memory_order_relaxed);  // New page is dirty
        frame.pin();
        frame.update_access_time();

        page_table_[page_id] = frame_id;
    }

    update_frame_access(frame_id);
    return PageRef{page};
}

bool BufferPool::flush_page(PageID page_id) {
    std::shared_lock<std::shared_mutex> lock(table_mutex_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;  // Page not in buffer pool
    }

    FrameID frame_id = it->second;
    Frame& frame = frames_[frame_id];

    std::shared_lock<std::shared_mutex> frame_lock(frame.mutex);
    if (!frame.page || !frame.is_dirty.load(std::memory_order_relaxed)) {
        return true;  // Page not dirty
    }

    bool success = write_page_to_storage(*frame.page);
    if (success) {
        frame.is_dirty.store(false, std::memory_order_relaxed);
        stats_.pages_written.fetch_add(1, std::memory_order_relaxed);
    }

    return success;
}

void BufferPool::flush_all_pages() {
    std::shared_lock<std::shared_mutex> lock(table_mutex_);

    for (const auto& [page_id, frame_id] : page_table_) {
        Frame& frame = frames_[frame_id];
        std::shared_lock<std::shared_mutex> frame_lock(frame.mutex);

        if (frame.page && frame.is_dirty.load(std::memory_order_relaxed)) {
            if (write_page_to_storage(*frame.page)) {
                frame.is_dirty.store(false, std::memory_order_relaxed);
                stats_.pages_written.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    stats_.total_flushes.fetch_add(1, std::memory_order_relaxed);
}

void BufferPool::reset() {
    std::unique_lock<std::shared_mutex> lock(table_mutex_);

    page_table_.clear();

    for (auto& frame : frames_) {
        std::unique_lock<std::shared_mutex> frame_lock(frame.mutex);
        frame.page.reset();
        frame.is_dirty.store(false, std::memory_order_relaxed);
        frame.pin_count.store(0, std::memory_order_relaxed);
        frame.last_access_time.store(0, std::memory_order_relaxed);
    }

    std::lock_guard<std::mutex> free_lock(free_frames_mutex_);
    free_frames_.clear();
    for (size_t i = 0; i < pool_size_; ++i) {
        free_frames_.push_back(static_cast<FrameID>(i));
    }

    eviction_policy_->reset();
    stats_.reset();
    next_page_id_.store(1, std::memory_order_relaxed);
}

size_t BufferPool::used_frames() const {
    std::shared_lock<std::shared_mutex> lock(table_mutex_);
    return page_table_.size();
}

double BufferPool::utilization() const {
    return static_cast<double>(used_frames()) / pool_size_;
}

FrameID BufferPool::find_free_frame() {
    std::lock_guard<std::mutex> lock(free_frames_mutex_);

    if (free_frames_.empty()) {
        return kInvalidFrameID;
    }

    FrameID frame_id = free_frames_.back();
    free_frames_.pop_back();
    return frame_id;
}

FrameID BufferPool::evict_frame() {
    FrameID victim = eviction_policy_->select_victim(frames_);
    if (victim == kInvalidFrameID) {
        return kInvalidFrameID;
    }

    Frame& frame = frames_[victim];
    std::unique_lock<std::shared_mutex> frame_lock(frame.mutex);

    if (!frame.page || frame.is_pinned()) {
        return kInvalidFrameID;  // Frame state changed
    }

    PageID page_id = frame.page->page_id();

    // Write dirty page to storage
    if (frame.is_dirty.load(std::memory_order_relaxed)) {
        if (!write_page_to_storage(*frame.page)) {
            return kInvalidFrameID;  // Failed to write page
        }
        stats_.pages_written.fetch_add(1, std::memory_order_relaxed);
    }

    // Remove from page table
    {
        std::unique_lock<std::shared_mutex> table_lock(table_mutex_);
        page_table_.erase(page_id);
    }

    // Clear frame
    frame.page.reset();
    frame.is_dirty.store(false, std::memory_order_relaxed);
    frame.pin_count.store(0, std::memory_order_relaxed);

    stats_.pages_evicted.fetch_add(1, std::memory_order_relaxed);
    return victim;
}

bool BufferPool::write_page_to_storage(const Page& page) {
    auto engine = storage_engine_.lock();
    if (!engine) {
        // No storage engine attached, just return true
        return true;
    }

    // Delegate to storage engine
    return engine->write_page_to_disk(page);
}

std::shared_ptr<Page> BufferPool::read_page_from_storage(PageID page_id) {
    auto engine = storage_engine_.lock();
    if (!engine) {
        // No storage engine attached, create a new page
        auto unique_page = PageFactory::create_page(page_id, PageType::Data);
        return std::shared_ptr<Page>(std::move(unique_page));
    }

    // Delegate to storage engine
    return engine->read_page_from_disk(page_id);
}

void BufferPool::update_frame_access(FrameID frame_id) {
    eviction_policy_->access_frame(frame_id);
}

// BufferPoolFactory implementation
std::unique_ptr<BufferPool> BufferPoolFactory::create(const BufferPoolConfig& config) {
    auto eviction_policy = create_eviction_policy(config.eviction_policy, config.pool_size);
    return std::make_unique<BufferPool>(config.pool_size, std::move(eviction_policy));
}

std::unique_ptr<EvictionPolicy> BufferPoolFactory::create_eviction_policy(
    BufferPoolConfig::EvictionPolicy policy, size_t pool_size) {
    switch (policy) {
        case BufferPoolConfig::EvictionPolicy::LRU:
            return std::make_unique<LRUEvictionPolicy>(pool_size);
        case BufferPoolConfig::EvictionPolicy::Clock:
            return std::make_unique<ClockEvictionPolicy>(pool_size);
        default:
            return std::make_unique<ClockEvictionPolicy>(pool_size);
    }
}

}  // namespace lumen
