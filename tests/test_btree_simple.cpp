#include <gtest/gtest.h>
#include <lumen/index/btree.h>
#include <lumen/storage/storage_engine.h>

#include <filesystem>
#include <iomanip>
#include <iostream>

using namespace lumen;

TEST(BTreeSimpleTest, NodeSerialization) {
    // Create test directory
    std::string test_dir =
        "test_btree_simple_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    // Set up storage
    StorageConfig storage_config;
    storage_config.data_directory = test_dir;
    storage_config.buffer_pool_size = 16;
    auto storage = StorageEngineFactory::create(storage_config);
    ASSERT_TRUE(storage->open("btree_simple_db"));

    // Create a simple leaf node
    BTreeLeafNode leaf(1, 3);  // page_id=1, min_degree=3

    std::cout << "Initial leaf state:" << std::endl;
    std::cout << "  page_id: " << leaf.page_id() << std::endl;
    std::cout << "  is_leaf: " << leaf.is_leaf() << std::endl;
    std::cout << "  num_keys: " << leaf.num_keys() << std::endl;
    std::cout << "  level: " << leaf.level() << std::endl;

    // Serialize to buffer
    char buffer[16384];
    std::memset(buffer, 0xFF, sizeof(buffer));  // Fill with 0xFF to see what gets written
    leaf.serialize_to(buffer);

    // Print first 64 bytes of serialized data
    std::cout << "\nSerialized data (first 64 bytes):" << std::endl;
    for (int i = 0; i < 64; i++) {
        if (i % 16 == 0)
            std::cout << std::setw(4) << i << ": ";
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)buffer[i]
                  << " ";
        if (i % 16 == 15)
            std::cout << std::endl;
    }
    std::cout << std::dec << std::endl;

    // Print the BTreeNodeHeader struct
    BTreeNodeHeader* header = reinterpret_cast<BTreeNodeHeader*>(buffer);
    std::cout << "\nHeader fields:" << std::endl;
    std::cout << "  page_id: " << header->page_id << std::endl;
    std::cout << "  node_type: " << (int)header->node_type << std::endl;
    std::cout << "  num_keys: " << header->num_keys << std::endl;
    std::cout << "  level: " << header->level << std::endl;
    std::cout << "  parent_id: " << header->parent_id << std::endl;
    std::cout << "  next_id: " << header->next_id << std::endl;
    std::cout << "  prev_id: " << header->prev_id << std::endl;
    std::cout << "  free_space: " << header->free_space << std::endl;
    std::cout << "  checksum: " << header->checksum << std::endl;

    // Deserialize back
    BTreeLeafNode leaf2(2, 3);  // Different page_id to see if it changes
    leaf2.deserialize_from(buffer);

    std::cout << "\nDeserialized leaf state:" << std::endl;
    std::cout << "  page_id: " << leaf2.page_id() << std::endl;
    std::cout << "  is_leaf: " << leaf2.is_leaf() << std::endl;
    std::cout << "  num_keys: " << leaf2.num_keys() << std::endl;
    std::cout << "  level: " << leaf2.level() << std::endl;

    // Clean up
    storage->close();
    std::filesystem::remove_all(test_dir);
}