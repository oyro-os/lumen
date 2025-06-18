#include <lumen/storage/page.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace lumen {

Page::Page(PageID page_id)
    : data_(allocate(kPageSize), [](void* ptr) { deallocate(ptr, kPageSize); }) {
    if (!data_) {
        throw std::bad_alloc();
    }

    header_.page_id = page_id;
    header_.page_type = PageType::Data;
    header_.flags = 0;
    header_.free_space_offset = kPageSize;  // Start from end of page
    header_.free_space_size = kPageSize - PageHeader::kSize;
    header_.slot_count = 0;
    header_.checksum = 0;
    header_.lsn = 0;

    // Initialize page data
    std::memset(data_.get(), 0, kPageSize);

    // Sync header to page data and clear dirty flag
    sync_header_to_data();
    header_.flags &= ~PageFlags::kDirty;  // Clear dirty flag for new page
}

Page::~Page() = default;

SlotEntry* Page::get_slot(uint16_t slot_id) {
    if (slot_id >= header_.slot_count) {
        return nullptr;
    }
    return &slot_directory()[slot_id];
}

const SlotEntry* Page::get_slot(uint16_t slot_id) const {
    if (slot_id >= header_.slot_count) {
        return nullptr;
    }
    return &slot_directory()[slot_id];
}

SlotID Page::insert_record(const void* data, size_t size) {
    if (size == 0 || !data) {
        throw std::invalid_argument("Invalid record data");
    }

    // Check if we have enough space (including slot directory entry)
    size_t required_space = size;
    bool need_new_slot = false;
    SlotID slot_id = find_free_slot();

    if (slot_id == static_cast<SlotID>(-1)) {
        // Need to add new slot
        slot_id = header_.slot_count;
        need_new_slot = true;
        required_space += sizeof(SlotEntry);
    }

    if (available_space() < required_space) {
        // Try compaction first
        compact();
        if (available_space() < required_space) {
            return static_cast<SlotID>(-1);  // No space available
        }
    }

    // Allocate space from the end (data grows down from end of page)
    uint16_t data_offset = header_.free_space_offset - static_cast<uint16_t>(size);

    // If we need a new slot, expand the slot directory
    if (need_new_slot) {
        header_.slot_count++;
    }

    // Update slot entry
    SlotEntry* slot = get_slot(slot_id);
    slot->offset = data_offset;
    slot->length = static_cast<uint16_t>(size);

    // Copy data to page
    std::memcpy(raw_data() + data_offset, data, size);

    // Update free space tracking
    header_.free_space_offset = data_offset;
    header_.free_space_size -= static_cast<uint16_t>(size);
    if (need_new_slot) {
        header_.free_space_size -= sizeof(SlotEntry);
    }

    mark_dirty();
    update_checksum();

    return slot_id;
}

bool Page::update_record(SlotID slot_id, const void* data, size_t size) {
    SlotEntry* slot = get_slot(slot_id);
    if (!slot || slot->is_free()) {
        return false;
    }

    if (size == slot->length) {
        // Same size, can update in place
        std::memcpy(raw_data() + slot->offset, data, size);
        mark_dirty();
        update_checksum();
        return true;
    }

    // Different size, need to delete and insert
    delete_record(slot_id);
    SlotID new_slot = insert_record(data, size);

    if (new_slot == static_cast<SlotID>(-1)) {
        return false;  // Couldn't fit new record
    }

    // If we got a different slot ID, we need to handle slot reuse
    if (new_slot != slot_id) {
        // Move the new record to the old slot position
        SlotEntry* new_slot_entry = get_slot(new_slot);
        SlotEntry* old_slot_entry = get_slot(slot_id);

        *old_slot_entry = *new_slot_entry;
        new_slot_entry->mark_free();
    }

    return true;
}

bool Page::delete_record(SlotID slot_id) {
    SlotEntry* slot = get_slot(slot_id);
    if (!slot || slot->is_free()) {
        return false;
    }

    // Mark slot as free
    header_.free_space_size += slot->length;
    slot->mark_free();

    mark_dirty();
    update_checksum();

    return true;
}

const void* Page::get_record(SlotID slot_id, size_t* size) const {
    const SlotEntry* slot = get_slot(slot_id);
    if (!slot || slot->is_free()) {
        return nullptr;
    }

    if (size) {
        *size = slot->length;
    }

    return raw_data() + slot->offset;
}

void Page::serialize_to(void* buffer) const {
    // Ensure header is synced to data
    std::memcpy(data_.get(), &header_, sizeof(header_));

    // Copy entire page
    std::memcpy(buffer, data_.get(), kPageSize);
}

void Page::deserialize_from(const void* buffer) {
    // Copy page data
    std::memcpy(data_.get(), buffer, kPageSize);

    // Extract header
    sync_header_from_data();
}

uint32_t Page::calculate_checksum() const {
    // Calculate checksum excluding the checksum field itself
    // Checksum field is at offset 16 in PageHeader
    uint32_t checksum = 0;
    const char* data = raw_data();

    // Hash everything before checksum field (first 16 bytes)
    for (size_t i = 0; i < 16; ++i) {
        checksum = checksum * 31 + static_cast<uint8_t>(data[i]);
    }

    // Skip checksum field (4 bytes) and hash the rest
    for (size_t i = 20; i < kPageSize; ++i) {
        checksum = checksum * 31 + static_cast<uint8_t>(data[i]);
    }

    return checksum;
}

bool Page::verify_checksum() const {
    return header_.checksum == calculate_checksum();
}

void Page::update_checksum() {
    header_.checksum = calculate_checksum();
    sync_header_to_data();
}

void Page::compact() {
    if (header_.slot_count == 0) {
        return;  // Nothing to compact
    }

    // Collect all non-free records
    struct RecordInfo {
        SlotID slot_id;
        uint16_t size;
        std::unique_ptr<char[]> data;
    };

    std::vector<RecordInfo> records;
    records.reserve(header_.slot_count);

    for (uint16_t i = 0; i < header_.slot_count; ++i) {
        const SlotEntry* slot = get_slot(i);
        if (!slot->is_free()) {
            RecordInfo record;
            record.slot_id = i;
            record.size = slot->length;
            record.data = std::make_unique<char[]>(slot->length);
            std::memcpy(record.data.get(), raw_data() + slot->offset, slot->length);
            records.push_back(std::move(record));
        }
    }

    // Reset page data area
    header_.free_space_offset = kPageSize;
    header_.free_space_size =
        kPageSize - PageHeader::kSize - (header_.slot_count * sizeof(SlotEntry));

    // Clear all slots
    for (uint16_t i = 0; i < header_.slot_count; ++i) {
        get_slot(i)->mark_free();
    }

    // Re-insert records in compact form
    for (auto& record : records) {
        uint16_t data_offset = header_.free_space_offset - record.size;

        SlotEntry* slot = get_slot(record.slot_id);
        slot->offset = data_offset;
        slot->length = record.size;

        std::memcpy(raw_data() + data_offset, record.data.get(), record.size);

        header_.free_space_offset = data_offset;
        header_.free_space_size -= record.size;
    }

    mark_dirty();
    update_checksum();
}

size_t Page::available_space() const {
    // Space available for new records (accounting for slot directory growth)
    size_t slot_dir_space = PageHeader::kSize + (header_.slot_count * sizeof(SlotEntry));
    size_t used_data_space = kPageSize - header_.free_space_offset;
    size_t total_used = slot_dir_space + used_data_space;

    if (total_used >= kPageSize) {
        return 0;
    }

    return kPageSize - total_used;
}

void Page::defragment() {
    compact();  // For now, defragmentation is the same as compaction
}

SlotID Page::find_free_slot() {
    for (uint16_t i = 0; i < header_.slot_count; ++i) {
        if (get_slot(i)->is_free()) {
            return i;
        }
    }
    return static_cast<SlotID>(-1);  // No free slot found
}

// PageFactory implementation
std::unique_ptr<Page> PageFactory::create_page(PageID page_id, PageType type) {
    auto page = std::make_unique<Page>(page_id);
    page->set_page_type(type);
    page->update_checksum();
    // Clear dirty flag after setup
    page->mark_clean();
    return page;
}

std::unique_ptr<Page> PageFactory::load_page(PageID page_id, const void* data) {
    auto page = std::make_unique<Page>(page_id);
    page->deserialize_from(data);
    return page;
}

// Header synchronization methods
void Page::sync_header_to_data() {
    std::memcpy(data_.get(), &header_, sizeof(header_));
}

void Page::sync_header_from_data() {
    std::memcpy(&header_, data_.get(), sizeof(header_));
}

}  // namespace lumen