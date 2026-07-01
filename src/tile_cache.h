#pragma once

#include "tile.h"

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace PixelForge {

// LRU cache for processed tiles with configurable memory and entry limits.
// Thread-safe: all public methods lock an internal mutex.
class TileCache {
public:
    // Construct with both entry count and memory byte budget
    explicit TileCache(size_t max_entries = 256,
                       size_t max_memory_bytes = 256 * 1024 * 1024);

    ~TileCache() = default;

    // Non-copyable
    TileCache(const TileCache&) = delete;
    TileCache& operator=(const TileCache&) = delete;

    // Retrieve a tile from the cache.  Returns nullptr on miss.
    // Moves the entry to the front of the LRU list on hit.
    Tile* get(int tile_x, int tile_y);

    // Insert a tile into the cache, taking ownership.
    // Evicts LRU entries if the cache exceeds its limits after insertion.
    void put(int tile_x, int tile_y, std::unique_ptr<Tile> tile);

    // Explicitly evict a single tile by grid coordinates.
    void evict(int tile_x, int tile_y);

    // Evict the single least-recently-used entry.
    void evict_lru();

    // Evict all entries.
    void clear();

    // Return the total memory consumed by cached tile pixel buffers.
    size_t memory_usage() const;

    // Number of tiles currently cached.
    size_t entry_count() const;

    // Limit getters / setters
    size_t get_max_entries()      const { return m_max_entries; }
    size_t get_max_memory_bytes() const { return m_max_memory; }
    void   set_max_entries(size_t n)    { m_max_entries = n; enforce_limits(); }
    void   set_max_memory_bytes(size_t n) { m_max_memory = n; enforce_limits(); }

    // Check if a tile is present without updating LRU order.
    bool contains(int tile_x, int tile_y) const;

    // Return all cached tile keys as (tile_x, tile_y) pairs.
    std::vector<std::pair<int,int>> keys() const;

    // Statistics
    struct Stats {
        uint64_t hits   = 0;
        uint64_t misses = 0;
        uint64_t evictions = 0;
        uint64_t insertions = 0;
    };
    Stats stats() const;
    void  reset_stats();

private:
    // Pack (tile_x, tile_y) into a single 64-bit key for hash-map lookup.
    // Uses the lower 32 bits for tile_x and upper 32 bits for tile_y.
    static uint64_t encode_key(int tile_x, int tile_y);

    // Decode a 64-bit key back into (tile_x, tile_y).
    static void decode_key(uint64_t key, int& tile_x, int& tile_y);

    // Internal eviction helper — evicts until both limits are satisfied.
    void enforce_limits();

    struct CacheEntry {
        std::unique_ptr<Tile> tile;
        size_t                memory_size = 0;
        // Iterator into the LRU list for O(1) promotion
        std::list<uint64_t>::iterator lru_iter;
    };

    std::unordered_map<uint64_t, CacheEntry> m_entries;
    std::list<uint64_t>                      m_lru_list;   // front = MRU, back = LRU

    size_t m_max_entries = 256;
    size_t m_max_memory  = 256 * 1024 * 1024;
    size_t m_current_memory = 0;

    mutable std::mutex m_mutex;

    // Stats
    mutable Stats m_stats;
};

} // namespace PixelForge
