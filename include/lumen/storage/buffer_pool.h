#ifndef LUMEN_STORAGE_BUFFER_POOL_H
#define LUMEN_STORAGE_BUFFER_POOL_H

#include <lumen/storage/page.h>
#include <lumen/storage/storage_interface.h>
#include <lumen/types.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace lumen {

// Forward declarations
class StorageEngine;

// Frame represents a slot in the buffer pool that can hold a page
struct Frame {
    std::shared_ptr<Page> page;
    std::atomic<bool> is_dirty{false};
    std::atomic<uint32_t> pin_count{0};
    std::atomic<uint64_t> last_access_time{0};
    mutable std::shared_mutex mutex;

    Frame() = default;
    ~Frame() = default;

    // Non-copyable and non-movable (due to atomics and mutex)
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    Frame(Frame&&) = delete;
    Frame& operator=(Frame&&) = delete;

    void pin() {
        pin_count.fetch_add(1, std::memory_order_relaxed);
    }

    void unpin() {
        pin_count.fetch_sub(1, std::memory_order_relaxed);
    }

    bool is_pinned() const {
        return pin_count.load(std::memory_order_relaxed) > 0;
    }

    void update_access_time();
    bool is_available() const {
        return !page || (!is_pinned() && !is_dirty.load(std::memory_order_relaxed));
    }
};

// Eviction policy interface
class EvictionPolicy {
   public:
    virtual ~EvictionPolicy() = default;
    virtual FrameID select_victim(const std::vector<Frame>& frames) = 0;
    virtual void access_frame(FrameID frame_id) = 0;
    virtual void reset() = 0;
};

// LRU eviction policy implementation
class LRUEvictionPolicy : public EvictionPolicy {
   public:
    explicit LRUEvictionPolicy(size_t pool_size);
    ~LRUEvictionPolicy() override = default;

    FrameID select_victim(const std::vector<Frame>& frames) override;
    void access_frame(FrameID frame_id) override;
    void reset() override;

   private:
    mutable std::mutex mutex_;
    size_t pool_size_;
    std::vector<uint64_t> access_times_;
};

// Clock eviction policy implementation (more efficient than LRU)
class ClockEvictionPolicy : public EvictionPolicy {
   public:
    explicit ClockEvictionPolicy(size_t pool_size);
    ~ClockEvictionPolicy() override = default;

    FrameID select_victim(const std::vector<Frame>& frames) override;
    void access_frame(FrameID frame_id) override;
    void reset() override;

   private:
    mutable std::mutex mutex_;
    size_t pool_size_;
    std::vector<std::atomic<bool>> reference_bits_;
    std::atomic<size_t> clock_hand_{0};
};

// Buffer pool statistics
struct BufferPoolStats {
    std::atomic<uint64_t> page_requests{0};
    std::atomic<uint64_t> page_hits{0};
    std::atomic<uint64_t> page_misses{0};
    std::atomic<uint64_t> pages_written{0};
    std::atomic<uint64_t> pages_evicted{0};
    std::atomic<uint64_t> total_flushes{0};

    double hit_ratio() const {
        uint64_t requests = page_requests.load();
        return requests > 0 ? static_cast<double>(page_hits.load()) / requests : 0.0;
    }

    void reset() {
        page_requests.store(0);
        page_hits.store(0);
        page_misses.store(0);
        pages_written.store(0);
        pages_evicted.store(0);
        total_flushes.store(0);
    }
};

// Main buffer pool manager class
class BufferPool {
   public:
    // Constructor with storage backend (new)
    explicit BufferPool(size_t pool_size, IStorageBackend* storage_backend,
                        std::unique_ptr<EvictionPolicy> eviction_policy = nullptr);

    // Constructor without storage backend (legacy)
    explicit BufferPool(size_t pool_size,
                        std::unique_ptr<EvictionPolicy> eviction_policy = nullptr);

    ~BufferPool();

    // Non-copyable and non-movable
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(BufferPool&&) = delete;

    // Page management
    PageRef fetch_page(PageID page_id);
    bool unpin_page(PageID page_id, bool is_dirty = false);
    bool delete_page(PageID page_id);
    PageRef new_page(PageType type = PageType::Data);
    PageRef new_page(PageID page_id, PageType type = PageType::Data);  // Use specific page ID

    // Buffer pool operations
    bool flush_page(PageID page_id);
    void flush_all_pages();
    void reset();

    // Statistics and monitoring
    const BufferPoolStats& stats() const {
        return stats_;
    }
    size_t pool_size() const {
        return pool_size_;
    }
    size_t used_frames() const;
    double utilization() const;

    // Configuration
    void set_storage_engine(std::weak_ptr<StorageEngine> engine) {
        storage_engine_ = engine;
    }

   private:
    // Core buffer pool data
    size_t pool_size_;
    std::vector<Frame> frames_;
    std::unordered_map<PageID, FrameID> page_table_;
    std::vector<FrameID> free_frames_;
    std::unique_ptr<EvictionPolicy> eviction_policy_;
    IStorageBackend* storage_backend_;             // Non-owning pointer
    std::weak_ptr<StorageEngine> storage_engine_;  // Legacy support

    // Synchronization
    mutable std::shared_mutex table_mutex_;
    mutable std::mutex free_frames_mutex_;
    std::condition_variable frame_available_;

    // Statistics
    BufferPoolStats stats_;
    std::atomic<PageID> next_page_id_{1};

    // Internal methods
    FrameID find_free_frame();
    FrameID evict_frame();
    bool write_page_to_storage(const Page& page);
    std::shared_ptr<Page> read_page_from_storage(PageID page_id);
    void update_frame_access(FrameID frame_id);
};

// Buffer pool configuration
struct BufferPoolConfig {
    size_t pool_size = 1024;  // Number of frames
    enum class EvictionPolicy { LRU, Clock } eviction_policy = EvictionPolicy::Clock;
    bool enable_statistics = true;
    size_t flush_threshold = 0;  // 0 means no automatic flushing

    static BufferPoolConfig default_config() {
        return BufferPoolConfig{};
    }
};

// Factory for creating buffer pools
class BufferPoolFactory {
   public:
    static std::unique_ptr<BufferPool> create(
        const BufferPoolConfig& config = BufferPoolConfig::default_config());
    static std::unique_ptr<EvictionPolicy> create_eviction_policy(
        BufferPoolConfig::EvictionPolicy policy, size_t pool_size);
};

}  // namespace lumen

#endif  // LUMEN_STORAGE_BUFFER_POOL_H
