#include "vfs.h"
#include "logger.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

VFS::VFS() : root(std::make_unique<DirectoryNode>("")), next_fd(1), cache_head(nullptr), cache_tail(nullptr), cache_capacity(3) {
    VFSLogger::get_instance().info("VFS", "Virtual File System initialized");
}

VFS::~VFS() {
    VFSLogger::get_instance().info("VFS", "Virtual File System shutting down...");
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

int VFS::open_file(const std::string& path) {
    VFSLogger::get_instance().info("VFS", "Opening file: " + path);
    VFSNode* node = lookup_node(path);
    if (!node) {
        VFSLogger::get_instance().warn("VFS", "File lookup failed: " + path);
        return -1;
    }
    
    // We cache file access (Bug 4 target)
    if (node->type == NodeType::File) {
        access_file_cache(path, static_cast<FileNode*>(node));
    }
    
    int fd = next_fd++;
    open_fds[fd] = node;
    VFSLogger::get_instance().debug("VFS", "File opened. FD assigned: " + std::to_string(fd));
    return fd;
}

int VFS::read_file(int fd, uint8_t* buf, size_t len) {
    VFSLogger::get_instance().debug("VFS", "Reading from file descriptor: " + std::to_string(fd));
    auto it = open_fds.find(fd);
    if (it == open_fds.end()) {
        VFSLogger::get_instance().warn("VFS", "Invalid file descriptor: " + std::to_string(fd));
        return -1;
    }
    
    VFSNode* node = it->second;
    
    // Bug 6: Type confusion.
    // If filename has ".raw", we bypass NodeType check and cast directly.
    bool bypass_check = (node->name.find(".raw") != std::string::npos);
    if (node->type != NodeType::File && !bypass_check) {
        VFSLogger::get_instance().error("VFS", "Type mismatch: FD target is not a file node");
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
    VFSLogger::get_instance().info("VFS", "Deleting file node: " + path);
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
            VFSLogger::get_instance().warn("VFS", "Dangling node pointer left in open FDs table (intended bug)");
            return true;
        }
        
        auto it = curr->children.find(part);
        if (it == curr->children.end() || it->second->type != NodeType::Directory) return false;
        curr = static_cast<DirectoryNode*>(it->second.get());
        start = end + 1;
    }
    return false;
}

std::vector<std::string> VFS::list_directory(const std::string& path) {
    VFSLogger::get_instance().debug("VFS", "Listing directory: " + path);
    std::vector<std::string> results;
    
    VFSNode* node = (path == "/") ? root.get() : lookup_node(path);
    if (!node || node->type != NodeType::Directory) {
        VFSLogger::get_instance().warn("VFS", "list_directory failed: Target not a valid directory node");
        return results;
    }
    
    DirectoryNode* dir = static_cast<DirectoryNode*>(node);
    for (const auto& [name, child] : dir->children) {
        std::string suffix = (child->type == NodeType::Directory) ? "/" : "";
        results.push_back(name + suffix);
    }
    return results;
}
