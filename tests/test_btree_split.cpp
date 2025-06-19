#include <gtest/gtest.h>
#include <lumen/index/btree.h>
#include <lumen/storage/storage_engine.h>

#include <filesystem>
#include <iostream>

using namespace lumen;

TEST(BTreeSplitTest, InternalNodeSearch) {
    // Create test directory
    std::string test_dir =
        "test_btree_split_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    // Set up storage
    StorageConfig storage_config;
    storage_config.data_directory = test_dir;
    storage_config.buffer_pool_size = 64;
    auto storage = StorageEngineFactory::create(storage_config);
    ASSERT_TRUE(storage->open("btree_split_db"));

    // Create an internal node manually
    BTreeInternalNode internal(1, 3);  // min_degree = 3

    // Set up as if it has 2 children with split key 5
    // child[0] contains keys < 5
    // child[1] contains keys >= 5
    internal.set_child_at(0, 100);                // left child page id
    internal.insert_key_child(0, Value(5), 101);  // split key and right child

    std::cout << "Internal node setup:" << std::endl;
    std::cout << "  num_keys: " << internal.num_keys() << std::endl;
    std::cout << "  key[0]: " << internal.key_at(0).getInt() << std::endl;
    std::cout << "  child[0]: " << internal.child_at(0) << std::endl;
    std::cout << "  child[1]: " << internal.child_at(1) << std::endl;

    // Test search with B+Tree logic
    BTreeConfig config;
    config.min_degree = 3;

    // Keys less than 5 should go to child[0]
    for (int i = 0; i < 5; i++) {
        Value key(i);
        size_t index = internal.search_key(key, config);

        // Apply B+Tree navigation logic
        if (index < internal.num_keys() &&
            internal.compare_keys(key, internal.key_at(index), config) >= 0) {
            index++;
        }

        PageID child = internal.child_at(index);
        std::cout << "Key " << i << " -> index " << index << " -> child " << child << std::endl;
        EXPECT_EQ(child, 100);
    }

    // Keys >= 5 should go to child[1]
    for (int i = 5; i < 10; i++) {
        Value key(i);
        size_t index = internal.search_key(key, config);

        // Apply B+Tree navigation logic
        if (index < internal.num_keys() &&
            internal.compare_keys(key, internal.key_at(index), config) >= 0) {
            index++;
        }

        PageID child = internal.child_at(index);
        std::cout << "Key " << i << " -> index " << index << " -> child " << child << std::endl;
        EXPECT_EQ(child, 101);
    }

    // Clean up
    storage->close();
    std::filesystem::remove_all(test_dir);
}