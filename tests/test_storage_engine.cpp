#include <gtest/gtest.h>
#include <lumen/storage/storage_engine.h>

#include <filesystem>
#include <thread>
#include <vector>

using namespace lumen;

class StorageEngineTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create unique test directory
        test_dir_ = "test_data_" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        config_.data_directory = test_dir_;
        config_.buffer_pool_size = 16;  // Small for testing
        storage_engine_ = StorageEngineFactory::create(config_);
    }

    void TearDown() override {
        storage_engine_.reset();
        // Clean up test directory
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::string test_dir_;
    StorageConfig config_;
    std::shared_ptr<StorageEngine> storage_engine_;
};

TEST_F(StorageEngineTest, BasicCreation) {
    EXPECT_FALSE(storage_engine_->is_open());
    EXPECT_EQ(storage_engine_->config().data_directory, test_dir_);
    EXPECT_EQ(storage_engine_->config().buffer_pool_size, 16u);
}

TEST_F(StorageEngineTest, CreateAndOpenDatabase) {
    const std::string db_name = "test_db";

    // Don't explicitly create database - let open handle it
    // EXPECT_TRUE(storage_engine_->create_database(db_name));
    // EXPECT_TRUE(storage_engine_->database_exists(db_name));

    // Open database (should create if missing)
    EXPECT_TRUE(storage_engine_->open(db_name));
    EXPECT_TRUE(storage_engine_->is_open());

    // Check metadata
    EXPECT_EQ(storage_engine_->metadata().magic_number, 0x4C554D4E);
    EXPECT_EQ(storage_engine_->metadata().version, 1u);
    EXPECT_EQ(storage_engine_->metadata().page_size, kPageSize);
    EXPECT_EQ(storage_engine_->page_count(), 0u);

    // Close database
    storage_engine_->close();
    EXPECT_FALSE(storage_engine_->is_open());
}

TEST_F(StorageEngineTest, OpenNonExistentDatabase) {
    config_.create_if_missing = false;
    auto engine = StorageEngineFactory::create(config_);

    EXPECT_FALSE(engine->open("non_existent_db"));
    EXPECT_FALSE(engine->is_open());
}

TEST_F(StorageEngineTest, CreateIfMissing) {
    const std::string db_name = "auto_created_db";

    EXPECT_TRUE(storage_engine_->open(db_name));
    EXPECT_TRUE(storage_engine_->is_open());
    EXPECT_TRUE(storage_engine_->database_exists(db_name));

    storage_engine_->close();
}

TEST_F(StorageEngineTest, ErrorIfExists) {
    const std::string db_name = "existing_db";

    // Create and close database
    EXPECT_TRUE(storage_engine_->create_database(db_name));

    // Try to open with error_if_exists
    config_.error_if_exists = true;
    auto engine = StorageEngineFactory::create(config_);
    EXPECT_FALSE(engine->open(db_name));
}

TEST_F(StorageEngineTest, PageOperations) {
    const std::string db_name = "page_test_db";
    EXPECT_TRUE(storage_engine_->open(db_name));

    // Create a new page
    PageRef page1 = storage_engine_->new_page(PageType::Data);
    ASSERT_TRUE(page1);
    PageID page_id = page1->page_id();
    EXPECT_EQ(page1->page_type(), PageType::Data);
    EXPECT_TRUE(page1->is_dirty());

    // Insert data
    const char* test_data = "Storage Engine Test Data";
    SlotID slot_id = page1->insert_record(test_data, strlen(test_data));
    EXPECT_NE(slot_id, static_cast<SlotID>(-1));

    // Flush page to disk
    EXPECT_TRUE(storage_engine_->flush_page(page_id));

    // Clear buffer pool to force disk read
    storage_engine_->buffer_pool()->reset();

    // Fetch page from disk
    PageRef page2 = storage_engine_->fetch_page(page_id);
    ASSERT_TRUE(page2);
    EXPECT_EQ(page2->page_id(), page_id);

    // Verify data
    size_t size;
    const void* data = page2->get_record(slot_id, &size);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(size, strlen(test_data));
    EXPECT_EQ(memcmp(data, test_data, size), 0);

    storage_engine_->close();
}

TEST_F(StorageEngineTest, MultiplePages) {
    const std::string db_name = "multi_page_db";
    EXPECT_TRUE(storage_engine_->open(db_name));

    std::vector<PageID> page_ids;

    // Create multiple pages
    for (int i = 0; i < 10; ++i) {
        PageRef page = storage_engine_->new_page(PageType::Data);
        ASSERT_TRUE(page);
        page_ids.push_back(page->page_id());

        // Insert unique data
        std::string data = "Page " + std::to_string(i) + " data";
        page->insert_record(data.c_str(), data.size());
    }

    EXPECT_EQ(storage_engine_->page_count(), 10u);

    // Flush all pages
    storage_engine_->flush_all_pages();

    // Clear buffer pool
    storage_engine_->buffer_pool()->reset();

    // Verify all pages
    for (size_t i = 0; i < page_ids.size(); ++i) {
        PageRef page = storage_engine_->fetch_page(page_ids[i]);
        ASSERT_TRUE(page);
        EXPECT_EQ(page->page_id(), page_ids[i]);

        // Verify data
        std::string expected_data = "Page " + std::to_string(i) + " data";
        size_t size;
        const void* data = page->get_record(0, &size);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(size, expected_data.size());
        EXPECT_EQ(memcmp(data, expected_data.c_str(), size), 0);
    }

    storage_engine_->close();
}

TEST_F(StorageEngineTest, PageDeletion) {
    const std::string db_name = "delete_test_db";
    EXPECT_TRUE(storage_engine_->open(db_name));

    // Create a page
    PageRef page = storage_engine_->new_page(PageType::Data);
    ASSERT_TRUE(page);
    PageID page_id = page->page_id();

    // Delete the page
    EXPECT_TRUE(storage_engine_->delete_page(page_id));

    // Verify page is deleted
    PageRef deleted_page = storage_engine_->fetch_page(page_id);
    EXPECT_FALSE(deleted_page);

    storage_engine_->close();
}

TEST_F(StorageEngineTest, DatabaseOperations) {
    // Create multiple databases
    std::vector<std::string> db_names = {"db1", "db2", "db3"};

    for (const auto& name : db_names) {
        EXPECT_TRUE(storage_engine_->create_database(name));
        EXPECT_TRUE(storage_engine_->database_exists(name));
    }

    // List databases
    auto databases = storage_engine_->list_databases();
    EXPECT_EQ(databases.size(), db_names.size());

    // Drop a database
    EXPECT_TRUE(storage_engine_->drop_database("db2"));
    EXPECT_FALSE(storage_engine_->database_exists("db2"));

    databases = storage_engine_->list_databases();
    EXPECT_EQ(databases.size(), 2u);
}

TEST_F(StorageEngineTest, PersistenceAcrossRestarts) {
    const std::string db_name = "persistent_db";
    PageID page_id;

    // Create database and add data
    {
        auto engine = StorageEngineFactory::create(config_);
        EXPECT_TRUE(engine->open(db_name));

        PageRef page = engine->new_page(PageType::Data);
        ASSERT_TRUE(page);
        page_id = page->page_id();

        const char* test_data = "Persistent data";
        page->insert_record(test_data, strlen(test_data));

        engine->flush_all_pages();
        engine->close();
    }

    // Reopen and verify data
    {
        auto engine = StorageEngineFactory::create(config_);
        EXPECT_TRUE(engine->open(db_name));

        PageRef page = engine->fetch_page(page_id);
        ASSERT_TRUE(page);

        size_t size;
        const void* data = page->get_record(0, &size);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(size, strlen("Persistent data"));
        EXPECT_EQ(memcmp(data, "Persistent data", size), 0);

        engine->close();
    }
}

TEST_F(StorageEngineTest, ConcurrentPageAccess) {
    const std::string db_name = "concurrent_db";
    EXPECT_TRUE(storage_engine_->open(db_name));

    const int num_threads = 4;
    const int pages_per_thread = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<PageID>> thread_page_ids(num_threads);

    // Create pages concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < pages_per_thread; ++i) {
                PageRef page = storage_engine_->new_page(PageType::Data);
                if (page) {
                    thread_page_ids[t].push_back(page->page_id());

                    std::string data = "Thread " + std::to_string(t) + " Page " + std::to_string(i);
                    page->insert_record(data.c_str(), data.size());
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all pages
    storage_engine_->flush_all_pages();
    storage_engine_->buffer_pool()->reset();

    for (int t = 0; t < num_threads; ++t) {
        for (size_t i = 0; i < thread_page_ids[t].size(); ++i) {
            PageRef page = storage_engine_->fetch_page(thread_page_ids[t][i]);
            ASSERT_TRUE(page);

            std::string expected = "Thread " + std::to_string(t) + " Page " + std::to_string(i);
            size_t size;
            const void* data = page->get_record(0, &size);
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(size, expected.size());
            EXPECT_EQ(memcmp(data, expected.c_str(), size), 0);
        }
    }

    storage_engine_->close();
}

TEST_F(StorageEngineTest, StorageManager) {
    auto& manager = StorageManager::instance();

    // Create engines
    auto engine1 = manager.create_engine("engine1", config_);
    auto engine2 = manager.create_engine("engine2", config_);

    ASSERT_TRUE(engine1);
    ASSERT_TRUE(engine2);
    EXPECT_NE(engine1, engine2);

    // Get engines
    EXPECT_EQ(manager.get_engine("engine1"), engine1);
    EXPECT_EQ(manager.get_engine("engine2"), engine2);
    EXPECT_EQ(manager.get_engine("non_existent"), nullptr);

    // List engines
    auto engines = manager.list_engines();
    EXPECT_GE(engines.size(), 2u);

    // Remove engine
    EXPECT_TRUE(manager.remove_engine("engine1"));
    EXPECT_EQ(manager.get_engine("engine1"), nullptr);
}
