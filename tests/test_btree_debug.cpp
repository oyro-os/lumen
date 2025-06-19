#include <gtest/gtest.h>
#include <lumen/index/btree.h>
#include <lumen/storage/storage_engine.h>

#include <filesystem>
#include <iostream>

using namespace lumen;

TEST(BTreeDebugTest, BasicCreation) {
    // Create test directory
    std::string test_dir =
        "test_btree_debug_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    // Set up storage
    StorageConfig storage_config;
    storage_config.data_directory = test_dir;
    storage_config.buffer_pool_size = 16;
    auto storage = StorageEngineFactory::create(storage_config);

    std::cout << "Opening storage engine..." << std::endl;
    ASSERT_TRUE(storage->open("btree_debug_db"));

    std::cout << "Creating B+Tree..." << std::endl;
    // Create B+Tree
    BTreeConfig btree_config;
    btree_config.min_degree = 3;

    try {
        auto btree = BTreeFactory::create(storage, btree_config);
        std::cout << "B+Tree created successfully" << std::endl;
        std::cout << "Root page ID: " << btree->root_page_id() << std::endl;
        std::cout << "Empty: " << btree->empty() << std::endl;
        std::cout << "Size: " << btree->size() << std::endl;

        // Flush all pages to disk
        std::cout << "\nFlushing storage..." << std::endl;
        storage->flush_all_pages();

        // Load root page directly
        std::cout << "\nLoading root page directly..." << std::endl;
        PageRef root_page = storage->fetch_page(btree->root_page_id());
        if (root_page) {
            std::cout << "Root page loaded" << std::endl;
            std::cout << "Page ID: " << root_page->page_id() << std::endl;
            std::cout << "Page type: " << static_cast<int>(root_page->page_type()) << std::endl;

            // Print first 64 bytes of page data
            const char* data = static_cast<const char*>(root_page->data());
            std::cout << "\nPage data (first 64 bytes):" << std::endl;
            for (int i = 0; i < 64; i++) {
                if (i % 16 == 0)
                    std::cout << std::setw(4) << i << ": ";
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << (int)(unsigned char)data[i] << " ";
                if (i % 16 == 15)
                    std::cout << std::endl;
            }
            std::cout << std::dec << std::endl;
        } else {
            std::cout << "Failed to load root page!" << std::endl;
        }

        // Load and check root node
        std::cout << "\nChecking root node..." << std::endl;
        auto root = btree->load_node(btree->root_page_id());
        if (root) {
            std::cout << "Root node loaded successfully" << std::endl;
            std::cout << "Root is leaf: " << root->is_leaf() << std::endl;
            std::cout << "Root num_keys: " << root->num_keys() << std::endl;
            std::cout << "Root is full: " << root->is_full() << std::endl;
        } else {
            std::cout << "Failed to load root node!" << std::endl;
        }

        // Try a simple insert
        std::cout << "\nTrying simple insert..." << std::endl;
        Value key(42);
        Value value("test_value");
        bool result = btree->insert(key, value);
        std::cout << "Insert result: " << result << std::endl;
        std::cout << "Size after insert: " << btree->size() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        FAIL();
    }

    // Clean up
    storage->close();
    std::filesystem::remove_all(test_dir);
}