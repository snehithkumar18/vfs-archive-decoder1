/**
 * @file btree_index.cc
 * @brief Implementation of BTreeIndex – a B-Tree for fast VFS path lookups.
 *
 * This file implements every operation declared in btree_index.h:
 *   - insert / split_child
 *   - search
 *   - remove  (with borrow-from-sibling and merge logic)
 *   - in-order traversal
 *   - height / node-count statistics
 *   - binary serialization (BFS) and deserialization
 *   - pretty-print for debugging
 *
 * All mutating operations log through VFSLogger so that archive-tool
 * users can trace index maintenance at DEBUG level.
 *
 * Copyright (c) 2026 Project Fenrer Contributors.
 */

#include "btree_index.h"
#include "logger.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>

// ===================================================================
//  BTreeNode implementation
// ===================================================================

BTreeNode::BTreeNode(bool leaf, int max_keys)
    : is_leaf(leaf), num_keys(0)
{
    // Pre-reserve to the maximum capacity so that later inserts
    // into the vectors never trigger a reallocation mid-split.
    keys.reserve(static_cast<size_t>(max_keys));
    values.reserve(static_cast<size_t>(max_keys));
    if (!leaf) {
        // An internal node can have at most (max_keys + 1) children.
        children.reserve(static_cast<size_t>(max_keys + 1));
    }
}

// ===================================================================
//  BTreeIndex – public interface
// ===================================================================

BTreeIndex::BTreeIndex(int min_degree)
    : t_(min_degree < 2 ? 2 : min_degree), root_(nullptr)
{
    VFSLogger::get_instance().info(
        "BTreeIndex",
        "Created B-Tree index with minimum degree t=" + std::to_string(t_));
}

BTreeIndex::~BTreeIndex()
{
    // unique_ptr chain handles all deallocation automatically.
    VFSLogger::get_instance().debug("BTreeIndex", "B-Tree index destroyed");
}

// -------------------------------------------------------------------
//  Insert
// -------------------------------------------------------------------

void BTreeIndex::insert(const std::string& key, const std::string& value)
{
    VFSLogger::get_instance().debug(
        "BTreeIndex", "Inserting key=\"" + key + "\" value=\"" + value + "\"");

    // Empty tree – create the root as a leaf with a single key.
    if (!root_) {
        int max_keys = 2 * t_ - 1;
        root_ = std::make_unique<BTreeNode>(/*leaf=*/true, max_keys);
        root_->keys.push_back(key);
        root_->values.push_back(value);
        root_->num_keys = 1;
        VFSLogger::get_instance().debug("BTreeIndex",
                                        "Created root node with first key");
        return;
    }

    // Check if the key already exists – update in place if so.
    auto [node, idx] = search_node(root_.get(), key);
    if (node) {
        VFSLogger::get_instance().debug(
            "BTreeIndex",
            "Key \"" + key + "\" already exists – updating value");
        node->values[static_cast<size_t>(idx)] = value;
        return;
    }

    // If the root is full, split it first so we always descend into a
    // node that has room for at least one more key.
    if (root_->num_keys == 2 * t_ - 1) {
        int max_keys = 2 * t_ - 1;
        auto new_root =
            std::make_unique<BTreeNode>(/*leaf=*/false, max_keys);

        // The old root becomes child[0] of the new root.
        new_root->children.push_back(std::move(root_));

        // Split the (now child-0) full node.
        split_child(new_root.get(), 0);

        // Decide which of the two children to descend into.
        int i = 0;
        if (new_root->keys[0] < key) {
            i = 1;
        }
        insert_non_full(new_root->children[static_cast<size_t>(i)].get(),
                        key, value);

        root_ = std::move(new_root);
        VFSLogger::get_instance().debug("BTreeIndex",
                                        "Root was full – split performed");
    } else {
        insert_non_full(root_.get(), key, value);
    }
}

// -------------------------------------------------------------------
//  Search
// -------------------------------------------------------------------

std::string BTreeIndex::search(const std::string& key) const
{
    if (!root_) {
        VFSLogger::get_instance().debug(
            "BTreeIndex", "Search for \"" + key + "\" – tree is empty");
        return "";
    }

    auto [node, idx] = search_node(root_.get(), key);
    if (node) {
        VFSLogger::get_instance().debug(
            "BTreeIndex",
            "Search hit: \"" + key + "\" -> \"" +
                node->values[static_cast<size_t>(idx)] + "\"");
        return node->values[static_cast<size_t>(idx)];
    }

    VFSLogger::get_instance().debug("BTreeIndex",
                                    "Search miss: \"" + key + "\"");
    return "";
}

// -------------------------------------------------------------------
//  Remove
// -------------------------------------------------------------------

bool BTreeIndex::remove(const std::string& key)
{
    if (!root_) {
        VFSLogger::get_instance().warn(
            "BTreeIndex",
            "Remove called on empty tree for key=\"" + key + "\"");
        return false;
    }

    // Verify the key actually exists before starting the complex removal.
    auto [found_node, found_idx] = search_node(root_.get(), key);
    if (!found_node) {
        VFSLogger::get_instance().warn(
            "BTreeIndex", "Key \"" + key + "\" not found – nothing to remove");
        return false;
    }

    VFSLogger::get_instance().debug("BTreeIndex",
                                    "Removing key=\"" + key + "\"");
    remove_from_node(root_.get(), key);

    // If the root has become empty (0 keys) after a merge, replace it
    // with its only child (or set to nullptr if it was a leaf).
    if (root_->num_keys == 0) {
        if (root_->is_leaf) {
            root_.reset();
            VFSLogger::get_instance().debug("BTreeIndex",
                                            "Tree is now empty");
        } else {
            root_ = std::move(root_->children[0]);
            VFSLogger::get_instance().debug(
                "BTreeIndex", "Root shrunk – new root promoted");
        }
    }

    return true;
}

// -------------------------------------------------------------------
//  Traversal
// -------------------------------------------------------------------

void BTreeIndex::traverse_inorder(
    const std::function<void(const std::string&,
                             const std::string&)>& visitor) const
{
    if (root_) {
        traverse_inorder_impl(root_.get(), visitor);
    }
}

// -------------------------------------------------------------------
//  Statistics
// -------------------------------------------------------------------

int BTreeIndex::get_height() const
{
    return get_height_impl(root_.get());
}

int BTreeIndex::get_node_count() const
{
    return get_node_count_impl(root_.get());
}

int BTreeIndex::get_key_count() const
{
    int count = 0;
    traverse_inorder([&count](const std::string&, const std::string&) {
        ++count;
    });
    return count;
}

// -------------------------------------------------------------------
//  Serialization
// -------------------------------------------------------------------

void BTreeIndex::serialize_to_buffer(std::vector<uint8_t>& buffer) const
{
    VFSLogger::get_instance().info("BTreeIndex",
                                   "Serializing B-Tree to buffer");

    // Header: min_degree + total node count.
    int total_nodes = get_node_count();
    write_uint32(buffer, static_cast<uint32_t>(t_));
    write_uint32(buffer, static_cast<uint32_t>(total_nodes));

    if (!root_) {
        VFSLogger::get_instance().debug("BTreeIndex",
                                        "Empty tree – header-only output");
        return;
    }

    // BFS traversal – write each node's payload in breadth-first order
    // so that the deserializer can reconstruct the same topology.
    std::queue<const BTreeNode*> bfs;
    bfs.push(root_.get());

    int nodes_written = 0;
    while (!bfs.empty()) {
        const BTreeNode* cur = bfs.front();
        bfs.pop();

        // -- Node header --
        write_uint32(buffer, static_cast<uint32_t>(cur->num_keys));
        buffer.push_back(cur->is_leaf ? 1 : 0);

        // -- Number of children (needed so deserializer knows how many
        //    children to expect for this node) --
        uint32_t num_children = static_cast<uint32_t>(cur->children.size());
        write_uint32(buffer, num_children);

        // -- Keys and values --
        for (int i = 0; i < cur->num_keys; ++i) {
            const std::string& k = cur->keys[static_cast<size_t>(i)];
            write_uint32(buffer, static_cast<uint32_t>(k.size()));
            buffer.insert(buffer.end(), k.begin(), k.end());

            const std::string& v = cur->values[static_cast<size_t>(i)];
            write_uint32(buffer, static_cast<uint32_t>(v.size()));
            buffer.insert(buffer.end(), v.begin(), v.end());
        }

        // Enqueue children for BFS.
        for (auto& child : cur->children) {
            if (child) {
                bfs.push(child.get());
            }
        }

        ++nodes_written;
    }

    VFSLogger::get_instance().info(
        "BTreeIndex",
        "Serialized " + std::to_string(nodes_written) + " nodes, " +
            std::to_string(buffer.size()) + " bytes total");
}

bool BTreeIndex::deserialize_from_buffer(const uint8_t* data, size_t size)
{
    VFSLogger::get_instance().info("BTreeIndex",
                                   "Deserializing B-Tree from buffer");

    size_t offset = 0;

    // -- Read header --
    if (size < 8) {
        VFSLogger::get_instance().error("BTreeIndex",
                                        "Buffer too small for header");
        return false;
    }

    uint32_t degree = read_uint32(data, offset, size);
    uint32_t total_nodes = read_uint32(data, offset, size);

    if (degree < 2 || degree > 1024) {
        VFSLogger::get_instance().error(
            "BTreeIndex",
            "Invalid degree " + std::to_string(degree) + " in buffer");
        return false;
    }

    t_ = static_cast<int>(degree);
    int max_keys = 2 * t_ - 1;

    if (total_nodes == 0) {
        root_.reset();
        VFSLogger::get_instance().debug("BTreeIndex",
                                        "Deserialized empty tree");
        return true;
    }

    // Reconstruct nodes in BFS order.  We read each node's metadata,
    // create the BTreeNode, and push placeholders for its children.
    // Because we serialized in BFS order the children appear in the
    // stream in exactly the order we need to attach them.

    struct PendingChild {
        BTreeNode* parent;
        int child_slot;
    };

    std::queue<PendingChild> pending;

    // -- Read root --
    auto read_one_node = [&](size_t& off) -> std::unique_ptr<BTreeNode> {
        if (off + 5 > size) return nullptr;

        uint32_t nk = read_uint32(data, off, size);
        bool leaf = (data[off++] != 0);
        uint32_t nc = read_uint32(data, off, size);

        auto node = std::make_unique<BTreeNode>(leaf, max_keys);
        node->num_keys = static_cast<int>(nk);

        for (uint32_t i = 0; i < nk; ++i) {
            std::string k = read_string(data, off, size);
            std::string v = read_string(data, off, size);
            node->keys.push_back(std::move(k));
            node->values.push_back(std::move(v));
        }

        // Reserve child slots (filled later in BFS order).
        for (uint32_t i = 0; i < nc; ++i) {
            node->children.push_back(nullptr);
        }

        return node;
    };

    root_ = read_one_node(offset);
    if (!root_) {
        VFSLogger::get_instance().error("BTreeIndex",
                                        "Failed to read root node");
        return false;
    }

    // Enqueue children slots for the root.
    for (int c = 0; c < static_cast<int>(root_->children.size()); ++c) {
        pending.push({root_.get(), c});
    }

    uint32_t nodes_read = 1;

    while (!pending.empty() && nodes_read < total_nodes) {
        PendingChild pc = pending.front();
        pending.pop();

        auto child = read_one_node(offset);
        if (!child) {
            VFSLogger::get_instance().error(
                "BTreeIndex",
                "Truncated data at node #" + std::to_string(nodes_read));
            return false;
        }

        // Enqueue grandchildren.
        BTreeNode* raw = child.get();
        for (int c = 0; c < static_cast<int>(raw->children.size()); ++c) {
            pending.push({raw, c});
        }

        pc.parent->children[static_cast<size_t>(pc.child_slot)] =
            std::move(child);
        ++nodes_read;
    }

    VFSLogger::get_instance().info(
        "BTreeIndex",
        "Deserialized " + std::to_string(nodes_read) + " nodes, t=" +
            std::to_string(t_));
    return true;
}

// -------------------------------------------------------------------
//  Pretty-print
// -------------------------------------------------------------------

void BTreeIndex::print_tree() const
{
    if (!root_) {
        std::cout << "(empty B-Tree)\n";
        return;
    }
    std::cout << "B-Tree (t=" << t_ << ", height=" << get_height()
              << ", keys=" << get_key_count() << "):\n";
    print_node(root_.get(), 0);
}

// ===================================================================
//  Private helpers
// ===================================================================

// -------------------------------------------------------------------
//  insert_non_full – insert into a node known to have room
// -------------------------------------------------------------------

void BTreeIndex::insert_non_full(BTreeNode* node,
                                 const std::string& key,
                                 const std::string& value)
{
    int i = node->num_keys - 1;

    if (node->is_leaf) {
        // Shift keys/values right to make room, then insert.
        node->keys.push_back("");
        node->values.push_back("");
        while (i >= 0 && node->keys[static_cast<size_t>(i)] > key) {
            node->keys[static_cast<size_t>(i + 1)] =
                std::move(node->keys[static_cast<size_t>(i)]);
            node->values[static_cast<size_t>(i + 1)] =
                std::move(node->values[static_cast<size_t>(i)]);
            --i;
        }
        node->keys[static_cast<size_t>(i + 1)] = key;
        node->values[static_cast<size_t>(i + 1)] = value;
        node->num_keys++;

        VFSLogger::get_instance().debug(
            "BTreeIndex",
            "Inserted key \"" + key + "\" into leaf (num_keys=" +
                std::to_string(node->num_keys) + ")");
    } else {
        // Find the child that will receive the new key.
        while (i >= 0 && node->keys[static_cast<size_t>(i)] > key) {
            --i;
        }
        ++i;

        // If that child is full, split it first.
        if (node->children[static_cast<size_t>(i)]->num_keys ==
            2 * t_ - 1) {
            split_child(node, i);

            // After split, keys[i] is the median pushed up.
            // Decide which of the two halves to descend into.
            if (node->keys[static_cast<size_t>(i)] < key) {
                ++i;
            }
        }

        insert_non_full(
            node->children[static_cast<size_t>(i)].get(), key, value);
    }
}

// -------------------------------------------------------------------
//  split_child – splits node->children[child_index] which must be full
// -------------------------------------------------------------------

void BTreeIndex::split_child(BTreeNode* parent, int child_index)
{
    int max_keys = 2 * t_ - 1;
    BTreeNode* full_child =
        parent->children[static_cast<size_t>(child_index)].get();

    // Create a new sibling that will hold the upper (t-1) keys.
    auto sibling =
        std::make_unique<BTreeNode>(full_child->is_leaf, max_keys);

    sibling->num_keys = t_ - 1;

    // Copy the upper (t-1) keys and values to the sibling.
    for (int j = 0; j < t_ - 1; ++j) {
        sibling->keys.push_back(
            std::move(full_child->keys[static_cast<size_t>(j + t_)]));
        sibling->values.push_back(
            std::move(full_child->values[static_cast<size_t>(j + t_)]));
    }

    // If the full child is internal, move the upper t children too.
    if (!full_child->is_leaf) {
        for (int j = 0; j < t_; ++j) {
            sibling->children.push_back(
                std::move(full_child->children[static_cast<size_t>(j + t_)]));
        }
        // Erase the moved children from full_child.
        full_child->children.resize(static_cast<size_t>(t_));
    }

    // The median key (index t-1) will be pushed up into the parent.
    std::string median_key =
        std::move(full_child->keys[static_cast<size_t>(t_ - 1)]);
    std::string median_val =
        std::move(full_child->values[static_cast<size_t>(t_ - 1)]);

    // Shrink full_child to hold only the lower (t-1) keys.
    full_child->keys.resize(static_cast<size_t>(t_ - 1));
    full_child->values.resize(static_cast<size_t>(t_ - 1));
    full_child->num_keys = t_ - 1;

    // Insert the sibling into the parent's children vector right after
    // child_index.
    parent->children.insert(
        parent->children.begin() + child_index + 1, std::move(sibling));

    // Insert the median key/value into the parent.
    parent->keys.insert(
        parent->keys.begin() + child_index, std::move(median_key));
    parent->values.insert(
        parent->values.begin() + child_index, std::move(median_val));
    parent->num_keys++;

    VFSLogger::get_instance().debug(
        "BTreeIndex",
        "Split child at index " + std::to_string(child_index) +
            " – parent now has " + std::to_string(parent->num_keys) +
            " keys");
}

// -------------------------------------------------------------------
//  search_node – recursive B-tree search returning (node, index)
// -------------------------------------------------------------------

std::pair<BTreeNode*, int>
BTreeIndex::search_node(BTreeNode* node, const std::string& key) const
{
    if (!node) return {nullptr, -1};

    int i = 0;
    while (i < node->num_keys &&
           key > node->keys[static_cast<size_t>(i)]) {
        ++i;
    }

    // Check if we found an exact match.
    if (i < node->num_keys &&
        node->keys[static_cast<size_t>(i)] == key) {
        return {node, i};
    }

    // Leaf with no match – key is absent.
    if (node->is_leaf) {
        return {nullptr, -1};
    }

    // Recurse into the appropriate child.
    return search_node(node->children[static_cast<size_t>(i)].get(), key);
}

// -------------------------------------------------------------------
//  remove_from_node – top-level per-node removal dispatcher
// -------------------------------------------------------------------

void BTreeIndex::remove_from_node(BTreeNode* node, const std::string& key)
{
    // Find the first key >= target.
    int idx = 0;
    while (idx < node->num_keys &&
           node->keys[static_cast<size_t>(idx)] < key) {
        ++idx;
    }

    // Case 1: key is present in this node.
    if (idx < node->num_keys &&
        node->keys[static_cast<size_t>(idx)] == key) {
        if (node->is_leaf) {
            remove_from_leaf(node, idx);
        } else {
            remove_from_internal(node, idx);
        }
        return;
    }

    // Case 2: key is not here and this is a leaf – nothing to do.
    if (node->is_leaf) {
        VFSLogger::get_instance().warn(
            "BTreeIndex",
            "Key \"" + key + "\" not found in leaf during removal");
        return;
    }

    // Case 3: key is in a subtree. Make sure the child we descend into
    // has at least t keys (so that we can safely delete from it).
    bool at_last_child = (idx == node->num_keys);

    if (node->children[static_cast<size_t>(idx)]->num_keys < t_) {
        fill_child(node, idx);
    }

    // After fill_child, if we were at the last child and a merge happened
    // the last child may have been absorbed into idx-1.
    if (at_last_child && idx > node->num_keys) {
        remove_from_node(
            node->children[static_cast<size_t>(idx - 1)].get(), key);
    } else {
        remove_from_node(
            node->children[static_cast<size_t>(idx)].get(), key);
    }
}

void BTreeIndex::remove_from_leaf(BTreeNode* node, int idx)
{
    // Simply shift keys left to overwrite the deleted entry.
    node->keys.erase(node->keys.begin() + idx);
    node->values.erase(node->values.begin() + idx);
    node->num_keys--;

    VFSLogger::get_instance().debug(
        "BTreeIndex",
        "Removed key at leaf index " + std::to_string(idx));
}

void BTreeIndex::remove_from_internal(BTreeNode* node, int idx)
{
    std::string key = node->keys[static_cast<size_t>(idx)];

    // If the child that precedes key has >= t keys, replace key with
    // its in-order predecessor and recursively delete that predecessor.
    if (node->children[static_cast<size_t>(idx)]->num_keys >= t_) {
        auto [pred_key, pred_val] = get_predecessor(node, idx);
        node->keys[static_cast<size_t>(idx)] = pred_key;
        node->values[static_cast<size_t>(idx)] = pred_val;
        remove_from_node(
            node->children[static_cast<size_t>(idx)].get(), pred_key);
    }
    // Else if the child that follows key has >= t keys, replace with
    // the in-order successor.
    else if (node->children[static_cast<size_t>(idx + 1)]->num_keys >=
             t_) {
        auto [succ_key, succ_val] = get_successor(node, idx);
        node->keys[static_cast<size_t>(idx)] = succ_key;
        node->values[static_cast<size_t>(idx)] = succ_val;
        remove_from_node(
            node->children[static_cast<size_t>(idx + 1)].get(), succ_key);
    }
    // Both children have exactly (t-1) keys – merge them and then
    // recursively delete from the merged child.
    else {
        merge_children(node, idx);
        remove_from_node(
            node->children[static_cast<size_t>(idx)].get(), key);
    }
}

std::pair<std::string, std::string>
BTreeIndex::get_predecessor(BTreeNode* node, int idx)
{
    BTreeNode* cur = node->children[static_cast<size_t>(idx)].get();
    while (!cur->is_leaf) {
        cur = cur->children[static_cast<size_t>(cur->num_keys)].get();
    }
    int last = cur->num_keys - 1;
    return {cur->keys[static_cast<size_t>(last)],
            cur->values[static_cast<size_t>(last)]};
}

std::pair<std::string, std::string>
BTreeIndex::get_successor(BTreeNode* node, int idx)
{
    BTreeNode* cur = node->children[static_cast<size_t>(idx + 1)].get();
    while (!cur->is_leaf) {
        cur = cur->children[0].get();
    }
    return {cur->keys[0], cur->values[0]};
}

// -------------------------------------------------------------------
//  fill_child – ensure children[idx] has at least t keys
// -------------------------------------------------------------------

void BTreeIndex::fill_child(BTreeNode* node, int idx)
{
    // Try borrowing from the left sibling.
    if (idx > 0 &&
        node->children[static_cast<size_t>(idx - 1)]->num_keys >= t_) {
        borrow_from_prev(node, idx);
    }
    // Try borrowing from the right sibling.
    else if (idx < node->num_keys &&
             node->children[static_cast<size_t>(idx + 1)]->num_keys >= t_) {
        borrow_from_next(node, idx);
    }
    // Neither sibling can spare a key – merge with one of them.
    else {
        if (idx < node->num_keys) {
            merge_children(node, idx);
        } else {
            merge_children(node, idx - 1);
        }
    }
}

void BTreeIndex::borrow_from_prev(BTreeNode* node, int idx)
{
    BTreeNode* child =
        node->children[static_cast<size_t>(idx)].get();
    BTreeNode* left_sib =
        node->children[static_cast<size_t>(idx - 1)].get();

    // Shift all keys/values in child right by one.
    child->keys.insert(child->keys.begin(),
                       node->keys[static_cast<size_t>(idx - 1)]);
    child->values.insert(child->values.begin(),
                         node->values[static_cast<size_t>(idx - 1)]);

    // Pull the last key of the left sibling up into the parent.
    node->keys[static_cast<size_t>(idx - 1)] =
        std::move(left_sib->keys.back());
    node->values[static_cast<size_t>(idx - 1)] =
        std::move(left_sib->values.back());
    left_sib->keys.pop_back();
    left_sib->values.pop_back();

    // If internal, move the rightmost child of left_sib to child[0].
    if (!child->is_leaf) {
        child->children.insert(
            child->children.begin(),
            std::move(left_sib->children.back()));
        left_sib->children.pop_back();
    }

    child->num_keys++;
    left_sib->num_keys--;

    VFSLogger::get_instance().debug(
        "BTreeIndex",
        "Borrowed key from left sibling at index " + std::to_string(idx));
}

void BTreeIndex::borrow_from_next(BTreeNode* node, int idx)
{
    BTreeNode* child =
        node->children[static_cast<size_t>(idx)].get();
    BTreeNode* right_sib =
        node->children[static_cast<size_t>(idx + 1)].get();

    // Push the separating key from the parent down into child.
    child->keys.push_back(node->keys[static_cast<size_t>(idx)]);
    child->values.push_back(node->values[static_cast<size_t>(idx)]);

    // Pull the first key of the right sibling up into the parent.
    node->keys[static_cast<size_t>(idx)] =
        std::move(right_sib->keys.front());
    node->values[static_cast<size_t>(idx)] =
        std::move(right_sib->values.front());
    right_sib->keys.erase(right_sib->keys.begin());
    right_sib->values.erase(right_sib->values.begin());

    // If internal, move the first child of right_sib to the end of child.
    if (!child->is_leaf) {
        child->children.push_back(
            std::move(right_sib->children.front()));
        right_sib->children.erase(right_sib->children.begin());
    }

    child->num_keys++;
    right_sib->num_keys--;

    VFSLogger::get_instance().debug(
        "BTreeIndex",
        "Borrowed key from right sibling at index " + std::to_string(idx));
}

// -------------------------------------------------------------------
//  merge_children – merge children[idx] and children[idx+1]
// -------------------------------------------------------------------

void BTreeIndex::merge_children(BTreeNode* node, int idx)
{
    BTreeNode* left =
        node->children[static_cast<size_t>(idx)].get();
    BTreeNode* right =
        node->children[static_cast<size_t>(idx + 1)].get();

    // Pull the separator key from the parent into the left child.
    left->keys.push_back(
        std::move(node->keys[static_cast<size_t>(idx)]));
    left->values.push_back(
        std::move(node->values[static_cast<size_t>(idx)]));

    // Append all keys/values from the right child.
    for (int i = 0; i < right->num_keys; ++i) {
        left->keys.push_back(
            std::move(right->keys[static_cast<size_t>(i)]));
        left->values.push_back(
            std::move(right->values[static_cast<size_t>(i)]));
    }

    // Append children if internal.
    if (!left->is_leaf) {
        for (auto& ch : right->children) {
            left->children.push_back(std::move(ch));
        }
    }

    left->num_keys += 1 + right->num_keys;

    // Remove the separator and the right child pointer from the parent.
    node->keys.erase(node->keys.begin() + idx);
    node->values.erase(node->values.begin() + idx);
    node->children.erase(node->children.begin() + idx + 1);
    node->num_keys--;

    VFSLogger::get_instance().debug(
        "BTreeIndex",
        "Merged children at parent index " + std::to_string(idx) +
            " – merged node has " + std::to_string(left->num_keys) +
            " keys");
}

// -------------------------------------------------------------------
//  Traversal helper
// -------------------------------------------------------------------

void BTreeIndex::traverse_inorder_impl(
    const BTreeNode* node,
    const std::function<void(const std::string&,
                             const std::string&)>& visitor) const
{
    if (!node) return;

    for (int i = 0; i < node->num_keys; ++i) {
        // Visit the left subtree of keys[i].
        if (!node->is_leaf && static_cast<size_t>(i) < node->children.size()) {
            traverse_inorder_impl(node->children[static_cast<size_t>(i)].get(),
                                  visitor);
        }
        visitor(node->keys[static_cast<size_t>(i)],
                node->values[static_cast<size_t>(i)]);
    }

    // Visit the rightmost subtree.
    if (!node->is_leaf &&
        static_cast<size_t>(node->num_keys) < node->children.size()) {
        traverse_inorder_impl(
            node->children[static_cast<size_t>(node->num_keys)].get(),
            visitor);
    }
}

// -------------------------------------------------------------------
//  Statistics helpers
// -------------------------------------------------------------------

int BTreeIndex::get_height_impl(const BTreeNode* node) const
{
    if (!node) return 0;
    if (node->is_leaf) return 1;
    // All leaves are at the same depth in a valid B-tree, so just
    // follow the first child.
    if (node->children.empty()) return 1;
    return 1 + get_height_impl(node->children[0].get());
}

int BTreeIndex::get_node_count_impl(const BTreeNode* node) const
{
    if (!node) return 0;
    int count = 1;
    for (auto& ch : node->children) {
        count += get_node_count_impl(ch.get());
    }
    return count;
}

// -------------------------------------------------------------------
//  Pretty-print helper
// -------------------------------------------------------------------

void BTreeIndex::print_node(const BTreeNode* node, int depth) const
{
    if (!node) return;

    std::string indent(static_cast<size_t>(depth) * 4, ' ');
    std::string type_tag = node->is_leaf ? "[LEAF]" : "[INTERNAL]";

    std::ostringstream oss;
    oss << indent << type_tag << " keys=" << node->num_keys << " {";
    for (int i = 0; i < node->num_keys; ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << node->keys[static_cast<size_t>(i)] << "\"";
    }
    oss << "}";
    std::cout << oss.str() << "\n";

    if (!node->is_leaf) {
        for (size_t c = 0; c < node->children.size(); ++c) {
            if (node->children[c]) {
                std::cout << indent << "  child[" << c << "]:\n";
                print_node(node->children[c].get(), depth + 1);
            }
        }
    }
}

// -------------------------------------------------------------------
//  Serialization primitives
// -------------------------------------------------------------------

void BTreeIndex::write_uint32(std::vector<uint8_t>& buf, uint32_t val) const
{
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

uint32_t BTreeIndex::read_uint32(const uint8_t* data, size_t& offset,
                                 size_t size) const
{
    if (offset + 4 > size) {
        VFSLogger::get_instance().error(
            "BTreeIndex",
            "read_uint32: buffer overrun at offset " +
                std::to_string(offset));
        return 0;
    }
    uint32_t val = static_cast<uint32_t>(data[offset]) |
                   (static_cast<uint32_t>(data[offset + 1]) << 8) |
                   (static_cast<uint32_t>(data[offset + 2]) << 16) |
                   (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return val;
}

std::string BTreeIndex::read_string(const uint8_t* data, size_t& offset,
                                    size_t size) const
{
    uint32_t len = read_uint32(data, offset, size);
    if (len > 0x00100000) {
        // Sanity check – no single key/value should exceed 1 MiB.
        VFSLogger::get_instance().error(
            "BTreeIndex",
            "read_string: implausible length " + std::to_string(len));
        return "";
    }
    if (offset + len > size) {
        VFSLogger::get_instance().error(
            "BTreeIndex",
            "read_string: buffer overrun, need " + std::to_string(len) +
                " bytes at offset " + std::to_string(offset));
        return "";
    }
    std::string result(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return result;
}
