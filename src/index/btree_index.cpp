#include <lumen/index/btree_index.h>
#include <lumen/storage/page.h>
#include <lumen/common/logging.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <stack>

namespace lumen {

BTreeIndex::BTreeIndex(std::shared_ptr<StorageEngine> storage, const BTreeIndexConfig& config)
    : storage_(std::move(storage)), config_(config) {
    if (!config_.comparator) {
        config_.comparator = [](const Value& a, const Value& b) -> int {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        };
    }

    // Create root leaf page
    root_page_id_ = create_page(BTreePageType::Leaf);
    LOG_DEBUG << "BTreeIndex constructor: created root page " << root_page_id_;
    if (root_page_id_ == kInvalidPageID) {
        throw std::runtime_error("Failed to create root page for B+Tree");
    }

    height_ = 1;
}

BTreeIndex::BTreeIndex(std::shared_ptr<StorageEngine> storage, PageID root_page_id, const BTreeIndexConfig& config)
    : storage_(std::move(storage)), config_(config), root_page_id_(root_page_id) {
    if (!config_.comparator) {
        config_.comparator = [](const Value& a, const Value& b) -> int {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        };
    }

    LOG_DEBUG << "BTreeIndex constructor: loading existing tree with root page " << root_page_id_;
    
    // Verify the root page exists and is valid
    PageRef root_page = fetch_page(root_page_id_);
    if (!root_page) {
        throw std::runtime_error("Failed to load root page for existing B+Tree");
    }
    
    // Determine height by following leftmost path to leaf
    height_ = 1;
    PageID current_page = root_page_id_;
    while (current_page != kInvalidPageID) {
        PageRef page = fetch_page(current_page);
        if (!page) break;
        
        BTreePageHeader* header = get_btree_header(page);
        if (header->node_type == BTreePageType::Leaf) {
            break;
        }
        
        // Follow first child
        current_page = get_child_page_id(page, 0);
        height_++;
    }
    
    // Count entries (this is expensive, might want to cache this)
    size_ = 0;
    auto it = begin();
    while (it != end()) {
        size_++;
        ++it;
    }
}

BTreeIndex::~BTreeIndex() = default;

bool BTreeIndex::insert(const Value& key, const Value& value) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);

    LOG_DEBUG << "BTreeIndex::insert called";

    // Find the leaf page where this key should be inserted
    PageID leaf_page_id = find_leaf_page(key);
    LOG_DEBUG << "BTreeIndex::insert: leaf_page_id = " << leaf_page_id;
    if (leaf_page_id == kInvalidPageID) {
        LOG_ERROR << "BTreeIndex::insert: leaf_page_id is invalid!";
        return false;
    }

    PageRef leaf_page = fetch_page(leaf_page_id);
    if (!leaf_page) {
        LOG_ERROR << "BTreeIndex::insert: failed to fetch leaf page!";
        return false;
    }

    LOG_DEBUG << "BTreeIndex::insert: about to call insert_into_leaf";

    // Check if page is full and needs splitting
    if (is_page_full(leaf_page)) {
        // Split the leaf page first
        PageID new_page_id = split_leaf_page(leaf_page);
        if (new_page_id == kInvalidPageID) {
            return false;
        }

        // Determine which page should receive the new key
        BTreePageHeader* header = get_btree_header(leaf_page);
        if (header->key_count > 0) {
            char* data_start = get_page_data_start(leaf_page);
            Value last_key;
            Value dummy_value;
            
            // Find the last key by walking through all entries
            size_t offset = 0;
            for (size_t i = 0; i < header->key_count; ++i) {
                if (i == header->key_count - 1) {
                    deserialize_key_value(data_start + offset, last_key, dummy_value);
                    break;
                }
                Value temp_key, temp_value;
                offset += deserialize_key_value(data_start + offset, temp_key, temp_value);
            }
            
            if (compare_keys(key, last_key) > 0) {
                // Insert into new page
                leaf_page = fetch_page(new_page_id);
                if (!leaf_page) {
                    return false;
                }
            }
        }
    }

    // Insert the key-value pair into the leaf page
    bool result = insert_into_leaf(leaf_page, key, value);
    if (result) {
        size_++;
    }

    return result;
}

bool BTreeIndex::remove(const Value& key) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);
    
    PageID leaf_page_id = find_leaf_page(key);
    if (leaf_page_id == kInvalidPageID) {
        return false;
    }

    PageRef leaf_page = fetch_page(leaf_page_id);
    if (!leaf_page) {
        return false;
    }

    BTreePageHeader* header = get_btree_header(leaf_page);
    char* data_start = get_page_data_start(leaf_page);

    // Find the key to remove
    size_t offset = 0;
    bool found = false;
    
    for (size_t i = 0; i < header->key_count; ++i) {
        Value current_key, current_value;
        size_t entry_size = deserialize_key_value(data_start + offset, current_key, current_value);
        
        if (compare_keys(current_key, key) == 0) {
            found = true;
            
            // Shift remaining entries left
            size_t remaining_size = 0;
            size_t temp_offset = offset + entry_size;
            for (size_t j = i + 1; j < header->key_count; ++j) {
                Value temp_key, temp_value;
                remaining_size += deserialize_key_value(data_start + temp_offset, temp_key, temp_value);
                temp_offset += get_key_value_size(temp_key, temp_value);
            }
            
            std::memmove(data_start + offset, data_start + offset + entry_size, remaining_size);
            
            // Update header
            header->key_count--;
            header->free_space += entry_size;
            
            leaf_page->mark_dirty();
            storage_->flush_page(leaf_page->page_id());
            size_--;
            break;
        }
        
        offset += entry_size;
    }

    return found;
}

std::optional<Value> BTreeIndex::find(const Value& key) const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    PageID leaf_page_id = find_leaf_page(key);
    if (leaf_page_id == kInvalidPageID) {
        return std::nullopt;
    }

    PageRef leaf_page = fetch_page(leaf_page_id);
    if (!leaf_page) {
        return std::nullopt;
    }

    BTreePageHeader* header = get_btree_header(leaf_page);
    char* data_start = get_page_data_start(leaf_page);

    // Binary search for the key
    size_t left = 0, right = header->key_count;
    while (left < right) {
        size_t mid = (left + right) / 2;
        
        Value mid_key, mid_value;
        size_t offset = 0;
        for (size_t i = 0; i <= mid; ++i) {
            if (i == mid) {
                deserialize_key_value(data_start + offset, mid_key, mid_value);
                break;
            }
            offset += get_key_value_size(mid_key, mid_value);
            deserialize_key_value(data_start + offset, mid_key, mid_value);
        }

        int cmp = compare_keys(mid_key, key);
        if (cmp < 0) {
            left = mid + 1;
        } else if (cmp > 0) {
            right = mid;
        } else {
            return mid_value;  // Found exact match
        }
    }

    return std::nullopt;
}

bool BTreeIndex::contains(const Value& key) const {
    return find(key).has_value();
}

std::vector<BTreeIndexEntry> BTreeIndex::range_scan(const Value& start_key, const Value& end_key) const {
    std::vector<BTreeIndexEntry> results;
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    PageID current_page_id = find_leaf_page(start_key);
    if (current_page_id == kInvalidPageID) {
        return results;
    }

    while (current_page_id != kInvalidPageID) {
        PageRef page = fetch_page(current_page_id);
        if (!page) {
            break;
        }

        BTreePageHeader* header = get_btree_header(page);
        char* data_start = get_page_data_start(page);

        size_t offset = 0;
        for (size_t i = 0; i < header->key_count; ++i) {
            Value key, value;
            offset += deserialize_key_value(data_start + offset, key, value);

            if (compare_keys(key, start_key) >= 0 && compare_keys(key, end_key) <= 0) {
                results.emplace_back(key, value);
            } else if (compare_keys(key, end_key) > 0) {
                return results;  // Past end key, stop scanning
            }
        }

        current_page_id = header->next_page;
    }

    return results;
}

std::vector<BTreeIndexEntry> BTreeIndex::range_scan_limit(const Value& start_key, const Value& end_key, size_t limit) const {
    std::vector<BTreeIndexEntry> results;
    results.reserve(limit);

    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    PageID current_page_id = find_leaf_page(start_key);
    if (current_page_id == kInvalidPageID) {
        return results;
    }

    while (current_page_id != kInvalidPageID && results.size() < limit) {
        PageRef page = fetch_page(current_page_id);
        if (!page) {
            break;
        }

        BTreePageHeader* header = get_btree_header(page);
        char* data_start = get_page_data_start(page);

        size_t offset = 0;
        for (size_t i = 0; i < header->key_count && results.size() < limit; ++i) {
            Value key, value;
            offset += deserialize_key_value(data_start + offset, key, value);

            if (compare_keys(key, start_key) >= 0 && compare_keys(key, end_key) <= 0) {
                results.emplace_back(key, value);
            } else if (compare_keys(key, end_key) > 0) {
                return results;  // Past end key, stop scanning
            }
        }

        current_page_id = header->next_page;
    }

    return results;
}

bool BTreeIndex::bulk_insert(const std::vector<BTreeIndexEntry>& entries) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);

    bool all_inserted = true;
    for (const auto& entry : entries) {
        // Unlock and relock for each insert to allow concurrent reads
        lock.unlock();
        bool result = insert(entry.key, entry.value);
        lock.lock();
        
        if (!result) {
            all_inserted = false;
        }
    }

    return all_inserted;
}

size_t BTreeIndex::bulk_remove(const std::vector<Value>& keys) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);

    size_t removed_count = 0;
    for (const auto& key : keys) {
        if (remove(key)) {
            removed_count++;
        }
    }

    return removed_count;
}

BTreeIndex::Iterator BTreeIndex::begin() const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    // Find leftmost leaf page
    PageID current_page = root_page_id_;
    
    while (true) {
        PageRef page = fetch_page(current_page);
        if (!page) {
            return end();
        }

        BTreePageHeader* header = get_btree_header(page);
        if (header->node_type == BTreePageType::Leaf) {
            return Iterator(this, current_page, 0);
        }

        // Get first child of internal node
        current_page = get_child_page_id(page, 0);
        if (current_page == kInvalidPageID) {
            return end();
        }
    }
}

BTreeIndex::Iterator BTreeIndex::end() const {
    return Iterator();
}

BTreeIndex::Iterator BTreeIndex::find_iterator(const Value& key) const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    PageID leaf_page_id = find_leaf_page(key);
    if (leaf_page_id == kInvalidPageID) {
        return end();
    }

    PageRef leaf_page = fetch_page(leaf_page_id);
    if (!leaf_page) {
        return end();
    }

    size_t key_index = search_key_in_page(leaf_page, key);
    BTreePageHeader* header = get_btree_header(leaf_page);

    if (key_index < header->key_count) {
        // Verify the key matches
        char* data_start = get_page_data_start(leaf_page);
        size_t offset = 0;
        for (size_t i = 0; i < key_index; ++i) {
            Value dummy_key, dummy_value;
            offset += deserialize_key_value(data_start + offset, dummy_key, dummy_value);
        }

        Value found_key, found_value;
        deserialize_key_value(data_start + offset, found_key, found_value);

        if (compare_keys(found_key, key) == 0) {
            return Iterator(this, leaf_page_id, key_index);
        }
    }

    return end();
}

PageID BTreeIndex::create_page(BTreePageType type) {
    // Use PageType::Index for all B+Tree pages
    // The specific B+Tree type (Internal vs Leaf) is stored in BTreePageHeader
    LOG_DEBUG << "create_page: creating page of type Index (BTreePageType " << (int)type << ")";
    
    PageRef page = storage_->new_page(PageType::Index);
    if (!page) {
        LOG_ERROR << "create_page: storage->new_page failed!";
        return kInvalidPageID;
    }

    LOG_DEBUG << "create_page: created page " << page->page_id();

    // Initialize B+Tree page header after the standard page header
    BTreePageHeader* btree_header = get_btree_header(page);
    std::memset(btree_header, 0, sizeof(BTreePageHeader));
    
    btree_header->node_type = type;
    btree_header->level = (type == BTreePageType::Leaf) ? 0 : 1;
    btree_header->key_count = 0;
    btree_header->parent_page = kInvalidPageID;
    btree_header->next_page = kInvalidPageID;
    btree_header->prev_page = kInvalidPageID;
    btree_header->free_space = kPageSize - PageHeaderV2::kSize - BTreePageHeader::kSize;

    LOG_DEBUG << "create_page: initialized header with node_type " << (int)btree_header->node_type;
    
    // Debug: Check the raw bytes
    char* raw_data = static_cast<char*>(page->data());
    LOG_DEBUG << "create_page: raw bytes at offset 16: " 
              << std::hex << std::setfill('0')
              << std::setw(2) << (unsigned)raw_data[16] << " "
              << std::setw(2) << (unsigned)raw_data[17] << " "
              << std::setw(2) << (unsigned)raw_data[18] << " "
              << std::setw(2) << (unsigned)raw_data[19] << std::dec;

    page->mark_dirty();
    LOG_DEBUG << "create_page: page marked dirty, is_dirty=" << page->is_dirty();
    
    // Debug: Check data before flush
    char* before_flush = static_cast<char*>(page->data());
    LOG_DEBUG << "create_page: BEFORE flush, raw bytes at offset 16: "
              << std::hex << std::setfill('0')
              << std::setw(2) << (unsigned)before_flush[16] << " "
              << std::setw(2) << (unsigned)before_flush[17] << " "
              << std::setw(2) << (unsigned)before_flush[18] << " "
              << std::setw(2) << (unsigned)before_flush[19] << std::dec;
    
    // Don't flush yet - keep the page in memory
    // storage_->flush_page(page->page_id());
    
    // Just return the page ID - the page will be flushed later when needed
    return page->page_id();
    
    /* // Commented out verification for now
    // Verify the header was saved correctly
    PageRef verification_page = storage_->fetch_page(page->page_id());
    if (verification_page) {
        BTreePageHeader* verify_header = get_btree_header(verification_page);
        LOG_DEBUG << "create_page: verification read shows node_type " << (int)verify_header->node_type;
        
        // Debug: Check the raw bytes in verification page
        char* verify_raw = static_cast<char*>(verification_page->data());
        LOG_DEBUG << "create_page: verification raw bytes at offset 16: "
                  << std::hex << std::setfill('0')
                  << std::setw(2) << (unsigned)verify_raw[16] << " "
                  << std::setw(2) << (unsigned)verify_raw[17] << " "
                  << std::setw(2) << (unsigned)verify_raw[18] << " "
                  << std::setw(2) << (unsigned)verify_raw[19] << std::dec;
                  
        if (verify_header->node_type != type) {
            LOG_ERROR << "create_page: verification FAILED! Expected node_type " 
                      << (int)type << " but got " << (int)verify_header->node_type;
        }
    } else {
        LOG_ERROR << "create_page: verification fetch_page FAILED for page " << page->page_id();
    }

    return page->page_id();
    */
}

PageRef BTreeIndex::fetch_page(PageID page_id) const {
    LOG_DEBUG << "BTreeIndex::fetch_page called for page_id = " << page_id;
    PageRef page = storage_->fetch_page(page_id);
    if (!page) {
        LOG_ERROR << "BTreeIndex::fetch_page failed for page_id = " << page_id;
    } else {
        // Check if it's a valid B+Tree page
        if (page->data() != nullptr) {
            BTreePageHeader* header = reinterpret_cast<BTreePageHeader*>(page->data());
            LOG_DEBUG << "BTreeIndex::fetch_page: page " << page_id 
                      << " has node_type=" << (int)header->node_type 
                      << " key_count=" << header->key_count;
        }
    }
    return page;
}

bool BTreeIndex::is_page_full(PageRef page) const {
    BTreePageHeader* header = get_btree_header(page);
    size_t max_keys = get_max_keys_per_page(header->node_type);
    return header->key_count >= max_keys;
}

bool BTreeIndex::insert_into_leaf(PageRef leaf_page, const Value& key, const Value& value) {
    BTreePageHeader* header = get_btree_header(leaf_page);
    char* data_start = get_page_data_start(leaf_page);

    // Calculate required space
    size_t entry_size = get_key_value_size(key, value);
    
    // Debug output
    LOG_DEBUG << "insert_into_leaf: key_count=" << header->key_count 
              << ", free_space=" << header->free_space 
              << ", entry_size=" << entry_size;
    
    if (header->free_space < entry_size) {
        LOG_DEBUG << "insert_into_leaf: Not enough space! free_space=" << header->free_space 
                  << " < entry_size=" << entry_size;
        return false;  // Not enough space
    }

    // Find the offset where to insert at the end
    size_t insert_offset = 0;
    for (size_t i = 0; i < header->key_count; ++i) {
        Value dummy_key, dummy_value;
        insert_offset += deserialize_key_value(data_start + insert_offset, dummy_key, dummy_value);
    }

    LOG_DEBUG << "insert_into_leaf: inserting at offset " << insert_offset;

    // Insert the new entry at the end
    serialize_key_value(data_start + insert_offset, key, value);

    // Update header
    header->key_count++;
    header->free_space -= entry_size;

    LOG_DEBUG << "insert_into_leaf: success! new key_count=" << header->key_count 
              << ", new free_space=" << header->free_space;

    leaf_page->mark_dirty();
    storage_->flush_page(leaf_page->page_id());

    return true;
}

PageID BTreeIndex::split_leaf_page(PageRef leaf_page) {
    BTreePageHeader* old_header = get_btree_header(leaf_page);
    
    // Create new leaf page
    PageID new_page_id = create_page(BTreePageType::Leaf);
    if (new_page_id == kInvalidPageID) {
        return kInvalidPageID;
    }

    PageRef new_page = fetch_page(new_page_id);
    if (!new_page) {
        return kInvalidPageID;
    }

    BTreePageHeader* new_header = get_btree_header(new_page);
    char* old_data = get_page_data_start(leaf_page);
    char* new_data = get_page_data_start(new_page);

    // Split point (move roughly half the keys to new page)
    size_t split_point = old_header->key_count / 2;

    // Move entries from split_point onwards to new page
    size_t move_offset = 0;
    for (size_t i = 0; i < split_point; ++i) {
        Value key, value;
        move_offset += deserialize_key_value(old_data + move_offset, key, value);
    }

    size_t remaining_size = 0;
    size_t temp_offset = move_offset;
    for (size_t i = split_point; i < old_header->key_count; ++i) {
        Value key, value;
        remaining_size += deserialize_key_value(old_data + temp_offset, key, value);
        temp_offset += get_key_value_size(key, value);
    }

    // Copy data to new page
    std::memcpy(new_data, old_data + move_offset, remaining_size);

    // Update headers
    new_header->key_count = old_header->key_count - split_point;
    new_header->parent_page = old_header->parent_page;
    new_header->next_page = old_header->next_page;
    new_header->prev_page = leaf_page->page_id();
    new_header->free_space = kPageSize - PageHeaderV2::kSize - BTreePageHeader::kSize - remaining_size;

    old_header->key_count = split_point;
    old_header->next_page = new_page_id;
    old_header->free_space = kPageSize - PageHeaderV2::kSize - BTreePageHeader::kSize - move_offset;

    // Update next page's prev pointer if it exists
    if (new_header->next_page != kInvalidPageID) {
        PageRef next_page = fetch_page(new_header->next_page);
        if (next_page) {
            BTreePageHeader* next_header = get_btree_header(next_page);
            next_header->prev_page = new_page_id;
            next_page->mark_dirty();
            storage_->flush_page(next_page->page_id());
        }
    }

    // Mark pages dirty and flush
    leaf_page->mark_dirty();
    new_page->mark_dirty();
    storage_->flush_page(leaf_page->page_id());
    storage_->flush_page(new_page_id);

    // Handle parent update
    if (old_header->parent_page == kInvalidPageID) {
        // This is the root, need to create new root
        split_root();
    } else {
        // Get the first key from new page for parent update
        Value split_key, dummy_value;
        deserialize_key_value(new_data, split_key, dummy_value);
        update_parent_after_split(old_header->parent_page, leaf_page->page_id(), new_page_id, split_key);
    }

    return new_page_id;
}

void BTreeIndex::split_root() {
    PageID old_root_id = root_page_id_;
    PageRef old_root = fetch_page(old_root_id);
    if (!old_root) {
        return;
    }

    BTreePageHeader* old_root_header = get_btree_header(old_root);

    // Create new root page
    PageID new_root_id = create_page(BTreePageType::Internal);
    if (new_root_id == kInvalidPageID) {
        return;
    }

    PageRef new_root = fetch_page(new_root_id);
    if (!new_root) {
        return;
    }

    BTreePageHeader* new_root_header = get_btree_header(new_root);
    new_root_header->level = old_root_header->level + 1;

    // The old root becomes the first child of the new root
    char* new_root_data = get_page_data_start(new_root);
    set_child_page_id(new_root, 0, old_root_id);

    // Update old root's parent
    old_root_header->parent_page = new_root_id;

    // Update tree properties
    root_page_id_ = new_root_id;
    height_++;

    // Mark dirty and flush
    old_root->mark_dirty();
    new_root->mark_dirty();
    storage_->flush_page(old_root_id);
    storage_->flush_page(new_root_id);
}

PageID BTreeIndex::find_leaf_page(const Value& key) const {
    PageID current_page = root_page_id_;
    LOG_DEBUG << "find_leaf_page: starting with root_page_id = " << current_page;

    while (current_page != kInvalidPageID) {
        PageRef page = fetch_page(current_page);
        if (!page) {
            LOG_ERROR << "find_leaf_page: failed to fetch page " << current_page;
            return kInvalidPageID;
        }

        BTreePageHeader* header = get_btree_header(page);
        LOG_DEBUG << "find_leaf_page: page " << current_page << " has node_type " << (int)header->node_type;
        
        if (header->node_type == BTreePageType::Leaf) {
            LOG_DEBUG << "find_leaf_page: found leaf page " << current_page;
            return current_page;
        }

        // Internal node - find child to follow
        size_t child_index = search_key_in_page(page, key);
        current_page = get_child_page_id(page, child_index);
        LOG_DEBUG << "find_leaf_page: following child " << current_page << " at index " << child_index;
    }

    LOG_DEBUG << "find_leaf_page: returning kInvalidPageID";
    return kInvalidPageID;
}

size_t BTreeIndex::search_key_in_page(PageRef page, const Value& key) const {
    BTreePageHeader* header = get_btree_header(page);
    char* data_start = get_page_data_start(page);

    size_t left = 0, right = header->key_count;
    
    while (left < right) {
        size_t mid = (left + right) / 2;
        
        // Find the key at position mid
        size_t offset = 0;
        Value mid_key, mid_value;
        for (size_t i = 0; i <= mid; ++i) {
            if (i == mid) {
                deserialize_key_value(data_start + offset, mid_key, mid_value);
                break;
            }
            offset += deserialize_key_value(data_start + offset, mid_key, mid_value);
        }

        int cmp = compare_keys(mid_key, key);
        if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return left;
}

PageID BTreeIndex::get_child_page_id(PageRef internal_page, size_t index) const {
    BTreePageHeader* header = get_btree_header(internal_page);
    if (index > header->key_count) {
        return kInvalidPageID;
    }

    char* data_start = get_page_data_start(internal_page);
    
    // Skip to the child pointers section (after all keys)
    size_t data_offset = 0;
    for (size_t i = 0; i < header->key_count; ++i) {
        Value key, dummy_value;
        data_offset += deserialize_key_value(data_start + data_offset, key, dummy_value);
    }

    // Child pointers start after all key-value pairs
    PageID* child_pointers = reinterpret_cast<PageID*>(data_start + data_offset);
    return child_pointers[index];
}

void BTreeIndex::set_child_page_id(PageRef internal_page, size_t index, PageID child_id) {
    BTreePageHeader* header = get_btree_header(internal_page);
    if (index > header->key_count) {
        return;
    }

    char* data_start = get_page_data_start(internal_page);
    
    // Skip to the child pointers section
    size_t data_offset = 0;
    for (size_t i = 0; i < header->key_count; ++i) {
        Value key, dummy_value;
        data_offset += deserialize_key_value(data_start + data_offset, key, dummy_value);
    }

    PageID* child_pointers = reinterpret_cast<PageID*>(data_start + data_offset);
    child_pointers[index] = child_id;
    
    internal_page->mark_dirty();
}

size_t BTreeIndex::serialize_key_value(char* buffer, const Value& key, const Value& value) const {
    size_t offset = 0;
    
    key.serialize(reinterpret_cast<byte*>(buffer + offset));
    offset += key.serializedSize();
    
    value.serialize(reinterpret_cast<byte*>(buffer + offset));
    offset += value.serializedSize();
    
    return offset;
}

size_t BTreeIndex::deserialize_key_value(const char* buffer, Value& key, Value& value) const {
    size_t key_offset = 0;
    size_t value_offset = 0;
    
    key = Value::deserialize(reinterpret_cast<const byte*>(buffer), key_offset);
    value = Value::deserialize(reinterpret_cast<const byte*>(buffer + key_offset), value_offset);
    
    return key_offset + value_offset;
}

size_t BTreeIndex::get_key_value_size(const Value& key, const Value& value) const {
    return key.serializedSize() + value.serializedSize();
}

void BTreeIndex::update_parent_after_split(PageID parent_id, PageID left_child, PageID right_child, const Value& split_key) {
    PageRef parent_page = fetch_page(parent_id);
    if (!parent_page) {
        return;
    }

    // Check if parent is full
    if (is_page_full(parent_page)) {
        // Split parent first
        PageID new_parent_id = split_internal_page(parent_page);
        if (new_parent_id == kInvalidPageID) {
            return;
        }
        
        // Determine which parent should receive the new key
        if (compare_keys(split_key, get_first_key_from_page(parent_page)) > 0) {
            parent_page = fetch_page(new_parent_id);
            if (!parent_page) {
                return;
            }
        }
    }

    // Insert split key and right child pointer into parent
    insert_into_internal(parent_page, split_key, right_child);
}

bool BTreeIndex::insert_into_internal(PageRef internal_page, const Value& key, PageID child_page) {
    BTreePageHeader* header = get_btree_header(internal_page);
    char* data_start = get_page_data_start(internal_page);

    // Calculate required space for key + child pointer
    size_t key_size = key.serializedSize();
    size_t total_size = key_size + sizeof(PageID);
    
    if (header->free_space < total_size) {
        return false;  // Not enough space
    }

    // Find insertion position
    size_t insert_pos = search_key_in_page(internal_page, key);

    // For internal nodes, we store keys followed by child pointers
    // Layout: [key0][key1]...[keyN][child0][child1]...[childN+1]
    
    // Insert key at correct position
    size_t key_offset = 0;
    for (size_t i = 0; i < insert_pos; ++i) {
        key_offset += deserialize_key_for_internal(data_start + key_offset).serializedSize();
    }

    // Shift existing keys
    if (insert_pos < header->key_count) {
        size_t remaining_key_size = 0;
        size_t temp_offset = key_offset;
        for (size_t i = insert_pos; i < header->key_count; ++i) {
            remaining_key_size += deserialize_key_for_internal(data_start + temp_offset).serializedSize();
            temp_offset += deserialize_key_for_internal(data_start + temp_offset).serializedSize();
        }
        
        std::memmove(data_start + key_offset + key_size, data_start + key_offset, remaining_key_size);
    }

    // Insert the key
    key.serialize(reinterpret_cast<byte*>(data_start + key_offset));

    // Update child pointers - they come after all keys
    size_t child_area_offset = 0;
    for (size_t i = 0; i < header->key_count + 1; ++i) {
        if (i < header->key_count) {
            child_area_offset += deserialize_key_for_internal(data_start + child_area_offset).serializedSize();
        }
    }

    // Shift child pointers
    if (insert_pos + 1 <= header->key_count) {
        size_t remaining_children = (header->key_count + 1) - (insert_pos + 1);
        std::memmove(data_start + child_area_offset + sizeof(PageID) * (insert_pos + 2),
                     data_start + child_area_offset + sizeof(PageID) * (insert_pos + 1),
                     remaining_children * sizeof(PageID));
    }

    // Insert the child pointer
    PageID* child_pointers = reinterpret_cast<PageID*>(data_start + child_area_offset);
    child_pointers[insert_pos + 1] = child_page;

    // Update header
    header->key_count++;
    header->free_space -= total_size;

    internal_page->mark_dirty();
    storage_->flush_page(internal_page->page_id());

    return true;
}

PageID BTreeIndex::split_internal_page(PageRef internal_page) {
    BTreePageHeader* old_header = get_btree_header(internal_page);
    
    // Create new internal page
    PageID new_page_id = create_page(BTreePageType::Internal);
    if (new_page_id == kInvalidPageID) {
        return kInvalidPageID;
    }

    PageRef new_page = fetch_page(new_page_id);
    if (!new_page) {
        return kInvalidPageID;
    }

    BTreePageHeader* new_header = get_btree_header(new_page);
    char* old_data = get_page_data_start(internal_page);
    char* new_data = get_page_data_start(new_page);

    // Split point (move roughly half the keys to new page)
    size_t split_point = old_header->key_count / 2;

    // Move keys from split_point onwards to new page
    size_t move_offset = 0;
    for (size_t i = 0; i < split_point; ++i) {
        move_offset += deserialize_key_for_internal(old_data + move_offset).serializedSize();
    }

    size_t remaining_key_size = 0;
    size_t temp_offset = move_offset;
    for (size_t i = split_point; i < old_header->key_count; ++i) {
        remaining_key_size += deserialize_key_for_internal(old_data + temp_offset).serializedSize();
        temp_offset += deserialize_key_for_internal(old_data + temp_offset).serializedSize();
    }

    // Copy keys to new page
    std::memcpy(new_data, old_data + move_offset, remaining_key_size);

    // Copy child pointers
    size_t child_area_offset = temp_offset;
    size_t children_to_move = (old_header->key_count - split_point + 1);
    std::memcpy(new_data + remaining_key_size, 
                old_data + child_area_offset + sizeof(PageID) * split_point,
                children_to_move * sizeof(PageID));

    // Update headers
    new_header->key_count = old_header->key_count - split_point;
    new_header->parent_page = old_header->parent_page;
    new_header->level = old_header->level;
    new_header->free_space = kPageSize - PageHeaderV2::kSize - BTreePageHeader::kSize - 
                            remaining_key_size - children_to_move * sizeof(PageID);

    old_header->key_count = split_point;
    old_header->free_space = kPageSize - PageHeaderV2::kSize - BTreePageHeader::kSize - 
                            move_offset - (split_point + 1) * sizeof(PageID);

    // Mark pages dirty and flush
    internal_page->mark_dirty();
    new_page->mark_dirty();
    storage_->flush_page(internal_page->page_id());
    storage_->flush_page(new_page_id);

    return new_page_id;
}

Value BTreeIndex::get_first_key_from_page(PageRef page) const {
    BTreePageHeader* header = get_btree_header(page);
    if (header->key_count == 0) {
        return Value();  // Empty value
    }

    char* data_start = get_page_data_start(page);
    
    if (header->node_type == BTreePageType::Leaf) {
        Value key, value;
        deserialize_key_value(data_start, key, value);
        return key;
    } else {
        return deserialize_key_for_internal(data_start);
    }
}

Value BTreeIndex::deserialize_key_for_internal(const char* buffer) const {
    size_t offset = 0;
    return Value::deserialize(reinterpret_cast<const byte*>(buffer), offset);
}

int BTreeIndex::compare_keys(const Value& a, const Value& b) const {
    return config_.comparator(a, b);
}

BTreePageHeader* BTreeIndex::get_btree_header(PageRef page) const {
    char* page_data = static_cast<char*>(page->data());
    BTreePageHeader* header = reinterpret_cast<BTreePageHeader*>(page_data + PageHeaderV2::kSize);
    LOG_DEBUG << "get_btree_header: page " << page->page_id() 
              << ", offset=" << PageHeaderV2::kSize 
              << ", node_type=" << (int)header->node_type;
    return header;
}

char* BTreeIndex::get_page_data_start(PageRef page) const {
    char* page_data = static_cast<char*>(page->data());
    return page_data + PageHeaderV2::kSize + BTreePageHeader::kSize;
}

size_t BTreeIndex::get_max_keys_per_page(BTreePageType type) const {
    // Conservative estimate based on average key/value sizes
    size_t available_space = kPageSize - PageHeaderV2::kSize - BTreePageHeader::kSize;
    
    if (type == BTreePageType::Internal) {
        // Internal nodes store keys and child pointers
        available_space -= sizeof(PageID);  // Reserve space for child pointers
    }
    
    // Assume average key+value size of 64 bytes
    return available_space / 64;
}

// Iterator implementation
BTreeIndex::Iterator::Iterator(const BTreeIndex* tree, PageID leaf_page, size_t index)
    : tree_(tree), current_page_(leaf_page), current_index_(index) {}

BTreeIndex::Iterator& BTreeIndex::Iterator::operator++() {
    advance();
    return *this;
}

BTreeIndex::Iterator BTreeIndex::Iterator::operator++(int) {
    Iterator tmp = *this;
    advance();
    return tmp;
}

bool BTreeIndex::Iterator::operator==(const Iterator& other) const {
    if (!valid() && !other.valid()) {
        return true;
    }
    return valid() && other.valid() && 
           current_page_ == other.current_page_ && 
           current_index_ == other.current_index_;
}

bool BTreeIndex::Iterator::operator!=(const Iterator& other) const {
    return !(*this == other);
}

BTreeIndexEntry BTreeIndex::Iterator::operator*() const {
    if (!valid()) {
        return BTreeIndexEntry();
    }

    PageRef page = tree_->fetch_page(current_page_);
    if (!page) {
        return BTreeIndexEntry();
    }

    BTreePageHeader* header = tree_->get_btree_header(page);
    if (current_index_ >= header->key_count) {
        return BTreeIndexEntry();
    }

    char* data_start = tree_->get_page_data_start(page);
    size_t offset = 0;
    
    for (size_t i = 0; i < current_index_; ++i) {
        Value dummy_key, dummy_value;
        offset += tree_->deserialize_key_value(data_start + offset, dummy_key, dummy_value);
    }

    Value key, value;
    tree_->deserialize_key_value(data_start + offset, key, value);
    
    return BTreeIndexEntry(key, value);
}

const BTreeIndexEntry* BTreeIndex::Iterator::operator->() const {
    current_entry_ = **this;
    return &current_entry_;
}

void BTreeIndex::Iterator::advance() {
    if (!valid()) {
        return;
    }

    current_index_++;
    
    PageRef page = tree_->fetch_page(current_page_);
    if (!page) {
        current_page_ = kInvalidPageID;
        return;
    }

    BTreePageHeader* header = tree_->get_btree_header(page);
    if (current_index_ >= header->key_count) {
        load_next_page();
    }
}

void BTreeIndex::Iterator::load_next_page() {
    if (!valid()) {
        return;
    }

    PageRef page = tree_->fetch_page(current_page_);
    if (!page) {
        current_page_ = kInvalidPageID;
        return;
    }

    BTreePageHeader* header = tree_->get_btree_header(page);
    current_page_ = header->next_page;
    current_index_ = 0;

    if (current_page_ == kInvalidPageID) {
        // End of iteration
        return;
    }
}

// Factory implementation
std::unique_ptr<BTreeIndex> BTreeIndexFactory::create(std::shared_ptr<StorageEngine> storage,
                                                      const BTreeIndexConfig& config) {
    return std::make_unique<BTreeIndex>(std::move(storage), config);
}

}  // namespace lumen