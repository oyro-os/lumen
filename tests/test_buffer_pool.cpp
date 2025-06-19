#include <gtest/gtest.h>
#include <lumen/storage/buffer_pool.h>

#include <thread>
#include <vector>

using namespace lumen;

class BufferPoolTest : public ::testing::Test {
   protected:
    void SetUp() override {
        config_.pool_size = 16;  // Small pool for testing
        config_.eviction_policy = BufferPoolConfig::EvictionPolicy::Clock;
        buffer_pool_ = BufferPoolFactory::create(config_);
    }

    void TearDown() override {
        buffer_pool_.reset();
    }

    BufferPoolConfig config_;
    std::unique_ptr<BufferPool> buffer_pool_;
};

TEST_F(BufferPoolTest, BasicCreation) {
    EXPECT_EQ(buffer_pool_->pool_size(), 16u);
    EXPECT_EQ(buffer_pool_->used_frames(), 0u);
    EXPECT_EQ(buffer_pool_->utilization(), 0.0);
}

TEST_F(BufferPoolTest, NewPageCreation) {
    PageRef page = buffer_pool_->new_page(PageType::Data);
    ASSERT_TRUE(page);
    EXPECT_EQ(page->page_type(), PageType::Data);
    EXPECT_TRUE(page->is_dirty());
    EXPECT_EQ(buffer_pool_->used_frames(), 1u);

    bool unpinned = buffer_pool_->unpin_page(page->page_id(), false);
    EXPECT_TRUE(unpinned);
}

TEST_F(BufferPoolTest, FetchPage) {
    // Create a new page first
    PageRef page1 = buffer_pool_->new_page(PageType::Data);
    ASSERT_TRUE(page1);
    PageID page_id = page1->page_id();
    buffer_pool_->unpin_page(page_id, false);

    // Fetch the same page
    PageRef page2 = buffer_pool_->fetch_page(page_id);
    ASSERT_TRUE(page2);
    EXPECT_EQ(page2->page_id(), page_id);
    EXPECT_EQ(page1.get(), page2.get());  // Should be same page object

    buffer_pool_->unpin_page(page_id, false);
}

TEST_F(BufferPoolTest, MultiplePages) {
    std::vector<PageRef> pages;
    std::vector<PageID> page_ids;

    // Create multiple pages
    for (int i = 0; i < 10; ++i) {
        PageRef page = buffer_pool_->new_page(PageType::Data);
        ASSERT_TRUE(page);
        page_ids.push_back(page->page_id());
        pages.push_back(page);
    }

    EXPECT_EQ(buffer_pool_->used_frames(), 10u);

    // Unpin all pages
    for (size_t i = 0; i < pages.size(); ++i) {
        buffer_pool_->unpin_page(page_ids[i], false);
    }

    // Fetch all pages again
    for (PageID page_id : page_ids) {
        PageRef page = buffer_pool_->fetch_page(page_id);
        ASSERT_TRUE(page);
        EXPECT_EQ(page->page_id(), page_id);
        buffer_pool_->unpin_page(page_id, false);
    }
}

TEST_F(BufferPoolTest, PageEviction) {
    std::vector<PageID> page_ids;

    // Fill the buffer pool to capacity first
    for (size_t i = 0; i < buffer_pool_->pool_size(); ++i) {
        PageRef page = buffer_pool_->new_page(PageType::Data);
        ASSERT_TRUE(page);
        page_ids.push_back(page->page_id());

        // Unpin immediately to allow eviction
        buffer_pool_->unpin_page(page->page_id(), false);
    }

    EXPECT_EQ(buffer_pool_->used_frames(), buffer_pool_->pool_size());

    // Now add more pages beyond capacity - should trigger eviction
    for (size_t i = 0; i < 5; ++i) {
        PageRef page = buffer_pool_->new_page(PageType::Data);
        if (page) {  // May fail if eviction fails
            buffer_pool_->unpin_page(page->page_id(), false);
        }
    }

    // Buffer pool should be at or near capacity
    EXPECT_LE(buffer_pool_->used_frames(), buffer_pool_->pool_size());
}

TEST_F(BufferPoolTest, DirtyPageHandling) {
    PageRef page = buffer_pool_->new_page(PageType::Data);
    ASSERT_TRUE(page);
    PageID page_id = page->page_id();

    // Insert some data to make it dirty
    const char* test_data = "Test data";
    SlotID slot_id = page->insert_record(test_data, strlen(test_data));
    EXPECT_NE(slot_id, static_cast<SlotID>(-1));

    // Unpin as dirty
    buffer_pool_->unpin_page(page_id, true);

    // Flush the page
    bool flushed = buffer_pool_->flush_page(page_id);
    EXPECT_TRUE(flushed);
    EXPECT_GT(buffer_pool_->stats().pages_written.load(), 0u);
}

TEST_F(BufferPoolTest, PageDeletion) {
    PageRef page = buffer_pool_->new_page(PageType::Data);
    ASSERT_TRUE(page);
    PageID page_id = page->page_id();

    // Unpin the page
    buffer_pool_->unpin_page(page_id, false);

    // Delete the page
    bool deleted = buffer_pool_->delete_page(page_id);
    EXPECT_TRUE(deleted);
    EXPECT_EQ(buffer_pool_->used_frames(), 0u);

    // Trying to fetch deleted page should create new one
    PageRef new_page = buffer_pool_->fetch_page(page_id);
    ASSERT_TRUE(new_page);
    buffer_pool_->unpin_page(page_id, false);
}

TEST_F(BufferPoolTest, PinnedPageProtection) {
    PageRef page = buffer_pool_->new_page(PageType::Data);
    ASSERT_TRUE(page);
    PageID page_id = page->page_id();

    // Page is pinned, should not be deletable
    bool deleted = buffer_pool_->delete_page(page_id);
    EXPECT_FALSE(deleted);

    // Unpin and try again
    buffer_pool_->unpin_page(page_id, false);
    deleted = buffer_pool_->delete_page(page_id);
    EXPECT_TRUE(deleted);
}

TEST_F(BufferPoolTest, FlushAllPages) {
    std::vector<PageRef> pages;
    std::vector<PageID> page_ids;

    // Create and dirty multiple pages
    for (int i = 0; i < 5; ++i) {
        PageRef page = buffer_pool_->new_page(PageType::Data);
        ASSERT_TRUE(page);
        page_ids.push_back(page->page_id());

        // Insert data to make dirty
        std::string data = "Test data " + std::to_string(i);
        page->insert_record(data.c_str(), data.size());

        buffer_pool_->unpin_page(page->page_id(), true);
        pages.push_back(page);
    }

    uint64_t writes_before = buffer_pool_->stats().pages_written.load();
    buffer_pool_->flush_all_pages();
    uint64_t writes_after = buffer_pool_->stats().pages_written.load();

    EXPECT_GT(writes_after, writes_before);
    EXPECT_GT(buffer_pool_->stats().total_flushes.load(), 0u);
}

TEST_F(BufferPoolTest, Statistics) {
    const auto& stats = buffer_pool_->stats();

    // Initial state
    EXPECT_EQ(stats.page_requests.load(), 0u);
    EXPECT_EQ(stats.page_hits.load(), 0u);
    EXPECT_EQ(stats.page_misses.load(), 0u);
    EXPECT_EQ(stats.hit_ratio(), 0.0);

    // Create and fetch pages
    PageRef page1 = buffer_pool_->new_page(PageType::Data);
    PageID page_id = page1->page_id();
    buffer_pool_->unpin_page(page_id, false);

    // This should be a miss (new page fetch)
    PageRef page2 = buffer_pool_->fetch_page(page_id);
    buffer_pool_->unpin_page(page_id, false);

    // This should be a hit (page already in buffer)
    PageRef page3 = buffer_pool_->fetch_page(page_id);
    buffer_pool_->unpin_page(page_id, false);

    EXPECT_GT(stats.page_requests.load(), 0u);
    EXPECT_GT(stats.page_hits.load(), 0u);
    EXPECT_GT(stats.hit_ratio(), 0.0);
}

TEST_F(BufferPoolTest, LRUEvictionPolicy) {
    BufferPoolConfig lru_config;
    lru_config.pool_size = 4;
    lru_config.eviction_policy = BufferPoolConfig::EvictionPolicy::LRU;
    auto lru_buffer_pool = BufferPoolFactory::create(lru_config);

    std::vector<PageID> page_ids;

    // Fill buffer pool
    for (int i = 0; i < 4; ++i) {
        PageRef page = lru_buffer_pool->new_page(PageType::Data);
        ASSERT_TRUE(page);
        page_ids.push_back(page->page_id());
        lru_buffer_pool->unpin_page(page->page_id(), false);
    }

    // Access first page to make it recently used
    PageRef page = lru_buffer_pool->fetch_page(page_ids[0]);
    lru_buffer_pool->unpin_page(page_ids[0], false);

    // Add one more page, should evict least recently used
    PageRef new_page = lru_buffer_pool->new_page(PageType::Data);
    ASSERT_TRUE(new_page);
    lru_buffer_pool->unpin_page(new_page->page_id(), false);

    EXPECT_GT(lru_buffer_pool->stats().pages_evicted.load(), 0u);
}

TEST_F(BufferPoolTest, ConcurrentAccess) {
    const int num_threads = 4;
    const int pages_per_thread = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<PageID>> thread_page_ids(num_threads);

    // Create pages concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < pages_per_thread; ++i) {
                PageRef page = buffer_pool_->new_page(PageType::Data);
                if (page) {
                    thread_page_ids[t].push_back(page->page_id());

                    // Insert some data
                    std::string data = "Thread " + std::to_string(t) + " Page " + std::to_string(i);
                    page->insert_record(data.c_str(), data.size());

                    buffer_pool_->unpin_page(page->page_id(), true);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify pages were created
    EXPECT_GT(buffer_pool_->used_frames(), 0u);

    // Access pages concurrently
    threads.clear();
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (PageID page_id : thread_page_ids[t]) {
                PageRef page = buffer_pool_->fetch_page(page_id);
                if (page) {
                    // Verify we can read the data
                    EXPECT_EQ(page->page_id(), page_id);
                    buffer_pool_->unpin_page(page_id, false);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(BufferPoolTest, Reset) {
    // Create some pages
    std::vector<PageRef> pages;
    for (int i = 0; i < 5; ++i) {
        PageRef page = buffer_pool_->new_page(PageType::Data);
        ASSERT_TRUE(page);
        pages.push_back(page);
    }

    EXPECT_GT(buffer_pool_->used_frames(), 0u);

    // Reset buffer pool
    buffer_pool_->reset();

    EXPECT_EQ(buffer_pool_->used_frames(), 0u);
    EXPECT_EQ(buffer_pool_->utilization(), 0.0);
    EXPECT_EQ(buffer_pool_->stats().page_requests.load(), 0u);
}

TEST_F(BufferPoolTest, InvalidOperations) {
    // Fetch invalid page
    PageRef invalid_page = buffer_pool_->fetch_page(kInvalidPageID);
    EXPECT_FALSE(invalid_page);

    // Unpin non-existent page
    bool unpinned = buffer_pool_->unpin_page(999999, false);
    EXPECT_FALSE(unpinned);

    // Delete non-existent page (should succeed as no-op)
    bool deleted = buffer_pool_->delete_page(999999);
    EXPECT_TRUE(deleted);
}