#include <gtest/gtest.h>
#include <lumen/index/btree_index.h>
#include <lumen/storage/storage_engine.h>
#include <lumen/common/logging.h>
#include <iostream>
#include <iomanip>

using namespace lumen;

TEST(BTreeIndexSimpleDebug, FirstInsert) {
    // Enable debug logging
    SET_LOG_LEVEL(LogLevel::DEBUG);
    
    // Create unique test directory
    std::string test_dir = "test_btree_index_debug_" +
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    
    // Set up storage
    StorageConfig storage_config;
    storage_config.data_directory = test_dir;
    storage_config.buffer_pool_size = 16;
    auto storage = StorageEngineFactory::create(storage_config);
    ASSERT_TRUE(storage->open("btree_index_debug_db"));
    
    std::cout << "\n========= Creating BTreeIndex =========" << std::endl;
    
    // Create B+Tree
    BTreeIndexConfig btree_config;
    btree_config.min_degree = 3;
    auto btree = BTreeIndexFactory::create(storage, btree_config);
    
    std::cout << "\nRoot page ID: " << btree->root_page_id() << std::endl;
    
    // Manually check the root page
    std::cout << "\n========= Checking root page =========" << std::endl;
    PageRef root_page = storage->fetch_page(btree->root_page_id());
    ASSERT_TRUE(root_page);
    
    const char* data = static_cast<const char*>(root_page->data());
    std::cout << "\nPage data at key offsets:" << std::endl;
    std::cout << "  Offset 4 (PageType): " << std::hex << (int)(unsigned char)data[4] << std::dec << std::endl;
    std::cout << "  Offset 16 (BTreePageType): " << std::hex << (int)(unsigned char)data[16] << std::dec << std::endl;
    
    std::cout << "\n========= Attempting first insert =========" << std::endl;
    Value key(1);
    Value value("test_value");
    bool result = btree->insert(key, value);
    
    std::cout << "\nInsert result: " << result << std::endl;
    EXPECT_TRUE(result);
    
    // Clean up
    btree.reset();
    storage->close();
    storage.reset();
    std::filesystem::remove_all(test_dir);
}