#include <lumen/index/btree.h>

#include <algorithm>
#include <cstring>
#include <stack>

namespace lumen {

// BTreeNode implementation
BTreeNode::BTreeNode(PageID page_id, BTreeNodeType type, size_t min_degree)
    : min_degree_(min_degree),
      min_keys_(type == BTreeNodeType::Leaf ? min_degree - 1 : min_degree),
      max_keys_(2 * min_degree - 1) {
    std::memset(&header_, 0, sizeof(header_));
    header_.page_id = page_id;
    header_.node_type = type;
    header_.num_keys = 0;
    header_.level = 0;
    header_.parent_id = kInvalidPageID;
    header_.next_id = kInvalidPageID;
    header_.prev_id = kInvalidPageID;
    header_.free_space = kPageSize - BTreeNodeHeader::kSize;

    keys_.reserve(max_keys_);
}

const Value& BTreeNode::key_at(size_t index) const {
    if (index >= header_.num_keys) {
        static Value empty_value;
        return empty_value;
    }
    return keys_[index];
}

void BTreeNode::set_key_at(size_t index, const Value& key) {
    if (index < keys_.size()) {
        keys_[index] = key;
    }
}

size_t BTreeNode::search_key(const Value& key, const BTreeConfig& config) const {
    // Binary search for the key
    size_t left = 0;
    size_t right = header_.num_keys;

    while (left < right) {
        size_t mid = (left + right) / 2;
        int cmp = compare_keys(keys_[mid], key, config);

        if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return left;
}

int BTreeNode::compare_keys(const Value& a, const Value& b, const BTreeConfig& config) const {
    if (config.comparator) {
        return config.comparator(a, b);
    }

    // Default comparison
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

// BTreeInternalNode implementation
BTreeInternalNode::BTreeInternalNode(PageID page_id, size_t min_degree)
    : BTreeNode(page_id, BTreeNodeType::Internal, min_degree) {
    children_.reserve(max_keys_ + 1);
}

PageID BTreeInternalNode::child_at(size_t index) const {
    if (index > header_.num_keys) {
        return kInvalidPageID;
    }
    return children_[index];
}

void BTreeInternalNode::set_child_at(size_t index, PageID child_id) {
    if (index <= header_.num_keys) {
        if (index >= children_.size()) {
            children_.resize(index + 1, kInvalidPageID);
        }
        children_[index] = child_id;
    }
}

void BTreeInternalNode::insert_child(size_t index, PageID child_id) {
    if (index <= header_.num_keys + 1) {
        if (children_.size() <= header_.num_keys + 1) {
            children_.resize(header_.num_keys + 2, kInvalidPageID);
        }
        children_.insert(children_.begin() + index, child_id);
    }
}

void BTreeInternalNode::remove_child(size_t index) {
    if (index <= header_.num_keys) {
        children_.erase(children_.begin() + index);
    }
}

void BTreeInternalNode::insert_key_child(size_t index, const Value& key, PageID child_id) {
    if (index <= header_.num_keys) {
        keys_.insert(keys_.begin() + index, key);
        insert_child(index + 1, child_id);
        header_.num_keys++;
    }
}

std::pair<Value, std::unique_ptr<BTreeInternalNode>> BTreeInternalNode::split() {
    size_t mid_index = header_.num_keys / 2;
    Value mid_key = keys_[mid_index];

    // Create new node for right half
    auto new_node = std::make_unique<BTreeInternalNode>(kInvalidPageID, min_degree_);
    new_node->header_.level = header_.level;
    new_node->set_parent_id(header_.parent_id);

    // Move keys and children to new node
    for (size_t i = mid_index + 1; i < header_.num_keys; ++i) {
        new_node->keys_.push_back(std::move(keys_[i]));
    }

    for (size_t i = mid_index + 1; i <= header_.num_keys; ++i) {
        new_node->children_.push_back(children_[i]);
    }

    new_node->header_.num_keys = new_node->keys_.size();

    // Remove moved keys and children from this node
    keys_.resize(mid_index);
    children_.resize(mid_index + 1);
    header_.num_keys = keys_.size();

    return {mid_key, std::move(new_node)};
}

void BTreeInternalNode::serialize_to(void* buffer) const {
    char* ptr = static_cast<char*>(buffer);

    // Write header
    std::memcpy(ptr, &header_, sizeof(header_));
    ptr += sizeof(header_);

    // Write keys
    for (size_t i = 0; i < header_.num_keys; ++i) {
        keys_[i].serialize(reinterpret_cast<byte*>(ptr));
        ptr += keys_[i].serializedSize();
    }

    // Write children
    for (size_t i = 0; i <= header_.num_keys; ++i) {
        std::memcpy(ptr, &children_[i], sizeof(PageID));
        ptr += sizeof(PageID);
    }
}

void BTreeInternalNode::deserialize_from(const void* buffer) {
    const char* ptr = static_cast<const char*>(buffer);

    // Read header
    std::memcpy(&header_, ptr, sizeof(header_));
    ptr += sizeof(header_);

    // Read keys
    keys_.clear();
    keys_.reserve(header_.num_keys);
    for (size_t i = 0; i < header_.num_keys; ++i) {
        Value key;
        size_t offset = 0;
        keys_.push_back(Value::deserialize(reinterpret_cast<const byte*>(ptr), offset));
        ptr += offset;
    }

    // Read children
    children_.clear();
    children_.reserve(header_.num_keys + 1);
    for (size_t i = 0; i <= header_.num_keys; ++i) {
        PageID child_id;
        std::memcpy(&child_id, ptr, sizeof(PageID));
        children_.push_back(child_id);
        ptr += sizeof(PageID);
    }
}

// BTreeLeafNode implementation
BTreeLeafNode::BTreeLeafNode(PageID page_id, size_t min_degree)
    : BTreeNode(page_id, BTreeNodeType::Leaf, min_degree) {
    values_.reserve(max_keys_);
}

const Value& BTreeLeafNode::value_at(size_t index) const {
    if (index >= header_.num_keys) {
        static Value empty_value;
        return empty_value;
    }
    return values_[index];
}

void BTreeLeafNode::set_value_at(size_t index, const Value& value) {
    if (index < values_.size()) {
        values_[index] = value;
    }
}

bool BTreeLeafNode::insert_entry(const BTreeEntry& entry, const BTreeConfig& config) {
    // Find insertion position
    size_t pos = search_key(entry.key, config);

    // Check for duplicate if not allowed
    if (!config.allow_duplicates && pos < header_.num_keys) {
        if (compare_keys(keys_[pos], entry.key, config) == 0) {
            return false;  // Duplicate key
        }
    }

    // If duplicates are allowed, insert after existing duplicates
    if (config.allow_duplicates) {
        while (pos < header_.num_keys && compare_keys(keys_[pos], entry.key, config) == 0) {
            pos++;
        }
    }

    // Insert key and value
    keys_.insert(keys_.begin() + pos, entry.key);
    values_.insert(values_.begin() + pos, entry.value);
    header_.num_keys++;

    return true;
}

bool BTreeLeafNode::remove_entry(const Value& key, const BTreeConfig& config) {
    size_t pos = search_key(key, config);

    if (pos >= header_.num_keys || compare_keys(keys_[pos], key, config) != 0) {
        return false;  // Key not found
    }

    // Remove key and value
    keys_.erase(keys_.begin() + pos);
    values_.erase(values_.begin() + pos);
    header_.num_keys--;

    return true;
}

std::optional<Value> BTreeLeafNode::find_value(const Value& key, const BTreeConfig& config) const {
    size_t pos = search_key(key, config);

    if (pos < header_.num_keys && compare_keys(keys_[pos], key, config) == 0) {
        return values_[pos];
    }

    return std::nullopt;
}

std::pair<Value, std::unique_ptr<BTreeLeafNode>> BTreeLeafNode::split() {
    size_t mid_index = header_.num_keys / 2;

    // Create new node for right half
    auto new_node = std::make_unique<BTreeLeafNode>(kInvalidPageID, min_degree_);
    new_node->set_parent_id(header_.parent_id);

    // Copy keys and values to new node (don't use move)
    for (size_t i = mid_index; i < header_.num_keys; ++i) {
        new_node->keys_.push_back(keys_[i]);
        new_node->values_.push_back(values_[i]);
    }

    new_node->header_.num_keys = new_node->keys_.size();

    // Remove moved keys and values from this node
    keys_.resize(mid_index);
    values_.resize(mid_index);
    header_.num_keys = keys_.size();

    // Update linked list pointers (will be fixed when new node gets page_id)
    new_node->set_next_leaf(header_.next_id);
    new_node->set_prev_leaf(header_.page_id);

    // Return the first key of the new node
    return {new_node->keys_[0], std::move(new_node)};
}

void BTreeLeafNode::serialize_to(void* buffer) const {
    char* ptr = static_cast<char*>(buffer);

    // Write header
    std::memcpy(ptr, &header_, sizeof(header_));
    ptr += sizeof(header_);

    // Write key-value pairs
    for (size_t i = 0; i < header_.num_keys; ++i) {
        keys_[i].serialize(reinterpret_cast<byte*>(ptr));
        ptr += keys_[i].serializedSize();
        values_[i].serialize(reinterpret_cast<byte*>(ptr));
        ptr += values_[i].serializedSize();
    }
}

void BTreeLeafNode::deserialize_from(const void* buffer) {
    const char* ptr = static_cast<const char*>(buffer);

    // Read header
    std::memcpy(&header_, ptr, sizeof(header_));
    ptr += sizeof(header_);

    // Read key-value pairs
    keys_.clear();
    values_.clear();
    keys_.reserve(header_.num_keys);
    values_.reserve(header_.num_keys);

    for (size_t i = 0; i < header_.num_keys; ++i) {
        size_t key_offset = 0;
        Value key = Value::deserialize(reinterpret_cast<const byte*>(ptr), key_offset);
        ptr += key_offset;

        size_t value_offset = 0;
        Value value = Value::deserialize(reinterpret_cast<const byte*>(ptr), value_offset);
        ptr += value_offset;

        keys_.push_back(std::move(key));
        values_.push_back(std::move(value));
    }
}

// BTree implementation
BTree::BTree(std::shared_ptr<StorageEngine> storage, const BTreeConfig& config)
    : storage_(std::move(storage)), config_(config) {
    if (!config_.comparator) {
        // Set default comparator
        config_.comparator = [](const Value& a, const Value& b) -> int {
            if (a < b)
                return -1;
            if (a > b)
                return 1;
            return 0;
        };
    }

    // Create root node
    root_page_id_ = create_node(BTreeNodeType::Leaf);
    if (root_page_id_ == kInvalidPageID) {
        throw std::runtime_error("Failed to create root node");
    }

    // Initialize and save the empty root leaf node
    auto root_node = std::make_unique<BTreeLeafNode>(root_page_id_, config_.min_degree);
    save_node(*root_node);

    height_ = 1;
}

BTree::~BTree() = default;

bool BTree::insert(const Value& key, const Value& value) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);
    return insert_internal(key, value);
}

bool BTree::remove(const Value& key) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);
    return remove_internal(key);
}

std::optional<Value> BTree::find(const Value& key) const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);
    return find_internal(key);
}

bool BTree::contains(const Value& key) const {
    return find(key).has_value();
}

std::vector<BTreeEntry> BTree::range_scan(const Value& start_key, const Value& end_key) const {
    std::vector<BTreeEntry> results;
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    // Find starting leaf
    BTreeLeafNode* leaf = find_leaf_node(start_key);
    if (!leaf) {
        return results;
    }

    // Find starting position in leaf
    size_t pos = leaf->search_key(start_key, config_);

    // Scan through leaves
    while (leaf) {
        for (size_t i = pos; i < leaf->num_keys(); ++i) {
            const Value& key = leaf->key_at(i);

            // Check if we've passed the end key
            if (config_.comparator(key, end_key) > 0) {
                delete leaf;
                return results;
            }

            results.emplace_back(key, leaf->value_at(i));
        }

        // Move to next leaf
        PageID next_id = leaf->next_leaf();
        delete leaf;

        if (next_id == kInvalidPageID) {
            break;
        }

        leaf = dynamic_cast<BTreeLeafNode*>(load_node(next_id).release());
        pos = 0;  // Start from beginning of next leaf
    }

    return results;
}

std::vector<BTreeEntry> BTree::range_scan_limit(const Value& start_key, const Value& end_key,
                                                size_t limit) const {
    std::vector<BTreeEntry> results;
    results.reserve(limit);

    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    // Find starting leaf
    BTreeLeafNode* leaf = find_leaf_node(start_key);
    if (!leaf) {
        return results;
    }

    // Find starting position in leaf
    size_t pos = leaf->search_key(start_key, config_);

    // Scan through leaves
    while (leaf && results.size() < limit) {
        for (size_t i = pos; i < leaf->num_keys() && results.size() < limit; ++i) {
            const Value& key = leaf->key_at(i);

            // Check if we've passed the end key
            if (config_.comparator(key, end_key) > 0) {
                delete leaf;
                return results;
            }

            results.emplace_back(key, leaf->value_at(i));
        }

        // Move to next leaf
        PageID next_id = leaf->next_leaf();
        delete leaf;

        if (next_id == kInvalidPageID) {
            break;
        }

        leaf = dynamic_cast<BTreeLeafNode*>(load_node(next_id).release());
        pos = 0;  // Start from beginning of next leaf
    }

    return results;
}

bool BTree::bulk_insert(const std::vector<BTreeEntry>& entries) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);

    bool all_inserted = true;
    for (const auto& entry : entries) {
        if (!insert_internal(entry.key, entry.value)) {
            all_inserted = false;
        }
    }

    return all_inserted;
}

size_t BTree::bulk_remove(const std::vector<Value>& keys) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex_);

    size_t removed_count = 0;
    for (const auto& key : keys) {
        if (remove_internal(key)) {
            removed_count++;
        }
    }

    return removed_count;
}

BTreeIterator BTree::begin() const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    // Find leftmost leaf
    PageID current_id = root_page_id_;
    std::unique_ptr<BTreeNode> node = load_node(current_id);

    while (!node->is_leaf()) {
        auto* internal = dynamic_cast<BTreeInternalNode*>(node.get());
        current_id = internal->child_at(0);
        node = load_node(current_id);
    }

    return BTreeIterator(this, current_id, 0);
}

BTreeIterator BTree::end() const {
    return BTreeIterator();
}

BTreeIterator BTree::find_iterator(const Value& key) const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex_);

    BTreeLeafNode* leaf = find_leaf_node(key);
    if (!leaf) {
        return end();
    }

    size_t pos = leaf->search_key(key, config_);
    PageID leaf_id = leaf->page_id();

    // Check if key is found before deleting leaf
    bool found = (pos < leaf->num_keys() && config_.comparator(leaf->key_at(pos), key) == 0);
    delete leaf;

    if (found) {
        return BTreeIterator(this, leaf_id, pos);
    }

    return end();
}

std::unique_ptr<BTreeNode> BTree::load_node(PageID page_id) const {
    if (page_id == kInvalidPageID) {
        return nullptr;
    }

    PageRef page = storage_->fetch_page(page_id);
    if (!page) {
        return nullptr;
    }

    // Read node type from page data
    const char* page_data = static_cast<const char*>(page->data());
    const char* node_data = page_data + PageHeader::kSize;  // Skip page header

    // Check page type
    if (page->page_type() != PageType::Index) {
        return nullptr;  // Not an index page
    }

    BTreeNodeHeader header;
    std::memcpy(&header, node_data, sizeof(header));

    std::unique_ptr<BTreeNode> node;
    if (header.node_type == BTreeNodeType::Internal) {
        node = std::make_unique<BTreeInternalNode>(page_id, config_.min_degree);
    } else {
        node = std::make_unique<BTreeLeafNode>(page_id, config_.min_degree);
    }

    node->deserialize_from(node_data);

    return node;
}

PageID BTree::create_node(BTreeNodeType type) {
    PageRef page = storage_->new_page(PageType::Index);
    if (!page) {
        return kInvalidPageID;
    }

    PageID page_id = page->page_id();

    // Create and serialize empty node to ensure valid initial state
    std::unique_ptr<BTreeNode> node;
    if (type == BTreeNodeType::Internal) {
        node = std::make_unique<BTreeInternalNode>(page_id, config_.min_degree);
    } else {
        node = std::make_unique<BTreeLeafNode>(page_id, config_.min_degree);
    }

    // Serialize to page
    char* page_data = static_cast<char*>(page->data());
    char* node_data = page_data + PageHeader::kSize;
    std::memset(node_data, 0, kPageSize - PageHeader::kSize);
    node->serialize_to(node_data);
    page->mark_dirty();

    return page_id;
}

void BTree::save_node(const BTreeNode& node) {
    PageRef page = storage_->fetch_page(node.page_id());
    if (!page) {
        return;
    }

    // Serialize node to page (after PageHeader)
    char* page_data = static_cast<char*>(page->data());
    char* node_data = page_data + PageHeader::kSize;
    node.serialize_to(node_data);
    page->mark_dirty();

    // Force flush to ensure persistence
    storage_->flush_page(node.page_id());
}

void BTree::delete_node(PageID page_id) {
    storage_->delete_page(page_id);
}

bool BTree::insert_internal(const Value& key, const Value& value) {
    // Start from root
    std::unique_ptr<BTreeNode> node = load_node(root_page_id_);
    if (!node) {
        return false;
    }

    // If root is full, split it
    if (node->is_full()) {
        split_root();
        node = load_node(root_page_id_);
        if (!node) {
            return false;
        }
    }

    // Find leaf node for insertion
    std::stack<PageID> path;
    while (!node->is_leaf()) {
        path.push(node->page_id());

        auto* internal = dynamic_cast<BTreeInternalNode*>(node.get());
        size_t child_index = node->search_key(key, config_);

        // Adjust for B+Tree navigation
        if (child_index < node->num_keys() &&
            node->compare_keys(key, node->key_at(child_index), config_) >= 0) {
            child_index++;
        }

        PageID child_id = internal->child_at(child_index);

        // Check if child is full
        std::unique_ptr<BTreeNode> child = load_node(child_id);
        if (child->is_full()) {
            split_child(internal, child_index);
            save_node(*internal);

            // Key might have moved, search again
            if (config_.comparator(key, internal->key_at(child_index)) >= 0) {
                child_index++;
            }
            child_id = internal->child_at(child_index);
        }

        node = load_node(child_id);
    }

    // Insert into leaf
    auto* leaf = dynamic_cast<BTreeLeafNode*>(node.get());
    bool result = leaf->insert_entry(BTreeEntry(key, value), config_);

    if (result) {
        save_node(*leaf);
        size_++;
    }

    return result;
}

bool BTree::remove_internal(const Value& key) {
    BTreeLeafNode* leaf = find_leaf_node(key);
    if (!leaf) {
        return false;
    }

    bool result = leaf->remove_entry(key, config_);
    if (result) {
        save_node(*leaf);
        size_--;
        
        // Handle underflow if necessary
        if (leaf->is_underflow() && leaf->page_id() != root_page_id_) {
            // Handle underflow by merging or redistributing with sibling
            merge_or_redistribute_leaf(leaf);
        }
    }

    delete leaf;
    return result;
}

void BTree::merge_or_redistribute_leaf(BTreeLeafNode* leaf) {
    PageID parent_id = leaf->parent_id();
    if (parent_id == kInvalidPageID) {
        return; // Root node, nothing to do
    }

    // Load parent
    auto parent_node = load_node(parent_id);
    if (!parent_node || !parent_node->is_internal()) {
        return;
    }

    auto* parent = dynamic_cast<BTreeInternalNode*>(parent_node.get());
    
    // Find this leaf's position in parent
    size_t leaf_index = 0;
    for (size_t i = 0; i <= parent->num_keys(); ++i) {
        if (parent->child_at(i) == leaf->page_id()) {
            leaf_index = i;
            break;
        }
    }
    
    // Try to borrow from or merge with siblings
    if (leaf_index > 0) {
        // Try left sibling
        PageID left_sibling_id = parent->child_at(leaf_index - 1);
        auto left_sibling_node = load_node(left_sibling_id);
        if (left_sibling_node && left_sibling_node->is_leaf()) {
            auto* left_sibling = dynamic_cast<BTreeLeafNode*>(left_sibling_node.get());
            
            if (left_sibling->num_keys() > left_sibling->min_keys()) {
                // Borrow from left sibling
                Value last_key = left_sibling->key_at(left_sibling->num_keys() - 1);
                Value last_value = left_sibling->value_at(left_sibling->num_keys() - 1);
                
                // Remove from left sibling
                left_sibling->remove_entry(last_key, config_);
                
                // Insert into leaf
                leaf->insert_entry(BTreeEntry(last_key, last_value), config_);
                
                // Update parent key
                parent->set_key_at(leaf_index - 1, leaf->key_at(0));
                
                save_node(*left_sibling);
                save_node(*leaf);
                save_node(*parent);
                return;
            }
        }
    }
    
    if (leaf_index < parent->num_keys()) {
        // Try right sibling
        PageID right_sibling_id = parent->child_at(leaf_index + 1);
        auto right_sibling_node = load_node(right_sibling_id);
        if (right_sibling_node && right_sibling_node->is_leaf()) {
            auto* right_sibling = dynamic_cast<BTreeLeafNode*>(right_sibling_node.get());
            
            if (right_sibling->num_keys() > right_sibling->min_keys()) {
                // Borrow from right sibling
                Value first_key = right_sibling->key_at(0);
                Value first_value = right_sibling->value_at(0);
                
                // Remove from right sibling
                right_sibling->remove_entry(first_key, config_);
                
                // Insert into leaf
                leaf->insert_entry(BTreeEntry(first_key, first_value), config_);
                
                // Update parent key
                parent->set_key_at(leaf_index, right_sibling->key_at(0));
                
                save_node(*right_sibling);
                save_node(*leaf);
                save_node(*parent);
                return;
            }
            
            // Merge with right sibling if both have minimum keys
            if (leaf->num_keys() + right_sibling->num_keys() <= leaf->max_keys()) {
                // Move all entries from right sibling to leaf
                for (size_t i = 0; i < right_sibling->num_keys(); ++i) {
                    Value key = right_sibling->key_at(i);
                    Value value = right_sibling->value_at(i);
                    leaf->insert_entry(BTreeEntry(key, value), config_);
                }
                
                // Update linked list pointers
                leaf->set_next_leaf(right_sibling->next_leaf());
                if (right_sibling->next_leaf() != kInvalidPageID) {
                    auto next_node = load_node(right_sibling->next_leaf());
                    if (next_node && next_node->is_leaf()) {
                        auto* next_leaf = dynamic_cast<BTreeLeafNode*>(next_node.get());
                        next_leaf->set_prev_leaf(leaf->page_id());
                        save_node(*next_leaf);
                    }
                }
                
                save_node(*leaf);
                delete_node(right_sibling->page_id());
                
                // Remove key and child from parent
                remove_key_from_internal(parent, right_sibling->page_id());
            }
        }
    }
}

void BTree::remove_key_from_internal(BTreeInternalNode* parent, PageID child_id) {
    // Find the child to remove
    size_t child_index = parent->num_keys() + 1; // Invalid index
    for (size_t i = 0; i <= parent->num_keys(); ++i) {
        if (parent->child_at(i) == child_id) {
            child_index = i;
            break;
        }
    }
    
    if (child_index > parent->num_keys()) {
        return; // Child not found
    }
    
    // Remove the child and corresponding key
    parent->remove_child(child_index);
    
    // If parent becomes empty and it's not the root, handle underflow
    if (parent->num_keys() == 0 && parent->page_id() == root_page_id_) {
        // Root is empty, promote the remaining child as new root
        if (parent->child_at(0) != kInvalidPageID) {
            root_page_id_ = parent->child_at(0);
            height_--;
            
            // Update the new root's parent to invalid
            auto new_root = load_node(root_page_id_);
            if (new_root) {
                new_root->set_parent_id(kInvalidPageID);
                save_node(*new_root);
            }
        }
        delete_node(parent->page_id());
    } else if (parent->num_keys() < parent->min_keys() && parent->page_id() != root_page_id_) {
        // Handle underflow in internal node by redistributing or merging
        merge_or_redistribute_internal(parent);
    } else {
        save_node(*parent);
    }
}

void BTree::merge_or_redistribute_internal(BTreeInternalNode* node) {
    PageID parent_id = node->parent_id();
    if (parent_id == kInvalidPageID) {
        return; // Root node, nothing to do
    }

    // Load parent
    auto parent_node = load_node(parent_id);
    if (!parent_node || !parent_node->is_internal()) {
        return;
    }

    auto* parent = dynamic_cast<BTreeInternalNode*>(parent_node.get());
    
    // Find this node's position in parent
    size_t node_index = 0;
    for (size_t i = 0; i <= parent->num_keys(); ++i) {
        if (parent->child_at(i) == node->page_id()) {
            node_index = i;
            break;
        }
    }
    
    // Try to borrow from or merge with siblings
    if (node_index > 0) {
        // Try left sibling
        PageID left_sibling_id = parent->child_at(node_index - 1);
        auto left_sibling_node = load_node(left_sibling_id);
        if (left_sibling_node && left_sibling_node->is_internal()) {
            auto* left_sibling = dynamic_cast<BTreeInternalNode*>(left_sibling_node.get());
            
            if (left_sibling->num_keys() > left_sibling->min_keys()) {
                // Borrow from left sibling
                Value separator_key = parent->key_at(node_index - 1);
                Value last_key = left_sibling->key_at(left_sibling->num_keys() - 1);
                PageID last_child = left_sibling->child_at(left_sibling->num_keys());
                
                // Remove last child from left sibling
                left_sibling->remove_child(left_sibling->num_keys());
                
                // Insert separator key and child into node
                node->insert_key_child(-1, separator_key, last_child);
                
                // Update parent separator
                parent->set_key_at(node_index - 1, last_key);
                
                // Update borrowed child's parent
                auto borrowed_child = load_node(last_child);
                if (borrowed_child) {
                    borrowed_child->set_parent_id(node->page_id());
                    save_node(*borrowed_child);
                }
                
                save_node(*left_sibling);
                save_node(*node);
                save_node(*parent);
                return;
            }
        }
    }
    
    if (node_index < parent->num_keys()) {
        // Try right sibling
        PageID right_sibling_id = parent->child_at(node_index + 1);
        auto right_sibling_node = load_node(right_sibling_id);
        if (right_sibling_node && right_sibling_node->is_internal()) {
            auto* right_sibling = dynamic_cast<BTreeInternalNode*>(right_sibling_node.get());
            
            if (right_sibling->num_keys() > right_sibling->min_keys()) {
                // Borrow from right sibling
                Value separator_key = parent->key_at(node_index);
                Value first_key = right_sibling->key_at(0);
                PageID first_child = right_sibling->child_at(0);
                
                // Remove first child from right sibling
                right_sibling->remove_child(0);
                
                // Insert separator key and child into node
                node->insert_key_child(node->num_keys(), separator_key, first_child);
                
                // Update parent separator
                parent->set_key_at(node_index, first_key);
                
                // Update borrowed child's parent
                auto borrowed_child = load_node(first_child);
                if (borrowed_child) {
                    borrowed_child->set_parent_id(node->page_id());
                    save_node(*borrowed_child);
                }
                
                save_node(*right_sibling);
                save_node(*node);
                save_node(*parent);
                return;
            }
            
            // Merge with right sibling if both have minimum keys
            if (node->num_keys() + right_sibling->num_keys() + 1 <= node->max_keys()) {
                // Pull down separator key from parent
                Value separator_key = parent->key_at(node_index);
                node->insert_key_child(node->num_keys(), separator_key, right_sibling->child_at(0));
                
                // Move all keys and children from right sibling to node
                for (size_t i = 0; i < right_sibling->num_keys(); ++i) {
                    node->insert_key_child(node->num_keys(), right_sibling->key_at(i), 
                                         right_sibling->child_at(i + 1));
                }
                
                // Update children's parent pointers
                for (size_t i = 0; i <= right_sibling->num_keys(); ++i) {
                    PageID child_id = right_sibling->child_at(i);
                    auto child = load_node(child_id);
                    if (child) {
                        child->set_parent_id(node->page_id());
                        save_node(*child);
                    }
                }
                
                save_node(*node);
                delete_node(right_sibling->page_id());
                
                // Remove key and child from parent
                remove_key_from_internal(parent, right_sibling->page_id());
            }
        }
    }
}

std::optional<Value> BTree::find_internal(const Value& key) const {
    BTreeLeafNode* leaf = find_leaf_node(key);
    if (!leaf) {
        return std::nullopt;
    }

    auto result = leaf->find_value(key, config_);
    delete leaf;
    return result;
}

void BTree::split_child(BTreeInternalNode* parent, size_t child_index) {
    PageID child_id = parent->child_at(child_index);
    std::unique_ptr<BTreeNode> child = load_node(child_id);

    if (child->is_leaf()) {
        auto* leaf_child = dynamic_cast<BTreeLeafNode*>(child.get());
        auto [split_key, new_node] = leaf_child->split();

        // Create page for new node
        PageID new_page_id = create_node(BTreeNodeType::Leaf);
        new_node->set_page_id(new_page_id);

        // Update parent pointers
        new_node->set_parent_id(parent->page_id());

        // Fix linked list pointers
        PageID old_next = leaf_child->next_leaf();
        leaf_child->set_next_leaf(new_page_id);
        new_node->set_prev_leaf(leaf_child->page_id());
        new_node->set_next_leaf(old_next);

        // Update the old next node's prev pointer if it exists
        if (old_next != kInvalidPageID) {
            auto next_node = load_node(old_next);
            if (next_node && next_node->is_leaf()) {
                auto* next_leaf = dynamic_cast<BTreeLeafNode*>(next_node.get());
                next_leaf->set_prev_leaf(new_page_id);
                save_node(*next_leaf);
            }
        }

        // Update parent
        parent->insert_key_child(child_index, split_key, new_page_id);

        // Save nodes - INCLUDING THE PARENT!
        save_node(*parent);
        save_node(*leaf_child);
        save_node(*new_node);

        // Force immediate flush of split nodes to ensure persistence
        storage_->flush_page(parent->page_id());
        storage_->flush_page(leaf_child->page_id());
        storage_->flush_page(new_page_id);
    } else {
        auto* internal_child = dynamic_cast<BTreeInternalNode*>(child.get());
        auto [split_key, new_node] = internal_child->split();

        // Create page for new node
        PageID new_page_id = create_node(BTreeNodeType::Internal);
        new_node->set_page_id(new_page_id);

        // Update parent pointers
        new_node->set_parent_id(parent->page_id());

        // Update parent
        parent->insert_key_child(child_index, split_key, new_page_id);

        // Save nodes - INCLUDING THE PARENT!
        save_node(*parent);
        save_node(*internal_child);
        save_node(*new_node);

        // Force immediate flush of split nodes to ensure persistence
        storage_->flush_page(parent->page_id());
        storage_->flush_page(internal_child->page_id());
        storage_->flush_page(new_page_id);
    }
}

void BTree::split_root() {
    std::unique_ptr<BTreeNode> old_root = load_node(root_page_id_);
    if (!old_root) {
        throw std::runtime_error("Failed to load root node for split");
    }

    // Create new root
    PageID new_root_id = create_node(BTreeNodeType::Internal);
    auto new_root = std::make_unique<BTreeInternalNode>(new_root_id, config_.min_degree);
    new_root->set_level(old_root->level() + 1);

    // Set old root as first child
    new_root->set_child_at(0, root_page_id_);

    // Split old root
    if (old_root->is_leaf()) {
        auto* leaf = dynamic_cast<BTreeLeafNode*>(old_root.get());
        auto [split_key, new_node] = leaf->split();

        PageID new_page_id = create_node(BTreeNodeType::Leaf);
        new_node->set_page_id(new_page_id);

        // Fix linked list pointers now that new node has page_id
        PageID old_next = leaf->next_leaf();
        leaf->set_next_leaf(new_page_id);
        new_node->set_prev_leaf(leaf->page_id());
        new_node->set_next_leaf(old_next);

        // Update the old next node's prev pointer if it exists
        if (old_next != kInvalidPageID) {
            auto next_node = load_node(old_next);
            if (next_node && next_node->is_leaf()) {
                auto* next_leaf = dynamic_cast<BTreeLeafNode*>(next_node.get());
                next_leaf->set_prev_leaf(new_page_id);
                save_node(*next_leaf);
            }
        }

        new_root->insert_key_child(0, split_key, new_page_id);

        save_node(*leaf);
        save_node(*new_node);

        // Force flush
        storage_->flush_page(leaf->page_id());
        storage_->flush_page(new_page_id);
    } else {
        auto* internal = dynamic_cast<BTreeInternalNode*>(old_root.get());
        auto [split_key, new_node] = internal->split();

        PageID new_page_id = create_node(BTreeNodeType::Internal);
        new_node->set_page_id(new_page_id);

        new_root->insert_key_child(0, split_key, new_page_id);

        save_node(*internal);
        save_node(*new_node);

        // Force flush
        storage_->flush_page(internal->page_id());
        storage_->flush_page(new_page_id);
    }

    // Save new root and update tree
    save_node(*new_root);
    storage_->flush_page(new_root_id);

    root_page_id_ = new_root_id;
    height_++;
}

BTreeLeafNode* BTree::find_leaf_node(const Value& key) const {
    PageID current_id = root_page_id_;
    std::unique_ptr<BTreeNode> node = load_node(current_id);

    while (node && !node->is_leaf()) {
        auto* internal = dynamic_cast<BTreeInternalNode*>(node.get());
        size_t child_index = node->search_key(key, config_);

        // In B+Tree internal nodes, if key >= keys[i], we go to child[i+1]
        // The search_key returns the position where key would be inserted
        // So if key >= keys[child_index], we need to go to the next child
        if (child_index < node->num_keys() &&
            node->compare_keys(key, node->key_at(child_index), config_) >= 0) {
            child_index++;
        }

        current_id = internal->child_at(child_index);
        node = load_node(current_id);
    }

    return dynamic_cast<BTreeLeafNode*>(node.release());
}

// BTreeIterator implementation
BTreeIterator::BTreeIterator(const BTree* tree, PageID leaf_id, size_t index)
    : tree_(tree), current_index_(index) {
    if (tree_ && leaf_id != kInvalidPageID) {
        auto node = tree_->load_node(leaf_id);
        if (node && node->is_leaf()) {
            current_leaf_.reset(dynamic_cast<BTreeLeafNode*>(node.release()));
        }
    }
}

BTreeIterator& BTreeIterator::operator++() {
    advance();
    return *this;
}

BTreeIterator BTreeIterator::operator++(int) {
    BTreeIterator tmp(tree_, current_leaf_ ? current_leaf_->page_id() : kInvalidPageID,
                      current_index_);
    advance();
    return tmp;
}

bool BTreeIterator::operator==(const BTreeIterator& other) const {
    if (!valid() && !other.valid()) {
        return true;
    }

    if (!valid() || !other.valid()) {
        return false;
    }

    return current_leaf_->page_id() == other.current_leaf_->page_id() &&
           current_index_ == other.current_index_;
}

bool BTreeIterator::operator!=(const BTreeIterator& other) const {
    return !(*this == other);
}

BTreeEntry BTreeIterator::operator*() const {
    if (!valid()) {
        return BTreeEntry();
    }

    return BTreeEntry(current_leaf_->key_at(current_index_),
                      current_leaf_->value_at(current_index_));
}

const BTreeEntry* BTreeIterator::operator->() const {
    static BTreeEntry entry;
    if (valid()) {
        entry = **this;
        return &entry;
    }
    return nullptr;
}

void BTreeIterator::advance() {
    if (!valid()) {
        return;
    }

    current_index_++;

    if (current_index_ >= current_leaf_->num_keys()) {
        load_next_leaf();
    }
}

void BTreeIterator::load_next_leaf() {
    if (!current_leaf_) {
        return;
    }

    PageID next_id = current_leaf_->next_leaf();
    if (next_id == kInvalidPageID) {
        current_leaf_.reset();
        current_index_ = 0;
        return;
    }

    auto node = tree_->load_node(next_id);
    if (node && node->is_leaf()) {
        current_leaf_.reset(dynamic_cast<BTreeLeafNode*>(node.release()));
        current_index_ = 0;
    } else {
        current_leaf_.reset();
        current_index_ = 0;
    }
}

// BTreeFactory implementation
std::unique_ptr<BTree> BTreeFactory::create(std::shared_ptr<StorageEngine> storage,
                                            const BTreeConfig& config) {
    return std::make_unique<BTree>(std::move(storage), config);
}

}  // namespace lumen