#include "vfs.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <iostream>

VFS::VFS() : root(std::make_unique<DirectoryNode>("")), next_fd(1), cache_head(nullptr), cache_tail(nullptr), cache_capacity(3) {}

VFS::~VFS() {
    clear_cache();
}

VFSNode* VFS::lookup_node(const std::string& path) {
    if (path.empty() || path[0] != '/') return nullptr;
    
    VFSNode* curr = root.get();
    size_t start = 1;
    
    while (start < path.size()) {
        size_t end = path.find('/', start);
        std::string part = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);
        if (part.empty()) {
            if (end == std::string::npos) break;
            start = end + 1;
            continue;
        }
        
        if (curr->type != NodeType::Directory) return nullptr;
        DirectoryNode* dir = static_cast<DirectoryNode*>(curr);
        
        auto it = dir->children.find(part);
        if (it == dir->children.end()) return nullptr;
        
        curr = it->second.get();
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return curr;
}

void VFS::add_node_to_tree(const std::string& path, std::unique_ptr<VFSNode> node) {
    if (path.empty() || path[0] != '/') return;
    
    DirectoryNode* curr = root.get();
    size_t start = 1;
    
    while (start < path.size()) {
        size_t end = path.find('/', start);
        std::string part = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);
        
        if (end == std::string::npos) {
            curr->children[part] = std::move(node);
            break;
        }
        
        auto it = curr->children.find(part);
        if (it == curr->children.end()) {
            auto new_dir = std::make_unique<DirectoryNode>(part);
            DirectoryNode* next = new_dir.get();
            curr->children[part] = std::move(new_dir);
            curr = next;
        } else {
            if (it->second->type != NodeType::Directory) return; // Conflict
            curr = static_cast<DirectoryNode*>(it->second.get());
        }
        start = end + 1;
    }
}

void VFS::decompress_rle(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    size_t src_idx = 0;
    size_t dst_idx = 0;
    
    while (src_idx < src_len) {
        uint8_t count = src[src_idx++];
        if (src_idx >= src_len) break;
        uint8_t val = src[src_idx++];
        
        // Bug 2: Missing bounds check on dst_idx + count against dst_len.
        // Writing run-length data will overflow dst when count is large.
        for (uint8_t i = 0; i < count; ++i) {
            dst[dst_idx++] = val; 
        }
    }
}

bool VFS::mount_archive(const uint8_t* data, size_t size) {
    if (size < sizeof(ArchiveHeader)) return false;
    
    const ArchiveHeader* header = reinterpret_cast<const ArchiveHeader*>(data);
    if (std::memcmp(header->magic, "FNFS", 4) != 0) return false;
    
    // Bug 1: Integer Overflow in directory allocation
    // num_files * sizeof(DirectoryEntry) can wrap around.
    uint32_t num_files = header->num_files;
    uint32_t total_dir_size = num_files * sizeof(DirectoryEntry);
    
    DirectoryEntry* entries = (DirectoryEntry*)std::malloc(total_dir_size);
    if (!entries) return false;
    
    const uint8_t* dir_ptr = data + header->dir_offset;
    uint32_t max_copy = num_files;
    
    for (uint32_t i = 0; i < max_copy; ++i) {
        // Safe check to avoid reading past archive size
        if (header->dir_offset + (i + 1) * sizeof(DirectoryEntry) > size) {
            break;
        }
        // If total_dir_size overflowed, this memcpy will overflow the allocated 'entries' buffer
        std::memcpy(&entries[i], dir_ptr + i * sizeof(DirectoryEntry), sizeof(DirectoryEntry));
    }
    
    // Now construct in-memory VFS nodes
    for (uint32_t i = 0; i < num_files; ++i) {
        if (header->dir_offset + (i + 1) * sizeof(DirectoryEntry) > size) {
            break;
        }
        
        // Bug 5: Heap buffer overread. filename is 32 chars and may not be null-terminated.
        // Constructing std::string directly from char array searches for null terminator.
        std::string filename(entries[i].filename); 
        
        std::string full_path = "/" + filename;
        auto file_node = std::make_unique<FileNode>(filename);
        
        file_node->original_size = entries[i].original_size;
        file_node->is_compressed = (entries[i].compression == 1);
        
        if (entries[i].offset + entries[i].size <= size) {
            const uint8_t* file_data_ptr = data + entries[i].offset;
            if (file_node->is_compressed) {
                // Allocate uncompressed buffer and decompress
                file_node->data.resize(file_node->original_size);
                decompress_rle(file_data_ptr, entries[i].size, file_node->data.data(), file_node->original_size);
            } else {
                file_node->data.assign(file_data_ptr, file_data_ptr + entries[i].size);
            }
        }
        
        add_node_to_tree(full_path, std::move(file_node));
    }
    
    std::free(entries);
    return true;
}

int VFS::open_file(const std::string& path) {
    VFSNode* node = lookup_node(path);
    if (!node) return -1;
    
    // We cache file access (Bug 4 target)
    if (node->type == NodeType::File) {
        access_file_cache(path, static_cast<FileNode*>(node));
    }
    
    int fd = next_fd++;
    open_fds[fd] = node;
    return fd;
}

int VFS::read_file(int fd, uint8_t* buf, size_t len) {
    auto it = open_fds.find(fd);
    if (it == open_fds.end()) return -1;
    
    VFSNode* node = it->second;
    
    // Bug 6: Type confusion.
    // If filename has ".raw", we bypass NodeType check and cast directly.
    bool bypass_check = (node->name.find(".raw") != std::string::npos);
    if (node->type != NodeType::File && !bypass_check) {
        return -1;
    }
    
    // Cast Node
    FileNode* file = static_cast<FileNode*>(node);
    
    // Type confusion will crash here as DirectoryNode doesn't have vector data.
    // UAF (Bug 3) will crash here accessing freed memory.
    size_t to_read = std::min(len, file->data.size());
    if (to_read > 0) {
        std::memcpy(buf, file->data.data(), to_read);
    }
    return to_read;
}

bool VFS::delete_file(const std::string& path) {
    if (path.empty() || path[0] != '/') return false;
    
    DirectoryNode* curr = root.get();
    size_t start = 1;
    
    while (start < path.size()) {
        size_t end = path.find('/', start);
        std::string part = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);
        
        if (end == std::string::npos) {
            auto it = curr->children.find(part);
            if (it == curr->children.end()) return false;
            
            // Bug 3: Use-After-Free
            // We erase the unique_ptr from children, which destroys the node.
            // But we do NOT invalidate/remove it from open_fds!
            curr->children.erase(it);
            return true;
        }
        
        auto it = curr->children.find(part);
        if (it == curr->children.end() || it->second->type != NodeType::Directory) return false;
        curr = static_cast<DirectoryNode*>(it->second.get());
        start = end + 1;
    }
    return false;
}

void VFS::access_file_cache(const std::string& path, FileNode* file) {
    if (cache_map.find(path) != cache_map.end()) {
        CacheNode* node = cache_map[path];
        if (node == cache_head) return;
        
        // Unlink node
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        if (node == cache_tail) cache_tail = node->prev;
        
        // Move to head
        node->next = cache_head;
        node->prev = nullptr;
        if (cache_head) cache_head->prev = node;
        cache_head = node;
        return;
    }
    
    CacheNode* node = new CacheNode{path, file, nullptr, nullptr};
    cache_map[path] = node;
    
    node->next = cache_head;
    if (cache_head) cache_head->prev = node;
    cache_head = node;
    if (!cache_tail) cache_tail = node;
    
    if (cache_map.size() > cache_capacity) {
        evict_cache();
    }
}

void VFS::evict_cache() {
    if (!cache_tail) return;
    
    CacheNode* to_evict = cache_tail;
    
    // Unlink tail
    cache_tail = cache_tail->prev;
    if (cache_tail) {
        cache_tail->next = nullptr;
    } else {
        cache_head = nullptr;
    }
    
    // Bug 4: Double Free cache eviction vulnerability.
    // If the path contains "double", we delete it, but do NOT erase it from cache_map.
    // Later, clear_cache() will double-free this node.
    if (to_evict->path.find("double") != std::string::npos) {
        delete to_evict;
    } else {
        cache_map.erase(to_evict->path);
        delete to_evict;
    }
}

void VFS::clear_cache() {
    // Delete all nodes in cache map
    for (auto const& [key, val] : cache_map) {
        // If a node was already deleted in evict_cache (path contains "double"),
        // this will call delete on a freed pointer -> Double Free!
        delete val; 
    }
    cache_map.clear();
    cache_head = nullptr;
    cache_tail = nullptr;
}
