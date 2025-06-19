#include <gtest/gtest.h>
#include <lumen/index/btree.h>
#include <lumen/storage/storage_engine.h>

#include <filesystem>
#include <iostream>

using namespace lumen;

TEST(BTreeInsertTest, MultipleInserts) {
    // Create test directory
    std::string test_dir =
        "test_btree_insert_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    // Set up storage
    StorageConfig storage_config;
    storage_config.data_directory = test_dir;
    storage_config.buffer_pool_size = 64;
    auto storage = StorageEngineFactory::create(storage_config);
    ASSERT_TRUE(storage->open("btree_insert_db"));

    // Create B+Tree with small min_degree to trigger splits
    BTreeConfig btree_config;
    btree_config.min_degree = 3;  // max_keys = 5
    auto btree = BTreeFactory::create(storage, btree_config);

    std::cout << "Initial state - Empty: " << btree->empty() << ", Size: " << btree->size()
              << std::endl;

    // Insert entries and check after each insert
    const int num_entries = 10;
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        Value value("value_" + std::to_string(i));

        std::cout << "\nInserting key " << i << "..." << std::endl;
        std::cout << "Tree height before: " << btree->height() << std::endl;
        bool insert_result = btree->insert(key, value);
        std::cout << "Insert result: " << insert_result << std::endl;
        std::cout << "Tree size: " << btree->size() << std::endl;
        std::cout << "Tree height after: " << btree->height() << std::endl;

        // Try to find the key we just inserted
        auto result = btree->find(key);
        if (result.has_value()) {
            std::cout << "Found key " << i << " with value: " << result->getString() << std::endl;
        } else {
            std::cout << "ERROR: Could not find key " << i << " after insert!" << std::endl;
        }

        // Also check all previous keys
        std::cout << "Checking all keys 0-" << i << "..." << std::endl;
        for (int j = 0; j <= i; ++j) {
            Value check_key(j);
            auto check_result = btree->find(check_key);
            if (!check_result.has_value()) {
                std::cout << "ERROR: Lost key " << j << " after inserting key " << i << std::endl;
            }
        }
    }

    // Final verification
    std::cout << "\nFinal verification of all keys..." << std::endl;
    for (int i = 0; i < num_entries; ++i) {
        Value key(i);
        auto result = btree->find(key);
        ASSERT_TRUE(result.has_value()) << "Key " << i << " not found";
        EXPECT_EQ(result->getString(), "value_" + std::to_string(i));
    }

    // Clean up
    storage->close();
    std::filesystem::remove_all(test_dir);
}