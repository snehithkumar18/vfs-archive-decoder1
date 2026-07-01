#include "tile_cache.h"

#include <algorithm>

namespace PixelForge {

// ---------------------------------------------------------------------------
// Key Encoding
// ---------------------------------------------------------------------------

uint64_t TileCache::encode_key(int tile_x, int tile_y) {
    // Store signed ints as unsigned halves of a 64-bit key.
    // This handles negative coordinates via two's complement reinterpretation.
    uint32_t ux = static_cast<uint32_t>(tile_x);
    uint32_t uy = static_cast<uint32_t>(tile_y);
    return (static_cast<uint64_t>(uy) << 32) | static_cast<uint64_t>(ux);
}

void TileCache::decode_key(uint64_t key, int& tile_x, int& tile_y) {
    tile_x = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
    tile_y = static_cast<int>(static_cast<uint32_t>(key >> 32));
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

TileCache::TileCache(size_t max_entries, size_t max_memory_bytes)
    : m_max_entries(max_entries),
      m_max_memory(max_memory_bytes),
      m_current_memory(0) {}

// ---------------------------------------------------------------------------
// get — cache lookup with LRU promotion
// ---------------------------------------------------------------------------

Tile* TileCache::get(int tile_x, int tile_y) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t key = encode_key(tile_x, tile_y);
    auto it = m_entries.find(key);
    if (it == m_entries.end()) {
        ++m_stats.misses;
        return nullptr;
    }

    // Promote to MRU position (front of list)
    m_lru_list.erase(it->second.lru_iter);
    m_lru_list.push_front(key);
    it->second.lru_iter = m_lru_list.begin();

    ++m_stats.hits;
    return it->second.tile.get();
}

// ---------------------------------------------------------------------------
// put — insert tile, evicting if necessary
// ---------------------------------------------------------------------------

void TileCache::put(int tile_x, int tile_y, std::unique_ptr<Tile> tile) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t key = encode_key(tile_x, tile_y);

    // If the key already exists, remove the old entry first
    auto existing = m_entries.find(key);
    if (existing != m_entries.end()) {
        m_current_memory -= existing->second.memory_size;
        m_lru_list.erase(existing->second.lru_iter);
        m_entries.erase(existing);
    }

    size_t mem = tile ? tile->memory_size() : 0;

    // Insert at MRU position
    m_lru_list.push_front(key);

    CacheEntry entry;
    entry.tile        = std::move(tile);
    entry.memory_size = mem;
    entry.lru_iter    = m_lru_list.begin();

    m_entries[key] = std::move(entry);
    m_current_memory += mem;
    ++m_stats.insertions;

    // Enforce limits — may evict LRU entries
    enforce_limits();
}

// ---------------------------------------------------------------------------
// evict — remove a specific entry
// ---------------------------------------------------------------------------

void TileCache::evict(int tile_x, int tile_y) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t key = encode_key(tile_x, tile_y);
    auto it = m_entries.find(key);
    if (it == m_entries.end()) return;

    m_current_memory -= it->second.memory_size;
    m_lru_list.erase(it->second.lru_iter);
    m_entries.erase(it);
    ++m_stats.evictions;
}

// ---------------------------------------------------------------------------
// evict_lru — remove the oldest entry
// ---------------------------------------------------------------------------

void TileCache::evict_lru() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_lru_list.empty()) return;

    uint64_t lru_key = m_lru_list.back();
    m_lru_list.pop_back();

    auto it = m_entries.find(lru_key);
    if (it != m_entries.end()) {
        m_current_memory -= it->second.memory_size;
        m_entries.erase(it);
        ++m_stats.evictions;
    }
}

// ---------------------------------------------------------------------------
// clear — remove all entries
// ---------------------------------------------------------------------------

void TileCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_stats.evictions += m_entries.size();
    m_entries.clear();
    m_lru_list.clear();
    m_current_memory = 0;
}

// ---------------------------------------------------------------------------
// memory_usage / entry_count
// ---------------------------------------------------------------------------

size_t TileCache::memory_usage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_memory;
}

size_t TileCache::entry_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

// ---------------------------------------------------------------------------
// contains
// ---------------------------------------------------------------------------

bool TileCache::contains(int tile_x, int tile_y) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.find(encode_key(tile_x, tile_y)) != m_entries.end();
}

// ---------------------------------------------------------------------------
// keys — list all cached tile coordinates
// ---------------------------------------------------------------------------

std::vector<std::pair<int,int>> TileCache::keys() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::pair<int,int>> result;
    result.reserve(m_entries.size());

    for (const auto& kv : m_entries) {
        int tx, ty;
        decode_key(kv.first, tx, ty);
        result.emplace_back(tx, ty);
    }
    return result;
}

// ---------------------------------------------------------------------------
// stats / reset_stats
// ---------------------------------------------------------------------------

TileCache::Stats TileCache::stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void TileCache::reset_stats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = Stats{};
}

// ---------------------------------------------------------------------------
// enforce_limits — bulk eviction
// ---------------------------------------------------------------------------

void TileCache::enforce_limits() {
    // Called with m_mutex already held.
    // Evict from LRU (back of list) until both limits are satisfied.
    while (!m_lru_list.empty()) {
        bool over_entries = m_entries.size() > m_max_entries;
        bool over_memory  = m_current_memory > m_max_memory;

        if (!over_entries && !over_memory) break;

        uint64_t lru_key = m_lru_list.back();
        m_lru_list.pop_back();

        auto it = m_entries.find(lru_key);
        if (it != m_entries.end()) {
            m_current_memory -= it->second.memory_size;
            m_entries.erase(it);
            ++m_stats.evictions;
        }
    }
}

} // namespace PixelForge
