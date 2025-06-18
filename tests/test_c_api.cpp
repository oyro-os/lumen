#include <gtest/gtest.h>
#include <lumen/lumen.h>

// Test initialization and shutdown
TEST(CApiTest, InitializeAndShutdown) {
    // Already initialized in test_main.cpp, just verify we can call shutdown
    lumen_shutdown();
    
    // Re-initialize
    EXPECT_EQ(LUMEN_OK, lumen_initialize());
}

// Test version string
TEST(CApiTest, VersionString) {
    const char* version = lumen_version_string();
    ASSERT_NE(nullptr, version);
    EXPECT_STREQ("0.1.0", version);  // Match actual version
}

// Test error messages
TEST(CApiTest, ErrorMessages) {
    EXPECT_STREQ("No error", lumen_error_message(LUMEN_OK));  // Match actual message
    EXPECT_STREQ("Invalid argument", lumen_error_message(LUMEN_ERROR_INVALID_ARGUMENT));
    EXPECT_STREQ("Out of memory", lumen_error_message(LUMEN_ERROR_OUT_OF_MEMORY));
    EXPECT_STREQ("File not found", lumen_error_message(LUMEN_ERROR_FILE_NOT_FOUND));
}

// Test value creation helpers
TEST(CApiTest, CreateValues) {
    // Test null value
    LumenValue null_val = lumen_value_null();
    EXPECT_EQ(LUMEN_TYPE_NULL, null_val.type);
    
    // Test int32 value
    LumenValue int32_val = lumen_value_int32(42);
    EXPECT_EQ(LUMEN_TYPE_INT32, int32_val.type);
    EXPECT_EQ(42, int32_val.value.i32);
    
    // Test int64 value
    LumenValue int64_val = lumen_value_int64(1234567890L);
    EXPECT_EQ(LUMEN_TYPE_INT64, int64_val.type);
    EXPECT_EQ(1234567890L, int64_val.value.i64);
    
    // Test double value
    LumenValue double_val = lumen_value_double(3.14159);
    EXPECT_EQ(LUMEN_TYPE_DOUBLE, double_val.type);
    EXPECT_DOUBLE_EQ(3.14159, double_val.value.f64);
    
    // Test string value
    const char* test_str = "Hello, Lumen!";
    LumenValue string_val = lumen_value_string(test_str);
    EXPECT_EQ(LUMEN_TYPE_STRING, string_val.type);
    EXPECT_EQ(test_str, string_val.value.string.data);
    EXPECT_EQ(13, string_val.value.string.length);
    
    // Test boolean value
    LumenValue bool_val = lumen_value_boolean(true);
    EXPECT_EQ(LUMEN_TYPE_BOOLEAN, bool_val.type);
    EXPECT_TRUE(bool_val.value.boolean);
}

// Test basic query builder
TEST(CApiTest, QueryBuilder) {
    LumenStorage storage = lumen_storage_create(":memory:");
    ASSERT_NE(nullptr, storage);
    
    LumenDatabase db = lumen_database_create(storage, "testdb");
    ASSERT_NE(nullptr, db);
    
    // Create query builder
    LumenQueryBuilder query = lumen_query_create(db, "users");
    ASSERT_NE(nullptr, query);
    
    // Test to_sql (for debugging)
    char* sql = lumen_query_to_sql(query);
    ASSERT_NE(nullptr, sql);
    EXPECT_STREQ("SELECT * FROM users", sql);  // No semicolon in our implementation
    lumen_free_string(sql);
    
    // Clean up
    EXPECT_EQ(LUMEN_OK, lumen_query_destroy(query));
    EXPECT_EQ(LUMEN_OK, lumen_database_destroy(db));
    EXPECT_EQ(LUMEN_OK, lumen_storage_destroy(storage));
}