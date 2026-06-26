#ifndef PIXELFORGE_FILTER_H
#define PIXELFORGE_FILTER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace PixelForge {

#include "image.h"

class FilterCache {
private:
    size_t capacity;
    std::unordered_map<std::string, Image*> cache_map;
    std::vector<std::string> eviction_queue;

public:
    explicit FilterCache(size_t cap = 4);
    ~FilterCache();

    Image* get(const std::string& key);
    void put(const std::string& key, Image* img);
    void evict(const std::string& key);
    void clear();
    
    size_t get_size() const { return cache_map.size(); }
};

// Filter operations
Image* apply_grayscale(const Image* src, FilterCache* cache = nullptr, const std::string& cache_key = "");
Image* apply_resize(const Image* src, int new_w, int new_h, FilterCache* cache = nullptr, const std::string& cache_key = "");
Image* apply_blur(const Image* src, int radius, FilterCache* cache = nullptr, const std::string& cache_key = "");
Image* apply_crop(const Image* src, int x, int y, int w, int h, FilterCache* cache = nullptr, const std::string& cache_key = "");

} // namespace PixelForge

#endif // PIXELFORGE_FILTER_H
