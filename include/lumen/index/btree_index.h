#ifndef LUMEN_INDEX_BTREE_INDEX_H
#define LUMEN_INDEX_BTREE_INDEX_H

#include <lumen/storage/page.h>
#include <lumen/storage/storage_engine.h>
#include <lumen/storage/single_file_storage.h>
#include <lumen/types.h>

#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace lumen {

// B+Tree page types using V2 format (from storage-format.md)
enum class BTreePageType : uint8_t {
    Internal = 0x04,  // BTREE_INTERNAL
    Leaf = 0x05       // BTREE_LEAF
};

// B+Tree page header (stored after PageHeaderV2)
struct BTreePageHeader {
    BTreePageType node_type;
    uint8_t level;           // Height from leaf (leaves are level 0)
    uint16_t key_count;      // Number of keys in this node
    PageID parent_page;      // Parent node page ID
    PageID next_page;        // Next sibling (for leaves, forms linked list)
    PageID prev_page;        // Previous sibling (for leaves)
    uint32_t free_space;     // Free space in page
    uint32_t reserved;       // Reserved for future use

    static constexpr size_t kSize = 24;
};

static_assert(sizeof(BTreePageHeader) == BTreePageHeader::kSize, "BTreePageHeader size mismatch");

// B+Tree configuration
struct BTreeIndexConfig {
    size_t min_degree = 32;  // Minimum degree for B+Tree
    std::function<int(const Value&, const Value&)> comparator = nullptr;
    bool allow_duplicates = false;

    static BTreeIndexConfig default_config() {
        return BTreeIndexConfig{};
    }
};

// Entry for B+Tree operations
struct BTreeIndexEntry {
    Value key;
    Value value;

    BTreeIndexEntry() = default;
    BTreeIndexEntry(const Value& k, const Value& v) : key(k), value(v) {}
    BTreeIndexEntry(Value&& k, Value&& v) : key(std::move(k)), value(std::move(v)) {}
};

// Direct page manipulation B+Tree implementation
class BTreeIndex {
   public:
    explicit BTreeIndex(std::shared_ptr<StorageEngine> storage,
                        const BTreeIndexConfig& config = BTreeIndexConfig::default_config());
    
    // Constructor to load existing B+Tree from a given root page
    BTreeIndex(std::shared_ptr<StorageEngine> storage, PageID root_page_id,
               const BTreeIndexConfig& config = BTreeIndexConfig::default_config());
    
    ~BTreeIndex();

    // Basic operations
    bool insert(const Value& key, const Value& value);
    bool remove(const Value& key);
    std::optional<Value> find(const Value& key) const;
    bool contains(const Value& key) const;

    // Range operations
    std::vector<BTreeIndexEntry> range_scan(const Value& start_key, const Value& end_key) const;
    std::vector<BTreeIndexEntry> range_scan_limit(const Value& start_key, const Value& end_key,
                                                  size_t limit) const;

    // Bulk operations
    bool bulk_insert(const std::vector<BTreeIndexEntry>& entries);
    size_t bulk_remove(const std::vector<Value>& keys);

    // Tree properties
    size_t size() const { return size_; }
    size_t height() const { return height_; }
    bool empty() const { return size_ == 0; }
    PageID root_page_id() const { return root_page_id_; }

    // Iterator support
    class Iterator;
    Iterator begin() const;
    Iterator end() const;
    Iterator find_iterator(const Value& key) const;

   private:
    std::shared_ptr<StorageEngine> storage_;
    BTreeIndexConfig config_;
    PageID root_page_id_;
    std::atomic<size_t> size_{0};
    std::atomic<size_t> height_{1};
    mutable std::shared_mutex tree_mutex_;

    // Page management
    PageID create_page(BTreePageType type);
    PageRef fetch_page(PageID page_id) const;

    // Direct page operations
    bool is_page_full(PageRef page) const;
    bool insert_into_leaf(PageRef leaf_page, const Value& key, const Value& value);
    bool insert_into_internal(PageRef internal_page, const Value& key, PageID child_page);
    PageID split_leaf_page(PageRef leaf_page);
    PageID split_internal_page(PageRef internal_page);
    PageID find_leaf_page(const Value& key) const;
    Value get_first_key_from_page(PageRef page) const;

    // Key operations on pages
    size_t search_key_in_page(PageRef page, const Value& key) const;
    PageID get_child_page_id(PageRef internal_page, size_t index) const;
    void set_child_page_id(PageRef internal_page, size_t index, PageID child_id);

    // Serialization helpers
    size_t serialize_key_value(char* buffer, const Value& key, const Value& value) const;
    size_t deserialize_key_value(const char* buffer, Value& key, Value& value) const;
    size_t get_key_value_size(const Value& key, const Value& value) const;
    Value deserialize_key_for_internal(const char* buffer) const;

    // Tree structure maintenance
    void split_root();
    void update_parent_after_split(PageID parent_id, PageID left_child, PageID right_child, const Value& split_key);

    // Helper methods
    int compare_keys(const Value& a, const Value& b) const;
    BTreePageHeader* get_btree_header(PageRef page) const;
    char* get_page_data_start(PageRef page) const;
    size_t get_max_keys_per_page(BTreePageType type) const;
};

// B+Tree iterator implementation
class BTreeIndex::Iterator {
   public:
    Iterator() = default;
    Iterator(const BTreeIndex* tree, PageID leaf_page, size_t index);

    // Iterator operations
    Iterator& operator++();
    Iterator operator++(int);
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

    // Dereference
    BTreeIndexEntry operator*() const;
    const BTreeIndexEntry* operator->() const;

    bool valid() const { return tree_ != nullptr && current_page_ != kInvalidPageID; }

   private:
    const BTreeIndex* tree_;
    PageID current_page_;
    size_t current_index_;
    mutable BTreeIndexEntry current_entry_;

    void advance();
    void load_next_page();
};

// Factory for creating B+Tree indexes
class BTreeIndexFactory {
   public:
    static std::unique_ptr<BTreeIndex> create(
        std::shared_ptr<StorageEngine> storage,
        const BTreeIndexConfig& config = BTreeIndexConfig::default_config());
};

}  // namespace lumen

#endif  // LUMEN_INDEX_BTREE_INDEX_H