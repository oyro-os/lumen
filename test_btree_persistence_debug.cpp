#include <lumen/index/btree_index.h>
#include <lumen/storage/storage_engine.h>
#include <lumen/common/logging.h>
#include <iostream>

using namespace lumen;

int main() {
    // Enable debug logging
    SET_LOG_LEVEL(LogLevel::DEBUG);
    
    std::cout << "Creating storage engine..." << std::endl;
    
    // Set up storage
    StorageConfig storage_config;
    storage_config.data_directory = "test_btree_persistence_debug";
    storage_config.buffer_pool_size = 64;
    auto storage = StorageEngineFactory::create(storage_config);
    
    if (!storage->open("btree_debug_db")) {
        std::cerr << "Failed to open storage" << std::endl;
        return 1;
    }
    
    std::cout << "Creating B+Tree..." << std::endl;
    
    // Create B+Tree
    BTreeIndexConfig btree_config;
    btree_config.min_degree = 3;
    auto btree = BTreeIndexFactory::create(storage, btree_config);
    
    std::cout << "Inserting first key..." << std::endl;
    
    // Insert one key
    Value key(1);
    Value value("test_value_1");
    bool result = btree->insert(key, value);
    
    std::cout << "Insert result: " << result << std::endl;
    
    if (!result) {
        std::cerr << "First insert failed!" << std::endl;
        return 1;
    }
    
    std::cout << "Flushing pages..." << std::endl;
    storage->flush_all_pages();
    
    std::cout << "Resetting buffer pool..." << std::endl;
    storage->buffer_pool()->reset();
    
    std::cout << "Trying to find key after reset..." << std::endl;
    auto found = btree->find(key);
    
    if (found.has_value()) {
        std::cout << "Found value: " << found->getString() << std::endl;
    } else {
        std::cout << "Key not found after buffer pool reset!" << std::endl;
    }
    
    std::cout << "Trying to insert second key after reset..." << std::endl;
    Value key2(2);
    Value value2("test_value_2");
    bool result2 = btree->insert(key2, value2);
    
    std::cout << "Second insert result: " << result2 << std::endl;
    
    // Clean up
    btree.reset();
    storage->close();
    storage.reset();
    
    // Remove test directory
    std::error_code ec;
    std::filesystem::remove_all("test_btree_persistence_debug", ec);
    
    return 0;
}