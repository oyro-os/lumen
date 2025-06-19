#include <lumen/index/btree_index.h>
#include <lumen/storage/storage_engine.h>
#include <lumen/common/logging.h>
#include <iostream>
#include <filesystem>

using namespace lumen;

int main() {
    // Enable debug logging
    SET_LOG_LEVEL(LogLevel::DEBUG);
    
    // Create storage
    StorageConfig storage_config;
    storage_config.data_directory = "test_btree_creation";
    auto storage = StorageEngineFactory::create(storage_config);
    if (!storage->open("btree_creation_db")) {
        std::cerr << "Failed to open storage" << std::endl;
        return 1;
    }
    
    std::cout << "Creating B+Tree..." << std::endl;
    
    // Create B+Tree
    BTreeIndexConfig btree_config;
    btree_config.min_degree = 3;
    auto btree = BTreeIndexFactory::create(storage, btree_config);
    
    std::cout << "B+Tree created with root page ID: " << btree->root_page_id() << std::endl;
    
    // Try to fetch the root page directly
    std::cout << "\nFetching root page directly..." << std::endl;
    PageRef root_page = storage->fetch_page(btree->root_page_id());
    if (root_page) {
        std::cout << "Root page fetched successfully" << std::endl;
        std::cout << "Page ID: " << root_page->page_id() << std::endl;
        std::cout << "Page type: " << (int)root_page->page_type() << std::endl;
        
        // Check the B+Tree header
        char* page_data = static_cast<char*>(root_page->data());
        BTreePageHeader* btree_header = reinterpret_cast<BTreePageHeader*>(page_data + 16);
        std::cout << "BTree node_type: " << (int)btree_header->node_type << std::endl;
        std::cout << "BTree key_count: " << btree_header->key_count << std::endl;
    } else {
        std::cout << "Failed to fetch root page!" << std::endl;
    }
    
    std::cout << "\nTrying first insert..." << std::endl;
    Value key(1);
    Value value("test_value");
    bool result = btree->insert(key, value);
    std::cout << "Insert result: " << result << std::endl;
    
    // Clean up
    btree.reset();
    storage->close();
    storage.reset();
    std::filesystem::remove_all("test_btree_creation");
    
    return result ? 0 : 1;
}