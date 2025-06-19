#include <lumen/memory/allocator.h>
#include <lumen/memory/memory_manager.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <shared_mutex>
#include <thread>

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace lumen {

// Global memory manager instance
static std::unique_ptr<MemoryManager> g_memory_manager;
static std::mutex g_memory_manager_mutex;

// MemoryStats implementation
void MemoryStats::reset() {
    total_memory.store(0);
    index_memory.store(0);
    buffer_memory.store(0);
    query_memory.store(0);
    system_memory.store(0);

    peak_total.store(0);
    peak_index.store(0);
    peak_buffer.store(0);
    peak_query.store(0);
    peak_system.store(0);

    oom_prevented.store(0);
    emergency_evictions.store(0);
    pressure_events.store(0);

    allocations.store(0);
    deallocations.store(0);
    failed_allocations.store(0);
}

double MemoryStats::utilization_ratio() const {
    size_t total = total_memory.load();
    size_t peak = peak_total.load();
    return peak > 0 ? static_cast<double>(total) / peak : 0.0;
}

size_t MemoryStats::available_memory() const {
    size_t total = total_memory.load();
    size_t peak = peak_total.load();
    return peak > total ? peak - total : 0;
}

// MemoryConfig implementations are now in the header for constexpr evaluation

// MemoryManager implementation
MemoryManager::MemoryManager(const MemoryConfig& config)
    : config_(config), allocator_(std::make_unique<SystemAllocator>()) {
    // Determine max memory
    if (config_.max_memory == 0) {
        size_t available = get_available_system_memory();
        config_.max_memory =
            std::max(config_.min_memory, std::min(available / 2, config_.target_memory * 2));
    }

    // Ensure max is at least target
    config_.max_memory = std::max(config_.max_memory, config_.target_memory);

    initialize_pools();

    if (config_.enable_auto_tuning) {
        enable_monitoring(true);
    }
}

MemoryManager::~MemoryManager() {
    shutdown_.store(true);
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void MemoryManager::initialize_pools() {
    std::unique_lock<std::shared_mutex> lock(pools_mutex_);

    // Calculate pool sizes based on target memory
    size_t target_memory = config_.target_memory;

    pool_limits_[static_cast<size_t>(MemoryPoolType::INDEX_CACHE)] = {
        .max_size = calculate_pool_size(MemoryPoolType::INDEX_CACHE, target_memory),
        .current_size = 0,
        .reserved_size = 0,
        .allocation_percent = config_.index_cache_percent};

    pool_limits_[static_cast<size_t>(MemoryPoolType::BUFFER_POOL)] = {
        .max_size = calculate_pool_size(MemoryPoolType::BUFFER_POOL, target_memory),
        .current_size = 0,
        .reserved_size = 0,
        .allocation_percent = config_.buffer_pool_percent};

    pool_limits_[static_cast<size_t>(MemoryPoolType::QUERY_RESULTS)] = {
        .max_size = calculate_pool_size(MemoryPoolType::QUERY_RESULTS, target_memory),
        .current_size = 0,
        .reserved_size = 0,
        .allocation_percent = config_.query_results_percent};

    pool_limits_[static_cast<size_t>(MemoryPoolType::SYSTEM_OVERHEAD)] = {
        .max_size = calculate_pool_size(MemoryPoolType::SYSTEM_OVERHEAD, target_memory),
        .current_size = 0,
        .reserved_size = 0,
        .allocation_percent = config_.system_overhead_percent};
}

size_t MemoryManager::calculate_pool_size(MemoryPoolType pool, size_t total_memory) const {
    double percent = 0.0;

    switch (pool) {
        case MemoryPoolType::INDEX_CACHE:
            percent = config_.index_cache_percent;
            break;
        case MemoryPoolType::BUFFER_POOL:
            percent = config_.buffer_pool_percent;
            break;
        case MemoryPoolType::QUERY_RESULTS:
            percent = config_.query_results_percent;
            break;
        case MemoryPoolType::SYSTEM_OVERHEAD:
            percent = config_.system_overhead_percent;
            break;
    }

    return static_cast<size_t>(total_memory * (percent / 100.0));
}

void* MemoryManager::allocate(size_t size, MemoryPoolType pool, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }

    // Check if allocation is possible
    if (!can_allocate(size, pool)) {
        // Try to free memory
        size_t freed = try_free_memory(size, pool);
        if (freed < size) {
            stats_.failed_allocations.fetch_add(1);
            return nullptr;
        }
    }

    // Attempt allocation
    void* ptr = allocator_->allocate(size, alignment);
    if (!ptr) {
        // Try emergency cleanup
        if (prevent_oom(size)) {
            ptr = allocator_->allocate(size, alignment);
        }

        if (!ptr) {
            stats_.failed_allocations.fetch_add(1);
            return nullptr;
        }
    }

    // Update pool stats
    update_pool_stats(pool, size, true);
    stats_.allocations.fetch_add(1);

    return ptr;
}

void MemoryManager::deallocate(void* ptr, size_t size, MemoryPoolType pool) {
    if (!ptr || size == 0) {
        return;
    }

    allocator_->deallocate(ptr, size);
    update_pool_stats(pool, size, false);
    stats_.deallocations.fetch_add(1);
}

void* MemoryManager::allocate_bulk(size_t count, size_t size, MemoryPoolType pool) {
    return allocate(count * size, pool);
}

void MemoryManager::deallocate_bulk(void* ptr, size_t count, size_t size, MemoryPoolType pool) {
    deallocate(ptr, count * size, pool);
}

bool MemoryManager::can_allocate(size_t size, MemoryPoolType pool) const {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);

    size_t pool_idx = static_cast<size_t>(pool);
    const auto& limits = pool_limits_[pool_idx];

    // Check pool limit
    if (!limits.can_allocate(size)) {
        return false;
    }

    // Check total memory limit
    size_t total_current = stats_.total_memory.load();
    if (total_current + size > config_.max_memory - config_.emergency_buffer) {
        return false;
    }

    return true;
}

MemoryPressureLevel MemoryManager::get_pressure_level() const {
    size_t total_current = stats_.total_memory.load();
    size_t max_memory = config_.max_memory;

    if (max_memory == 0) {
        return MemoryPressureLevel::LOW;
    }

    double usage_ratio = static_cast<double>(total_current) / max_memory;

    if (usage_ratio >= config_.critical_pressure_threshold) {
        return MemoryPressureLevel::CRITICAL;
    } else if (usage_ratio >= config_.high_pressure_threshold) {
        return MemoryPressureLevel::HIGH;
    } else if (usage_ratio >= config_.medium_pressure_threshold) {
        return MemoryPressureLevel::MEDIUM;
    } else {
        return MemoryPressureLevel::LOW;
    }
}

size_t MemoryManager::try_free_memory(size_t needed, MemoryPoolType pool) {
    size_t freed = 0;

    // Try pool-specific cleanup first
    switch (pool) {
        case MemoryPoolType::INDEX_CACHE:
            freed += cleanup_index_cache(needed);
            break;
        case MemoryPoolType::BUFFER_POOL:
            freed += cleanup_buffer_pool(needed);
            break;
        case MemoryPoolType::QUERY_RESULTS:
            freed += cleanup_query_results(needed);
            break;
        case MemoryPoolType::SYSTEM_OVERHEAD:
            freed += cleanup_system_overhead(needed);
            break;
    }

    // If still not enough, try global cleanup
    if (freed < needed) {
        handle_memory_pressure();

        // Try other pools if needed
        if (pool != MemoryPoolType::QUERY_RESULTS) {
            freed += cleanup_query_results(needed - freed);
        }
        if (pool != MemoryPoolType::BUFFER_POOL && freed < needed) {
            freed += cleanup_buffer_pool(needed - freed);
        }
    }

    return freed;
}

void MemoryManager::handle_memory_pressure() {
    MemoryPressureLevel level = get_pressure_level();
    current_pressure_.store(level);

    if (level == MemoryPressureLevel::LOW) {
        return;
    }

    stats_.pressure_events.fetch_add(1);
    handle_pressure_level(level);
}

void MemoryManager::handle_pressure_level(MemoryPressureLevel level) {
    switch (level) {
        case MemoryPressureLevel::MEDIUM:
            // Start gentle cleanup
            cleanup_query_results(1024 * 1024);  // Free 1MB
            break;

        case MemoryPressureLevel::HIGH:
            // More aggressive cleanup
            cleanup_query_results(5 * 1024 * 1024);  // Free 5MB
            cleanup_buffer_pool(2 * 1024 * 1024);    // Free 2MB
            break;

        case MemoryPressureLevel::CRITICAL:
            // Emergency cleanup
            stats_.emergency_evictions.fetch_add(1);
            force_cleanup();
            break;

        default:
            break;
    }
}

bool MemoryManager::prevent_oom(size_t size) {
    size_t total_current = stats_.total_memory.load();

    // Check if we're close to OOM
    if (total_current + size > config_.max_memory - config_.emergency_buffer) {
        stats_.oom_prevented.fetch_add(1);

        // Try emergency cleanup
        size_t needed = size + config_.emergency_buffer;
        size_t freed = 0;

        // Clean up in order of priority
        freed += cleanup_query_results(needed);
        if (freed < needed) {
            freed += cleanup_buffer_pool(needed - freed);
        }
        if (freed < needed) {
            freed += cleanup_index_cache(needed - freed);
        }

        return freed >= needed;
    }

    return true;
}

void MemoryManager::force_cleanup() {
    // Emergency cleanup - free as much as possible
    cleanup_query_results(SIZE_MAX);
    cleanup_buffer_pool(SIZE_MAX);
    cleanup_system_overhead(SIZE_MAX);

    // Notify pressure callbacks
    MemoryPressureManager::instance().notify_pressure(MemoryPressureLevel::CRITICAL, 0);
}

void MemoryManager::update_pool_stats(MemoryPoolType pool, size_t size, bool is_allocation) {
    size_t pool_idx = static_cast<size_t>(pool);

    {
        std::unique_lock<std::shared_mutex> lock(pools_mutex_);
        if (is_allocation) {
            pool_limits_[pool_idx].current_size += size;
        } else {
            pool_limits_[pool_idx].current_size = pool_limits_[pool_idx].current_size > size
                                                      ? pool_limits_[pool_idx].current_size - size
                                                      : 0;
        }
    }

    // Update pool-specific stats
    std::atomic<size_t>* pool_stat = nullptr;
    std::atomic<size_t>* peak_stat = nullptr;

    switch (pool) {
        case MemoryPoolType::INDEX_CACHE:
            pool_stat = &stats_.index_memory;
            peak_stat = &stats_.peak_index;
            break;
        case MemoryPoolType::BUFFER_POOL:
            pool_stat = &stats_.buffer_memory;
            peak_stat = &stats_.peak_buffer;
            break;
        case MemoryPoolType::QUERY_RESULTS:
            pool_stat = &stats_.query_memory;
            peak_stat = &stats_.peak_query;
            break;
        case MemoryPoolType::SYSTEM_OVERHEAD:
            pool_stat = &stats_.system_memory;
            peak_stat = &stats_.peak_system;
            break;
    }

    if (pool_stat && peak_stat) {
        if (is_allocation) {
            pool_stat->fetch_add(size);
            stats_.total_memory.fetch_add(size);
        } else {
            pool_stat->fetch_sub(size);
            stats_.total_memory.fetch_sub(size);
        }

        // Update peaks
        size_t current = pool_stat->load();
        size_t current_peak = peak_stat->load();
        while (current > current_peak && !peak_stat->compare_exchange_weak(current_peak, current)) {
            // Retry if another thread updated peak
        }

        size_t total_current = stats_.total_memory.load();
        size_t total_peak = stats_.peak_total.load();
        while (total_current > total_peak &&
               !stats_.peak_total.compare_exchange_weak(total_peak, total_current)) {
            // Retry if another thread updated peak
        }
    }
}

// Index cache cleanup - evict least recently used index pages
size_t MemoryManager::cleanup_index_cache(size_t needed) {
    size_t freed = 0;
    size_t current_usage = stats_.index_memory.load();
    
    // Don't cleanup if we're already below 50% of limit
    size_t index_limit = get_pool_limit(MemoryPoolType::INDEX_CACHE);
    if (current_usage < index_limit / 2) {
        return 0;
    }
    
    // Calculate target cleanup amount (either needed or 25% of current usage)
    size_t cleanup_target = std::min(needed, current_usage / 4);
    
    // Perform actual index cache cleanup by triggering memory pressure callbacks
    // This will notify all registered index components to free memory
    size_t pressure_freed = 0;
    if (cleanup_target > 0) {
        pressure_freed = MemoryPressureManager::instance().notify_pressure(MemoryPressureLevel::MEDIUM, cleanup_target);
        // The notify_pressure call returns the actual amount freed by callbacks
    }
    
    freed = pressure_freed;
    
    if (freed > 0) {
        update_pool_stats(MemoryPoolType::INDEX_CACHE, freed, false);
        stats_.pressure_events.fetch_add(1);
    }
    
    return freed;
}

size_t MemoryManager::cleanup_buffer_pool(size_t needed) {
    size_t freed = 0;
    size_t current_usage = stats_.buffer_memory.load();
    
    // Don't cleanup if we're already below 30% of limit
    size_t buffer_limit = get_pool_limit(MemoryPoolType::BUFFER_POOL);
    if (current_usage < buffer_limit * 3 / 10) {
        return 0;
    }
    
    // Calculate target cleanup amount (either needed or 20% of current usage)
    size_t cleanup_target = std::min(needed, current_usage / 5);
    
    // Perform actual buffer pool cleanup by triggering high pressure events
    // This will cause buffer pools to evict pages and flush dirty pages
    size_t pressure_freed = 0;
    if (cleanup_target > 0) {
        pressure_freed = MemoryPressureManager::instance().notify_pressure(MemoryPressureLevel::HIGH, cleanup_target);
        // The notify_pressure call returns the actual amount freed by buffer pool callbacks
    }
    
    freed = pressure_freed;
    
    if (freed > 0) {
        update_pool_stats(MemoryPoolType::BUFFER_POOL, freed, false);
        stats_.pressure_events.fetch_add(1);
    }
    
    return freed;
}

size_t MemoryManager::cleanup_query_results(size_t needed) {
    size_t freed = 0;
    size_t current_usage = stats_.query_memory.load();
    
    // Query results can be more aggressively cleaned up
    if (current_usage == 0) {
        return 0;
    }
    
    // Calculate target cleanup amount (either needed or 50% of current usage)
    size_t cleanup_target = std::min(needed, current_usage / 2);
    
    // Perform actual query results cleanup through pressure callbacks
    // Query execution engines will free cached results and temporary data
    size_t pressure_freed = 0;
    if (cleanup_target > 0) {
        pressure_freed = MemoryPressureManager::instance().notify_pressure(MemoryPressureLevel::HIGH, cleanup_target);
        // The notify_pressure call returns the actual amount freed by query engine callbacks
    }
    
    freed = pressure_freed;
    
    if (freed > 0) {
        update_pool_stats(MemoryPoolType::QUERY_RESULTS, freed, false);
        stats_.pressure_events.fetch_add(1);
    }
    
    return freed;
}

size_t MemoryManager::cleanup_system_overhead(size_t needed) {
    size_t freed = 0;
    size_t current_usage = stats_.system_memory.load();
    
    // System overhead cleanup is limited - only cleanup temporary allocations
    if (current_usage == 0) {
        return 0;
    }
    
    // Calculate target cleanup amount (conservative - only 10% of current usage)
    size_t cleanup_target = std::min(needed, current_usage / 10);
    
    // Perform actual system overhead cleanup through pressure callbacks
    // System components will free temporary buffers, metadata caches, etc.
    size_t pressure_freed = 0;
    if (cleanup_target > 0) {
        pressure_freed = MemoryPressureManager::instance().notify_pressure(MemoryPressureLevel::CRITICAL, cleanup_target);
        // The notify_pressure call returns the actual amount freed by system component callbacks
    }
    
    freed = pressure_freed;
    
    if (freed > 0) {
        update_pool_stats(MemoryPoolType::SYSTEM_OVERHEAD, freed, false);
        stats_.pressure_events.fetch_add(1);
    }
    
    return freed;
}

void MemoryManager::enable_monitoring(bool enable) {
    if (enable == monitoring_enabled_.load()) {
        return;
    }

    monitoring_enabled_.store(enable);

    if (enable) {
        monitoring_thread_ = std::thread([this]() { monitor_memory_usage(); });
    } else {
        shutdown_.store(true);
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
        shutdown_.store(false);
    }
}

void MemoryManager::monitor_memory_usage() {
    while (!shutdown_.load()) {
        handle_memory_pressure();

        // Auto-tune if enabled
        if (config_.enable_auto_tuning) {
            tune_memory_allocation();
        }

        std::this_thread::sleep_for(config_.monitoring_interval);
    }
}

void MemoryManager::tune_memory_allocation() {
    if (!config_.enable_auto_tuning) {
        return;
    }
    
    std::lock_guard<std::mutex> config_lock(config_mutex_);
    std::shared_lock<std::shared_mutex> pools_lock(pools_mutex_);
    
    // Get current memory usage statistics
    size_t total_usage = stats_.total_memory.load();
    size_t available_memory = get_available_system_memory();
    
    if (available_memory == 0 || total_usage == 0) {
        return;
    }
    
    // Calculate memory pressure ratio
    double pressure_ratio = static_cast<double>(total_usage) / available_memory;
    
    // Auto-tune pool allocations based on usage patterns
    double index_utilization = static_cast<double>(stats_.index_memory.load()) / get_pool_limit(MemoryPoolType::INDEX_CACHE);
    double buffer_utilization = static_cast<double>(stats_.buffer_memory.load()) / get_pool_limit(MemoryPoolType::BUFFER_POOL);
    double query_utilization = static_cast<double>(stats_.query_memory.load()) / get_pool_limit(MemoryPoolType::QUERY_RESULTS);
    
    // Adjust pool percentages based on utilization
    if (index_utilization > 0.8 && buffer_utilization < 0.5) {
        // High index usage, low buffer usage - shift 5% from buffer to index
        config_.index_cache_percent = std::min(75.0, config_.index_cache_percent + 2.5);
        config_.buffer_pool_percent = std::max(15.0, config_.buffer_pool_percent - 2.5);
    } else if (buffer_utilization > 0.8 && index_utilization < 0.5) {
        // High buffer usage, low index usage - shift 5% from index to buffer
        config_.buffer_pool_percent = std::min(35.0, config_.buffer_pool_percent + 2.5);
        config_.index_cache_percent = std::max(55.0, config_.index_cache_percent - 2.5);
    }
    
    // Adjust query results pool based on usage
    if (query_utilization > 0.9) {
        // High query usage - increase allocation
        config_.query_results_percent = std::min(15.0, config_.query_results_percent + 1.0);
        config_.system_overhead_percent = std::max(1.0, config_.system_overhead_percent - 1.0);
    } else if (query_utilization < 0.2) {
        // Low query usage - decrease allocation
        config_.query_results_percent = std::max(3.0, config_.query_results_percent - 1.0);
        config_.system_overhead_percent = std::min(5.0, config_.system_overhead_percent + 1.0);
    }
    
    // Adjust pressure thresholds based on system behavior
    if (pressure_ratio > 0.9) {
        // High pressure - be more aggressive
        config_.medium_pressure_threshold = std::max(0.6, config_.medium_pressure_threshold - 0.05);
        config_.high_pressure_threshold = std::max(0.75, config_.high_pressure_threshold - 0.05);
    } else if (pressure_ratio < 0.5) {
        // Low pressure - be more relaxed
        config_.medium_pressure_threshold = std::min(0.8, config_.medium_pressure_threshold + 0.05);
        config_.high_pressure_threshold = std::min(0.9, config_.high_pressure_threshold + 0.05);
    }
    
    // Update pool limits with new configuration
    update_pool_limits();
}

void MemoryManager::update_pool_limits() {
    std::lock_guard<std::shared_mutex> lock(pools_mutex_);
    
    size_t total_memory = config_.max_memory > 0 ? config_.max_memory : get_available_system_memory();
    
    // Recalculate pool limits based on current percentages
    pool_limits_[static_cast<size_t>(MemoryPoolType::INDEX_CACHE)].max_size = 
        calculate_pool_size(MemoryPoolType::INDEX_CACHE, total_memory);
    
    pool_limits_[static_cast<size_t>(MemoryPoolType::BUFFER_POOL)].max_size = 
        calculate_pool_size(MemoryPoolType::BUFFER_POOL, total_memory);
    
    pool_limits_[static_cast<size_t>(MemoryPoolType::QUERY_RESULTS)].max_size = 
        calculate_pool_size(MemoryPoolType::QUERY_RESULTS, total_memory);
    
    pool_limits_[static_cast<size_t>(MemoryPoolType::SYSTEM_OVERHEAD)].max_size = 
        calculate_pool_size(MemoryPoolType::SYSTEM_OVERHEAD, total_memory);
}

// System memory functions
size_t MemoryManager::get_available_system_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return static_cast<size_t>(status.ullAvailPhys);
    }
    return 1024ULL * 1024 * 1024;  // 1GB fallback
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<size_t>(info.freeram * info.mem_unit);
    }
    return 1024ULL * 1024 * 1024;  // 1GB fallback
#elif defined(__APPLE__)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t total_memory = 0;
    size_t length = sizeof(total_memory);
    if (sysctl(mib, 2, &total_memory, &length, nullptr, 0) == 0) {
        // On macOS, assume 70% of total memory is available
        return static_cast<size_t>(total_memory * 0.7);
    }
    return 1024ULL * 1024 * 1024;  // 1GB fallback
#else
    return 1024ULL * 1024 * 1024;  // 1GB fallback
#endif
}

size_t MemoryManager::get_total_system_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return static_cast<size_t>(status.ullTotalPhys);
    }
    return 4ULL * 1024 * 1024 * 1024;  // 4GB fallback
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<size_t>(info.totalram * info.mem_unit);
    }
    return 4ULL * 1024 * 1024 * 1024;  // 4GB fallback
#elif defined(__APPLE__)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t total_memory = 0;
    size_t length = sizeof(total_memory);
    if (sysctl(mib, 2, &total_memory, &length, nullptr, 0) == 0) {
        return static_cast<size_t>(total_memory);
    }
    return 4ULL * 1024 * 1024 * 1024;  // 4GB fallback
#else
    return 4ULL * 1024 * 1024 * 1024;  // 4GB fallback
#endif
}

size_t MemoryManager::get_process_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;
#elif defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss);  // On macOS, ru_maxrss is in bytes
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss * 1024);  // On Linux, convert KB to bytes
    }
    return 0;
#endif
}

// Pool management
size_t MemoryManager::get_pool_limit(MemoryPoolType pool) const {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    return pool_limits_[static_cast<size_t>(pool)].max_size;
}

size_t MemoryManager::get_pool_usage(MemoryPoolType pool) const {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    return pool_limits_[static_cast<size_t>(pool)].current_size;
}

size_t MemoryManager::get_pool_available(MemoryPoolType pool) const {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    return pool_limits_[static_cast<size_t>(pool)].available_size();
}

// Global memory manager
MemoryManager* get_memory_manager() {
    std::lock_guard<std::mutex> lock(g_memory_manager_mutex);
    if (!g_memory_manager) {
        g_memory_manager = std::make_unique<MemoryManager>();
    }
    return g_memory_manager.get();
}

void set_memory_manager(std::unique_ptr<MemoryManager> manager) {
    std::lock_guard<std::mutex> lock(g_memory_manager_mutex);
    g_memory_manager = std::move(manager);
}

// MemoryPressureManager implementation
MemoryPressureManager& MemoryPressureManager::instance() {
    static MemoryPressureManager instance;
    return instance;
}

void MemoryPressureManager::register_callback(std::weak_ptr<MemoryPressureCallback> callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_.push_back(callback);
}

void MemoryPressureManager::unregister_callback(std::weak_ptr<MemoryPressureCallback> callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    auto shared_callback = callback.lock();
    if (shared_callback) {
        callbacks_.erase(
            std::remove_if(callbacks_.begin(), callbacks_.end(),
                           [&shared_callback](const std::weak_ptr<MemoryPressureCallback>& cb) {
                               auto shared_cb = cb.lock();
                               return !shared_cb || shared_cb == shared_callback;
                           }),
            callbacks_.end());
    }
}

size_t MemoryPressureManager::notify_pressure(MemoryPressureLevel level, size_t needed) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);

    cleanup_expired_callbacks();

    size_t total_freed = 0;
    for (auto& weak_callback : callbacks_) {
        auto callback = weak_callback.lock();
        if (callback) {
            total_freed += callback->on_memory_pressure(level, needed);
        }
    }

    return total_freed;
}

void MemoryPressureManager::cleanup_expired_callbacks() {
    // C++20 ranges version
    auto expired_filter = [](const std::weak_ptr<MemoryPressureCallback>& cb) {
        return cb.expired();
    };

    std::erase_if(callbacks_, expired_filter);
}

}  // namespace lumen