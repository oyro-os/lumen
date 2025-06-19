#ifndef LUMEN_MEMORY_MEMORY_MANAGER_H
#define LUMEN_MEMORY_MEMORY_MANAGER_H

#include <lumen/memory/allocator.h>
#include <lumen/types.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lumen {

// C++20 concepts for memory management
template<typename T>
concept MemoryAllocatable = requires(T t, size_t size) {
    requires std::is_trivially_destructible_v<T>;
    requires alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__;
};

template<typename T>
concept MemoryPoolAllocatable = MemoryAllocatable<T> && requires {
    requires sizeof(T) >= sizeof(void*);  // Minimum size for free list
};

// Memory pressure levels for adaptive management
enum class MemoryPressureLevel : uint8_t {
    LOW = 0,      // < 70% usage
    MEDIUM = 1,   // 70-85% usage
    HIGH = 2,     // 85-95% usage
    CRITICAL = 3  // > 95% usage
};

// Memory allocation pool for different component types
enum class MemoryPoolType : uint8_t {
    INDEX_CACHE = 0,     // B+Tree nodes, vector indexes, secondary indexes
    BUFFER_POOL = 1,     // Hot data pages
    QUERY_RESULTS = 2,   // Active collections
    SYSTEM_OVERHEAD = 3  // Metadata, lock tables, misc buffers
};

// Memory usage statistics
struct MemoryStats {
    // Current usage by pool
    std::atomic<size_t> total_memory{0};
    std::atomic<size_t> index_memory{0};
    std::atomic<size_t> buffer_memory{0};
    std::atomic<size_t> query_memory{0};
    std::atomic<size_t> system_memory{0};

    // Peak usage
    std::atomic<size_t> peak_total{0};
    std::atomic<size_t> peak_index{0};
    std::atomic<size_t> peak_buffer{0};
    std::atomic<size_t> peak_query{0};
    std::atomic<size_t> peak_system{0};

    // Pressure events
    std::atomic<uint64_t> oom_prevented{0};
    std::atomic<uint64_t> emergency_evictions{0};
    std::atomic<uint64_t> pressure_events{0};

    // Performance metrics
    std::atomic<uint64_t> allocations{0};
    std::atomic<uint64_t> deallocations{0};
    std::atomic<uint64_t> failed_allocations{0};

    void reset();
    double utilization_ratio() const;
    size_t available_memory() const;
};

// C++20 consteval memory size helpers
namespace memory_sizes {
consteval size_t KB(size_t n) {
    return n * 1024;
}
consteval size_t MB(size_t n) {
    return KB(n) * 1024;
}
consteval size_t GB(size_t n) {
    return MB(n) * 1024;
}
}  // namespace memory_sizes

// Configuration for memory manager
struct MemoryConfig {
    // Memory limits
    size_t min_memory = memory_sizes::MB(10);      // 10MB minimum
    size_t target_memory = memory_sizes::MB(100);  // 100MB soft target
    size_t max_memory = 0;                         // 0 = use available RAM

    // Pool allocation percentages (should sum to 100)
    double index_cache_percent = 65.0;     // 60-70% for indexes
    double buffer_pool_percent = 25.0;     // 20-30% for buffer pool
    double query_results_percent = 7.5;    // 5-10% for query results
    double system_overhead_percent = 2.5;  // <5% for system overhead

    // Pressure thresholds
    double medium_pressure_threshold = 0.70;
    double high_pressure_threshold = 0.85;
    double critical_pressure_threshold = 0.95;

    // Emergency buffer (reserved for OOM prevention)
    size_t emergency_buffer = memory_sizes::MB(5);  // 5MB

    // Auto-tuning parameters
    bool enable_auto_tuning = true;
    std::chrono::milliseconds monitoring_interval{1000};  // 1 second

    static constexpr MemoryConfig create_efficient_config() noexcept {
        using namespace memory_sizes;

        MemoryConfig config;
        config.min_memory = MB(10);      // 10MB minimum
        config.target_memory = MB(100);  // 100MB soft target
        config.max_memory = 0;           // Use available RAM

        // Optimized for efficiency
        config.index_cache_percent = 70.0;     // More for indexes
        config.buffer_pool_percent = 20.0;     // Less for buffer pool
        config.query_results_percent = 7.0;    // Minimal for queries
        config.system_overhead_percent = 3.0;  // Minimal system overhead

        config.enable_auto_tuning = true;
        config.monitoring_interval = std::chrono::milliseconds(500);  // More frequent

        return config;
    }

    static constexpr MemoryConfig create_default_config() noexcept {
        return MemoryConfig{};
    }

    // C++20 constexpr validation
    constexpr bool is_valid() const noexcept {
        return min_memory > 0 && target_memory >= min_memory &&
               (max_memory == 0 || max_memory >= target_memory) &&
               (index_cache_percent + buffer_pool_percent + query_results_percent +
                system_overhead_percent) <= 100.0 &&
               medium_pressure_threshold < high_pressure_threshold &&
               high_pressure_threshold < critical_pressure_threshold;
    }
};

// Pool allocation limits
struct PoolLimits {
    size_t max_size;
    size_t current_size;
    size_t reserved_size;
    double allocation_percent;

    bool can_allocate(size_t size) const {
        return current_size + size <= max_size;
    }

    size_t available_size() const {
        return max_size > current_size ? max_size - current_size : 0;
    }
};

// Adaptive memory budget manager
class MemoryManager {
   public:
    explicit MemoryManager(const MemoryConfig& config = MemoryConfig::create_efficient_config());
    ~MemoryManager();

    // Non-copyable and non-movable
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    MemoryManager(MemoryManager&&) = delete;
    MemoryManager& operator=(MemoryManager&&) = delete;

    // Memory allocation for specific pools
    void* allocate(size_t size, MemoryPoolType pool, size_t alignment = alignof(std::max_align_t));
    void deallocate(void* ptr, size_t size, MemoryPoolType pool);

    // C++20 templated type-safe allocation
    template<MemoryAllocatable T>
    [[nodiscard]] T* allocate(MemoryPoolType pool) {
        void* ptr = allocate(sizeof(T), pool, alignof(T));
        return static_cast<T*>(ptr);
    }

    template<MemoryAllocatable T>
    [[nodiscard]] std::span<T> allocate_array(size_t count, MemoryPoolType pool) {
        void* ptr = allocate(sizeof(T) * count, pool, alignof(T));
        return std::span<T>(static_cast<T*>(ptr), count);
    }

    template<MemoryAllocatable T>
    void deallocate(T* ptr, MemoryPoolType pool) noexcept {
        deallocate(static_cast<void*>(ptr), sizeof(T), pool);
    }

    template<MemoryAllocatable T>
    void deallocate_array(std::span<T> span, MemoryPoolType pool) noexcept {
        deallocate(static_cast<void*>(span.data()), sizeof(T) * span.size(), pool);
    }

    // Bulk operations
    void* allocate_bulk(size_t count, size_t size, MemoryPoolType pool);
    void deallocate_bulk(void* ptr, size_t count, size_t size, MemoryPoolType pool);

    // Memory pressure management
    MemoryPressureLevel get_pressure_level() const;
    bool can_allocate(size_t size, MemoryPoolType pool) const;
    size_t try_free_memory(size_t needed, MemoryPoolType pool);

    // Pool management
    void adjust_pool_limits();
    void rebalance_pools();
    size_t get_pool_limit(MemoryPoolType pool) const;
    size_t get_pool_usage(MemoryPoolType pool) const;
    size_t get_pool_available(MemoryPoolType pool) const;

    // Statistics and monitoring
    const MemoryStats& stats() const {
        return stats_;
    }
    MemoryConfig config() const {
        return config_;
    }
    void update_config(const MemoryConfig& config);

    // System memory information
    static size_t get_available_system_memory();
    static size_t get_total_system_memory();
    static size_t get_process_memory_usage();

    // Emergency handling
    void handle_memory_pressure();
    bool prevent_oom(size_t size);
    void force_cleanup();

    // Auto-tuning
    void enable_monitoring(bool enable = true);
    void tune_memory_allocation();

   private:
    // Configuration
    MemoryConfig config_;
    mutable std::mutex config_mutex_;

    // Pool limits
    std::array<PoolLimits, 4> pool_limits_;
    mutable std::shared_mutex pools_mutex_;

    // Statistics
    MemoryStats stats_;

    // Underlying allocator
    std::unique_ptr<Allocator> allocator_;

    // Pressure handling
    std::atomic<MemoryPressureLevel> current_pressure_{MemoryPressureLevel::LOW};
    std::chrono::steady_clock::time_point last_pressure_check_;
    mutable std::mutex pressure_mutex_;

    // Auto-tuning
    std::atomic<bool> monitoring_enabled_{false};
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_{false};

    // Internal methods
    void initialize_pools();
    void update_pool_limits();
    void monitor_memory_usage();
    void handle_pressure_level(MemoryPressureLevel level);

    size_t calculate_pool_size(MemoryPoolType pool, size_t total_memory) const;
    void update_pool_stats(MemoryPoolType pool, size_t size, bool is_allocation);
    void update_peak_stats();

    // Pool-specific cleanup methods
    size_t cleanup_index_cache(size_t needed);
    size_t cleanup_buffer_pool(size_t needed);
    size_t cleanup_query_results(size_t needed);
    size_t cleanup_system_overhead(size_t needed);
};

// Global memory manager instance
MemoryManager* get_memory_manager();
void set_memory_manager(std::unique_ptr<MemoryManager> manager);

// Convenience functions for pool-specific allocation
namespace memory {

inline void* allocate_index(size_t size, size_t alignment = alignof(std::max_align_t)) {
    return get_memory_manager()->allocate(size, MemoryPoolType::INDEX_CACHE, alignment);
}

inline void* allocate_buffer(size_t size, size_t alignment = alignof(std::max_align_t)) {
    return get_memory_manager()->allocate(size, MemoryPoolType::BUFFER_POOL, alignment);
}

inline void* allocate_query(size_t size, size_t alignment = alignof(std::max_align_t)) {
    return get_memory_manager()->allocate(size, MemoryPoolType::QUERY_RESULTS, alignment);
}

inline void* allocate_system(size_t size, size_t alignment = alignof(std::max_align_t)) {
    return get_memory_manager()->allocate(size, MemoryPoolType::SYSTEM_OVERHEAD, alignment);
}

inline void deallocate_index(void* ptr, size_t size) {
    get_memory_manager()->deallocate(ptr, size, MemoryPoolType::INDEX_CACHE);
}

inline void deallocate_buffer(void* ptr, size_t size) {
    get_memory_manager()->deallocate(ptr, size, MemoryPoolType::BUFFER_POOL);
}

inline void deallocate_query(void* ptr, size_t size) {
    get_memory_manager()->deallocate(ptr, size, MemoryPoolType::QUERY_RESULTS);
}

inline void deallocate_system(void* ptr, size_t size) {
    get_memory_manager()->deallocate(ptr, size, MemoryPoolType::SYSTEM_OVERHEAD);
}

}  // namespace memory

// Memory pressure callback interface
class MemoryPressureCallback {
   public:
    virtual ~MemoryPressureCallback() = default;
    virtual size_t on_memory_pressure(MemoryPressureLevel level, size_t needed) = 0;
};

// Memory pressure manager for registering callbacks
class MemoryPressureManager {
   public:
    static MemoryPressureManager& instance();

    void register_callback(std::weak_ptr<MemoryPressureCallback> callback);
    void unregister_callback(std::weak_ptr<MemoryPressureCallback> callback);

    size_t notify_pressure(MemoryPressureLevel level, size_t needed);

   private:
    mutable std::mutex callbacks_mutex_;
    std::vector<std::weak_ptr<MemoryPressureCallback>> callbacks_;

    void cleanup_expired_callbacks();
};

}  // namespace lumen

#endif  // LUMEN_MEMORY_MEMORY_MANAGER_H