#include "filter.h"
#include "logger.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace PixelForge {

// Redundant Image implementation removed. Handled by image.cc.

// ==========================================
// FilterCache Implementation
// ==========================================

FilterCache::FilterCache(size_t cap) : capacity(cap) {
    VFSLogger::get_instance().info("FilterCache", "Initialized with capacity " + std::to_string(capacity));
}

Image* FilterCache::get(const std::string& key) {
    auto it = cache_map.find(key);
    if (it != cache_map.end()) {
        VFSLogger::get_instance().info("FilterCache", "Cache hit for key: " + key);
        // Fast path: retrieve cached entry directly
        return it->second.get();
    }
    VFSLogger::get_instance().info("FilterCache", "Cache miss for key: " + key);
    return nullptr;
}

void FilterCache::put(const std::string& key, const Image* img) {
    if (!img) return;
    VFSLogger::get_instance().info("FilterCache", "Caching image under key: " + key);
    cache_map[key] = std::make_unique<Image>(*img);
    eviction_queue.push_back(key);

    if (cache_map.size() > capacity) {
        // Trigger eviction of the oldest element
        if (!eviction_queue.empty()) {
            std::string oldest = eviction_queue.front();
            eviction_queue.erase(eviction_queue.begin());
            VFSLogger::get_instance().info("FilterCache", "Capacity exceeded. Evicting key: " + oldest);
            evict(oldest);
        }
    }
}

void FilterCache::evict(const std::string& key) {
    auto it = cache_map.find(key);
    if (it != cache_map.end()) {
        cache_map.erase(it);
        VFSLogger::get_instance().info("FilterCache", "Evicted image for key: " + key);
    }
}

void FilterCache::clear() {
    VFSLogger::get_instance().info("FilterCache", "Clearing all cache entries.");
    cache_map.clear();
    eviction_queue.clear();
}

// ==========================================
// Filter Operations Implementation
// ==========================================

Image* apply_grayscale(const Image* src, FilterCache* cache, const std::string& cache_key) {
    if (!src || !src->data) return nullptr;

    if (cache && !cache_key.empty()) {
        Image* cached = cache->get(cache_key);
        if (cached) {
            // Return reference to the cached version
            VFSLogger::get_instance().info("Filter", "Using cached grayscale image");
            return new Image(*cached);
        }
    }

    VFSLogger::get_instance().info("Filter", "Applying grayscale filter");
    Image* dst = new Image(src->width, src->height, PixelFormat::Grayscale);
    int total_pixels = src->width * src->height;
    
    if (src->channels >= 3) {
        for (int i = 0; i < total_pixels; ++i) {
            int idx = i * src->channels;
            uint8_t r = src->data[idx + 0];
            uint8_t g = src->data[idx + 1];
            uint8_t b = src->data[idx + 2];
            uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
            
            dst->data[i] = gray;
        }
    } else {
        // Already 1 channel or similar, just copy
        std::memcpy(dst->data, src->data, static_cast<size_t>(src->width) * src->height * src->channels);
    }

    if (cache && !cache_key.empty()) {
        cache->put(cache_key, dst);
    }
    return dst;
}

Image* apply_resize(const Image* src, int new_w, int new_h, FilterCache* cache, const std::string& cache_key) {
    if (!src || !src->data || new_w <= 0 || new_h <= 0) return nullptr;

    if (cache && !cache_key.empty()) {
        Image* cached = cache->get(cache_key);
        if (cached) {
            VFSLogger::get_instance().info("Filter", "Using cached resized image");
            return new Image(*cached);
        }
    }

    VFSLogger::get_instance().info("Filter", "Applying resize filter to " + std::to_string(new_w) + "x" + std::to_string(new_h));
    Image* dst = new Image(new_w, new_h, src->channels);
    
    float x_ratio = static_cast<float>(src->width) / new_w;
    float y_ratio = static_cast<float>(src->height) / new_h;

    for (int y = 0; y < new_h; ++y) {
        for (int x = 0; x < new_w; ++x) {
            float px = x * x_ratio;
            float py = y * y_ratio;
            int ix = std::clamp(static_cast<int>(std::floor(px)), 0, static_cast<int>(src->width) - 1);
            int iy = std::clamp(static_cast<int>(std::floor(py)), 0, static_cast<int>(src->height) - 1);
            int ix_next = std::clamp(ix + 1, 0, static_cast<int>(src->width) - 1);
            int iy_next = std::clamp(iy + 1, 0, static_cast<int>(src->height) - 1);

            float dx = px - ix;
            float dy = py - iy;

            for (int c = 0; c < src->channels; ++c) {
                float val_tl = src->data[(iy * src->width + ix) * src->channels + c];
                float val_tr = src->data[(iy * src->width + ix_next) * src->channels + c];
                float val_bl = src->data[(iy_next * src->width + ix) * src->channels + c];
                float val_br = src->data[(iy_next * src->width + ix_next) * src->channels + c];

                float interpolated = (1.0f - dx) * (1.0f - dy) * val_tl +
                                     dx * (1.0f - dy) * val_tr +
                                     (1.0f - dx) * dy * val_bl +
                                     dx * dy * val_br;

                dst->data[(y * new_w + x) * src->channels + c] = static_cast<uint8_t>(std::clamp(interpolated, 0.0f, 255.0f));
            }
        }
    }

    if (cache && !cache_key.empty()) {
        cache->put(cache_key, dst);
    }
    return dst;
}

Image* apply_blur(const Image* src, int radius, FilterCache* cache, const std::string& cache_key) {
    if (!src || !src->data || radius < 0) return nullptr;
    if (radius == 0) return new Image(*src);

    if (cache && !cache_key.empty()) {
        Image* cached = cache->get(cache_key);
        if (cached) {
            VFSLogger::get_instance().info("Filter", "Using cached blurred image");
            return new Image(*cached);
        }
    }

    VFSLogger::get_instance().info("Filter", "Applying box blur filter with radius " + std::to_string(radius));
    
    int w = src->width;
    int h = src->height;
    int ch = src->channels;
    
    // Temporary image for horizontal pass
    Image temp(w, h, ch);
    Image* dst = new Image(w, h, ch);

    // Horizontal pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            std::vector<int> sums(ch, 0);
            int count = 0;
            for (int k = -radius; k <= radius; ++k) {
                int nx = std::clamp(x + k, 0, w - 1);
                for (int c = 0; c < ch; ++c) {
                    sums[c] += src->data[(y * w + nx) * ch + c];
                }
                count++;
            }
            for (int c = 0; c < ch; ++c) {
                temp.data[(y * w + x) * ch + c] = static_cast<uint8_t>(sums[c] / count);
            }
        }
    }

    // Vertical pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            std::vector<int> sums(ch, 0);
            int count = 0;
            for (int k = -radius; k <= radius; ++k) {
                int ny = std::clamp(y + k, 0, h - 1);
                for (int c = 0; c < ch; ++c) {
                    sums[c] += temp.data[(ny * w + x) * ch + c];
                }
                count++;
            }
            for (int c = 0; c < ch; ++c) {
                dst->data[(y * w + x) * ch + c] = static_cast<uint8_t>(sums[c] / count);
            }
        }
    }

    if (cache && !cache_key.empty()) {
        cache->put(cache_key, dst);
    }
    return dst;
}

Image* apply_crop(const Image* src, int x, int y, int w, int h, FilterCache* cache, const std::string& cache_key) {
    if (!src || !src->data || w <= 0 || h <= 0) return nullptr;

    if (cache && !cache_key.empty()) {
        Image* cached = cache->get(cache_key);
        if (cached) {
            VFSLogger::get_instance().info("Filter", "Using cached cropped image");
            return new Image(*cached);
        }
    }

    VFSLogger::get_instance().info("Filter", "Applying crop filter to region [x=" + std::to_string(x) + ", y=" + std::to_string(y) + ", w=" + std::to_string(w) + ", h=" + std::to_string(h) + "]");
    Image* dst = new Image(w, h, src->channels);

    for (int cy = 0; cy < h; ++cy) {
        for (int cx = 0; cx < w; ++cx) {
            int sx = x + cx;
            int sy = y + cy;
            int dst_idx = (cy * w + cx) * src->channels;
            if (sx >= 0 && sx < src->width && sy >= 0 && sy < src->height) {
                int src_idx = (sy * src->width + sx) * src->channels;
                std::memcpy(dst->data + dst_idx, src->data + src_idx, src->channels);
            } else {
                std::memset(dst->data + dst_idx, 0, src->channels); // Pad with black
            }
        }
    }

    if (cache && !cache_key.empty()) {
        cache->put(cache_key, dst);
    }
    return dst;
}

} // namespace PixelForge
