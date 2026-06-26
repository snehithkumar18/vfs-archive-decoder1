/**
 * @file btree_index.h
 * @brief B-Tree based index for fast file-name lookups in the FenrerVFS.
 *
 * Provides a disk-serializable B-Tree that maps string keys (file names or
 * paths) to string values (canonical VFS paths). The minimum degree 't'
 * is configurable at construction time and governs the branching factor:
 *   - Every non-root node has at least (t-1) keys and at most (2t-1) keys.
 *   - Every internal node has at least t children and at most 2t children.
 *
 * The tree supports:
 *   - O(log n) insert, search, and delete
 *   - In-order traversal yielding all key-value pairs in sorted order
 *   - Serialization to / deserialization from a flat byte buffer so the
 *     index can be persisted inside an FNFS archive footer.
 *
 * Thread-safety: None. Callers must synchronize externally.
 *
 * Copyright (c) 2026 Project Fenrer Contributors.
 */

#ifndef BTREE_INDEX_H
#define BTREE_INDEX_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// BTreeNode  –  a single node in the B-Tree
// ---------------------------------------------------------------------------

/**
 * @struct BTreeNode
 * @brief Internal representation of a B-Tree node.
 *
 * Keys and values are parallel vectors: keys[i] maps to values[i].
 * Children follow the standard B-Tree invariant:
 *   children[i] contains keys < keys[i]
 *   children[i+1] contains keys > keys[i]
 *
 * The node owns its child pointers via unique_ptr so the tree cleans up
 * automatically when the root is destroyed.
 */
struct BTreeNode {
    /// Sorted array of search keys (e.g. file basenames or full paths).
    std::vector<std::string> keys;

    /// Parallel array of associated values (canonical VFS paths).
    std::vector<std::string> values;

    /// Child pointers (size = num_keys + 1 for internal nodes, 0 for leaves).
    std::vector<std::unique_ptr<BTreeNode>> children;

    /// True when this node is a leaf (has no children).
    bool is_leaf;

    /// Current number of keys stored in this node.
    int num_keys;

    /// Construct a new empty BTreeNode.
    /// @param leaf  whether the new node is a leaf
    /// @param max_keys  maximum number of keys (2*t - 1) – used to pre-reserve
    explicit BTreeNode(bool leaf, int max_keys);
};

// ---------------------------------------------------------------------------
// BTreeIndex  –  the full B-Tree index
// ---------------------------------------------------------------------------

/**
 * @class BTreeIndex
 * @brief B-Tree index mapping string keys to string values.
 *
 * Typical usage inside FenrerVFS:
 * @code
 *   BTreeIndex idx(3);                       // minimum degree 3
 *   idx.insert("readme.txt", "/docs/readme.txt");
 *   idx.insert("main.cpp",   "/src/main.cpp");
 *   auto result = idx.search("main.cpp");    // -> "/src/main.cpp"
 * @endcode
 */
class BTreeIndex {
public:
    // ----- Construction / configuration ------------------------------------

    /**
     * Construct a new, empty BTreeIndex.
     * @param min_degree  Minimum degree 't' (must be >= 2). Controls the
     *                    branching factor: nodes hold [t-1 .. 2t-1] keys.
     */
    explicit BTreeIndex(int min_degree = 3);

    /// Destructor – the tree is cleaned up automatically via unique_ptr chain.
    ~BTreeIndex();

    // Prevent accidental copies; move is fine.
    BTreeIndex(const BTreeIndex&) = delete;
    BTreeIndex& operator=(const BTreeIndex&) = delete;
    BTreeIndex(BTreeIndex&&) noexcept = default;
    BTreeIndex& operator=(BTreeIndex&&) noexcept = default;

    // ----- Core operations -------------------------------------------------

    /**
     * Insert a key-value pair into the tree.
     * If the key already exists its value is updated in-place.
     * @param key    Search key (e.g. "readme.txt").
     * @param value  Associated payload (e.g. "/docs/readme.txt").
     */
    void insert(const std::string& key, const std::string& value);

    /**
     * Search for a key.
     * @param key  The key to look up.
     * @return The associated value, or an empty string if not found.
     */
    std::string search(const std::string& key) const;

    /**
     * Remove a key from the tree.
     * @param key  The key to remove.
     * @return true if the key was found and removed, false otherwise.
     */
    bool remove(const std::string& key);

    // ----- Traversal -------------------------------------------------------

    /**
     * In-order traversal – visits every key-value pair in sorted key order.
     * @param visitor  Callback receiving (key, value) for each entry.
     */
    void traverse_inorder(
        const std::function<void(const std::string& key,
                                 const std::string& value)>& visitor) const;

    // ----- Statistics ------------------------------------------------------

    /** @return Height of the tree (0 for an empty tree, 1 for a root-only). */
    int get_height() const;

    /** @return Total number of BTreeNode objects in the tree. */
    int get_node_count() const;

    /** @return Total number of key-value pairs stored. */
    int get_key_count() const;

    /** @return The minimum degree 't' this tree was created with. */
    int get_min_degree() const { return t_; }

    // ----- Serialization ---------------------------------------------------

    /**
     * Serialize the entire tree into a contiguous byte buffer.
     * Format (BFS order):
     *   [4 bytes] min_degree
     *   [4 bytes] total_node_count
     *   For each node (BFS):
     *     [4 bytes] num_keys
     *     [1 byte ] is_leaf
     *     For each key/value:
     *       [4 bytes] key_length, [key_length bytes] key_data
     *       [4 bytes] val_length, [val_length bytes] val_data
     * @param buffer  Output buffer – contents are appended.
     */
    void serialize_to_buffer(std::vector<uint8_t>& buffer) const;

    /**
     * Reconstruct a tree from a buffer previously produced by
     * serialize_to_buffer().
     * @param data  Pointer to the serialized bytes.
     * @param size  Number of bytes available.
     * @return true on success, false on malformed input.
     */
    bool deserialize_from_buffer(const uint8_t* data, size_t size);

    // ----- Debugging -------------------------------------------------------

    /**
     * Pretty-print the tree structure to stdout with indentation showing
     * depth. Useful for debugging small trees.
     */
    void print_tree() const;

private:
    // ----- Internal helpers ------------------------------------------------

    /** Minimum degree – every non-root node has [t-1 .. 2t-1] keys. */
    int t_;

    /** Root of the B-Tree. nullptr when the tree is empty. */
    std::unique_ptr<BTreeNode> root_;

    // -- Insertion helpers --
    void insert_non_full(BTreeNode* node, const std::string& key,
                         const std::string& value);
    void split_child(BTreeNode* parent, int child_index);

    // -- Search helper --
    std::pair<BTreeNode*, int> search_node(BTreeNode* node,
                                           const std::string& key) const;

    // -- Removal helpers --
    void remove_from_node(BTreeNode* node, const std::string& key);
    void remove_from_leaf(BTreeNode* node, int idx);
    void remove_from_internal(BTreeNode* node, int idx);
    std::pair<std::string, std::string> get_predecessor(BTreeNode* node,
                                                         int idx);
    std::pair<std::string, std::string> get_successor(BTreeNode* node,
                                                       int idx);
    void fill_child(BTreeNode* node, int idx);
    void borrow_from_prev(BTreeNode* node, int idx);
    void borrow_from_next(BTreeNode* node, int idx);
    void merge_children(BTreeNode* node, int idx);

    // -- Traversal helper --
    void traverse_inorder_impl(
        const BTreeNode* node,
        const std::function<void(const std::string&,
                                 const std::string&)>& visitor) const;

    // -- Statistics helpers --
    int get_height_impl(const BTreeNode* node) const;
    int get_node_count_impl(const BTreeNode* node) const;

    // -- Print helper --
    void print_node(const BTreeNode* node, int depth) const;

    // -- Serialization helpers --
    void write_uint32(std::vector<uint8_t>& buf, uint32_t val) const;
    uint32_t read_uint32(const uint8_t* data, size_t& offset,
                         size_t size) const;
    std::string read_string(const uint8_t* data, size_t& offset,
                            size_t size) const;
};

#endif // BTREE_INDEX_H
