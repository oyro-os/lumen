#include <gtest/gtest.h>
#include <lumen/storage/page.h>

#include <cstring>
#include <thread>
#include <vector>

using namespace lumen;

class PageTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PageTest, PageCreation) {
    auto page = PageFactory::create_page(100, PageType::Data);

    EXPECT_EQ(page->page_id(), 100u);
    EXPECT_EQ(page->page_type(), PageType::Data);
    EXPECT_FALSE(page->is_dirty());
    EXPECT_FALSE(page->is_pinned());
    EXPECT_FALSE(page->is_locked());
    EXPECT_FALSE(page->is_deleted());
    EXPECT_EQ(page->slot_count(), 0u);
    EXPECT_GT(page->free_space_size(), 0u);
}

TEST_F(PageTest, RecordInsertion) {
    auto page = PageFactory::create_page(1, PageType::Data);

    const char* test_data = "Hello, Lumen!";
    size_t data_size = std::strlen(test_data);

    SlotID slot_id = page->insert_record(test_data, data_size);
    EXPECT_NE(slot_id, static_cast<SlotID>(-1));
    EXPECT_EQ(page->slot_count(), 1u);
    EXPECT_TRUE(page->is_dirty());

    // Verify record can be retrieved
    size_t retrieved_size;
    const void* retrieved_data = page->get_record(slot_id, &retrieved_size);
    ASSERT_NE(retrieved_data, nullptr);
    EXPECT_EQ(retrieved_size, data_size);
    EXPECT_EQ(std::memcmp(retrieved_data, test_data, data_size), 0);
}

TEST_F(PageTest, MultipleRecords) {
    auto page = PageFactory::create_page(2, PageType::Data);

    std::vector<std::string> test_records = {
        "Record 1", "This is record number 2", "Short",
        "A much longer record that contains more data than the others"};

    std::vector<SlotID> slot_ids;

    // Insert all records
    for (const auto& record : test_records) {
        SlotID slot_id = page->insert_record(record.c_str(), record.size());
        EXPECT_NE(slot_id, static_cast<SlotID>(-1));
        slot_ids.push_back(slot_id);
    }

    EXPECT_EQ(page->slot_count(), test_records.size());

    // Verify all records
    for (size_t i = 0; i < test_records.size(); ++i) {
        size_t size;
        const void* data = page->get_record(slot_ids[i], &size);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(size, test_records[i].size());
        EXPECT_EQ(std::memcmp(data, test_records[i].c_str(), size), 0);
    }
}

TEST_F(PageTest, RecordUpdate) {
    auto page = PageFactory::create_page(3, PageType::Data);

    const char* original_data = "Original";
    SlotID slot_id = page->insert_record(original_data, std::strlen(original_data));

    // Update with same size
    const char* updated_data = "Modified";
    EXPECT_TRUE(page->update_record(slot_id, updated_data, std::strlen(updated_data)));

    size_t size;
    const void* data = page->get_record(slot_id, &size);
    EXPECT_EQ(size, std::strlen(updated_data));
    EXPECT_EQ(std::memcmp(data, updated_data, size), 0);

    // Update with different size
    const char* longer_data = "This is a much longer update";
    EXPECT_TRUE(page->update_record(slot_id, longer_data, std::strlen(longer_data)));

    data = page->get_record(slot_id, &size);
    EXPECT_EQ(size, std::strlen(longer_data));
    EXPECT_EQ(std::memcmp(data, longer_data, size), 0);
}

TEST_F(PageTest, RecordDeletion) {
    auto page = PageFactory::create_page(4, PageType::Data);

    const char* test_data = "To be deleted";
    SlotID slot_id = page->insert_record(test_data, std::strlen(test_data));

    EXPECT_TRUE(page->delete_record(slot_id));
    EXPECT_EQ(page->get_record(slot_id), nullptr);

    // Deleting again should fail
    EXPECT_FALSE(page->delete_record(slot_id));
}

TEST_F(PageTest, PageCompaction) {
    auto page = PageFactory::create_page(5, PageType::Data);

    // Insert several records
    std::vector<SlotID> slot_ids;
    for (int i = 0; i < 10; ++i) {
        std::string data = "Record " + std::to_string(i);
        SlotID slot_id = page->insert_record(data.c_str(), data.size());
        slot_ids.push_back(slot_id);
    }

    size_t free_space_before_deletion = page->free_space_size();

    // Delete every other record
    for (size_t i = 1; i < slot_ids.size(); i += 2) {
        page->delete_record(slot_ids[i]);
    }

    size_t free_space_after_deletion = page->free_space_size();
    EXPECT_GT(free_space_after_deletion, free_space_before_deletion);

    // Compact the page
    page->compact();

    // Verify remaining records are still accessible
    for (size_t i = 0; i < slot_ids.size(); i += 2) {
        std::string expected_data = "Record " + std::to_string(i);
        size_t size;
        const void* data = page->get_record(slot_ids[i], &size);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(size, expected_data.size());
        EXPECT_EQ(std::memcmp(data, expected_data.c_str(), size), 0);
    }
}

TEST_F(PageTest, PageSerialization) {
    auto original_page = PageFactory::create_page(6, PageType::Index);

    // Insert some test data
    const char* test_data = "Serialization test data";
    SlotID slot_id = original_page->insert_record(test_data, std::strlen(test_data));

    // Serialize page
    std::vector<char> buffer(kPageSize);
    original_page->serialize_to(buffer.data());

    // Create new page and deserialize
    auto loaded_page = PageFactory::load_page(6, buffer.data());

    EXPECT_EQ(loaded_page->page_id(), original_page->page_id());
    EXPECT_EQ(loaded_page->page_type(), original_page->page_type());
    EXPECT_EQ(loaded_page->slot_count(), original_page->slot_count());

    // Verify data integrity
    size_t size;
    const void* data = loaded_page->get_record(slot_id, &size);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(size, std::strlen(test_data));
    EXPECT_EQ(std::memcmp(data, test_data, size), 0);
}

TEST_F(PageTest, ChecksumVerification) {
    auto page = PageFactory::create_page(7, PageType::Data);

    // Test basic checksum functionality
    uint32_t checksum1 = page->calculate_checksum();
    uint32_t checksum2 = page->calculate_checksum();
    EXPECT_EQ(checksum1, checksum2);  // Consistent calculation

    // Modify page and verify checksum changes
    const char* test_data = "Checksum test";
    page->insert_record(test_data, std::strlen(test_data));
    uint32_t checksum3 = page->calculate_checksum();
    EXPECT_NE(checksum1, checksum3);  // Checksum should change after modification
}

TEST_F(PageTest, PageLocking) {
    auto page = PageFactory::create_page(8, PageType::Data);

    // Test read lock
    {
        PageReadLock lock(*page);
        // Should be able to read
        EXPECT_EQ(page->page_id(), 8u);
    }

    // Test write lock
    {
        PageWriteLock lock(*page);
        // Should be able to write
        const char* test_data = "Lock test";
        page->insert_record(test_data, std::strlen(test_data));
    }
}

TEST_F(PageTest, PageReference) {
    auto page = std::make_shared<Page>(9);

    PageRef ref(page);
    EXPECT_TRUE(ref);
    EXPECT_EQ(ref->page_id(), 9u);
    EXPECT_EQ(ref.get(), page.get());

    ref.reset();
    EXPECT_FALSE(ref);
    EXPECT_EQ(ref.get(), nullptr);
}

TEST_F(PageTest, LargeRecordHandling) {
    auto page = PageFactory::create_page(10, PageType::Data);

    // Try to insert a record that's too large for the page
    std::vector<char> large_data(kPageSize, 'A');
    SlotID slot_id = page->insert_record(large_data.data(), large_data.size());
    EXPECT_EQ(slot_id, static_cast<SlotID>(-1));  // Should fail

    // Insert maximum possible record
    size_t max_record_size = page->free_space_size() - sizeof(SlotEntry);
    std::vector<char> max_data(max_record_size, 'B');
    slot_id = page->insert_record(max_data.data(), max_data.size());
    EXPECT_NE(slot_id, static_cast<SlotID>(-1));  // Should succeed
}

TEST_F(PageTest, SlotReuse) {
    auto page = PageFactory::create_page(11, PageType::Data);

    // Insert and delete a record
    const char* test_data = "Temporary";
    SlotID slot_id = page->insert_record(test_data, std::strlen(test_data));
    page->delete_record(slot_id);

    // Insert another record - should reuse the slot
    const char* new_data = "Reused slot";
    SlotID new_slot_id = page->insert_record(new_data, std::strlen(new_data));
    EXPECT_EQ(new_slot_id, slot_id);  // Should reuse the same slot
}