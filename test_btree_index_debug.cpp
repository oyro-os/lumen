#include <lumen/index/btree_index.h>
#include <lumen/storage/storage_engine.h>
#include <lumen/common/logging.h>
#include <iostream>
#include <iomanip>

using namespace lumen;

int main() {
    // Enable debug logging
    SET_LOG_LEVEL(LogLevel::DEBUG);
    
    // Create test directory
    std::string test_dir = "test_btree_index_debug";
    
    // Set up storage
    StorageConfig storage_config;
    storage_config.data_directory = test_dir;
    storage_config.buffer_pool_size = 16;
    auto storage = StorageEngineFactory::create(storage_config);
    
    std::cout << "Opening storage engine..." << std::endl;
    if (!storage->open("btree_index_debug_db")) {
        std::cerr << "Failed to open storage" << std::endl;
        return 1;
    }
    
    std::cout << "\nCreating BTreeIndex..." << std::endl;
    
    // Create B+Tree
    BTreeIndexConfig btree_config;
    btree_config.min_degree = 3;
    auto btree = BTreeIndexFactory::create(storage, btree_config);
    
    std::cout << "\nBTreeIndex created with root page ID: " << btree->root_page_id() << std::endl;
    
    // Flush and fetch root page
    storage->flush_all_pages();
    
    std::cout << "\nFetching root page directly..." << std::endl;
    PageRef root_page = storage->fetch_page(btree->root_page_id());
    if (root_page) {
        std::cout << "Root page fetched successfully" << std::endl;
        std::cout << "Page ID: " << root_page->page_id() << std::endl;
        std::cout << "Page type: " << (int)root_page->page_type() << std::endl;
        
        // Print raw page data
        const char* data = static_cast<const char*>(root_page->data());
        std::cout << "\nPage data (first 64 bytes):" << std::endl;
        for (int i = 0; i < 64; i += 16) {
            std::cout << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
            for (int j = 0; j < 16 && i + j < 64; j++) {
                std::cout << std::setw(2) << (unsigned)(unsigned char)data[i + j] << " ";
            }
            std::cout << std::endl;
        }
        
        // Check the B+Tree header at offset 16
        BTreePageHeader* btree_header = reinterpret_cast<BTreePageHeader*>(const_cast<char*>(data) + 16);
        std::cout << "\nBTreePageHeader at offset 16:" << std::endl;
        std::cout << "  node_type: " << std::hex << (int)btree_header->node_type 
                  << " (expected Leaf=" << (int)BTreePageType::Leaf << ")" << std::dec << std::endl;
        std::cout << "  level: " << (int)btree_header->level << std::endl;
        std::cout << "  key_count: " << btree_header->key_count << std::endl;
    } else {
        std::cout << "Failed to fetch root page!" << std::endl;
    }
    
    // Clean up
    btree.reset();
    storage->close();
    storage.reset();
    std::filesystem::remove_all(test_dir);
    
    return 0;
}