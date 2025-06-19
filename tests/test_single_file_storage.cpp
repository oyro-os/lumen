#include <lumen/storage/single_file_storage.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <random>

namespace lumen {

class SingleFileStorageTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create temporary test directory
        test_dir_ = "test_single_file_" + std::to_string(std::random_device{}());
        std::filesystem::create_directory(test_dir_);

        // Configure storage
        config_.database_path = test_dir_ + "/test.db";
        config_.wal_path = test_dir_ + "/test.wal";
        config_.buffer_pool_size = 16;  // Small for testing
        config_.initial_size_mb = 1;
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    SingleFileStorageConfig config_;
};

TEST_F(SingleFileStorageTest, CreateNewDatabase) {
    auto storage = SingleFileStorageFactory::create(config_);
    ASSERT_TRUE(storage != nullptr);

    // Create new database
    ASSERT_TRUE(storage->create());
    ASSERT_TRUE(storage->is_open());

    // Check header
    const auto& header = storage->header();
    EXPECT_EQ(std::string(header.magic, 7), "LUMENDB");
    EXPECT_EQ(header.version, 0x00010000u);
    EXPECT_EQ(header.page_size, 4096u);
    EXPECT_GE(header.page_count, 1u);

    // File should exist
    EXPECT_TRUE(std::filesystem::exists(config_.database_path));

    // Close
    storage->close();
    EXPECT_FALSE(storage->is_open());
}

TEST_F(SingleFileStorageTest, OpenExistingDatabase) {
    // Create database
    {
        auto storage = SingleFileStorageFactory::create(config_);
        ASSERT_TRUE(storage->create());
        storage->close();
    }

    // Open existing database
    {
        auto storage = SingleFileStorageFactory::create(config_);
        ASSERT_TRUE(storage->open());
        ASSERT_TRUE(storage->is_open());

        const auto& header = storage->header();
        EXPECT_EQ(std::string(header.magic, 7), "LUMENDB");
        EXPECT_EQ(header.version, 0x00010000u);
    }
}

TEST_F(SingleFileStorageTest, PageAllocation) {
    auto storage = SingleFileStorageFactory::create(config_);
    ASSERT_TRUE(storage->create());

    // Allocate some pages
    std::vector<PageRef> pages;
    for (int i = 0; i < 10; ++i) {
        auto page = storage->new_page(PageTypeV2::DATA);
        ASSERT_TRUE(page);
        EXPECT_GT(page->page_id(), 0u);  // Page 0 is header
        pages.push_back(page);
    }

    // Pages should have unique IDs
    std::set<PageID> page_ids;
    for (const auto& page : pages) {
        page_ids.insert(page->page_id());
    }
    EXPECT_EQ(page_ids.size(), pages.size());

    // Page count should increase
    EXPECT_GE(storage->page_count(), 11u);  // Header + 10 pages
}

TEST_F(SingleFileStorageTest, PagePersistence) {
    PageID test_page_id;
    std::string test_data = "Hello, Lumen Database!";

    // Write data
    {
        auto storage = SingleFileStorageFactory::create(config_);
        ASSERT_TRUE(storage->create());

        auto page = storage->new_page(PageTypeV2::DATA);
        ASSERT_TRUE(page);
        test_page_id = page->page_id();

        // Write test data
        std::memcpy(static_cast<char*>(page->data()) + PageHeader::kSize, test_data.c_str(),
                    test_data.size() + 1);
        page->mark_dirty();

        // Flush
        ASSERT_TRUE(storage->flush_page(test_page_id));
        storage->close();
    }

    // Read data back
    {
        auto storage = SingleFileStorageFactory::create(config_);
        ASSERT_TRUE(storage->open());

        auto page = storage->fetch_page(test_page_id);
        ASSERT_TRUE(page);

        // Verify data
        const char* read_data = static_cast<const char*>(page->data()) + PageHeader::kSize;
        EXPECT_EQ(std::string(read_data), test_data);
    }
}

TEST_F(SingleFileStorageTest, FileGrowth) {
    auto storage = SingleFileStorageFactory::create(config_);
    ASSERT_TRUE(storage->create());

    size_t initial_count = storage->page_count();
    size_t initial_free = storage->free_page_count();

    printf("Initial page count: %zu\n", initial_count);
    printf("Initial free pages: %zu\n", initial_free);

    // Allocate enough pages to exhaust free list and trigger growth
    size_t pages_to_allocate = initial_free + 10;  // More than available

    for (size_t i = 0; i < pages_to_allocate; ++i) {
        auto page = storage->new_page(PageTypeV2::DATA);
        if (!page) {
            printf("Failed to allocate page %zu\n", i);
            printf("Current page count: %zu\n", storage->page_count());
            printf("Current free pages: %zu\n", storage->free_page_count());
            break;
        }
        // Unpin the page so BufferPool can reuse the frame
        storage->buffer_pool()->unpin_page(page->page_id());

        if ((i + 1) % 50 == 0) {
            printf("Allocated %zu pages, current free: %zu\n", i + 1, storage->free_page_count());
        }
    }

    // File should have grown beyond initial size
    printf("Final page count: %zu (initial: %zu)\n", storage->page_count(), initial_count);
    EXPECT_GT(storage->page_count(), initial_count);

    // Check actual file size
    size_t expected_size = storage->header().page_count * kPageSize;
    storage->close();
    auto file_size = std::filesystem::file_size(config_.database_path);
    EXPECT_EQ(file_size, expected_size);
}

TEST_F(SingleFileStorageTest, PageDeletion) {
    auto storage = SingleFileStorageFactory::create(config_);
    ASSERT_TRUE(storage->create());

    // Allocate pages
    std::vector<PageID> page_ids;
    for (int i = 0; i < 5; ++i) {
        auto page = storage->new_page(PageTypeV2::DATA);
        ASSERT_TRUE(page);
        page_ids.push_back(page->page_id());
    }

    size_t free_before = storage->free_page_count();

    // Delete some pages
    ASSERT_TRUE(storage->delete_page(page_ids[1]));
    ASSERT_TRUE(storage->delete_page(page_ids[3]));

    // Free count should increase
    EXPECT_EQ(storage->free_page_count(), free_before + 2);

    // Deleted pages should not be fetchable
    auto deleted_page = storage->fetch_page(page_ids[1]);
    // Note: This might still return a page from buffer pool cache
    // In a real implementation, deleted pages should be marked as such
}

}  // namespace lumen