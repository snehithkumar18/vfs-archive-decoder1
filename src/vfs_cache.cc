#include "vfs.h"
#include "logger.h"

void VFS::access_file_cache(const std::string& path, FileNode* file) {
    VFSLogger::get_instance().debug("VFSCache", "Accessing cache for path: " + path);
    
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
    
    // Allocate new node using our custom allocator if desired, or plain new.
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
    VFSLogger::get_instance().info("VFSCache", "Evicting cache entry for: " + to_evict->path);
    
    // Unlink tail
    cache_tail = cache_tail->prev;
    if (cache_tail) {
        cache_tail->next = nullptr;
    } else {
        cache_head = nullptr;
    }
    
    // Bug 4: Double Free cache eviction vulnerability.
    // If the path contains "double", we delete it, but do NOT erase it from cache_map.
    // Later, clear_cache() will double-free this node when iterating over cache_map.
    if (to_evict->path.find("double") != std::string::npos) {
        VFSLogger::get_instance().warn("VFSCache", "Intentionally leaving stale pointer in map for UAF/Double-free tests: " + to_evict->path);
        delete to_evict; // Free 1
    } else {
        cache_map.erase(to_evict->path);
        delete to_evict;
    }
}

void VFS::clear_cache() {
    VFSLogger::get_instance().debug("VFSCache", "Clearing LRU Cache...");
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
