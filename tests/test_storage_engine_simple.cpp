#include <gtest/gtest.h>
#include <lumen/storage/storage_engine.h>
#include <iostream>

using namespace lumen;

TEST(SimpleStorageTest, BasicOpen) {
    StorageConfig config;
    config.data_directory = "test_simple_data";
    
    auto engine = StorageEngineFactory::create(config);
    
    std::cout << "Opening database..." << std::endl;
    bool result = engine->open("simple_db");
    std::cout << "Open result: " << result << std::endl;
    
    if (!result) {
        std::cout << "Failed to open database" << std::endl;
    }
    
    EXPECT_TRUE(result);
    
    // Cleanup
    std::filesystem::remove_all(config.data_directory);
}