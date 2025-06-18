#include <gtest/gtest.h>
#include <lumen/memory/allocator.h>

#include <thread>
#include <vector>

using namespace lumen;

class AllocatorTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AllocatorTest, BasicAllocation) {
    auto* allocator = get_allocator();
    ASSERT_NE(allocator, nullptr);

    // Test basic allocation
    void* ptr = allocator->allocate(1024);
    ASSERT_NE(ptr, nullptr);

    // Should be able to write to allocated memory
    std::memset(ptr, 0x42, 1024);

    allocator->deallocate(ptr, 1024);
}

TEST_F(AllocatorTest, AlignedAllocation) {
    auto* allocator = get_allocator();

    // Test various alignments
    size_t alignments[] = {8, 16, 32, 64, 128, 256};
    for (size_t alignment : alignments) {
        void* ptr = allocator->allocate(1024, alignment);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, 0)
            << "Allocation not aligned to " << alignment;
        allocator->deallocate(ptr, 1024);
    }
}

TEST_F(AllocatorTest, ZeroSizeAllocation) {
    auto* allocator = get_allocator();
    void* ptr = allocator->allocate(0);
    // Zero-size allocation should return nullptr
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(AllocatorTest, CategorizedAllocation) {
    auto* allocator = get_allocator();

    void* ptr = allocator->allocate(1024, AllocationCategory::Page);
    ASSERT_NE(ptr, nullptr);
    allocator->deallocate(ptr, 1024);
}

TEST_F(AllocatorTest, BulkAllocation) {
    auto* allocator = get_allocator();

    size_t count = 100;
    size_t size = 64;
    void* ptr = allocator->allocate_bulk(count, size);
    ASSERT_NE(ptr, nullptr);

    // Should be able to write to all allocated memory
    std::memset(ptr, 0x42, count * size);

    allocator->deallocate_bulk(ptr, count, size);
}

TEST_F(AllocatorTest, ConvenienceFunctions) {
    // Test global allocation functions
    void* ptr1 = allocate(1024);
    ASSERT_NE(ptr1, nullptr);
    deallocate(ptr1, 1024);

    void* ptr2 = allocate(2048, AllocationCategory::Index);
    ASSERT_NE(ptr2, nullptr);
    deallocate(ptr2, 2048);
}

TEST_F(AllocatorTest, StlAllocator) {
    // Test STL allocator with vector
    std::vector<int, StlAllocator<int>> vec;
    vec.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        vec.push_back(i);
    }

    EXPECT_EQ(vec.size(), 1000u);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(vec[i], i);
    }
}

TEST_F(AllocatorTest, MemoryPool) {
    MemoryPool<64> pool;

    // Allocate some blocks
    std::vector<void*> blocks;
    for (int i = 0; i < 100; ++i) {
        void* block = pool.allocate();
        ASSERT_NE(block, nullptr);
        blocks.push_back(block);
    }

    EXPECT_EQ(pool.allocated_blocks(), 100u);

    // Deallocate half
    for (int i = 0; i < 50; ++i) {
        pool.deallocate(blocks[i]);
    }

    EXPECT_EQ(pool.allocated_blocks(), 50u);

    // Allocate again - should reuse deallocated blocks
    for (int i = 0; i < 50; ++i) {
        void* block = pool.allocate();
        ASSERT_NE(block, nullptr);
    }

    EXPECT_EQ(pool.allocated_blocks(), 100u);
}

TEST_F(AllocatorTest, AlignedHelpers) {
    struct alignas(64) AlignedStruct {
        char data[64];
    };

    // Test aligned allocation helper
    AlignedStruct* ptr = allocate_aligned<AlignedStruct>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0);

    // Construct object
    new (ptr) AlignedStruct{};

    // Destroy and deallocate
    ptr->~AlignedStruct();
    deallocate_aligned(ptr);
}

TEST_F(AllocatorTest, ThreadSafety) {
    const int num_threads = 4;
    const int allocations_per_thread = 1000;

    auto thread_func = []() {
        for (int i = 0; i < allocations_per_thread; ++i) {
            size_t size = 64 + (i % 256);
            void* ptr = allocate(size);
            ASSERT_NE(ptr, nullptr);
            std::memset(ptr, i & 0xFF, size);
            deallocate(ptr, size);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func);
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(AllocatorTest, MemoryStats) {
    auto* allocator = get_allocator();

    size_t initial_allocated = allocator->allocated_size();

    // Allocate some memory
    std::vector<std::pair<void*, size_t>> allocations;
    for (int i = 0; i < 10; ++i) {
        size_t size = 1024 * (i + 1);
        void* ptr = allocator->allocate(size);
        ASSERT_NE(ptr, nullptr);
        allocations.push_back({ptr, size});
    }

    // Allocated size should have increased
    size_t after_alloc = allocator->allocated_size();
    EXPECT_GT(after_alloc, initial_allocated);

    // Peak should be at least current
    EXPECT_GE(allocator->peak_allocated_size(), after_alloc);

    // Deallocate all
    for (const auto& [ptr, size] : allocations) {
        allocator->deallocate(ptr, size);
    }
}