#pragma once

#include "image.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <list>
#include <mutex>
#include <functional>

namespace PixelForge {

// Eviction callback receives the key and a pointer to the image about
// to be evicted (the pointer is only valid during the callback).
using FrameCacheEvictionCallback = std::function<void(int key, const Image* img)>;

class FrameCache {
public:
    // max_entries: maximum number of cached frames (0 = unlimited by count)
    // max_memory:  maximum total bytes of pixel data (0 = unlimited by memory)
    explicit FrameCache(size_t max_entries = 128, size_t max_memory = 0);
    ~FrameCache();

    // Non-copyable
    FrameCache(const FrameCache&) = delete;
    FrameCache& operator=(const FrameCache&) = delete;

    // -----------------------------------------------------------------------
    // Cache operations (all thread-safe)
    // -----------------------------------------------------------------------

    // Retrieve a cached frame.  Returns nullptr on miss.
    // The returned pointer is owned by the cache — do NOT delete it.
    // Marks the entry as recently used.
    Image* get(int key);

    // Store a frame in the cache.  Makes a DEEP COPY of `img`.
    // May trigger eviction if limits are exceeded.
    void put(int key, const Image* img);

    // Store a frame by moving an owned image into the cache.
    void put(int key, std::unique_ptr<Image> img);

    // Remove a specific entry
    bool invalidate(int key);

    // Remove all entries
    void invalidate_all();

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    // Total bytes of pixel data across all cached images
    size_t memory_usage() const;

    // Number of cached entries
    size_t entry_count() const;

    // Does the cache contain this key?
    bool contains(int key) const;

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    void set_max_entries(size_t max_entries);
    size_t max_entries() const { return m_max_entries; }

    void set_max_memory(size_t max_bytes);
    size_t max_memory() const { return m_max_memory; }

    // Set a callback invoked just before an entry is evicted
    void set_eviction_callback(FrameCacheEvictionCallback cb);

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    struct Stats {
        uint64_t hits       = 0;
        uint64_t misses     = 0;
        uint64_t evictions  = 0;
        uint64_t insertions = 0;
    };
    Stats stats() const;
    void reset_stats();

private:
    // Internal entry stored in the cache
    struct CacheEntry {
        int key;
        std::unique_ptr<Image> image;
        size_t byte_size;   // cached image byte count for fast memory tracking
    };

    // LRU list: front = least recently used, back = most recently used
    using LRUList = std::list<CacheEntry>;
    using LRUIterator = LRUList::iterator;

    // Promote an entry to most-recently-used (move to back of list)
    void touch(LRUIterator it);

    // Evict least-recently-used entries until within limits
    void evict_to_fit(size_t additional_bytes);

    // Evict the single least-recently-used entry
    void evict_one();

    // Compute the byte size of an image's pixel data
    static size_t image_byte_size(const Image* img);

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------
    LRUList m_lru_list;
    std::unordered_map<int, LRUIterator> m_map;

    size_t m_max_entries;
    size_t m_max_memory;
    size_t m_current_memory = 0;

    mutable std::mutex m_mutex;

    FrameCacheEvictionCallback m_eviction_cb;
    mutable Stats m_stats;
};

} // namespace PixelForge
