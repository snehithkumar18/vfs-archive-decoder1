#ifndef VFS_H
#define VFS_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <cstddef>

#pragma pack(push, 1)
struct ArchiveHeader {
    char magic[4];          // Must be "FNFS"
    uint32_t num_files;     // Bug 1: Integer overflow
    uint32_t dir_offset;    // Offset to directory entries
};

struct DirectoryEntry {
    char filename[32];      // Bug 5: Unterminated string
    uint32_t size;          // Compressed size
    uint32_t original_size; // Original size
    uint32_t offset;        // Offset of file data
    uint8_t compression;    // 0 = None, 1 = RLE, 2 = Huffman
    uint32_t checksum;      // Simple checksum
};
#pragma pack(pop)

enum class NodeType {
    File,
    Directory
};

class VFSNode {
public:
    NodeType type;
    std::string name;
    virtual ~VFSNode() = default;
protected:
    VFSNode(NodeType t, std::string n) : type(t), name(std::move(n)) {}
};

class DirectoryNode : public VFSNode {
public:
    std::map<std::string, std::unique_ptr<VFSNode>> children;
    DirectoryNode(std::string n) : VFSNode(NodeType::Directory, std::move(n)) {}
};

class FileNode : public VFSNode {
public:
    std::vector<uint8_t> data;
    uint32_t original_size;
    bool is_compressed;
    FileNode(std::string n) : VFSNode(NodeType::File, std::move(n)), original_size(0), is_compressed(false) {}
};

struct CacheNode {
    std::string path;
    FileNode* file_ptr;
    CacheNode* prev;
    CacheNode* next;
};

class VFS {
private:
    std::unique_ptr<DirectoryNode> root;
    std::map<int, VFSNode*> open_fds; // Bug 3: Use-After-Free FDs
    int next_fd;

    // Cache (Bug 4: LRU double free)
    std::map<std::string, CacheNode*> cache_map;
    CacheNode* cache_head;
    CacheNode* cache_tail;
    size_t cache_capacity;

    VFSNode* lookup_node(const std::string& path);
    void add_node_to_tree(const std::string& path, std::unique_ptr<VFSNode> node);

public:
    friend class VFSStats;

    VFS();
    ~VFS();

    bool mount_archive(const uint8_t* data, size_t size); // Bug 1, 5 (implemented in vfs_parser.cc)
    int open_file(const std::string& path);
    int read_file(int fd, uint8_t* buf, size_t len); // Bug 6: Type confusion
    bool delete_file(const std::string& path); // Bug 3: Leaves dangling pointer in open_fds
    std::vector<std::string> list_directory(const std::string& path);
    void access_file_cache(const std::string& path, FileNode* file);
    void evict_cache(); // Bug 4: Double free (implemented in vfs_cache.cc)
    void clear_cache();
};

#endif // VFS_H
