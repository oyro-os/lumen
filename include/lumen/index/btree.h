#ifndef LUMEN_INDEX_BTREE_H
#define LUMEN_INDEX_BTREE_H

#include <lumen/storage/page.h>
#include <lumen/storage/storage_engine.h>
#include <lumen/types.h>

#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace lumen {

// B+Tree configuration
struct BTreeConfig {
    // Minimum degree (t) - minimum number of keys in a node
    // A node can have at most 2t-1 keys
    size_t min_degree = 32;  // Good for 4KB pages (~200 keys max)

    // Comparator for keys
    std::function<int(const Value&, const Value&)> comparator = nullptr;

    // Whether to allow duplicate keys
    bool allow_duplicates = false;

    // Page size for nodes
    size_t page_size = kPageSize;

    static BTreeConfig default_config() {
        return BTreeConfig{};
    }
};

// Forward declarations
class BTreeNode;
class BTreeIterator;

// Key-Value pair for B+Tree entries
struct BTreeEntry {
    Value key;
    Value value;

    BTreeEntry() = default;
    BTreeEntry(const Value& k, const Value& v) : key(k), value(v) {}
    BTreeEntry(Value&& k, Value&& v) : key(std::move(k)), value(std::move(v)) {}
};

// B+Tree node types
enum class BTreeNodeType : uint8_t { Internal = 0, Leaf = 1 };

// B+Tree node header
struct BTreeNodeHeader {
    PageID page_id;
    BTreeNodeType node_type;
    uint16_t num_keys;
    uint16_t level;  // Height from leaf level (leaves are level 0)
    PageID parent_id;
    PageID next_id;  // For leaf nodes - linked list
    PageID prev_id;  // For leaf nodes - linked list
    uint32_t free_space;
    uint32_t checksum;

    static constexpr size_t kSize = 32;
};

// Base class for B+Tree nodes
class BTreeNode {
   public:
    BTreeNode(PageID page_id, BTreeNodeType type, size_t min_degree);
    virtual ~BTreeNode() = default;

    // Node properties
    PageID page_id() const {
        return header_.page_id;
    }
    BTreeNodeType node_type() const {
        return header_.node_type;
    }
    uint16_t num_keys() const {
        return header_.num_keys;
    }
    uint16_t level() const {
        return header_.level;
    }
    PageID parent_id() const {
        return header_.parent_id;
    }

    bool is_leaf() const {
        return header_.node_type == BTreeNodeType::Leaf;
    }
    bool is_internal() const {
        return header_.node_type == BTreeNodeType::Internal;
    }
    bool is_full() const {
        return header_.num_keys >= max_keys_;
    }
    bool is_underflow() const {
        return header_.num_keys < min_keys_;
    }
    
    size_t min_keys() const {
        return min_keys_;
    }
    
    size_t max_keys() const {
        return max_keys_;
    }

    // Key access
    const Value& key_at(size_t index) const;
    void set_key_at(size_t index, const Value& key);

    // Search
    size_t search_key(const Value& key, const BTreeConfig& config) const;

    // Key comparison
    int compare_keys(const Value& a, const Value& b, const BTreeConfig& config) const;

    // Serialization
    virtual void serialize_to(void* buffer) const = 0;
    virtual void deserialize_from(const void* buffer) = 0;

    // Lock management
    void read_lock() {
        mutex_.lock_shared();
    }
    void read_unlock() {
        mutex_.unlock_shared();
    }
    void write_lock() {
        mutex_.lock();
    }
    void write_unlock() {
        mutex_.unlock();
    }

   public:
    // Header access methods
    void set_parent_id(PageID parent_id) {
        header_.parent_id = parent_id;
    }
    void set_level(uint16_t level) {
        header_.level = level;
    }
    void set_page_id(PageID page_id) {
        header_.page_id = page_id;
    }

   protected:
    BTreeNodeHeader header_;
    std::vector<Value> keys_;
    size_t min_degree_;
    size_t min_keys_;
    size_t max_keys_;
    mutable std::shared_mutex mutex_;
};

// Internal node in B+Tree
class BTreeInternalNode : public BTreeNode {
   public:
    BTreeInternalNode(PageID page_id, size_t min_degree);

    // Child access
    PageID child_at(size_t index) const;
    void set_child_at(size_t index, PageID child_id);
    void insert_child(size_t index, PageID child_id);
    void remove_child(size_t index);

    // Key-child pair insertion
    void insert_key_child(size_t index, const Value& key, PageID child_id);

    // Split operation
    std::pair<Value, std::unique_ptr<BTreeInternalNode>> split();

    // Serialization
    void serialize_to(void* buffer) const override;
    void deserialize_from(const void* buffer) override;

   private:
    std::vector<PageID> children_;
};

// Leaf node in B+Tree
class BTreeLeafNode : public BTreeNode {
   public:
    BTreeLeafNode(PageID page_id, size_t min_degree);

    // Value access
    const Value& value_at(size_t index) const;
    void set_value_at(size_t index, const Value& value);

    // Entry operations
    bool insert_entry(const BTreeEntry& entry, const BTreeConfig& config);
    bool remove_entry(const Value& key, const BTreeConfig& config);
    std::optional<Value> find_value(const Value& key, const BTreeConfig& config) const;

    // Split operation
    std::pair<Value, std::unique_ptr<BTreeLeafNode>> split();

    // Linked list navigation
    PageID next_leaf() const {
        return header_.next_id;
    }
    PageID prev_leaf() const {
        return header_.prev_id;
    }
    void set_next_leaf(PageID next_id) {
        header_.next_id = next_id;
    }
    void set_prev_leaf(PageID prev_id) {
        header_.prev_id = prev_id;
    }

    // Serialization
    void serialize_to(void* buffer) const override;
    void deserialize_from(const void* buffer) override;

   private:
    std::vector<Value> values_;
};

// B+Tree index implementation
class BTree {
   public:
    explicit BTree(std::shared_ptr<StorageEngine> storage,
                   const BTreeConfig& config = BTreeConfig::default_config());
    ~BTree();

    // Basic operations
    bool insert(const Value& key, const Value& value);
    bool remove(const Value& key);
    std::optional<Value> find(const Value& key) const;
    bool contains(const Value& key) const;

    // Range operations
    std::vector<BTreeEntry> range_scan(const Value& start_key, const Value& end_key) const;
    std::vector<BTreeEntry> range_scan_limit(const Value& start_key, const Value& end_key,
                                             size_t limit) const;

    // Bulk operations
    bool bulk_insert(const std::vector<BTreeEntry>& entries);
    size_t bulk_remove(const std::vector<Value>& keys);

    // Tree properties
    size_t size() const {
        return size_;
    }
    size_t height() const {
        return height_;
    }
    bool empty() const {
        return size_ == 0;
    }
    PageID root_page_id() const {
        return root_page_id_;
    }

    // Iterator support
    BTreeIterator begin() const;
    BTreeIterator end() const;
    BTreeIterator find_iterator(const Value& key) const;

    // Maintenance
    void validate() const;  // Debug method to validate tree structure
    size_t node_count() const;
    double fill_factor() const;

    // Node management (public for iterator access)
    std::unique_ptr<BTreeNode> load_node(PageID page_id) const;

   private:
    std::shared_ptr<StorageEngine> storage_;
    BTreeConfig config_;
    PageID root_page_id_;
    std::atomic<size_t> size_{0};
    std::atomic<size_t> height_{0};
    mutable std::shared_mutex tree_mutex_;

    // Node management
    PageID create_node(BTreeNodeType type);
    void save_node(const BTreeNode& node);
    void delete_node(PageID page_id);

    // Internal operations
    bool insert_internal(const Value& key, const Value& value);
    bool remove_internal(const Value& key);
    std::optional<Value> find_internal(const Value& key) const;

    // Split handling
    void split_child(BTreeInternalNode* parent, size_t child_index);
    void split_root();

    // Merge handling
    void merge_nodes(BTreeInternalNode* parent, size_t child_index);
    void redistribute_keys(BTreeInternalNode* parent, size_t child_index);

    // Helper methods
    BTreeLeafNode* find_leaf_node(const Value& key) const;
    void update_parent_key(PageID node_id, const Value& old_key, const Value& new_key);
    void merge_or_redistribute_leaf(BTreeLeafNode* leaf);
    void merge_or_redistribute_internal(BTreeInternalNode* node);
    void remove_key_from_internal(BTreeInternalNode* parent, PageID child_id);
};

// B+Tree iterator for range scans
class BTreeIterator {
   public:
    BTreeIterator() = default;
    BTreeIterator(const BTree* tree, PageID leaf_id, size_t index);

    // Iterator operations
    BTreeIterator& operator++();
    BTreeIterator operator++(int);
    bool operator==(const BTreeIterator& other) const;
    bool operator!=(const BTreeIterator& other) const;

    // Dereference
    BTreeEntry operator*() const;
    const BTreeEntry* operator->() const;

    // Check validity
    bool valid() const {
        return tree_ != nullptr && current_leaf_ != nullptr;
    }

   private:
    const BTree* tree_;
    std::unique_ptr<BTreeLeafNode> current_leaf_;
    size_t current_index_;

    void advance();
    void load_next_leaf();
};

// Factory for creating B+Tree indexes
class BTreeFactory {
   public:
    static std::unique_ptr<BTree> create(std::shared_ptr<StorageEngine> storage,
                                         const BTreeConfig& config = BTreeConfig::default_config());
};

}  // namespace lumen

#endif  // LUMEN_INDEX_BTREE_H