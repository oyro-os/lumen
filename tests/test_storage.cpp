#include <gtest/gtest.h>
#include <lumen/lumen.h>

class StorageTest : public ::testing::Test {
   protected:
    void TearDown() override {
        if (storage) {
            lumen_storage_destroy(storage);
            storage = nullptr;
        }
    }

    LumenStorage storage = nullptr;
};

// Test basic storage creation and destruction
TEST_F(StorageTest, CreateMemoryStorage) {
    storage = lumen_storage_create(":memory:");
    ASSERT_NE(nullptr, storage);
}

TEST_F(StorageTest, DestroyStorage) {
    storage = lumen_storage_create(":memory:");
    ASSERT_NE(nullptr, storage);

    LumenResult result = lumen_storage_destroy(storage);
    EXPECT_EQ(LUMEN_OK, result);
    storage = nullptr;  // Prevent double-free in TearDown
}

TEST_F(StorageTest, CloseStorage) {
    storage = lumen_storage_create(":memory:");
    ASSERT_NE(nullptr, storage);

    LumenResult result = lumen_storage_close(storage);
    EXPECT_EQ(LUMEN_OK, result);
}

// Test basic database operations
TEST_F(StorageTest, CreateDatabase) {
    storage = lumen_storage_create(":memory:");
    ASSERT_NE(nullptr, storage);

    LumenDatabase db = lumen_database_create(storage, "testdb");
    ASSERT_NE(nullptr, db);

    EXPECT_EQ(LUMEN_OK, lumen_database_destroy(db));
}

TEST_F(StorageTest, CreateMultipleDatabases) {
    storage = lumen_storage_create(":memory:");
    ASSERT_NE(nullptr, storage);

    LumenDatabase db1 = lumen_database_create(storage, "db1");
    LumenDatabase db2 = lumen_database_create(storage, "db2");

    ASSERT_NE(nullptr, db1);
    ASSERT_NE(nullptr, db2);
    EXPECT_NE(db1, db2);

    EXPECT_EQ(LUMEN_OK, lumen_database_destroy(db1));
    EXPECT_EQ(LUMEN_OK, lumen_database_destroy(db2));
}
