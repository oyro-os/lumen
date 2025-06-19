#include <gtest/gtest.h>
#include <lumen/index/btree_index.h>
#include <lumen/storage/storage_engine.h>

#include <algorithm>
#include <random>
#include <thread>
#include <unordered_set>

using namespace lumen;

class BTreeIndexTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create unique test directory
        test_dir_ = "test_btree_index_" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

        // Set up storage
        StorageConfig storage_config;
        storage_config.data_directory = test_dir_;
        storage_config.buffer_pool_size = 64;  // Small for testing
        storage_ = StorageEngineFactory::create(storage_config);
        ASSERT_TRUE(storage_->open("btree_index_test_db"));

        // Create B+Tree with small min_degree for testing
        BTreeIndexConfig btree_config;
        btree_config.min_degree = 3;  // Small for easier testing
        btree_ = BTreeIndexFactory::create(storage_, btree_config);
    }

    void TearDown() override {
        btree_.reset();
        storage_->close();
        storage_.reset();

        // Clean up test directory
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::string test_dir_;
    std::shared_ptr<StorageEngine> storage_;
    std::unique_ptr<BTreeIndex> btree_;
};

TEST_F(BTreeIndexTest, EmptyTree) {
    EXPECT_TRUE(btree_->empty());
    EXPECT_EQ(btree_->size(), 0u);
    EXPECT_EQ(btree_->height(), 1u);  // Root page exists
    EXPECT_NE(btree_->root_page_id(), kInvalidPageID);
}

TEST_F(BTreeIndexTest, SingleInsert) {
    Value key(42);
    Value value("test_value");

    EXPECT_TRUE(btree_->insert(key, value));
    EXPECT_FALSE(btree_->empty());
    EXPECT_EQ(btree_->size(), 1u);

    auto result = btree_->find(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString(), "test_value");
}

TEST_F(BTreeIndexTest, MultipleInserts) {
    const int num_entries = 10;

    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        Value value("value_" + std::to_string(i));
        EXPECT_TRUE(btree_->insert(key, value));
    }

    EXPECT_EQ(btree_->size(), num_entries);

    // Verify all entries
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        auto result = btree_->find(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->getString(), "value_" + std::to_string(i));
    }
}

TEST_F(BTreeIndexTest, DuplicateKeyReject) {
    Value key(100);
    Value value1("first");
    Value value2("second");

    EXPECT_TRUE(btree_->insert(key, value1));
    EXPECT_FALSE(btree_->insert(key, value2));  // Should reject duplicate

    auto result = btree_->find(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString(), "first");
}

TEST_F(BTreeIndexTest, FindNonExistent) {
    Value key1(10);
    Value value1("ten");

    EXPECT_TRUE(btree_->insert(key1, value1));

    Value key2(20);
    auto result = btree_->find(key2);
    EXPECT_FALSE(result.has_value());

    EXPECT_FALSE(btree_->contains(key2));
    EXPECT_TRUE(btree_->contains(key1));
}

TEST_F(BTreeIndexTest, RangeScan) {
    // Insert test data
    std::vector<int> keys = {5, 10, 15, 20, 25, 30, 35, 40};
    for (int key : keys) {
        Value k(key);
        Value v("value_" + std::to_string(key));
        EXPECT_TRUE(btree_->insert(k, v));
    }

    // Scan range [15, 30]
    Value start_key(15);
    Value end_key(30);
    auto results = btree_->range_scan(start_key, end_key);

    EXPECT_EQ(results.size(), 4u);  // 15, 20, 25, 30

    std::vector<int> expected = {15, 20, 25, 30};
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].key.getInt(), expected[i]);
        EXPECT_EQ(results[i].value.getString(), "value_" + std::to_string(expected[i]));
    }
}

TEST_F(BTreeIndexTest, RangeScanWithLimit) {
    // Insert test data
    for (int i = 0; i < 100; ++i) {
        Value key(i);
        Value value(i * 10);
        EXPECT_TRUE(btree_->insert(key, value));
    }

    // Scan with limit
    Value start_key(20);
    Value end_key(80);
    auto results = btree_->range_scan_limit(start_key, end_key, 10);

    EXPECT_EQ(results.size(), 10u);

    // Verify first 10 results
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].key.getInt(), 20 + i);
        EXPECT_EQ(results[i].value.getInt(), (20 + i) * 10);
    }
}

TEST_F(BTreeIndexTest, Iterator) {
    // Insert test data
    std::vector<int> keys = {30, 10, 20, 50, 40};
    for (int key : keys) {
        Value k(key);
        Value v(key * 100);
        EXPECT_TRUE(btree_->insert(k, v));
    }

    // Iterate through tree (should be in sorted order)
    std::vector<int> collected;
    for (auto it = btree_->begin(); it != btree_->end(); ++it) {
        collected.push_back(it->key.getInt());
    }

    std::vector<int> expected = {10, 20, 30, 40, 50};
    EXPECT_EQ(collected, expected);
}

TEST_F(BTreeIndexTest, BulkInsert) {
    std::vector<BTreeIndexEntry> entries;
    for (int i = 0; i < 50; ++i) {
        entries.emplace_back(Value(i), Value("bulk_" + std::to_string(i)));
    }

    EXPECT_TRUE(btree_->bulk_insert(entries));
    EXPECT_EQ(btree_->size(), 50u);

    // Verify all entries
    for (int i = 0; i < 50; ++i) {
        auto result = btree_->find(Value(i));
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->getString(), "bulk_" + std::to_string(i));
    }
}

TEST_F(BTreeIndexTest, Persistence) {
    const int num_entries = 50;
    PageID saved_root_page_id;

    // Insert data and save root page ID
    {
        for (int i = 0; i < num_entries; ++i) {
            Value key(i);
            Value value("persist_" + std::to_string(i));
            EXPECT_TRUE(btree_->insert(key, value));
        }

        EXPECT_EQ(btree_->size(), num_entries);
        saved_root_page_id = btree_->root_page_id();
        
        // Force flush
        storage_->flush_all_pages();
    }

    // Clear the existing btree instance
    btree_.reset();
    
    // Clear buffer pool to ensure we read from disk
    storage_->buffer_pool()->reset();

    // Create new BTreeIndex instance using the saved root page ID
    btree_ = std::make_unique<BTreeIndex>(storage_, saved_root_page_id, BTreeIndexConfig{.min_degree = 3});
    
    // Verify size is correct
    EXPECT_EQ(btree_->size(), num_entries);

    // Verify data still accessible
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        auto result = btree_->find(key);
        ASSERT_TRUE(result.has_value()) << "Key " << i << " not found after persistence test";
        EXPECT_EQ(result->getString(), "persist_" + std::to_string(i));
    }
}

TEST_F(BTreeIndexTest, StringKeys) {
    std::vector<std::string> keys = {"apple", "banana", "cherry", "date", "elderberry"};

    for (const auto& key : keys) {
        Value k(key);
        Value v(static_cast<int64_t>(key.length()));
        EXPECT_TRUE(btree_->insert(k, v));
    }

    // Verify
    for (const auto& key : keys) {
        Value k(key);
        auto result = btree_->find(k);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->getInt(), key.length());
    }

    // Range scan
    Value start("banana");
    Value end("date");
    auto results = btree_->range_scan(start, end);

    EXPECT_EQ(results.size(), 3u);  // banana, cherry, date
}

TEST_F(BTreeIndexTest, MixedValueTypes) {
    // Insert different value types
    EXPECT_TRUE(btree_->insert(Value(1), Value(100)));
    EXPECT_TRUE(btree_->insert(Value(2), Value("string_value")));
    EXPECT_TRUE(btree_->insert(Value(3), Value(3.14)));
    EXPECT_TRUE(btree_->insert(Value(4), Value(true)));

    // Verify
    auto v1 = btree_->find(Value(1));
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1->getInt(), 100);

    auto v2 = btree_->find(Value(2));
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v2->getString(), "string_value");

    auto v3 = btree_->find(Value(3));
    ASSERT_TRUE(v3.has_value());
    EXPECT_DOUBLE_EQ(v3->getFloat(), 3.14);

    auto v4 = btree_->find(Value(4));
    ASSERT_TRUE(v4.has_value());
    EXPECT_EQ(v4->getBool(), true);
}

TEST_F(BTreeIndexTest, FindIterator) {
    // Insert test data
    for (int i = 0; i < 10; ++i) {
        Value key(i * 5);  // 0, 5, 10, 15, ...
        Value value(i);
        EXPECT_TRUE(btree_->insert(key, value));
    }

    // Find existing key
    auto it = btree_->find_iterator(Value(15));
    ASSERT_NE(it, btree_->end());
    EXPECT_EQ(it->key.getInt(), 15);
    EXPECT_EQ(it->value.getInt(), 3);

    // Find non-existing key
    auto it2 = btree_->find_iterator(Value(17));
    EXPECT_EQ(it2, btree_->end());
}

TEST_F(BTreeIndexTest, PageSplitting) {
    // Insert enough entries to cause page splits
    const int num_entries = 100;

    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        Value value(i * i);
        EXPECT_TRUE(btree_->insert(key, value));
    }

    EXPECT_EQ(btree_->size(), num_entries);
    EXPECT_GT(btree_->height(), 1u);  // Should have grown

    // Verify all entries still findable
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        auto result = btree_->find(key);
        ASSERT_TRUE(result.has_value()) << "Key " << i << " not found after splitting";
        EXPECT_EQ(result->getInt(), i * i);
    }
}

TEST_F(BTreeIndexTest, RandomInserts) {
    const int num_entries = 500;
    std::vector<int> keys;

    // Generate random keys
    for (int i = 0; i < num_entries; ++i) {
        keys.push_back(i);
    }

    // Shuffle keys
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(keys.begin(), keys.end(), gen);

    // Insert in random order
    for (int key : keys) {
        Value k(key);
        Value v("random_" + std::to_string(key));
        EXPECT_TRUE(btree_->insert(k, v));
    }

    EXPECT_EQ(btree_->size(), num_entries);

    // Verify all entries
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        auto result = btree_->find(key);
        ASSERT_TRUE(result.has_value()) << "Key " << i << " not found in random insert test";
        EXPECT_EQ(result->getString(), "random_" + std::to_string(i));
    }
}