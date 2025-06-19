#include <gtest/gtest.h>
#include <lumen/index/btree.h>
#include <lumen/storage/storage_engine.h>

#include <algorithm>
#include <random>
#include <thread>
#include <unordered_set>

using namespace lumen;

class BTreeTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create unique test directory
        test_dir_ = "test_btree_" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

        // Set up storage
        StorageConfig storage_config;
        storage_config.data_directory = test_dir_;
        storage_config.buffer_pool_size = 64;  // Small for testing
        storage_ = StorageEngineFactory::create(storage_config);
        ASSERT_TRUE(storage_->open("btree_test_db"));

        // Create B+Tree with small min_degree for testing
        BTreeConfig btree_config;
        btree_config.min_degree = 3;  // Small for easier testing
        btree_ = BTreeFactory::create(storage_, btree_config);
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
    std::unique_ptr<BTree> btree_;
};

TEST_F(BTreeTest, EmptyTree) {
    EXPECT_TRUE(btree_->empty());
    EXPECT_EQ(btree_->size(), 0u);
    EXPECT_EQ(btree_->height(), 1u);  // Root node exists
    EXPECT_NE(btree_->root_page_id(), kInvalidPageID);
}

TEST_F(BTreeTest, SingleInsert) {
    Value key(42);
    Value value("test_value");

    EXPECT_TRUE(btree_->insert(key, value));
    EXPECT_FALSE(btree_->empty());
    EXPECT_EQ(btree_->size(), 1u);

    auto result = btree_->find(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString(), "test_value");
}

TEST_F(BTreeTest, MultipleInserts) {
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

TEST_F(BTreeTest, DuplicateKeyReject) {
    Value key(100);
    Value value1("first");
    Value value2("second");

    EXPECT_TRUE(btree_->insert(key, value1));
    EXPECT_FALSE(btree_->insert(key, value2));  // Should reject duplicate

    auto result = btree_->find(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString(), "first");
}

TEST_F(BTreeTest, DuplicateKeyAllow) {
    // Create tree that allows duplicates
    BTreeConfig config;
    config.min_degree = 3;
    config.allow_duplicates = true;
    auto dup_tree = BTreeFactory::create(storage_, config);

    Value key(100);
    Value value1("first");
    Value value2("second");

    EXPECT_TRUE(dup_tree->insert(key, value1));
    EXPECT_TRUE(dup_tree->insert(key, value2));  // Should allow duplicate

    // First occurrence should be returned
    auto result = dup_tree->find(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getString(), "first");
}

TEST_F(BTreeTest, FindNonExistent) {
    Value key1(10);
    Value value1("ten");

    EXPECT_TRUE(btree_->insert(key1, value1));

    Value key2(20);
    auto result = btree_->find(key2);
    EXPECT_FALSE(result.has_value());

    EXPECT_FALSE(btree_->contains(key2));
    EXPECT_TRUE(btree_->contains(key1));
}

TEST_F(BTreeTest, RangeScan) {
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

TEST_F(BTreeTest, RangeScanWithLimit) {
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

TEST_F(BTreeTest, Iterator) {
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

TEST_F(BTreeTest, BulkInsert) {
    std::vector<BTreeEntry> entries;
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

TEST_F(BTreeTest, NodeSplitting) {
    // Insert enough entries to cause splits
    // With min_degree=3, max_keys=5, so 6 keys will cause split
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
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->getInt(), i * i);
    }
}

TEST_F(BTreeTest, RandomInserts) {
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
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->getString(), "random_" + std::to_string(i));
    }
}

TEST_F(BTreeTest, StringKeys) {
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

TEST_F(BTreeTest, MixedValueTypes) {
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

TEST_F(BTreeTest, CustomComparator) {
    // Create tree with reverse comparator
    BTreeConfig config;
    config.min_degree = 3;
    config.comparator = [](const Value& a, const Value& b) -> int {
        // Reverse comparison
        if (a > b)
            return -1;
        if (a < b)
            return 1;
        return 0;
    };

    auto reverse_tree = BTreeFactory::create(storage_, config);

    // Insert values
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(reverse_tree->insert(Value(i), Value(i * 10)));
    }

    // Iterate should give reverse order
    std::vector<int> collected;
    for (auto it = reverse_tree->begin(); it != reverse_tree->end(); ++it) {
        collected.push_back(it->key.getInt());
    }

    std::vector<int> expected = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    EXPECT_EQ(collected, expected);
}

TEST_F(BTreeTest, Persistence) {
    const int num_entries = 50;

    // Insert data and close
    {
        for (int i = 0; i < num_entries; ++i) {
            Value key(i);
            Value value("persist_" + std::to_string(i));
            EXPECT_TRUE(btree_->insert(key, value));
        }

        EXPECT_EQ(btree_->size(), num_entries);
    }

    // Force flush
    storage_->flush_all_pages();

    // Clear buffer pool to ensure we read from disk
    storage_->buffer_pool()->reset();

    // Verify data still accessible
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        auto result = btree_->find(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->getString(), "persist_" + std::to_string(i));
    }
}

TEST_F(BTreeTest, ConcurrentReads) {
    const int num_entries = 1000;
    const int num_threads = 4;

    // Insert test data
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        Value value(i * i);
        EXPECT_TRUE(btree_->insert(key, value));
    }

    // Concurrent reads
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread reads different keys
            for (int i = t; i < num_entries; i += num_threads) {
                Value key(i);
                auto result = btree_->find(key);
                if (result.has_value() && result->getInt() == i * i) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_entries);
}

TEST_F(BTreeTest, EmptyRangeScan) {
    // Insert some data
    for (int i = 0; i < 10; ++i) {
        Value key(i * 10);  // 0, 10, 20, ..., 90
        Value value(i);
        EXPECT_TRUE(btree_->insert(key, value));
    }

    // Scan range with no data
    Value start(25);
    Value end(29);
    auto results = btree_->range_scan(start, end);

    EXPECT_TRUE(results.empty());
}

TEST_F(BTreeTest, FullRangeScan) {
    const int num_entries = 20;

    // Insert data
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        Value value(i);
        EXPECT_TRUE(btree_->insert(key, value));
    }

    // Scan entire range
    Value start(std::numeric_limits<int>::min());
    Value end(std::numeric_limits<int>::max());
    auto results = btree_->range_scan(start, end);

    EXPECT_EQ(results.size(), num_entries);
}

TEST_F(BTreeTest, FindIterator) {
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