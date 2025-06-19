#include <gtest/gtest.h>
#include <lumen/memory/memory_manager.h>
#include <memory>

namespace lumen {

class MemoryManagerTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a test memory manager with small limits using C++20 designated initializers
        using namespace memory_sizes;

        MemoryConfig config = {.min_memory = MB(1),     // 1MB minimum
                               .target_memory = MB(4),  // 4MB target
                               .max_memory = MB(8),     // 8MB maximum
                               .index_cache_percent = 70.0,
                               .buffer_pool_percent = 20.0,
                               .query_results_percent = 7.0,
                               .system_overhead_percent = 3.0,
                               .enable_auto_tuning = false,  // Disable for testing
                               .monitoring_interval = std::chrono::milliseconds(100)};

        // Validate config at compile time in debug builds
        static_assert(MemoryConfig{}.is_valid(), "Default config must be valid");
        ASSERT_TRUE(config.is_valid()) << "Test config must be valid";

        manager_ = std::make_unique<MemoryManager>(config);
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<MemoryManager> manager_;
};

TEST_F(MemoryManagerTest, BasicAllocation) {
    // Test basic allocation for different pools
    void* ptr1 = manager_->allocate(1024, MemoryPoolType::INDEX_CACHE);
    ASSERT_NE(ptr1, nullptr);

    void* ptr2 = manager_->allocate(512, MemoryPoolType::BUFFER_POOL);
    ASSERT_NE(ptr2, nullptr);

    void* ptr3 = manager_->allocate(256, MemoryPoolType::QUERY_RESULTS);
    ASSERT_NE(ptr3, nullptr);

    void* ptr4 = manager_->allocate(128, MemoryPoolType::SYSTEM_OVERHEAD);
    ASSERT_NE(ptr4, nullptr);

    // Check statistics
    const auto& stats = manager_->stats();
    EXPECT_GT(stats.total_memory.load(), 0);
    EXPECT_EQ(stats.allocations.load(), 4);

    // Clean up
    manager_->deallocate(ptr1, 1024, MemoryPoolType::INDEX_CACHE);
    manager_->deallocate(ptr2, 512, MemoryPoolType::BUFFER_POOL);
    manager_->deallocate(ptr3, 256, MemoryPoolType::QUERY_RESULTS);
    manager_->deallocate(ptr4, 128, MemoryPoolType::SYSTEM_OVERHEAD);

    EXPECT_EQ(stats.deallocations.load(), 4);
}

TEST_F(MemoryManagerTest, PoolLimits) {
    // Test that pools have proper limits
    size_t index_limit = manager_->get_pool_limit(MemoryPoolType::INDEX_CACHE);
    size_t buffer_limit = manager_->get_pool_limit(MemoryPoolType::BUFFER_POOL);
    size_t query_limit = manager_->get_pool_limit(MemoryPoolType::QUERY_RESULTS);
    size_t system_limit = manager_->get_pool_limit(MemoryPoolType::SYSTEM_OVERHEAD);

    EXPECT_GT(index_limit, 0);
    EXPECT_GT(buffer_limit, 0);
    EXPECT_GT(query_limit, 0);
    EXPECT_GT(system_limit, 0);

    // Index cache should have the largest allocation
    EXPECT_GT(index_limit, buffer_limit);
    EXPECT_GT(index_limit, query_limit);
    EXPECT_GT(index_limit, system_limit);
}

TEST_F(MemoryManagerTest, PressureDetection) {
    // Initially should be low pressure
    EXPECT_EQ(manager_->get_pressure_level(), MemoryPressureLevel::LOW);

    // Allocate a large amount to increase pressure
    std::vector<void*> ptrs;
    const size_t allocation_size = 512 * 1024;  // 512KB

    // Try to allocate until we get pressure or fail
    for (int i = 0; i < 20; ++i) {
        void* ptr = manager_->allocate(allocation_size, MemoryPoolType::INDEX_CACHE);
        if (ptr) {
            ptrs.push_back(ptr);
        } else {
            break;  // Allocation failed
        }

        // Check if pressure increased
        if (manager_->get_pressure_level() != MemoryPressureLevel::LOW) {
            break;
        }
    }

    // Clean up
    for (void* ptr : ptrs) {
        manager_->deallocate(ptr, allocation_size, MemoryPoolType::INDEX_CACHE);
    }
}

TEST_F(MemoryManagerTest, ConvenienceFunctions) {
    // Test convenience allocation functions
    void* ptr1 = memory::allocate_index(1024);
    ASSERT_NE(ptr1, nullptr);

    void* ptr2 = memory::allocate_buffer(512);
    ASSERT_NE(ptr2, nullptr);

    void* ptr3 = memory::allocate_query(256);
    ASSERT_NE(ptr3, nullptr);

    void* ptr4 = memory::allocate_system(128);
    ASSERT_NE(ptr4, nullptr);

    // Clean up using convenience functions
    memory::deallocate_index(ptr1, 1024);
    memory::deallocate_buffer(ptr2, 512);
    memory::deallocate_query(ptr3, 256);
    memory::deallocate_system(ptr4, 128);
}

TEST_F(MemoryManagerTest, SystemMemoryInfo) {
    // Test system memory information functions
    size_t total_memory = MemoryManager::get_total_system_memory();
    size_t available_memory = MemoryManager::get_available_system_memory();
    size_t process_memory = MemoryManager::get_process_memory_usage();

    EXPECT_GT(total_memory, 0);
    EXPECT_GT(available_memory, 0);
    // Process memory might be 0 on some systems where it's not available
    EXPECT_GE(process_memory, 0);

    // Available should be less than or equal to total
    EXPECT_LE(available_memory, total_memory);
}

TEST_F(MemoryManagerTest, BulkOperations) {
    // Test bulk allocation/deallocation
    void* ptr = manager_->allocate_bulk(10, 64, MemoryPoolType::INDEX_CACHE);
    ASSERT_NE(ptr, nullptr);

    const auto& stats = manager_->stats();
    EXPECT_GT(stats.total_memory.load(), 0);

    manager_->deallocate_bulk(ptr, 10, 64, MemoryPoolType::INDEX_CACHE);
}

TEST_F(MemoryManagerTest, GlobalMemoryManager) {
    // Test global memory manager functions
    MemoryManager* global_manager = get_memory_manager();
    ASSERT_NE(global_manager, nullptr);

    // Should be able to allocate through global manager
    void* ptr = global_manager->allocate(1024, MemoryPoolType::INDEX_CACHE);
    EXPECT_NE(ptr, nullptr);

    if (ptr) {
        global_manager->deallocate(ptr, 1024, MemoryPoolType::INDEX_CACHE);
    }
}

TEST_F(MemoryManagerTest, TypeSafeAllocation) {
    // Test C++20 templated type-safe allocation

    // Single object allocation
    auto* int_ptr = manager_->allocate<int>(MemoryPoolType::INDEX_CACHE);
    ASSERT_NE(int_ptr, nullptr);

    // Construct and use the object
    new (int_ptr) int(42);
    EXPECT_EQ(*int_ptr, 42);

    // Clean up (trivial destructor for int, but showing the pattern)
    std::destroy_at(int_ptr);
    manager_->deallocate(int_ptr, MemoryPoolType::INDEX_CACHE);

    // Array allocation using span
    auto span = manager_->allocate_array<uint64_t>(10, MemoryPoolType::BUFFER_POOL);
    ASSERT_EQ(span.size(), 10);
    ASSERT_NE(span.data(), nullptr);

    // Use the array
    for (size_t i = 0; i < span.size(); ++i) {
        new (&span[i]) uint64_t(i * 2);
        EXPECT_EQ(span[i], i * 2);
    }

    // Clean up array (trivial destructor for uint64_t, but showing the pattern)
    std::destroy(span.begin(), span.end());
    manager_->deallocate_array(span, MemoryPoolType::BUFFER_POOL);
}

TEST_F(MemoryManagerTest, Cpp20Features) {
    // Test constexpr config validation
    constexpr auto default_config = MemoryConfig::create_default_config();
    static_assert(default_config.is_valid(), "Default config must be valid at compile time");

    constexpr auto efficient_config = MemoryConfig::create_efficient_config();
    static_assert(efficient_config.is_valid(), "Efficient config must be valid at compile time");

    // Test memory size helpers
    using namespace memory_sizes;
    static_assert(KB(1) == 1024);
    static_assert(MB(1) == 1024 * 1024);
    static_assert(GB(1) == 1024ULL * 1024 * 1024);

    // Runtime validation
    EXPECT_TRUE(default_config.is_valid());
    EXPECT_TRUE(efficient_config.is_valid());

    // Test memory size calculations
    EXPECT_EQ(KB(2), 2048);
    EXPECT_EQ(MB(5), 5 * 1024 * 1024);
}

}  // namespace lumen